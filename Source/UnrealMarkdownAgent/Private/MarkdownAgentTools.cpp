// UnrealMarkdown - MIT License, see LICENSE in the plugin root.
// 本文件已按 ZBSStudio 的本地集成需要新增。

#include "MarkdownAgentTools.h"

// MarkdownAgentTools.h 在没装 ZBSAgent 时是空的，这里也跟着整段不编。
#if UE_ZBS_MARKDOWN_AGENT_TOOLS

#include "MarkdownFile.h"
#include "MarkdownSyntax.h"
#include "UnrealMarkdown.h"

#include "AssetImportTask.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetToolsModule.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "IAssetTools.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace MarkdownAgentPrivate
{
	/** 工具名。命名与 MCP 侧的 PascalCase UFUNCTION 刻意区分，统一走 snake_case。 */
	const TCHAR* const QueryTool = TEXT("markdown_query");
	const TCHAR* const EditTool = TEXT("markdown_edit");
	const TCHAR* const AssetTool = TEXT("markdown_asset");

	/** 单次 read 回传的正文上限，避免把超长文档整段塞进模型上下文。 */
	constexpr int32 DefaultMaxChars = 100000;

	/** search 的默认与最大命中条数。 */
	constexpr int32 DefaultSearchLimit = 50;
	constexpr int32 MaxSearchLimit = 500;

	/** 紧凑序列化，与项目其他 Agent 工具的结果写法保持一致。 */
	FString Json(const TSharedRef<FJsonObject>& Object)
	{
		FString Text;
		FJsonSerializer::Serialize(Object, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text));
		return Text;
	}

	/** 统一收尾：始终回填 bSucceeded，失败时补 error 与稳定的 errorCode。 */
	FString Respond(const TSharedRef<FJsonObject>& Object, bool bSucceeded, const FString& Error, const FString& ErrorCode = FString())
	{
		Object->SetBoolField(TEXT("bSucceeded"), bSucceeded);
		if (!Error.IsEmpty())
		{
			Object->SetStringField(TEXT("error"), Error);
		}
		if (!ErrorCode.IsEmpty())
		{
			Object->SetStringField(TEXT("errorCode"), ErrorCode);
		}
		return Json(Object);
	}

	/** 可选的字符串参数：缺失、null 或非字符串一律当作空串，不抛异常。 */
	FString GetOptionalString(const TSharedRef<FJsonObject>& Object, const TCHAR* Field)
	{
		const TSharedPtr<FJsonValue> Value = Object->TryGetField(Field);
		if (Value.IsValid() && Value->Type == EJson::String)
		{
			return Value->AsString();
		}
		return FString();
	}

	/**
	 * 把模型给出的文件夹写法归一化成 /Game 长包名。
	 *
	 * 接受 /Game/Docs、Game/Docs、Content/Docs、Docs 以及反斜杠写法。
	 */
	bool NormalizeFolder(const FString& Text, FString& OutFolder, FString& OutError)
	{
		FString Working = Text;
		Working.TrimStartAndEndInline();
		Working.ReplaceInline(TEXT("\\"), TEXT("/"));
		if (Working.IsEmpty())
		{
			OutError = TEXT("folder is empty; pass a content folder such as /Game/Docs or Docs.");
			return false;
		}
		if (!Working.StartsWith(TEXT("/")))
		{
			Working = TEXT("/") + Working;
		}
		if (!Working.StartsWith(TEXT("/Game")))
		{
			Working = Working.StartsWith(TEXT("/Content"))
				? TEXT("/Game") + Working.RightChop(8)
				: TEXT("/Game") + Working;
		}
		while (Working.Len() > 5 && Working.EndsWith(TEXT("/")))
		{
			Working.LeftChopInline(1);
		}
		if (!FPackageName::IsValidLongPackageName(Working, true))
		{
			OutError = FString::Printf(TEXT("'%s' is not a valid content folder."), *Text);
			return false;
		}
		OutFolder = Working;
		return true;
	}

	/**
	 * 把模型给出的资产写法归一化成长包名。
	 *
	 * 接受长包名、/Game 相对路径与 .uasset 磁盘路径；裸资产名配合 BaseFolder 使用。
	 */
	bool NormalizeAsset(const FString& Text, const FString& BaseFolder, FString& OutPackage, FString& OutError)
	{
		FString Working = Text;
		Working.TrimStartAndEndInline();
		Working.ReplaceInline(TEXT("\\"), TEXT("/"));
		Working.ReplaceInline(TEXT(".uasset"), TEXT(""));
		if (Working.IsEmpty())
		{
			OutError = TEXT("assetPath is empty; pass an asset package path or a bare asset name.");
			return false;
		}

		// 裸名字（不含 /）按 BaseFolder 补全，方便模型先列目录再按名字读。
		if (!Working.Contains(TEXT("/")))
		{
			if (BaseFolder.IsEmpty())
			{
				OutError = FString::Printf(TEXT("'%s' is a bare asset name; pass folder together with it."), *Text);
				return false;
			}
			Working = BaseFolder + TEXT("/") + Working;
		}
		else if (!Working.StartsWith(TEXT("/")))
		{
			Working = TEXT("/") + Working;
			if (!Working.StartsWith(TEXT("/Game")))
			{
				Working = Working.StartsWith(TEXT("/Content"))
					? TEXT("/Game") + Working.RightChop(8)
					: TEXT("/Game") + Working;
			}
		}

		if (!FPackageName::IsValidLongPackageName(Working))
		{
			OutError = FString::Printf(TEXT("'%s' is not a valid asset package path."), *Text);
			return false;
		}
		OutPackage = Working;
		return true;
	}

	UMarkdownFile* LoadAsset(const FString& PackageName)
	{
		if (PackageName.IsEmpty())
		{
			return nullptr;
		}
		return LoadObject<UMarkdownFile>(nullptr, *PackageName, nullptr, LOAD_Quiet);
	}

	/**
	 * 收集工程内所有 Markdown 资产的长包名；Folder 非空时只在该目录下找。
	 *
	 * 回传长包名而不是 FAssetData，有两个原因：软路径的 ToString 会带 .AssetName 后缀，
	 * 与工具参数要求的写法不一致；资产本体由 LoadAsset 统一按需加载。
	 */
	TArray<FString> CollectAssets(const FString& Folder, bool bRecursive)
	{
		FARFilter Filter;
		Filter.ClassPaths.Add(UMarkdownFile::StaticClass()->GetClassPathName());
		Filter.bRecursiveClasses = true;
		Filter.bRecursivePaths = bRecursive;
		if (!Folder.IsEmpty())
		{
			Filter.PackagePaths.Add(*Folder);
		}

		TArray<FAssetData> Assets;
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		Registry.GetAssets(Filter, Assets);

		TArray<FString> Names;
		Names.Reserve(Assets.Num());
		for (const FAssetData& Data : Assets)
		{
			const FString PackageName = Data.GetSoftObjectPath().GetLongPackageName();
			// 注册表里可能有磁盘上已经不存在的条目（资产被删但索引没刷新）。
			// 这种条目一旦去加载就会在 LinkerLoad 里 Fatal，把整个编辑器带走，必须先剔掉。
			if (!FPackageName::DoesPackageExist(PackageName))
			{
				continue;
			}
			Names.Add(PackageName);
		}
		Names.Sort();
		return Names;
	}

	/**
	 * 把资产写回它自己的包。
	 *
	 * 不能拿 Package->GetLoadedPath() 拼文件名：新建包时该路径为空，保存会静默失败。
	 */
	bool SaveAsset(UMarkdownFile* Asset)
	{
		if (!Asset || !Asset->GetOutermost())
		{
			return false;
		}

		UPackage* Package = Asset->GetOutermost();
		Package->MarkPackageDirty();

		const FString FileName = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		return UPackage::SavePackage(Package, Asset, *FileName, SaveArgs);
	}

	/** 把资产元数据填进 Out；不包含正文。 */
	void DescribeMetadata(const UMarkdownFile* Asset, const FString& PackageName, const TSharedRef<FJsonObject>& Out)
	{
		Out->SetStringField(TEXT("assetPath"), PackageName);
		Out->SetStringField(TEXT("name"), Asset->GetName());
		Out->SetNumberField(TEXT("lines"), Asset->GetLineCount());
		Out->SetNumberField(TEXT("chars"), Asset->GetCharCount());
		Out->SetStringField(TEXT("contentHash"), Asset->GetContentHash());
		Out->SetStringField(TEXT("sourceFile"), Asset->SourceFilePath);
		Out->SetBoolField(TEXT("dirty"), Asset->GetOutermost() ? Asset->GetOutermost()->IsDirty() : false);
	}

	/**
	 * 按参数里的 assetPath / folder 解析出一个已存在的资产。
	 *
	 * 失败时已填好 error 与 errorCode，调用方直接 Respond 即可。
	 */
	bool ResolveExisting(
		const TSharedRef<FJsonObject>& Args,
		UMarkdownFile*& OutAsset,
		FString& OutPackageName,
		FString& OutError,
		FString& OutErrorCode)
	{
		OutAsset = nullptr;
		OutPackageName.Reset();

		FString AssetText;
		if (!Args->TryGetStringField(TEXT("assetPath"), AssetText) || AssetText.TrimStartAndEnd().IsEmpty())
		{
			OutError = TEXT("assetPath is required.");
			OutErrorCode = TEXT("missing_asset_path");
			return false;
		}

		FString BaseFolder;
		const FString FolderText = GetOptionalString(Args, TEXT("folder"));
		if (!FolderText.TrimStartAndEnd().IsEmpty() && !NormalizeFolder(FolderText, BaseFolder, OutError))
		{
			OutErrorCode = TEXT("invalid_folder");
			return false;
		}

		if (!NormalizeAsset(AssetText, BaseFolder, OutPackageName, OutError))
		{
			OutErrorCode = TEXT("invalid_asset_path");
			return false;
		}

		OutAsset = LoadAsset(OutPackageName);
		if (!OutAsset)
		{
			OutError = FString::Printf(TEXT("No Markdown asset at %s."), *OutPackageName);
			OutErrorCode = TEXT("asset_not_found");
			return false;
		}
		return true;
	}

	/**
	 * 可选的内容指纹守卫。
	 *
	 * 模型先 markdown_query 拿到 contentHash，写入时带上 expectedHash；
	 * 期间若有人改过同一个资产，写入会被拒，避免把别人的改动覆盖掉。
	 */
	bool CheckExpectedHash(
		const TSharedRef<FJsonObject>& Args,
		const UMarkdownFile* Asset,
		FString& OutError,
		FString& OutErrorCode)
	{
		const FString Expected = GetOptionalString(Args, TEXT("expectedHash"));
		if (Expected.TrimStartAndEnd().IsEmpty())
		{
			return true;
		}
		const FString Actual = Asset->GetContentHash();
		if (!Expected.Equals(Actual, ESearchCase::IgnoreCase))
		{
			OutError = FString::Printf(
				TEXT("The asset changed since it was read: expected %s, found %s. Read it again before writing."),
				*Expected, *Actual);
			OutErrorCode = TEXT("stale_asset");
			return false;
		}
		return true;
	}

	// ========================================================================
	// markdown_query
	// ========================================================================

	FString ExecuteQuery(const TSharedRef<FJsonObject>& Args, bool& bOutSucceeded)
	{
		const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();

		FString Action;
		if (!Args->TryGetStringField(TEXT("action"), Action) || Action.TrimStartAndEnd().IsEmpty())
		{
			return Respond(Result, false, TEXT("action is required: list, read, info, outline or search."), TEXT("missing_action"));
		}
		Action = Action.TrimStartAndEnd().ToLower();

		if (Action == TEXT("list"))
		{
			FString Folder;
			FString FolderError;
			const FString FolderText = GetOptionalString(Args, TEXT("folder"));
			if (!FolderText.TrimStartAndEnd().IsEmpty() && !NormalizeFolder(FolderText, Folder, FolderError))
			{
				return Respond(Result, false, FolderError, TEXT("invalid_folder"));
			}

			bool bRecursive = true;
			Args->TryGetBoolField(TEXT("recursive"), bRecursive);

			TArray<TSharedPtr<FJsonValue>> Items;
			int32 Skipped = 0;
			for (const FString& PackageName : CollectAssets(Folder, bRecursive))
			{
				UMarkdownFile* Asset = LoadAsset(PackageName);
				if (!Asset)
				{
					++Skipped;
					continue;
				}
				const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
				DescribeMetadata(Asset, PackageName, Entry);
				Items.Add(MakeShared<FJsonValueObject>(Entry));
			}

			Result->SetNumberField(TEXT("count"), Items.Num());
			Result->SetNumberField(TEXT("skipped"), Skipped);
			Result->SetArrayField(TEXT("assets"), Items);
			bOutSucceeded = true;
			return Respond(Result, true, FString());
		}

		if (Action == TEXT("search"))
		{
			FString Query;
			if (!Args->TryGetStringField(TEXT("query"), Query) || Query.IsEmpty())
			{
				return Respond(Result, false, TEXT("search needs a non-empty query."), TEXT("missing_query"));
			}

			FString Folder;
			FString FolderError;
			const FString FolderText = GetOptionalString(Args, TEXT("folder"));
			if (!FolderText.TrimStartAndEnd().IsEmpty() && !NormalizeFolder(FolderText, Folder, FolderError))
			{
				return Respond(Result, false, FolderError, TEXT("invalid_folder"));
			}

			bool bRecursive = true;
			Args->TryGetBoolField(TEXT("recursive"), bRecursive);

			int32 Limit = DefaultSearchLimit;
			Args->TryGetNumberField(TEXT("limit"), Limit);
			Limit = FMath::Clamp(Limit, 1, MaxSearchLimit);

			TArray<TSharedPtr<FJsonValue>> Matches;
			bool bTruncated = false;
			int32 Skipped = 0;
			for (const FString& PackageName : CollectAssets(Folder, bRecursive))
			{
				if (bTruncated)
				{
					break;
				}

				UMarkdownFile* Asset = LoadAsset(PackageName);
				if (!Asset)
				{
					++Skipped;
					continue;
				}
				// 逐行找：模型关心的是「哪一行」，行号按 1 起算便于直接指认。
				TArray<FString> Lines;
				Asset->Content.ParseIntoArrayLines(Lines, /*InCullEmpty*/ false);
				for (int32 Index = 0; Index < Lines.Num(); ++Index)
				{
					if (!Lines[Index].Contains(Query, ESearchCase::IgnoreCase))
					{
						continue;
					}
					if (Matches.Num() >= Limit)
					{
						bTruncated = true;
						break;
					}
					const TSharedRef<FJsonObject> Hit = MakeShared<FJsonObject>();
					Hit->SetStringField(TEXT("assetPath"), PackageName);
					Hit->SetNumberField(TEXT("line"), Index + 1);
					Hit->SetStringField(TEXT("text"), Lines[Index]);
					Matches.Add(MakeShared<FJsonValueObject>(Hit));
				}
			}

			Result->SetStringField(TEXT("query"), Query);
			Result->SetNumberField(TEXT("count"), Matches.Num());
			Result->SetNumberField(TEXT("skipped"), Skipped);
			Result->SetBoolField(TEXT("truncated"), bTruncated);
			Result->SetArrayField(TEXT("matches"), Matches);
			bOutSucceeded = true;
			return Respond(Result, true, FString());
		}

		// read / info / outline 都要一个具体资产
		UMarkdownFile* Asset = nullptr;
		FString PackageName;
		FString Error;
		FString ErrorCode;
		if (!ResolveExisting(Args, Asset, PackageName, Error, ErrorCode))
		{
			return Respond(Result, false, Error, ErrorCode);
		}

		if (Action == TEXT("info"))
		{
			DescribeMetadata(Asset, PackageName, Result);
			bOutSucceeded = true;
			return Respond(Result, true, FString());
		}

		if (Action == TEXT("outline"))
		{
			TArray<TSharedPtr<FJsonValue>> Entries;
			for (const FMarkdownOutlineEntry& Entry : UMarkdownSyntaxLibrary::ExtractOutline(Asset->Content))
			{
				const TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
				// 大纲里的行号从 0 起算（编辑器内部用），对外统一成 1 起算。
				Item->SetNumberField(TEXT("line"), Entry.Line + 1);
				Item->SetNumberField(TEXT("level"), Entry.Level);
				Item->SetStringField(TEXT("text"), Entry.Text);
				Entries.Add(MakeShared<FJsonValueObject>(Item));
			}

			Result->SetStringField(TEXT("assetPath"), PackageName);
			Result->SetNumberField(TEXT("count"), Entries.Num());
			Result->SetArrayField(TEXT("entries"), Entries);
			bOutSucceeded = true;
			return Respond(Result, true, FString());
		}

		if (Action == TEXT("read"))
		{
			int32 MaxChars = DefaultMaxChars;
			Args->TryGetNumberField(TEXT("maxChars"), MaxChars);
			MaxChars = FMath::Clamp(MaxChars, 256, 200000);

			DescribeMetadata(Asset, PackageName, Result);
			const bool bTruncated = Asset->Content.Len() > MaxChars;
			Result->SetBoolField(TEXT("truncated"), bTruncated);
			Result->SetStringField(TEXT("content"), bTruncated ? Asset->Content.Left(MaxChars) : Asset->Content);
			bOutSucceeded = true;
			return Respond(Result, true, FString());
		}

		return Respond(Result, false, FString::Printf(TEXT("Unknown query action '%s'."), *Action), TEXT("unknown_action"));
	}

	// ========================================================================
	// markdown_edit
	// ========================================================================

	FString ExecuteEdit(const TSharedRef<FJsonObject>& Args, bool& bOutSucceeded)
	{
		const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();

		FString Action;
		if (!Args->TryGetStringField(TEXT("action"), Action) || Action.TrimStartAndEnd().IsEmpty())
		{
			return Respond(Result, false, TEXT("action is required: set, append, insert_line, replace or save."), TEXT("missing_action"));
		}
		Action = Action.TrimStartAndEnd().ToLower();

		UMarkdownFile* Asset = nullptr;
		FString PackageName;
		FString Error;
		FString ErrorCode;
		if (!ResolveExisting(Args, Asset, PackageName, Error, ErrorCode))
		{
			return Respond(Result, false, Error, ErrorCode);
		}
		if (!CheckExpectedHash(Args, Asset, Error, ErrorCode))
		{
			return Respond(Result, false, Error, ErrorCode);
		}

		// 默认立刻落盘：Agent 一轮里常常写完就不再回来，
		// 留一个脏包在内存里，后续 read 拿到的是新内容而磁盘还是旧的。
		bool bAutoSave = true;
		Args->TryGetBoolField(TEXT("autoSave"), bAutoSave);

		bool bChanged = false;
		if (Action == TEXT("set"))
		{
			FString Content;
			if (!Args->TryGetStringField(TEXT("content"), Content))
			{
				return Respond(Result, false, TEXT("set needs content (an empty document is still a legal value)."), TEXT("missing_content"));
			}
			bChanged = !Asset->Content.Equals(Content, ESearchCase::CaseSensitive);
			Asset->SetContent(Content);
		}
		else if (Action == TEXT("append"))
		{
			FString Text;
			if (!Args->TryGetStringField(TEXT("text"), Text) || Text.IsEmpty())
			{
				return Respond(Result, false, TEXT("append needs a non-empty text."), TEXT("missing_text"));
			}
			Asset->AppendText(Text);
			bChanged = true;
		}
		else if (Action == TEXT("insert_line"))
		{
			FString Text;
			if (!Args->TryGetStringField(TEXT("text"), Text) || Text.IsEmpty())
			{
				return Respond(Result, false, TEXT("insert_line needs a non-empty text."), TEXT("missing_text"));
			}

			int32 LineIndex = 0;
			if (!Args->TryGetNumberField(TEXT("lineIndex"), LineIndex) || LineIndex < 0)
			{
				return Respond(Result, false, TEXT("insert_line needs lineIndex, a zero-based line number."), TEXT("invalid_line_index"));
			}

			if (!Asset->InsertAtLine(LineIndex, Text))
			{
				return Respond(Result, false,
					FString::Printf(TEXT("lineIndex %d is past the end; the document has %d lines."), LineIndex, Asset->GetLineCount()),
					TEXT("invalid_line_index"));
			}
			bChanged = true;
		}
		else if (Action == TEXT("replace"))
		{
			FString OldText;
			FString NewText;
			if (!Args->TryGetStringField(TEXT("oldText"), OldText) || OldText.IsEmpty())
			{
				return Respond(Result, false, TEXT("replace needs a non-empty oldText."), TEXT("missing_text"));
			}
			Args->TryGetStringField(TEXT("newText"), NewText);

			bool bReplaceAll = false;
			Args->TryGetBoolField(TEXT("replaceAll"), bReplaceAll);

			const int32 Replaced = Asset->ReplaceInContent(OldText, NewText, bReplaceAll);
			Result->SetNumberField(TEXT("replaced"), Replaced);
			if (Replaced == 0)
			{
				return Respond(Result, false,
					FString::Printf(TEXT("oldText was not found in %s."), *PackageName),
					TEXT("text_not_found"));
			}
			bChanged = true;
		}
		else if (Action != TEXT("save"))
		{
			return Respond(Result, false, FString::Printf(TEXT("Unknown edit action '%s'."), *Action), TEXT("unknown_action"));
		}

		bool bSaved = false;
		if (bAutoSave || Action == TEXT("save"))
		{
			bSaved = SaveAsset(Asset);
			if (!bSaved)
			{
				Result->SetStringField(TEXT("assetPath"), PackageName);
				return Respond(Result, false,
					FString::Printf(TEXT("The edit was applied in memory but saving %s failed; the package is still dirty."), *PackageName),
					TEXT("save_failed"));
			}
		}

		Result->SetStringField(TEXT("action"), Action);
		Result->SetBoolField(TEXT("changed"), bChanged);
		Result->SetBoolField(TEXT("saved"), bSaved);
		DescribeMetadata(Asset, PackageName, Result);
		bOutSucceeded = true;
		return Respond(Result, true, FString());
	}

	// ========================================================================
	// markdown_asset
	// ========================================================================

	FString ExecuteAssetOps(const TSharedRef<FJsonObject>& Args, bool& bOutSucceeded)
	{
		const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();

		FString Action;
		if (!Args->TryGetStringField(TEXT("action"), Action) || Action.TrimStartAndEnd().IsEmpty())
		{
			return Respond(Result, false, TEXT("action is required: create, import, reimport, export or delete."), TEXT("missing_action"));
		}
		Action = Action.TrimStartAndEnd().ToLower();

		// assetPath 先只做语法归一化；create / import 要的是「尚不存在」的路径，
		// 因此不能走 ResolveExisting。
		FString AssetText;
		if (!Args->TryGetStringField(TEXT("assetPath"), AssetText) || AssetText.TrimStartAndEnd().IsEmpty())
		{
			return Respond(Result, false, TEXT("assetPath is required."), TEXT("missing_asset_path"));
		}

		FString PackageName;
		FString Error;
		if (!NormalizeAsset(AssetText, FString(), PackageName, Error))
		{
			return Respond(Result, false, Error, TEXT("invalid_asset_path"));
		}

		if (Action == TEXT("create"))
		{
			if (LoadAsset(PackageName))
			{
				return Respond(Result, false, FString::Printf(TEXT("%s already exists; use markdown_edit to change it."), *PackageName), TEXT("asset_exists"));
			}

			int32 LastSlash = INDEX_NONE;
			if (!PackageName.FindLastChar(TEXT('/'), LastSlash) || LastSlash <= 0 || LastSlash + 1 >= PackageName.Len())
			{
				return Respond(Result, false, TEXT("assetPath must look like /Game/Folder/Name."), TEXT("invalid_asset_path"));
			}

			FString Content;
			Args->TryGetStringField(TEXT("content"), Content);

			UPackage* Package = CreatePackage(*PackageName);
			if (!Package)
			{
				return Respond(Result, false, TEXT("Failed to create the destination package."), TEXT("create_failed"));
			}

			const FString AssetName = PackageName.RightChop(LastSlash + 1);
			UMarkdownFile* Created = NewObject<UMarkdownFile>(Package, *AssetName, RF_Public | RF_Standalone);
			if (!Created)
			{
				return Respond(Result, false, TEXT("Failed to create the asset object."), TEXT("create_failed"));
			}

			Created->SetContent(Content);
			FAssetRegistryModule::AssetCreated(Created);

			if (!SaveAsset(Created))
			{
				return Respond(Result, false, FString::Printf(TEXT("Created %s in memory but failed to save it."), *PackageName), TEXT("save_failed"));
			}

			Result->SetStringField(TEXT("action"), Action);
			DescribeMetadata(Created, PackageName, Result);
			bOutSucceeded = true;
			return Respond(Result, true, FString());
		}

		if (Action == TEXT("import"))
		{
			FString SourceFile;
			if (!Args->TryGetStringField(TEXT("sourceFile"), SourceFile) || SourceFile.IsEmpty())
			{
				return Respond(Result, false, TEXT("import needs sourceFile, the .md or .txt file to read."), TEXT("missing_source_file"));
			}
			SourceFile = FPaths::IsRelative(SourceFile)
				? FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), SourceFile))
				: FPaths::ConvertRelativePathToFull(SourceFile);

			if (!FPaths::FileExists(SourceFile))
			{
				return Respond(Result, false, FString::Printf(TEXT("Source file not found: %s"), *SourceFile), TEXT("source_file_missing"));
			}
			if (LoadAsset(PackageName))
			{
				return Respond(Result, false, FString::Printf(TEXT("%s already exists; use reimport or markdown_edit."), *PackageName), TEXT("asset_exists"));
			}

			int32 LastSlash = INDEX_NONE;
			if (!PackageName.FindLastChar(TEXT('/'), LastSlash) || LastSlash <= 0 || LastSlash + 1 >= PackageName.Len())
			{
				return Respond(Result, false, TEXT("assetPath must look like /Game/Folder/Name."), TEXT("invalid_asset_path"));
			}

			UAssetImportTask* Task = NewObject<UAssetImportTask>();
			Task->AddToRoot();
			Task->Filename = SourceFile;
			Task->DestinationPath = PackageName.Left(LastSlash);
			Task->DestinationName = PackageName.RightChop(LastSlash + 1);
			Task->bAutomated = true;
			Task->bReplaceExisting = false;
			Task->bSave = true;
			Task->bAsync = false;

			TArray<UAssetImportTask*> Tasks;
			Tasks.Add(Task);
			FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().ImportAssetTasks(Tasks);
			Task->RemoveFromRoot();

			UMarkdownFile* Imported = LoadAsset(PackageName);
			if (!Imported)
			{
				return Respond(Result, false,
					FString::Printf(TEXT("Import produced no Markdown asset at %s."), *PackageName),
					TEXT("import_failed"));
			}

			// 记下来源，之后 reimport 才有东西可回读。
			Imported->SourceFilePath = SourceFile;
			if (!SaveAsset(Imported))
			{
				return Respond(Result, false, TEXT("Imported but failed to save the source path."), TEXT("save_failed"));
			}

			Result->SetStringField(TEXT("action"), Action);
			DescribeMetadata(Imported, PackageName, Result);
			bOutSucceeded = true;
			return Respond(Result, true, FString());
		}

		// 其余 action 都要求资产已存在
		UMarkdownFile* Asset = LoadAsset(PackageName);
		if (!Asset)
		{
			return Respond(Result, false, FString::Printf(TEXT("No Markdown asset at %s."), *PackageName), TEXT("asset_not_found"));
		}

		if (Action == TEXT("reimport"))
		{
			if (Asset->SourceFilePath.IsEmpty())
			{
				return Respond(Result, false,
					FString::Printf(TEXT("%s was created inside the editor and has no source file to reimport from."), *PackageName),
					TEXT("no_source_file"));
			}
			if (!FPaths::FileExists(Asset->SourceFilePath))
			{
				return Respond(Result, false,
					FString::Printf(TEXT("Source file missing: %s"), *Asset->SourceFilePath),
					TEXT("source_file_missing"));
			}

			FString Loaded;
			if (!FFileHelper::LoadFileToString(Loaded, *Asset->SourceFilePath))
			{
				return Respond(Result, false,
					FString::Printf(TEXT("Cannot read %s"), *Asset->SourceFilePath),
					TEXT("source_file_unreadable"));
			}

			const bool bUnchanged = Asset->Content.Equals(Loaded, ESearchCase::CaseSensitive);
			if (!bUnchanged)
			{
				Asset->SetContent(Loaded);
				if (!SaveAsset(Asset))
				{
					return Respond(Result, false, TEXT("Reimported but failed to save."), TEXT("save_failed"));
				}
			}

			Result->SetStringField(TEXT("action"), Action);
			Result->SetBoolField(TEXT("changed"), !bUnchanged);
			DescribeMetadata(Asset, PackageName, Result);
			bOutSucceeded = true;
			return Respond(Result, true, FString());
		}

		if (Action == TEXT("export"))
		{
			FString Destination;
			if (!Args->TryGetStringField(TEXT("destinationFile"), Destination) || Destination.IsEmpty())
			{
				return Respond(Result, false, TEXT("export needs destinationFile, a .md path to write."), TEXT("missing_destination"));
			}
			Destination = FPaths::IsRelative(Destination)
				? FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), Destination))
				: FPaths::ConvertRelativePathToFull(Destination);

			IFileManager::Get().MakeDirectory(*FPaths::GetPath(Destination), true);
			if (!Asset->ExportToFile(Destination))
			{
				return Respond(Result, false,
					FString::Printf(TEXT("Failed to write %s"), *Destination),
					TEXT("export_failed"));
			}

			Result->SetStringField(TEXT("action"), Action);
			Result->SetStringField(TEXT("file"), Destination);
			Result->SetNumberField(TEXT("bytes"), IFileManager::Get().FileSize(*Destination));
			DescribeMetadata(Asset, PackageName, Result);
			bOutSucceeded = true;
			return Respond(Result, true, FString());
		}

		if (Action == TEXT("delete"))
		{
			// ForceDeleteObjects 会连 .uasset 文件一起删；DeleteSingleObject 只清内存，
			// 会留下一个磁盘上还在、编辑器里看不到的孤儿包。
			TArray<UObject*> Objects;
			Objects.Add(Asset);

			const int32 Deleted = ObjectTools::ForceDeleteObjects(Objects, /*bShowConfirmation*/ false);
			if (Deleted == 0)
			{
				return Respond(Result, false,
					FString::Printf(TEXT("Failed to delete %s; it may still be referenced."), *PackageName),
					TEXT("delete_failed"));
			}

			Result->SetStringField(TEXT("action"), Action);
			Result->SetStringField(TEXT("assetPath"), PackageName);
			Result->SetNumberField(TEXT("deleted"), Deleted);
			bOutSucceeded = true;
			return Respond(Result, true, FString());
		}

		return Respond(Result, false, FString::Printf(TEXT("Unknown asset action '%s'."), *Action), TEXT("unknown_action"));
	}
}

using namespace MarkdownAgentPrivate;

FString FMarkdownAgentTools::GetSchema() const
{
	const FString Schema = TEXT(R"json([
{"type":"function","function":{"name":"markdown_query","description":"Read Markdown assets stored as engine assets under /Game. action=list enumerates MarkdownFile assets, optionally under folder, with path, line count, character count, content hash and source file. action=read returns the raw text (see truncated and maxChars). action=info returns the same metadata without the text. action=outline returns every heading as line/level/text. action=search takes a literal query and reports which assets and which 1-based lines contain it, case-insensitively. folder accepts /Game/Docs, Content/Docs or Docs; assetPath also accepts a bare asset name when folder is given. Read before writing, and keep contentHash for expectedHash.","parameters":{"type":"object","properties":{"action":{"type":"string","enum":["list","read","info","outline","search"]},"assetPath":{"type":"string"},"folder":{"type":"string"},"query":{"type":"string"},"recursive":{"type":"boolean"},"limit":{"type":"integer","minimum":1,"maximum":500},"maxChars":{"type":"integer","minimum":256,"maximum":200000}},"required":["action"],"additionalProperties":false}}},
{"type":"function","function":{"name":"markdown_edit","description":"Change the text of an existing Markdown asset. action=set replaces the whole document (an empty string empties it), action=append adds text at the very end, action=insert_line inserts at a zero-based lineIndex where an index equal to the current line count appends, action=replace swaps a literal oldText and reports how many places changed, action=save only writes the current content to disk. Every content change saves the package immediately unless autoSave is false, because a dirty package left in memory makes later reads disagree with the file on disk. Returns lines, chars, contentHash and whether the file was saved. Pass expectedHash from markdown_query to refuse the write when somebody else changed the asset first.","parameters":{"type":"object","properties":{"action":{"type":"string","enum":["set","append","insert_line","replace","save"]},"assetPath":{"type":"string"},"folder":{"type":"string"},"content":{"type":"string"},"text":{"type":"string"},"lineIndex":{"type":"integer","minimum":0},"oldText":{"type":"string"},"newText":{"type":"string"},"replaceAll":{"type":"boolean"},"autoSave":{"type":"boolean"},"expectedHash":{"type":"string"}},"required":["action","assetPath"],"additionalProperties":false}}},
{"type":"function","function":{"name":"markdown_asset","description":"Manage the lifetime of Markdown assets. action=create makes a new asset from content and never overwrites an existing one. action=import reads a .md or .txt file from disk into a new asset, records the source path so reimport works later, and saves; the .txt extension is accepted without asking. action=reimport reloads the recorded source file and reports changed=false when the text already matches. action=export writes the asset text to destinationFile as UTF-8. action=delete removes the asset together with its .uasset file and fails when the asset is still referenced. Relative paths resolve against the project directory.","parameters":{"type":"object","properties":{"action":{"type":"string","enum":["create","import","reimport","export","delete"]},"assetPath":{"type":"string"},"content":{"type":"string"},"sourceFile":{"type":"string"},"destinationFile":{"type":"string"}},"required":["action","assetPath"],"additionalProperties":false}}}
])json");

	// schema 少写一个花括号会让整段解析失败、工具从模型清单里静默消失，因此这里必须报错。
	TArray<TSharedPtr<FJsonValue>> Tools;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Schema), Tools))
	{
		UE_LOG(LogUnrealMarkdown, Error, TEXT("Markdown Agent 工具 schema 不是合法 JSON，这些工具不会下发给模型。"));
		return FString();
	}

	FString Serialized;
	FJsonSerializer::Serialize(Tools, TJsonWriterFactory<>::Create(&Serialized));
	return Serialized;
}

int32 FMarkdownAgentTools::GetRequiredPermissionLevel(const FString& ToolName) const
{
	// 读取不动任何东西；改文本要 3 级；建 / 删 / 导入导出资产要 4 级。
	// 这个划分同时决定宿主按等级推导的写入总闸（3 文本 / 4 资产）。
	if (ToolName == QueryTool)
	{
		return 2;
	}
	if (ToolName == EditTool)
	{
		return 3;
	}
	if (ToolName == AssetTool)
	{
		return 4;
	}
	return 0;
}

bool FMarkdownAgentTools::IsRuntimeTool(const FString& ToolName) const
{
	return false;
}

FString FMarkdownAgentTools::Execute(const FZBSAgentToolCall& Call, bool& bOutSucceeded)
{
	bOutSucceeded = false;

	TSharedPtr<FJsonObject> Arguments;
	const FString Trimmed = Call.Arguments.TrimStartAndEnd();
	if (!Trimmed.IsEmpty())
	{
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Call.Arguments), Arguments) || !Arguments.IsValid())
		{
			const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
			return Respond(Result, false, TEXT("Arguments are not valid JSON."), TEXT("invalid_arguments"));
		}
	}
	if (!Arguments.IsValid())
	{
		Arguments = MakeShared<FJsonObject>();
	}

	if (Call.Name == QueryTool)
	{
		return ExecuteQuery(Arguments.ToSharedRef(), bOutSucceeded);
	}
	if (Call.Name == EditTool)
	{
		return ExecuteEdit(Arguments.ToSharedRef(), bOutSucceeded);
	}
	if (Call.Name == AssetTool)
	{
		return ExecuteAssetOps(Arguments.ToSharedRef(), bOutSucceeded);
	}

	const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	return Respond(Result, false, TEXT("Unknown Markdown tool."), TEXT("unknown_tool"));
}

#endif // UE_ZBS_MARKDOWN_AGENT_TOOLS

// UnrealMarkdown - MIT License, see LICENSE in the plugin root.
// 本文件已按 ZBSStudio 的本地集成需要扩充：补齐增删查改与导入 / 重导入。

#include "MarkdownToolset.h"
#include "MarkdownFile.h"
#include "MarkdownSyntax.h"
#include "UnrealMarkdown.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "ObjectTools.h"
#include "AssetImportTask.h"
#include "EditorReimportHandler.h"
#include "UObject/SavePackage.h"

#define LOCTEXT_NAMESPACE "MarkdownToolset"

// ============================================================================
// Helpers
// ============================================================================

static UMarkdownFile* LoadMarkdownAsset(const FString& AssetPath)
{
	return LoadObject<UMarkdownFile>(nullptr, *AssetPath, nullptr, LOAD_Quiet);
}

static TArray<FAssetData> GetAllMarkdownAssets()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(UMarkdownFile::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;
	Filter.bRecursivePaths = true;

	TArray<FAssetData> Assets;
	AssetRegistry.GetAssets(Filter, Assets);
	return Assets;
}

/**
 * 把资产写回它自己的包。
 *
 * 早先直接拿 Package->GetLoadedPath() 拼文件名，新建包时这个路径是空的，
 * 保存会静默失败；这里统一按包名推算物理路径。
 */
static bool SaveMarkdownAsset(UMarkdownFile* Asset)
{
	if (!Asset)
	{
		return false;
	}

	UPackage* Package = Asset->GetOutermost();
	if (!Package)
	{
		return false;
	}

	Package->MarkPackageDirty();

	const FString PackageName = Package->GetName();
	const FString FileName = FPackageName::LongPackageNameToFilename(
		PackageName, FPackageName::GetAssetPackageExtension());

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	return UPackage::SavePackage(Package, Asset, *FileName, SaveArgs);
}

// ============================================================================
// Read Operations
// ============================================================================

TArray<FString> UMarkdownToolset::ListMarkdownFiles()
{
	TArray<FString> Paths;
	for (const FAssetData& Data : GetAllMarkdownAssets())
	{
		Paths.Add(Data.GetSoftObjectPath().ToString());
	}
	Paths.Sort();
	return Paths;
}

FString UMarkdownToolset::ReadMarkdownContent(const FString& AssetPath)
{
	UMarkdownFile* Md = LoadMarkdownAsset(AssetPath);
	if (!Md)
	{
		return FString::Printf(TEXT("Error: Asset not found: %s"), *AssetPath);
	}
	return Md->Content;
}

FString UMarkdownToolset::GetMarkdownInfo(const FString& AssetPath)
{
	UMarkdownFile* Md = LoadMarkdownAsset(AssetPath);
	if (!Md)
	{
		return FString::Printf(TEXT("{\"error\":\"Asset not found: %s\"}"), *AssetPath);
	}

	return FString::Printf(
		TEXT("{\"name\":\"%s\",\"path\":\"%s\",\"contentLength\":%d,\"lineCount\":%d,\"sourceFile\":\"%s\"}"),
		*Md->GetName(),
		*AssetPath,
		Md->Content.Len(),
		Md->GetLineCount(),
		*Md->SourceFilePath);
}

// ============================================================================
// Write Operations
// ============================================================================

FString UMarkdownToolset::SetMarkdownContent(const FString& AssetPath, const FString& NewContent)
{
	UMarkdownFile* Md = LoadMarkdownAsset(AssetPath);
	if (!Md)
	{
		return FString::Printf(TEXT("Error: Asset not found: %s"), *AssetPath);
	}

	Md->SetContent(NewContent);

	if (!SaveMarkdownAsset(Md))
	{
		return FString::Printf(TEXT("Error: Failed to save %s"), *AssetPath);
	}

	return FString::Printf(TEXT("OK: Updated content of %s (%d chars)"), *AssetPath, NewContent.Len());
}

FString UMarkdownToolset::CreateMarkdownFile(const FString& AssetPath, const FString& InitialContent)
{
	FString PackagePath = AssetPath;
	FString AssetName;
	int32 LastSlash;
	if (PackagePath.FindLastChar(TEXT('/'), LastSlash))
	{
		AssetName = PackagePath.RightChop(LastSlash + 1);
		PackagePath = PackagePath.Left(LastSlash);
	}
	else
	{
		AssetName = PackagePath;
		PackagePath = TEXT("/Game");
	}

	if (AssetName.IsEmpty()) return TEXT("Error: Invalid asset name.");
	if (LoadMarkdownAsset(AssetPath)) return FString::Printf(TEXT("Error: Asset already exists: %s"), *AssetPath);

	UPackage* Package = CreatePackage(*(PackagePath / AssetName));
	if (!Package) return TEXT("Error: Failed to create package.");

	UMarkdownFile* NewAsset = NewObject<UMarkdownFile>(Package, UMarkdownFile::StaticClass(), *AssetName, RF_Public | RF_Standalone);
	if (!NewAsset) return TEXT("Error: Failed to create UMarkdownFile object.");

	NewAsset->Content = InitialContent;
	NewAsset->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(NewAsset);

	if (!SaveMarkdownAsset(NewAsset))
	{
		return TEXT("Error: Failed to save the new asset.");
	}

	return FString::Printf(TEXT("OK: Created %s (%d chars)"), *AssetPath, InitialContent.Len());
}

FString UMarkdownToolset::ExportMarkdownFile(const FString& AssetPath, const FString& OutputFilePath)
{
	UMarkdownFile* Md = LoadMarkdownAsset(AssetPath);
	if (!Md) return FString::Printf(TEXT("Error: Asset not found: %s"), *AssetPath);

	if (FFileHelper::SaveStringToFile(Md->Content, *OutputFilePath))
		return FString::Printf(TEXT("OK: Exported %s -> %s"), *AssetPath, *OutputFilePath);
	return FString::Printf(TEXT("Error: Failed to write to %s"), *OutputFilePath);
}

TArray<FString> UMarkdownToolset::SearchMarkdownFiles(const FString& SearchTerm)
{
	TArray<FString> Matches;
	FString LowerTerm = SearchTerm.ToLower();
	for (const FAssetData& Data : GetAllMarkdownAssets())
	{
		UMarkdownFile* Md = LoadMarkdownAsset(Data.GetSoftObjectPath().ToString());
		if (Md && Md->Content.ToLower().Contains(LowerTerm))
			Matches.Add(Data.GetSoftObjectPath().ToString());
	}
	Matches.Sort();
	return Matches;
}

// ============================================================================
// 大纲与局部修改
// ============================================================================

TArray<FString> UMarkdownToolset::GetMarkdownOutline(const FString& AssetPath)
{
	TArray<FString> Result;

	UMarkdownFile* Md = LoadMarkdownAsset(AssetPath);
	if (!Md)
	{
		return Result;
	}

	for (const FMarkdownOutlineEntry& Entry : UMarkdownSyntaxLibrary::ExtractOutline(Md->Content))
	{
		Result.Add(FString::Printf(TEXT("%d|%d|%s"), Entry.Line, Entry.Level, *Entry.Text));
	}
	return Result;
}

FString UMarkdownToolset::AppendMarkdownContent(const FString& AssetPath, const FString& Text)
{
	UMarkdownFile* Md = LoadMarkdownAsset(AssetPath);
	if (!Md)
	{
		return FString::Printf(TEXT("Error: Asset not found: %s"), *AssetPath);
	}
	if (Text.IsEmpty())
	{
		return TEXT("Error: Text is empty.");
	}

	// 资产末尾没有换行时先补一个，否则新内容会粘到最后一行末尾。
	FString Payload = Text;
	if (!Md->Content.IsEmpty() && !Md->Content.EndsWith(TEXT("\n")))
	{
		Payload = TEXT("\n") + Payload;
	}

	Md->AppendText(Payload);
	if (!SaveMarkdownAsset(Md))
	{
		return FString::Printf(TEXT("Error: Failed to save %s"), *AssetPath);
	}

	return FString::Printf(TEXT("OK: Appended %d chars to %s, %d lines now"),
		Payload.Len(), *AssetPath, Md->GetLineCount());
}

FString UMarkdownToolset::ReplaceInMarkdownContent(const FString& AssetPath, const FString& OldText, const FString& NewText, bool bReplaceAll)
{
	UMarkdownFile* Md = LoadMarkdownAsset(AssetPath);
	if (!Md)
	{
		return FString::Printf(TEXT("Error: Asset not found: %s"), *AssetPath);
	}

	const int32 Replaced = Md->ReplaceInContent(OldText, NewText, bReplaceAll);
	if (Replaced == 0)
	{
		return FString::Printf(TEXT("Error: '%s' not found in %s"), *OldText, *AssetPath);
	}

	if (!SaveMarkdownAsset(Md))
	{
		return FString::Printf(TEXT("Error: Replaced %d occurrence(s) but failed to save %s"), Replaced, *AssetPath);
	}

	return FString::Printf(TEXT("OK: Replaced %d occurrence(s) in %s"), Replaced, *AssetPath);
}

FString UMarkdownToolset::SaveMarkdownFile(const FString& AssetPath)
{
	UMarkdownFile* Md = LoadMarkdownAsset(AssetPath);
	if (!Md)
	{
		return FString::Printf(TEXT("Error: Asset not found: %s"), *AssetPath);
	}

	if (!SaveMarkdownAsset(Md))
	{
		return FString::Printf(TEXT("Error: Failed to save %s"), *AssetPath);
	}

	return FString::Printf(TEXT("OK: Saved %s (%d chars)"), *AssetPath, Md->Content.Len());
}

// ============================================================================
// 资产生命周期
// ============================================================================

FString UMarkdownToolset::RenameMarkdownFile(const FString& AssetPath, const FString& NewAssetPath)
{
	UMarkdownFile* Md = LoadMarkdownAsset(AssetPath);
	if (!Md)
	{
		return FString::Printf(TEXT("Error: Asset not found: %s"), *AssetPath);
	}

	int32 LastSlash = INDEX_NONE;
	if (!NewAssetPath.FindLastChar(TEXT('/'), LastSlash) || LastSlash <= 0
		|| LastSlash + 1 >= NewAssetPath.Len())
	{
		return TEXT("Error: NewAssetPath must look like /Game/Folder/Name.");
	}

	const FString NewPackageName = NewAssetPath.Left(LastSlash);
	const FString NewAssetName = NewAssetPath.RightChop(LastSlash + 1);

	if (!NewPackageName.StartsWith(TEXT("/Game/")))
	{
		return TEXT("Error: NewAssetPath must live under /Game/.");
	}
	if (LoadMarkdownAsset(NewAssetPath))
	{
		return FString::Printf(TEXT("Error: Target already exists: %s"), *NewAssetPath);
	}

	UPackage* OldPackage = Md->GetOutermost();
	if (!OldPackage)
	{
		return TEXT("Error: Asset has no outer package.");
	}
	const FString OldPackageName = OldPackage->GetName();

	// 这里是复制 + 删除，不是引擎原生的重命名：
	// 原生重命名会顺带改引用者，而这个插件没接 AssetRenameManager，
	// 因此引用旧路径的资产不会自动改道，调用方要自己更新。
	UPackage* NewPackage = CreatePackage(*FString::Printf(TEXT("%s/%s"), *NewPackageName, *NewAssetName));
	if (!NewPackage)
	{
		return TEXT("Error: Failed to create the destination package.");
	}

	UMarkdownFile* Copy = DuplicateObject<UMarkdownFile>(Md, NewPackage, *NewAssetName);
	if (!Copy)
	{
		return TEXT("Error: Failed to duplicate the asset.");
	}

	Copy->SourceFilePath = Md->SourceFilePath;
	FAssetRegistryModule::AssetCreated(Copy);

	if (!SaveMarkdownAsset(Copy))
	{
		return FString::Printf(TEXT("Error: Failed to save %s"), *NewAssetPath);
	}

	ObjectTools::DeleteSingleObject(Md, /*bPerformReferenceCheck*/ false);

	return FString::Printf(TEXT("OK: Moved %s -> %s (copied then deleted; references to the old path were not rewritten)"),
		*OldPackageName, *NewAssetPath);
}

FString UMarkdownToolset::DeleteMarkdownFile(const FString& AssetPath)
{
	UMarkdownFile* Md = LoadMarkdownAsset(AssetPath);
	if (!Md)
	{
		return FString::Printf(TEXT("Error: Asset not found: %s"), *AssetPath);
	}

	const FString PackageName = Md->GetOutermost() ? Md->GetOutermost()->GetName() : AssetPath;

	// 不做引用检查：Markdown 资产通常只被文档类引用，强删比留半边更干净。
	if (!ObjectTools::DeleteSingleObject(Md, /*bPerformReferenceCheck*/ false))
	{
		return FString::Printf(TEXT("Error: Failed to delete %s"), *AssetPath);
	}

	return FString::Printf(TEXT("OK: Deleted %s (package %s)"), *AssetPath, *PackageName);
}

FString UMarkdownToolset::ImportMarkdownFile(const FString& SourceFile, const FString& AssetPath)
{
	if (!FPaths::FileExists(SourceFile))
	{
		return FString::Printf(TEXT("Error: Source file not found: %s"), *SourceFile);
	}

	int32 LastSlash = INDEX_NONE;
	if (!AssetPath.FindLastChar(TEXT('/'), LastSlash) || LastSlash <= 0
		|| LastSlash + 1 >= AssetPath.Len())
	{
		return TEXT("Error: AssetPath must look like /Game/Folder/Name.");
	}

	const FString DestinationPath = AssetPath.Left(LastSlash);
	const FString DestinationName = AssetPath.RightChop(LastSlash + 1);

	if (LoadMarkdownAsset(AssetPath))
	{
		return FString::Printf(TEXT("Error: Asset already exists: %s"), *AssetPath);
	}

	UAssetImportTask* Task = NewObject<UAssetImportTask>();
	Task->AddToRoot();
	Task->Filename = SourceFile;
	Task->DestinationPath = DestinationPath;
	Task->DestinationName = DestinationName;
	Task->bAutomated = true;
	Task->bReplaceExisting = false;
	Task->bSave = true;
	Task->bAsync = false;

	TArray<UAssetImportTask*> Tasks;
	Tasks.Add(Task);

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().ImportAssetTasks(Tasks);
	Task->RemoveFromRoot();

	if (Task->ImportedObjectPaths.Num() == 0)
	{
		return FString::Printf(TEXT("Error: Import produced no asset for %s"), *SourceFile);
	}

	// 记下源文件，之后才能重导入。
	if (UMarkdownFile* Imported = LoadMarkdownAsset(AssetPath))
	{
		Imported->SourceFilePath = FPaths::ConvertRelativePathToFull(SourceFile);
		SaveMarkdownAsset(Imported);
	}

	return FString::Printf(TEXT("OK: Imported %s -> %s"), *SourceFile, *AssetPath);
}

FString UMarkdownToolset::ReimportMarkdownFile(const FString& AssetPath)
{
	UMarkdownFile* Md = LoadMarkdownAsset(AssetPath);
	if (!Md)
	{
		return FString::Printf(TEXT("Error: Asset not found: %s"), *AssetPath);
	}
	if (Md->SourceFilePath.IsEmpty())
	{
		return FString::Printf(TEXT("Error: %s was created in the editor, there is nothing to reimport."), *AssetPath);
	}
	if (!FPaths::FileExists(Md->SourceFilePath))
	{
		return FString::Printf(TEXT("Error: Source file missing: %s"), *Md->SourceFilePath);
	}

	FString Loaded;
	if (!FFileHelper::LoadFileToString(Loaded, *Md->SourceFilePath))
	{
		return FString::Printf(TEXT("Error: Cannot read %s"), *Md->SourceFilePath);
	}

	if (Md->Content.Equals(Loaded, ESearchCase::CaseSensitive))
	{
		return FString::Printf(TEXT("OK: No change, %s already matches %s"), *AssetPath, *Md->SourceFilePath);
	}

	Md->SetContent(Loaded);
	if (!SaveMarkdownAsset(Md))
	{
		return FString::Printf(TEXT("Error: Reimported but failed to save %s"), *AssetPath);
	}

	return FString::Printf(TEXT("OK: Reimported %s from %s (%d chars)"),
		*AssetPath, *Md->SourceFilePath, Loaded.Len());
}

#undef LOCTEXT_NAMESPACE

// UnrealMarkdown - MIT License, see LICENSE in the plugin root.
// 新增文件：Agent 侧 Markdown 工具的契约与往返测试。

#include "Misc/AutomationTest.h"

// 工具本体不存在时这些用例也没有意义，因此一同包在宏里。
#if WITH_AUTOMATION_TESTS && WITH_EDITOR && UE_ZBS_MARKDOWN_AGENT_TOOLS

#include "MarkdownAgentTools.h"
#include "MarkdownFile.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace MarkdownAgentTestPrivate
{
	/** 测试资产固定在 _Working 下，方便清理时一眼认出来。 */
	const TCHAR* const TestPackage = TEXT("/Game/_Working/MarkdownAgent/Request01/DA_AgentToolTest");

	TSharedPtr<FJsonObject> Parse(const FString& Text)
	{
		TSharedPtr<FJsonObject> Object;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Object))
		{
			return nullptr;
		}
		return Object;
	}

	/** 调一次工具并返回解析后的结果；解析失败时 OutSucceeded 为 false 且返回空指针。 */
	TSharedPtr<FJsonObject> Run(FMarkdownAgentTools& Tools, const FString& Name, const FString& Arguments, bool& bOutSucceeded)
	{
		FZBSAgentToolCall Call;
		Call.Name = Name;
		Call.Arguments = Arguments;
		const FString Output = Tools.Execute(Call, bOutSucceeded);
		const TSharedPtr<FJsonObject> Root = Parse(Output);
		if (!Root.IsValid())
		{
			bOutSucceeded = false;
		}
		return Root;
	}

	/**
	 * 删掉测试资产，连带磁盘文件。
	 *
	 * 必须先让资产从内存里卸载再删文件：只删 .uasset 会把一个「内存里有、磁盘上没有」
	 * 的包留在包表里，后续任何加载都会在 LinkerLoad 里 Fatal。
	 */
	void RemoveTestAsset()
	{
		if (UMarkdownFile* Asset = LoadObject<UMarkdownFile>(nullptr, TestPackage, nullptr, LOAD_Quiet))
		{
			TArray<UObject*> Objects;
			Objects.Add(Asset);
			ObjectTools::ForceDeleteObjects(Objects, /*ShowConfirmation*/ false);
		}

		const FString Filename = FPackageName::LongPackageNameToFilename(
			TestPackage, FPackageName::GetAssetPackageExtension());
		IFileManager::Get().Delete(*Filename, /*RequireExists*/ false, /*EvenReadOnly*/ true, /*Quiet*/ true);
	}
}

// ============================================================================
// 契约：schema 与权限等级
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMarkdownAgentToolContractTest, "UnrealMarkdown.AgentToolContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMarkdownAgentToolContractTest::RunTest(const FString& Parameters)
{
	FMarkdownAgentTools Tools;

	// ---- schema 合法且工具名齐全 ----
	const FString Schema = Tools.GetSchema();
	if (!TestFalse(TEXT("schema 非空"), Schema.IsEmpty()))
	{
		return false;
	}

	TArray<TSharedPtr<FJsonValue>> Declared;
	if (!TestTrue(TEXT("schema 是合法 JSON"),
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Schema), Declared)))
	{
		return false;
	}
	TestEqual(TEXT("声明了三个工具"), Declared.Num(), 3);

	const TArray<FString> Expected = { TEXT("markdown_query"), TEXT("markdown_edit"), TEXT("markdown_asset") };
	TSet<FString> InSchema;
	for (const TSharedPtr<FJsonValue>& Value : Declared)
	{
		const TSharedPtr<FJsonObject> Entry = Value->AsObject();
		const TSharedPtr<FJsonObject> Function = Entry.IsValid() ? Entry->GetObjectField(TEXT("function")) : nullptr;
		if (Function.IsValid())
		{
			InSchema.Add(Function->GetStringField(TEXT("name")));
			// 每个工具都必须有参数定义，否则模型没法构造调用
			TestTrue(FString::Printf(TEXT("%s 带 parameters"), *Function->GetStringField(TEXT("name"))),
				Function->HasField(TEXT("parameters")));
		}
	}
	for (const FString& Name : Expected)
	{
		TestTrue(FString::Printf(TEXT("schema 含 %s"), *Name), InSchema.Contains(Name));
		TestTrue(FString::Printf(TEXT("%s 有权限等级"), *Name), Tools.GetRequiredPermissionLevel(Name) > 0);
	}

	// ---- 等级划分：读 2 / 改文本 3 / 动资产 4，与宿主按等级推导写入总闸的规则一致 ----
	TestEqual(TEXT("查询是 2 级"), Tools.GetRequiredPermissionLevel(TEXT("markdown_query")), 2);
	TestEqual(TEXT("改文本是 3 级"), Tools.GetRequiredPermissionLevel(TEXT("markdown_edit")), 3);
	TestEqual(TEXT("动资产是 4 级"), Tools.GetRequiredPermissionLevel(TEXT("markdown_asset")), 4);
	TestEqual(TEXT("未知工具等级为 0"), Tools.GetRequiredPermissionLevel(TEXT("markdown_unknown")), 0);

	// ---- 都不驱动运行中的世界 ----
	TestFalse(TEXT("查询不走运行时通路"), Tools.IsRuntimeTool(TEXT("markdown_query")));
	TestFalse(TEXT("编辑不走运行时通路"), Tools.IsRuntimeTool(TEXT("markdown_edit")));
	TestFalse(TEXT("资产操作不走运行时通路"), Tools.IsRuntimeTool(TEXT("markdown_asset")));

	// ---- 坏参数一律给稳定的 errorCode，而不是崩或静默成功 ----
	{
		bool bSucceeded = true;
		const TSharedPtr<FJsonObject> Root = MarkdownAgentTestPrivate::Run(Tools, TEXT("markdown_query"), TEXT("{not json"), bSucceeded);
		TestFalse(TEXT("坏 JSON 判失败"), bSucceeded);
		if (Root.IsValid())
		{
			TestEqual(TEXT("坏 JSON 有 errorCode"), Root->GetStringField(TEXT("errorCode")), FString(TEXT("invalid_arguments")));
		}
	}
	{
		bool bSucceeded = true;
		const TSharedPtr<FJsonObject> Root = MarkdownAgentTestPrivate::Run(Tools, TEXT("markdown_query"), TEXT("{}"), bSucceeded);
		TestFalse(TEXT("缺 action 判失败"), bSucceeded);
		if (Root.IsValid())
		{
			TestEqual(TEXT("缺 action 有 errorCode"), Root->GetStringField(TEXT("errorCode")), FString(TEXT("missing_action")));
		}
	}
	{
		bool bSucceeded = true;
		MarkdownAgentTestPrivate::Run(Tools, TEXT("markdown_unknown"), TEXT("{}"), bSucceeded);
		TestFalse(TEXT("未知工具判失败"), bSucceeded);
	}

	return true;
}

// ============================================================================
// 往返：建 → 查 → 改 → 导出 → 删
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMarkdownAgentToolRoundTripTest, "UnrealMarkdown.AgentToolRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMarkdownAgentToolRoundTripTest::RunTest(const FString& Parameters)
{
	FMarkdownAgentTools Tools;

	// 上一轮若中途失败会留下资产，先清掉保证从已知状态开始
	MarkdownAgentTestPrivate::RemoveTestAsset();

	const FString PackagePath = MarkdownAgentTestPrivate::TestPackage;
	const FString FirstContent = TEXT("# Agent Note\n\nbody line\n\nThe Unique Token line\n");

	// ---- create ----
	{
		bool bSucceeded = false;
		const FString Arguments = FString::Printf(
			TEXT("{\"action\":\"create\",\"assetPath\":\"%s\",\"content\":\"%s\"}"),
			*PackagePath, *FirstContent.Replace(TEXT("\n"), TEXT("\\n")));
		const TSharedPtr<FJsonObject> Root = MarkdownAgentTestPrivate::Run(Tools, TEXT("markdown_asset"), Arguments, bSucceeded);
		if (!TestTrue(TEXT("创建成功"), bSucceeded))
		{
			return false;
		}
		TestEqual(TEXT("创建后行数为 6（末尾换行也算一行）"), static_cast<int32>(Root->GetNumberField(TEXT("lines"))), 6);
		TestTrue(TEXT("创建后资产可加载"),
			LoadObject<UMarkdownFile>(nullptr, *PackagePath, nullptr, LOAD_Quiet) != nullptr);
	}

	// ---- 重复创建被拒 ----
	{
		bool bSucceeded = true;
		const FString Arguments = FString::Printf(TEXT("{\"action\":\"create\",\"assetPath\":\"%s\"}"), *PackagePath);
		const TSharedPtr<FJsonObject> Root = MarkdownAgentTestPrivate::Run(Tools, TEXT("markdown_asset"), Arguments, bSucceeded);
		TestFalse(TEXT("重复创建判失败"), bSucceeded);
		if (Root.IsValid())
		{
			TestEqual(TEXT("重复创建给 asset_exists"), Root->GetStringField(TEXT("errorCode")), FString(TEXT("asset_exists")));
		}
	}

	// ---- read / info / outline / search ----
	FString Hash;
	{
		bool bSucceeded = false;
		const FString Arguments = FString::Printf(TEXT("{\"action\":\"read\",\"assetPath\":\"%s\"}"), *PackagePath);
		const TSharedPtr<FJsonObject> Root = MarkdownAgentTestPrivate::Run(Tools, TEXT("markdown_query"), Arguments, bSucceeded);
		if (!TestTrue(TEXT("读取成功"), bSucceeded))
		{
			return false;
		}
		TestEqual(TEXT("读到的正文与写入一致"), Root->GetStringField(TEXT("content")), FirstContent);
		TestFalse(TEXT("短文档没有被截断"), Root->GetBoolField(TEXT("truncated")));
		Hash = Root->GetStringField(TEXT("contentHash"));
		TestEqual(TEXT("hash 是 32 位十六进制"), Hash.Len(), 32);
	}
	{
		bool bSucceeded = false;
		const FString Arguments = FString::Printf(TEXT("{\"action\":\"outline\",\"assetPath\":\"%s\"}"), *PackagePath);
		const TSharedPtr<FJsonObject> Root = MarkdownAgentTestPrivate::Run(Tools, TEXT("markdown_query"), Arguments, bSucceeded);
		TestTrue(TEXT("大纲成功"), bSucceeded);
		if (Root.IsValid())
		{
			const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
			Root->TryGetArrayField(TEXT("entries"), Entries);
			TestTrue(TEXT("抽出 1 个标题"), Entries != nullptr && Entries->Num() == 1);
			if (Entries != nullptr && Entries->Num() == 1)
			{
				const TSharedPtr<FJsonObject> Entry = (*Entries)[0]->AsObject();
				// 对外行号从 1 起算
				TestEqual(TEXT("标题在第 1 行"), static_cast<int32>(Entry->GetNumberField(TEXT("line"))), 1);
				TestEqual(TEXT("标题层级 1"), static_cast<int32>(Entry->GetNumberField(TEXT("level"))), 1);
				TestEqual(TEXT("标题文本"), Entry->GetStringField(TEXT("text")), FString(TEXT("Agent Note")));
			}
		}
	}
	{
		bool bSucceeded = false;
		const FString Arguments = FString::Printf(
			TEXT("{\"action\":\"search\",\"query\":\"unique token\"}"));
		const TSharedPtr<FJsonObject> Root = MarkdownAgentTestPrivate::Run(Tools, TEXT("markdown_query"), Arguments, bSucceeded);
		TestTrue(TEXT("搜索成功"), bSucceeded);
		if (Root.IsValid())
		{
			// 搜索是大小写不敏感的，且必须能找到刚建的那一行
			bool bFound = false;
			const TArray<TSharedPtr<FJsonValue>>* Matches = nullptr;
			if (Root->TryGetArrayField(TEXT("matches"), Matches) && Matches != nullptr)
			{
				for (const TSharedPtr<FJsonValue>& Value : *Matches)
				{
					const TSharedPtr<FJsonObject> Hit = Value->AsObject();
					if (Hit.IsValid() && Hit->GetStringField(TEXT("assetPath")) == PackagePath)
					{
						bFound = true;
						TestEqual(TEXT("命中在第 5 行"), static_cast<int32>(Hit->GetNumberField(TEXT("line"))), 5);
					}
				}
			}
			TestTrue(TEXT("搜索命中测试资产"), bFound);
		}
	}

	// ---- replace：带 expectedHash 守卫 ----
	{
		bool bSucceeded = false;
		const FString Arguments = FString::Printf(
			TEXT("{\"action\":\"replace\",\"assetPath\":\"%s\",\"oldText\":\"body line\",\"newText\":\"BODY LINE\",\"expectedHash\":\"%s\"}"),
			*PackagePath, *Hash);
		const TSharedPtr<FJsonObject> Root = MarkdownAgentTestPrivate::Run(Tools, TEXT("markdown_edit"), Arguments, bSucceeded);
		TestTrue(TEXT("带正确 hash 的替换成功"), bSucceeded);
		if (Root.IsValid())
		{
			TestEqual(TEXT("替换 1 处"), static_cast<int32>(Root->GetNumberField(TEXT("replaced"))), 1);
			TestTrue(TEXT("默认已落盘"), Root->GetBoolField(TEXT("saved")));
		}
	}
	{
		// hash 已过期（上面那次替换改过内容），必须被拒
		bool bSucceeded = true;
		const FString Arguments = FString::Printf(
			TEXT("{\"action\":\"replace\",\"assetPath\":\"%s\",\"oldText\":\"body line\",\"newText\":\"x\",\"expectedHash\":\"%s\"}"),
			*PackagePath, *Hash);
		const TSharedPtr<FJsonObject> Root = MarkdownAgentTestPrivate::Run(Tools, TEXT("markdown_edit"), Arguments, bSucceeded);
		TestFalse(TEXT("过期 hash 判失败"), bSucceeded);
		if (Root.IsValid())
		{
			TestEqual(TEXT("过期 hash 给 stale_asset"), Root->GetStringField(TEXT("errorCode")), FString(TEXT("stale_asset")));
		}
	}

	// ---- append 与 insert_line ----
	{
		bool bSucceeded = false;
		const FString Arguments = FString::Printf(
			TEXT("{\"action\":\"append\",\"assetPath\":\"%s\",\"text\":\"tail\"}"), *PackagePath);
		MarkdownAgentTestPrivate::Run(Tools, TEXT("markdown_edit"), Arguments, bSucceeded);
		TestTrue(TEXT("追加成功"), bSucceeded);
	}
	{
		bool bSucceeded = false;
		const FString Arguments = FString::Printf(
			TEXT("{\"action\":\"insert_line\",\"assetPath\":\"%s\",\"lineIndex\":0,\"text\":\"prepended\"}"), *PackagePath);
		MarkdownAgentTestPrivate::Run(Tools, TEXT("markdown_edit"), Arguments, bSucceeded);
		TestTrue(TEXT("插入成功"), bSucceeded);
	}
	{
		bool bSucceeded = false;
		const FString Arguments = FString::Printf(TEXT("{\"action\":\"read\",\"assetPath\":\"%s\"}"), *PackagePath);
		const TSharedPtr<FJsonObject> Root = MarkdownAgentTestPrivate::Run(Tools, TEXT("markdown_query"), Arguments, bSucceeded);
		if (TestTrue(TEXT("回读成功"), bSucceeded))
		{
			const FString Text = Root->GetStringField(TEXT("content"));
			TestTrue(TEXT("开头是插入的行"), Text.StartsWith(TEXT("prepended")));
			TestTrue(TEXT("结尾是追加的文本"), Text.EndsWith(TEXT("tail")));
			TestTrue(TEXT("替换结果可见"), Text.Contains(TEXT("BODY LINE")));
		}
	}

	// ---- 越界行号被拒 ----
	{
		bool bSucceeded = true;
		const FString Arguments = FString::Printf(
			TEXT("{\"action\":\"insert_line\",\"assetPath\":\"%s\",\"lineIndex\":9999,\"text\":\"x\"}"), *PackagePath);
		const TSharedPtr<FJsonObject> Root = MarkdownAgentTestPrivate::Run(Tools, TEXT("markdown_edit"), Arguments, bSucceeded);
		TestFalse(TEXT("越界行号判失败"), bSucceeded);
		if (Root.IsValid())
		{
			TestEqual(TEXT("越界行号给 invalid_line_index"), Root->GetStringField(TEXT("errorCode")), FString(TEXT("invalid_line_index")));
		}
	}

	// ---- export 到 Saved，内容应与资产一致 ----
	const FString ExportFile = FPaths::Combine(
		FPaths::ProjectSavedDir(), TEXT("MarkdownAgentTests"), TEXT("agent_export.md"));
	{
		bool bSucceeded = false;
		const FString Arguments = FString::Printf(
			TEXT("{\"action\":\"export\",\"assetPath\":\"%s\",\"destinationFile\":\"%s\"}"),
			*PackagePath, *ExportFile.Replace(TEXT("\\"), TEXT("/")));
		MarkdownAgentTestPrivate::Run(Tools, TEXT("markdown_asset"), Arguments, bSucceeded);
		TestTrue(TEXT("导出成功"), bSucceeded);
		TestTrue(TEXT("导出的文件存在"), FPaths::FileExists(ExportFile));
	}

	// ---- reimport：资产没有源文件，应明确拒绝 ----
	{
		bool bSucceeded = true;
		const FString Arguments = FString::Printf(TEXT("{\"action\":\"reimport\",\"assetPath\":\"%s\"}"), *PackagePath);
		const TSharedPtr<FJsonObject> Root = MarkdownAgentTestPrivate::Run(Tools, TEXT("markdown_asset"), Arguments, bSucceeded);
		TestFalse(TEXT("无源文件时重导入判失败"), bSucceeded);
		if (Root.IsValid())
		{
			TestEqual(TEXT("无源文件给 no_source_file"), Root->GetStringField(TEXT("errorCode")), FString(TEXT("no_source_file")));
		}
	}

	// ---- delete ----
	{
		bool bSucceeded = false;
		const FString Arguments = FString::Printf(TEXT("{\"action\":\"delete\",\"assetPath\":\"%s\"}"), *PackagePath);
		MarkdownAgentTestPrivate::Run(Tools, TEXT("markdown_asset"), Arguments, bSucceeded);
		TestTrue(TEXT("删除成功"), bSucceeded);
		TestFalse(TEXT("删除后资产不可加载"),
			LoadObject<UMarkdownFile>(nullptr, *PackagePath, nullptr, LOAD_Quiet) != nullptr);
	}

	// 收尾：清掉导出文件与可能残留的资产
	IFileManager::Get().Delete(*ExportFile, false, true, true);
	MarkdownAgentTestPrivate::RemoveTestAsset();
	return true;
}

#endif

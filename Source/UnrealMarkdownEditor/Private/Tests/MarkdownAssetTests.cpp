// UnrealMarkdown - MIT License, see LICENSE in the plugin root.
// 新增文件：Markdown 资产与语法工具的自动化测试。

#include "Misc/AutomationTest.h"

// 这些用例要写临时文件、且只在编辑器侧有意义。
#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "MarkdownFile.h"
#include "MarkdownSyntax.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

namespace MarkdownTestPrivate
{
	/** 临时文件目录，测试结束会整体删掉。 */
	static FString MakeTempDir(const FString& Suffix)
	{
		const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MarkdownTests"), Suffix);
		IFileManager::Get().MakeDirectory(*Dir, true);
		return Dir;
	}

	static UMarkdownFile* MakeAsset(const FString& Content)
	{
		// 不能用 NewObject<UMarkdownFile>() 的默认外层：那会挂到瞬态包上，
		// 而 MarkPackageDirty 对 RF_Transient 链会直接返回、不置脏标志，
		// 于是「改内容要标脏」这条断言永远失败。这里给每个资产一个独立普通包。
		static int32 Serial = 0;
		const FString PackageName = FString::Printf(TEXT("/Temp/MarkdownTests/Package_%d"), ++Serial);
		UPackage* Package = CreatePackage(*PackageName);

		UMarkdownFile* Asset = NewObject<UMarkdownFile>(Package, NAME_None, RF_Transactional);
		Asset->SetContent(Content);
		return Asset;
	}
}

// ============================================================================
// 纯字符串工具：格式工具栏与 MCP 都依赖这几个函数
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMarkdownSyntaxTest, "UnrealMarkdown.Syntax",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMarkdownSyntaxTest::RunTest(const FString& Parameters)
{
	// ---- WrapSelection：有选区 ----
	{
		FString OutText;
		int32 OutStart = 0;
		int32 OutEnd = 0;
		UMarkdownSyntaxLibrary::WrapSelection(TEXT("hello world"), 6, 11, TEXT("**"), TEXT("**"),
			OutText, OutStart, OutEnd);
		TestEqual(TEXT("加粗只包住选区"), OutText, FString(TEXT("hello **world**")));
		TestEqual(TEXT("选区起点回到标记内侧"), OutStart, 8);
		TestEqual(TEXT("选区终点回到标记内侧"), OutEnd, 13);
	}

	// ---- WrapSelection：空选区，光标落在标记之间 ----
	{
		FString OutText;
		int32 OutStart = 0;
		int32 OutEnd = 0;
		UMarkdownSyntaxLibrary::WrapSelection(TEXT("abc"), 1, 1, TEXT("*"), TEXT("*"),
			OutText, OutStart, OutEnd);
		TestEqual(TEXT("空选区插入成对标记"), OutText, FString(TEXT("a**bc")));
		TestEqual(TEXT("光标落在两个星号之间"), OutStart, 2);
		TestEqual(TEXT("空选区不产生选中范围"), OutEnd, 2);
	}

	// ---- ToggleLinePrefix：加 -> 去 ----
	{
		FString Once;
		int32 Start = 0;
		int32 End = 0;
		UMarkdownSyntaxLibrary::ToggleLinePrefix(TEXT("a\nb"), 0, 3, TEXT("- "), Once, Start, End);
		TestEqual(TEXT("两行都加上列表前缀"), Once, FString(TEXT("- a\n- b")));

		FString Twice;
		UMarkdownSyntaxLibrary::ToggleLinePrefix(Once, Start, End, TEXT("- "), Twice, Start, End);
		TestEqual(TEXT("再应用一次等于撤销"), Twice, FString(TEXT("a\nb")));
	}

	// ---- ToggleLinePrefix：保留缩进 ----
	{
		FString OutText;
		int32 Start = 0;
		int32 End = 0;
		UMarkdownSyntaxLibrary::ToggleLinePrefix(TEXT("  nested"), 0, 8, TEXT("- "), OutText, Start, End);
		TestEqual(TEXT("前缀加在缩进之后"), OutText, FString(TEXT("  - nested")));
	}

	// ---- GetLineContinuation ----
	TestEqual(TEXT("无序列表续行"), UMarkdownSyntaxLibrary::GetLineContinuation(TEXT("- item")), FString(TEXT("- ")));
	TestEqual(TEXT("有序列表序号加一"), UMarkdownSyntaxLibrary::GetLineContinuation(TEXT("3. item")), FString(TEXT("4. ")));
	TestEqual(TEXT("任务列表续行保留勾选框"), UMarkdownSyntaxLibrary::GetLineContinuation(TEXT("- [x] done")), FString(TEXT("- [ ] ")));
	TestEqual(TEXT("引用块续行"), UMarkdownSyntaxLibrary::GetLineContinuation(TEXT("> quote")), FString(TEXT("> ")));
	TestEqual(TEXT("嵌套列表保留缩进"), UMarkdownSyntaxLibrary::GetLineContinuation(TEXT("  - nested")), FString(TEXT("  - ")));
	TestEqual(TEXT("普通段落不续行"), UMarkdownSyntaxLibrary::GetLineContinuation(TEXT("plain text")), FString());

	// ---- ExtractOutline ----
	{
		const FString Source = TEXT("# Title\n\nbody text\n\n## Sub\n### Deep\n#not-a-heading\n    #### 四空格缩进不算\n");
		const TArray<FMarkdownOutlineEntry> Outline = UMarkdownSyntaxLibrary::ExtractOutline(Source);

		TestEqual(TEXT("一共三个标题"), Outline.Num(), 3);
		if (Outline.Num() == 3)
		{
			TestEqual(TEXT("第一个标题在第 0 行"), Outline[0].Line, 0);
			TestEqual(TEXT("第一个标题层级 1"), Outline[0].Level, 1);
			TestEqual(TEXT("第一个标题文本"), Outline[0].Text, FString(TEXT("Title")));
			TestEqual(TEXT("第二个标题层级 2"), Outline[1].Level, 2);
			TestEqual(TEXT("第三个标题层级 3"), Outline[2].Level, 3);
			TestEqual(TEXT("第三个标题在第 5 行"), Outline[2].Line, 5);
		}
	}

	return true;
}

// ============================================================================
// 资产内容操作：增删查改的底层
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMarkdownAssetContentTest, "UnrealMarkdown.AssetContent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMarkdownAssetContentTest::RunTest(const FString& Parameters)
{
	UMarkdownFile* Asset = MarkdownTestPrivate::MakeAsset(TEXT(""));
	TestNotNull(TEXT("资产可创建"), Asset);
	if (!Asset)
	{
		return false;
	}

	TestTrue(TEXT("空文档 IsEmpty"), Asset->IsEmpty());
	TestEqual(TEXT("空文档 0 行"), Asset->GetLineCount(), 0);
	TestEqual(TEXT("空文档 0 字符"), Asset->GetCharCount(), 0);
	TestEqual(TEXT("空串忽略 SetContent 之外的空操作"), Asset->GetContentHash().Len(), 32);

	// ---- SetContent + 计数 ----
	Asset->SetContent(TEXT("# A\nline2\nline3"));
	TestEqual(TEXT("三行"), Asset->GetLineCount(), 3);
	TestFalse(TEXT("非空"), Asset->IsEmpty());

	// ---- SetContent 幂等：内容相同不标脏 ----
	Asset->GetOutermost()->SetDirtyFlag(false);
	Asset->SetContent(TEXT("# A\nline2\nline3"));
	TestFalse(TEXT("内容没变时不会把包装脏"), Asset->GetOutermost()->IsDirty());

	Asset->SetContent(TEXT("# A\nline2 changed\nline3"));
	TestTrue(TEXT("内容变了会标脏"), Asset->GetOutermost()->IsDirty());

	// ---- hash：内容相同 hash 相同，改一个字符就不同 ----
	const FString HashA = Asset->GetContentHash();
	UMarkdownFile* Same = MarkdownTestPrivate::MakeAsset(TEXT("# A\nline2 changed\nline3"));
	TestEqual(TEXT("相同内容 hash 相同"), Same->GetContentHash(), HashA);
	Same->SetContent(TEXT("# A\nline2 changed!\nline3"));
	TestNotEqual(TEXT("内容变化后 hash 不同"), Same->GetContentHash(), HashA);

	// ---- AppendText ----
	Asset->AppendText(TEXT("\nline4"));
	TestEqual(TEXT("追加后四行"), Asset->GetLineCount(), 4);
	TestTrue(TEXT("追加内容可见"), Asset->Content.EndsWith(TEXT("line4")));

	// ---- InsertAtLine：中间插入 ----
	Asset->SetContent(TEXT("one\ntwo\nthree"));
	TestTrue(TEXT("在第 1 行前插入成功"), Asset->InsertAtLine(1, TEXT("inserted")));
	TestEqual(TEXT("插入后内容"), Asset->Content, FString(TEXT("one\ninserted\ntwo\nthree")));
	TestEqual(TEXT("插入后四行"), Asset->GetLineCount(), 4);

	// ---- InsertAtLine：越界拒绝 ----
	TestFalse(TEXT("超过行数的索引被拒绝"), Asset->InsertAtLine(99, TEXT("x")));
	TestFalse(TEXT("负索引被拒绝"), Asset->InsertAtLine(-1, TEXT("x")));
	TestFalse(TEXT("空文本被拒绝"), Asset->InsertAtLine(0, FString()));

	// ---- InsertAtLine：追加到末尾时不与最后一行粘连 ----
	Asset->SetContent(TEXT("no-trailing-newline"));
	TestTrue(TEXT("在行数位置插入成功"), Asset->InsertAtLine(1, TEXT("appended")));
	TestEqual(TEXT("追加时自动补换行"), Asset->Content, FString(TEXT("no-trailing-newline\nappended")));

	// ---- ReplaceInContent ----
	Asset->SetContent(TEXT("aa bb aa"));
	TestEqual(TEXT("只替换第一处"), Asset->ReplaceInContent(TEXT("aa"), TEXT("XX"), false), 1);
	TestEqual(TEXT("替换后的内容"), Asset->Content, FString(TEXT("XX bb aa")));

	Asset->SetContent(TEXT("aa bb aa"));
	TestEqual(TEXT("替换全部返回两处"), Asset->ReplaceInContent(TEXT("aa"), TEXT("XX"), true), 2);
	TestEqual(TEXT("全部替换后的内容"), Asset->Content, FString(TEXT("XX bb XX")));

	TestEqual(TEXT("找不到时返回 0"), Asset->ReplaceInContent(TEXT("zz"), TEXT("y"), true), 0);
	TestEqual(TEXT("空串匹配直接返回 0"), Asset->ReplaceInContent(FString(), TEXT("y"), true), 0);

	// 测试包退出时不应挂着脏标记，否则关闭编辑器会弹保存提示。
	Asset->GetOutermost()->SetDirtyFlag(false);
	Same->GetOutermost()->SetDirtyFlag(false);

	return true;
}

// ============================================================================
// 磁盘读写：导入 / 导出 / 重导入的同一条链路
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMarkdownFileIOTest, "UnrealMarkdown.FileIO",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMarkdownFileIOTest::RunTest(const FString& Parameters)
{
	const FString Dir = MarkdownTestPrivate::MakeTempDir(TEXT("FileIO"));
	const FString FilePath = FPaths::Combine(Dir, TEXT("RoundTrip.md"));

	UMarkdownFile* Asset = NewObject<UMarkdownFile>();
	if (!TestNotNull(TEXT("资产可创建"), Asset))
	{
		return false;
	}

	const FString Source = TEXT("# 标题\n\n正文包含中文与 emoji 🙂\n\n- a\n- b\n");
	Asset->SetContent(Source);

	// ---- 导出 ----
	TestTrue(TEXT("导出到磁盘成功"), Asset->ExportToFile(FilePath));
	TestTrue(TEXT("导出的文件确实存在"), FPaths::FileExists(FilePath));

	// ---- 导入 ----
	UMarkdownFile* Loaded = NewObject<UMarkdownFile>();
	TestTrue(TEXT("从磁盘导入成功"), Loaded->ImportFromFile(FilePath));
	TestEqual(TEXT("往返后内容逐字符一致"), Loaded->Content, Source);
	TestFalse(TEXT("导入会记下来源文件"), Loaded->SourceFilePath.IsEmpty());
	TestTrue(TEXT("来源文件是绝对路径"), FPaths::IsRelative(Loaded->SourceFilePath) == false);
	TestEqual(TEXT("往返后 hash 一致"), Loaded->GetContentHash(), Asset->GetContentHash());

	// ---- 重导入语义：文件改了，再读一次能拿到新内容 ----
	const FString Updated = Source + TEXT("\n## 追加的一节\n");
	TestTrue(TEXT("改写源文件成功"), FFileHelper::SaveStringToFile(Updated, *FilePath));
	TestTrue(TEXT("重导入成功"), Loaded->ImportFromFile(Loaded->SourceFilePath));
	TestEqual(TEXT("重导入拿到新内容"), Loaded->Content, Updated);
	TestNotEqual(TEXT("重导入后 hash 变化"), Loaded->GetContentHash(), Asset->GetContentHash());

	// ---- 不存在的文件 ----
	UMarkdownFile* Missing = NewObject<UMarkdownFile>();
	TestFalse(TEXT("不存在的文件返回 false"),
		Missing->ImportFromFile(FPaths::Combine(Dir, TEXT("NoSuchFile.md"))));
	TestTrue(TEXT("读取失败不污染原内容"), Missing->IsEmpty());

	IFileManager::Get().DeleteDirectory(*FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MarkdownTests")), false, true);
	return true;
}

#endif

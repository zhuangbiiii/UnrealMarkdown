// UnrealMarkdown - MIT License, see LICENSE in the plugin root.
// 本文件已按 ZBSStudio 的本地集成需要重写。

#include "MarkdownAssetEditor.h"
#include "MarkdownFile.h"
#include "MarkdownPreviewWidget.h"
#include "MarkdownSyntax.h"
#include "UnrealMarkdown.h"

#include "DesktopPlatformModule.h"
#include "FileHelpers.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IDesktopPlatform.h"
#include "InputCoreTypes.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "MarkdownStyleCompat.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "MarkdownAssetEditor"

const FName FMarkdownAssetEditor::EditTabId(TEXT("MarkdownAssetEditor_Edit"));
const FName FMarkdownAssetEditor::PreviewTabId(TEXT("MarkdownAssetEditor_Preview"));

namespace MarkdownAssetEditorPrivate
{
	/**
	 * FTextLocation 转字符下标。
	 *
	 * Slate 按 \n 分行，位置里不含 \r，所以要自己按行累加，不能直接拿
	 * LineIndex * 行宽 去算。Location 无效时返回 INDEX_NONE。
	 */
	static int32 TextLocationToOffset(const FString& Source, const FTextLocation& Location)
	{
		if (!Location.IsValid())
		{
			return INDEX_NONE;
		}

		int32 LineStart = 0;
		int32 CurrentLine = 0;
		for (int32 Index = 0; Index < Source.Len() && CurrentLine < Location.GetLineIndex(); ++Index)
		{
			if (Source[Index] == TEXT('\n'))
			{
				++CurrentLine;
				LineStart = Index + 1;
			}
		}

		return FMath::Clamp(LineStart + Location.GetOffset(), 0, Source.Len());
	}

	/** 字符下标转 FTextLocation，是 TextLocationToOffset 的逆运算。 */
	static FTextLocation OffsetToTextLocation(const FString& Source, int32 Offset)
	{
		const int32 Clamped = FMath::Clamp(Offset, 0, Source.Len());

		int32 LineIndex = 0;
		int32 LineStart = 0;
		for (int32 Index = 0; Index < Clamped; ++Index)
		{
			if (Source[Index] == TEXT('\n'))
			{
				++LineIndex;
				LineStart = Index + 1;
			}
		}

		return FTextLocation(LineIndex, Clamped - LineStart);
	}
}

FMarkdownAssetEditor::FMarkdownAssetEditor() {}

FMarkdownAssetEditor::~FMarkdownAssetEditor() {}

void FMarkdownAssetEditor::InitMarkdownEditor(
	EToolkitMode::Type Mode,
	const TSharedPtr<IToolkitHost>& InitToolkitHost,
	UMarkdownFile* InMarkdownFile)
{
	MarkdownFile = InMarkdownFile;

	// Initialize edit buffer from file content
	if (MarkdownFile)
	{
		EditBuffer = FText::FromString(MarkdownFile->Content);
	}

	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_MarkdownAssetEditor_Layout_v3")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Horizontal)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.6f)
				->AddTab(EditTabId, ETabState::OpenedTab)
			)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.4f)
				->AddTab(PreviewTabId, ETabState::OpenedTab)
			)
		);

	InitAssetEditor(
		Mode,
		InitToolkitHost,
		TEXT("MarkdownAssetEditor"),
		StandaloneDefaultLayout,
		true,  // create standalone menu
		true,  // create default toolbar
		InMarkdownFile);

	ExtendToolbar();
	RegenerateMenusAndToolbars();
}

// ============================================================================
// FAssetEditorToolkit
// ============================================================================

void FMarkdownAssetEditor::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(EditTabId,
		FOnSpawnTab::CreateSP(this, &FMarkdownAssetEditor::SpawnTab_Edit))
		.SetDisplayName(LOCTEXT("EditTab", "Edit"))
		.SetIcon(FSlateIcon(MARKDOWN_STYLE::GetAppStyleSetName(), "LevelEditor.Tabs.Viewports"));

	InTabManager->RegisterTabSpawner(PreviewTabId,
		FOnSpawnTab::CreateSP(this, &FMarkdownAssetEditor::SpawnTab_Preview))
		.SetDisplayName(LOCTEXT("PreviewTab", "Preview"))
		.SetIcon(FSlateIcon(MARKDOWN_STYLE::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));
}

void FMarkdownAssetEditor::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
	InTabManager->UnregisterTabSpawner(EditTabId);
	InTabManager->UnregisterTabSpawner(PreviewTabId);
}

FName FMarkdownAssetEditor::GetToolkitFName() const { return FName("MarkdownAssetEditor"); }
FText FMarkdownAssetEditor::GetBaseToolkitName() const { return LOCTEXT("AppLabel", "Markdown Editor"); }
FString FMarkdownAssetEditor::GetWorldCentricTabPrefix() const { return LOCTEXT("TabPrefix", "Markdown ").ToString(); }
FLinearColor FMarkdownAssetEditor::GetWorldCentricTabColorScale() const { return FLinearColor(0.3f, 0.5f, 0.9f, 0.5f); }

// ============================================================================
// Commands & Toolbar
// ============================================================================

void FMarkdownAssetEditor::ExtendToolbar()
{
	struct Local
	{
		static void FillToolbar(FToolBarBuilder& ToolbarBuilder, FMarkdownAssetEditor* Editor, bool bCanReimport)
		{
			ToolbarBuilder.BeginSection("Markdown");
			{
				ToolbarBuilder.AddToolBarButton(
					FUIAction(FExecuteAction::CreateRaw(Editor, &FMarkdownAssetEditor::OnSave)),
					NAME_None,
					LOCTEXT("SaveBtn", "Save"),
					LOCTEXT("SaveBtnTip", "把当前文本写入资产"),
					FSlateIcon(MARKDOWN_STYLE::GetAppStyleSetName(), "AssetEditor.SaveAsset"));

				if (bCanReimport)
				{
					ToolbarBuilder.AddToolBarButton(
						FUIAction(FExecuteAction::CreateRaw(Editor, &FMarkdownAssetEditor::OnReimport)),
						NAME_None,
						LOCTEXT("ReimportBtn", "Reimport"),
						LOCTEXT("ReimportBtnTip", "从导入时的 .md 源文件重新读取，覆盖当前内容"),
						FSlateIcon(MARKDOWN_STYLE::GetAppStyleSetName(), "ContentBrowser.ImportIcon"));
				}

				ToolbarBuilder.AddToolBarButton(
					FUIAction(FExecuteAction::CreateRaw(Editor, &FMarkdownAssetEditor::OnExport)),
					NAME_None,
					LOCTEXT("ExportBtn", "Export"),
					LOCTEXT("ExportBtnTip", "导出为磁盘上的 .md 文件"),
					FSlateIcon(MARKDOWN_STYLE::GetAppStyleSetName(), "AssetEditor.SaveAsset.Greyscale"));
			}
			ToolbarBuilder.EndSection();
		}
	};

	// 没有源文件时不给重导入按钮，避免点了才报错。
	const bool bCanReimport = MarkdownFile != nullptr
		&& !MarkdownFile->SourceFilePath.IsEmpty()
		&& FPaths::FileExists(MarkdownFile->SourceFilePath);

	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);
	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateStatic(&Local::FillToolbar, this, bCanReimport));
	AddToolbarExtender(ToolbarExtender);
}

TSharedRef<SWidget> FMarkdownAssetEditor::MakeWrapButton(const FText& Label, const FText& Tooltip, const FString& Prefix, const FString& Suffix)
{
	return SNew(SButton)
		.ContentPadding(FMargin(6.0f, 2.0f))
		.ToolTipText(Tooltip)
		.OnClicked_Lambda([this, Prefix, Suffix]()
		{
			ApplyWrap(Prefix, Suffix);
			return FReply::Handled();
		})
		[
			SNew(STextBlock)
			.Text(Label)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
		];
}

TSharedRef<SWidget> FMarkdownAssetEditor::MakePrefixButton(const FText& Label, const FText& Tooltip, const FString& Prefix)
{
	return SNew(SButton)
		.ContentPadding(FMargin(6.0f, 2.0f))
		.ToolTipText(Tooltip)
		.OnClicked_Lambda([this, Prefix]()
		{
			ApplyLinePrefix(Prefix);
			return FReply::Handled();
		})
		[
			SNew(STextBlock)
			.Text(Label)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
		];
}

TSharedRef<SWidget> FMarkdownAssetEditor::BuildFormatBar()
{
	return SNew(SBorder)
		.BorderImage(MARKDOWN_STYLE::GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(4.0f, 3.0f))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot().AutoWidth()
			[ MakeWrapButton(LOCTEXT("BtnBold", "B"), LOCTEXT("BtnBoldTip", "加粗选区（Ctrl+B）"), TEXT("**"), TEXT("**")) ]
			+ SHorizontalBox::Slot().AutoWidth()
			[ MakeWrapButton(LOCTEXT("BtnItalic", "I"), LOCTEXT("BtnItalicTip", "斜体选区（Ctrl+I）"), TEXT("*"), TEXT("*")) ]
			+ SHorizontalBox::Slot().AutoWidth()
			[ MakeWrapButton(LOCTEXT("BtnCode", "code"), LOCTEXT("BtnCodeTip", "行内代码"), TEXT("`"), TEXT("`")) ]
			+ SHorizontalBox::Slot().AutoWidth()
			[ MakeWrapButton(LOCTEXT("BtnLink", "Link"), LOCTEXT("BtnLinkTip", "链接（Ctrl+K）"), TEXT("["), TEXT("](url)")) ]

			+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[ MakePrefixButton(LOCTEXT("BtnH1", "H1"), LOCTEXT("BtnH1Tip", "标题一"), TEXT("# ")) ]
			+ SHorizontalBox::Slot().AutoWidth()
			[ MakePrefixButton(LOCTEXT("BtnH2", "H2"), LOCTEXT("BtnH2Tip", "标题二"), TEXT("## ")) ]
			+ SHorizontalBox::Slot().AutoWidth()
			[ MakePrefixButton(LOCTEXT("BtnH3", "H3"), LOCTEXT("BtnH3Tip", "标题三"), TEXT("### ")) ]

			+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[ MakePrefixButton(LOCTEXT("BtnBullet", "- "), LOCTEXT("BtnBulletTip", "无序列表"), TEXT("- ")) ]
			+ SHorizontalBox::Slot().AutoWidth()
			[ MakePrefixButton(LOCTEXT("BtnOrdered", "1."), LOCTEXT("BtnOrderedTip", "有序列表"), TEXT("1. ")) ]
			+ SHorizontalBox::Slot().AutoWidth()
			[ MakePrefixButton(LOCTEXT("BtnTask", "[ ]"), LOCTEXT("BtnTaskTip", "任务列表"), TEXT("- [ ] ")) ]
			+ SHorizontalBox::Slot().AutoWidth()
			[ MakePrefixButton(LOCTEXT("BtnQuote", ">"), LOCTEXT("BtnQuoteTip", "引用块"), TEXT("> ")) ]

			+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.ContentPadding(FMargin(6.0f, 2.0f))
				.ToolTipText(LOCTEXT("BtnCodeBlockTip", "插入代码块"))
				.OnClicked_Lambda([this]()
				{
					InsertBlock(TEXT("```\n\n```"));
					return FReply::Handled();
				})
				[
					SNew(STextBlock)
					.Text(LOCTEXT("BtnCodeBlock", "```"))
					.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.ContentPadding(FMargin(6.0f, 2.0f))
				.ToolTipText(LOCTEXT("BtnRuleTip", "插入分隔线"))
				.OnClicked_Lambda([this]()
				{
					InsertBlock(TEXT("---"));
					return FReply::Handled();
				})
				[
					SNew(STextBlock)
					.Text(LOCTEXT("BtnRule", "---"))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
				]
			]
		];
}

// ============================================================================
// Markdown 编辑动作
// ============================================================================

void FMarkdownAssetEditor::GetSelectionOffsets(int32& OutStart, int32& OutEnd) const
{
	OutStart = 0;
	OutEnd = 0;

	const FString Source = EditBuffer.ToString();
	if (!TextEditorWidget.IsValid())
	{
		return;
	}

	const FTextSelection Selection = TextEditorWidget->GetSelection();
	const int32 RawStart = MarkdownAssetEditorPrivate::TextLocationToOffset(Source, Selection.GetBeginning());
	const int32 RawEnd = MarkdownAssetEditorPrivate::TextLocationToOffset(Source, Selection.GetEnd());
	if (RawStart != INDEX_NONE && RawEnd != INDEX_NONE)
	{
		OutStart = FMath::Min(RawStart, RawEnd);
		OutEnd = FMath::Max(RawStart, RawEnd);
		return;
	}

	// 没有选区时退化成光标位置：格式按钮仍然可用，只是在光标处插入成对标记。
	const int32 Cursor = MarkdownAssetEditorPrivate::TextLocationToOffset(Source, TextEditorWidget->GetCursorLocation());
	if (Cursor != INDEX_NONE)
	{
		OutStart = Cursor;
		OutEnd = Cursor;
	}
}

void FMarkdownAssetEditor::ApplyText(const FString& NewText, int32 SelectionStart, int32 SelectionEnd)
{
	EditBuffer = FText::FromString(NewText);

	if (TextEditorWidget.IsValid())
	{
		TextEditorWidget->SetText(EditBuffer);
		TextEditorWidget->SelectText(
			MarkdownAssetEditorPrivate::OffsetToTextLocation(NewText, SelectionStart),
			MarkdownAssetEditorPrivate::OffsetToTextLocation(NewText, SelectionEnd));
	}

	// 控件不可用时也要把结果落进资产，不能因为缺控件丢掉编辑。
	OnEditTextChanged(EditBuffer);
}

void FMarkdownAssetEditor::ApplyWrap(const FString& Prefix, const FString& Suffix)
{
	int32 SelectionStart = 0;
	int32 SelectionEnd = 0;
	GetSelectionOffsets(SelectionStart, SelectionEnd);

	FString NewText;
	int32 NewStart = 0;
	int32 NewEnd = 0;
	UMarkdownSyntaxLibrary::WrapSelection(EditBuffer.ToString(), SelectionStart, SelectionEnd,
		Prefix, Suffix, NewText, NewStart, NewEnd);
	ApplyText(NewText, NewStart, NewEnd);
}

void FMarkdownAssetEditor::ApplyLinePrefix(const FString& Prefix)
{
	int32 SelectionStart = 0;
	int32 SelectionEnd = 0;
	GetSelectionOffsets(SelectionStart, SelectionEnd);

	FString NewText;
	int32 NewStart = 0;
	int32 NewEnd = 0;
	UMarkdownSyntaxLibrary::ToggleLinePrefix(EditBuffer.ToString(), SelectionStart, SelectionEnd,
		Prefix, NewText, NewStart, NewEnd);
	ApplyText(NewText, NewStart, NewEnd);
}

void FMarkdownAssetEditor::InsertBlock(const FString& Text)
{
	int32 SelectionStart = 0;
	int32 SelectionEnd = 0;
	GetSelectionOffsets(SelectionStart, SelectionEnd);

	const FString Source = EditBuffer.ToString();

	FString Payload = Text;
	// 插入点不在行首就先补一个换行，否则会和上一行粘在一起。
	if (SelectionStart > 0 && Source[SelectionStart - 1] != TEXT('\n'))
	{
		Payload = TEXT("\n") + Payload;
	}
	if (!Payload.EndsWith(TEXT("\n")))
	{
		Payload += TEXT("\n");
	}

	const FString NewText = Source.Left(SelectionStart) + Payload + Source.RightChop(SelectionEnd);
	const int32 Caret = SelectionStart + Payload.Len();
	ApplyText(NewText, Caret, Caret);
}

FReply FMarkdownAssetEditor::OnEditorKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	const bool bControl = InKeyEvent.IsControlDown();
	const bool bShift = InKeyEvent.IsShiftDown();

	// 先于文本框处理这几个组合键，否则 Ctrl+B 会被当成普通按键吞掉。
	if (bControl && !InKeyEvent.IsAltDown() && !bShift)
	{
		if (Key == EKeys::B)
		{
			ApplyWrap(TEXT("**"), TEXT("**"));
			return FReply::Handled();
		}
		if (Key == EKeys::I)
		{
			ApplyWrap(TEXT("*"), TEXT("*"));
			return FReply::Handled();
		}
		if (Key == EKeys::K)
		{
			ApplyWrap(TEXT("["), TEXT("](url)"));
			return FReply::Handled();
		}
	}

	// Tab 走两空格缩进；ToggleLinePrefix 在两段缩进已存在时执行移除，所以 Shift+Tab 天然就是反向缩进。
	if (Key == EKeys::Tab && !bControl && !InKeyEvent.IsAltDown())
	{
		ApplyLinePrefix(TEXT("  "));
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

// ============================================================================
// 资产级动作
// ============================================================================

void FMarkdownAssetEditor::OnSave()
{
	if (!MarkdownFile)
	{
		return;
	}

	// 文本改动平时已经写回资产，这里再同步一次以覆盖「打开后未触碰」的情况。
	MarkdownFile->SetContent(EditBuffer.ToString());
	MarkdownFile->MarkPackageDirty();

	TArray<UPackage*> PackagesToSave;
	PackagesToSave.Add(MarkdownFile->GetOutermost());
	FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, false, false);
}

void FMarkdownAssetEditor::OnExport()
{
	if (!MarkdownFile)
	{
		return;
	}

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		return;
	}

	TArray<FString> OutFiles;
	const FString DefaultName = MarkdownFile->GetName() + TEXT(".md");
	if (DesktopPlatform->SaveFileDialog(
		nullptr, TEXT("Export Markdown File"), FPaths::ProjectDir(), DefaultName,
		TEXT("Markdown Files (*.md)|*.md|Text Files (*.txt)|*.txt|All Files (*.*)|*.*"),
		EFileDialogFlags::None, OutFiles))
	{
		if (OutFiles.Num() > 0 && MarkdownFile->ExportToFile(OutFiles[0]))
		{
			UE_LOG(LogUnrealMarkdown, Log, TEXT("已导出到 %s"), *OutFiles[0]);
		}
	}
}

void FMarkdownAssetEditor::OnReimport()
{
	if (!MarkdownFile || MarkdownFile->SourceFilePath.IsEmpty())
	{
		return;
	}

	// 覆盖手改内容属于破坏性操作，先确认。
	const EAppReturnType::Type Choice = FMessageDialog::Open(
		EAppMsgType::YesNo,
		FText::Format(
			LOCTEXT("ReimportConfirm", "从源文件重新读取会覆盖当前编辑的 {0} 个字符。\n\n源文件：{1}\n\n继续吗？"),
			FText::AsNumber(MarkdownFile->Content.Len()),
			FText::FromString(MarkdownFile->SourceFilePath)),
		LOCTEXT("ReimportConfirmTitle", "Reimport Markdown"));
	if (Choice != EAppReturnType::Yes)
	{
		return;
	}

	if (!MarkdownFile->ImportFromFile(MarkdownFile->SourceFilePath))
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(LOCTEXT("ReimportFailed", "读取源文件失败：{0}"),
				FText::FromString(MarkdownFile->SourceFilePath)));
		return;
	}

	EditBuffer = FText::FromString(MarkdownFile->Content);
	if (TextEditorWidget.IsValid())
	{
		TextEditorWidget->SetText(EditBuffer);
	}
	RefreshPreview();
}

FText FMarkdownAssetEditor::GetStatusText() const
{
	const int32 LineCount = MarkdownFile ? MarkdownFile->GetLineCount() : 0;
	const int32 CharCount = EditBuffer.ToString().Len();
	const bool bDirty = MarkdownFile != nullptr
		&& MarkdownFile->GetOutermost() != nullptr
		&& MarkdownFile->GetOutermost()->IsDirty();

	return FText::FromString(FString::Printf(
		TEXT("%d 行 · %d 字符 · %s    |    Ctrl+B 加粗   Ctrl+I 斜体   Ctrl+K 链接   Tab 缩进 / 反向缩进"),
		LineCount, CharCount, bDirty ? TEXT("未保存") : TEXT("已保存")));
}

// ============================================================================
// Tabs
// ============================================================================

TSharedRef<SDockTab> FMarkdownAssetEditor::SpawnTab_Edit(const FSpawnTabArgs& Args)
{
	TextEditorWidget = SNew(SMultiLineEditableText)
		.Text_Raw(this, &FMarkdownAssetEditor::GetEditText)
		.OnTextChanged_Raw(this, &FMarkdownAssetEditor::OnEditTextChanged)
		.OnKeyDownHandler(FOnKeyDown::CreateRaw(this, &FMarkdownAssetEditor::OnEditorKeyDown))
		.Font(FCoreStyle::GetDefaultFontStyle("Mono", 10))
		.Margin(FMargin(6.0f))
		.IsReadOnly(false);

	return SNew(SDockTab)
		.Label(LOCTEXT("EditTab", "Edit"))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight().Padding(2.0f)
			[
				BuildFormatBar()
			]

			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				SNew(SBorder)
				.BorderImage(MARKDOWN_STYLE::GetBrush("ToolPanel.GroupBorder"))
				[
					TextEditorWidget.ToSharedRef()
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(4.0f, 2.0f)
			[
				SAssignNew(StatusTextBlock, STextBlock)
				.Text(GetStatusText())
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
				.ColorAndOpacity(FLinearColor(0.65f, 0.65f, 0.65f))
			]
		];
}

TSharedRef<SDockTab> FMarkdownAssetEditor::SpawnTab_Preview(const FSpawnTabArgs& Args)
{
	TSharedRef<SMarkdownPreview> Preview = SNew(SMarkdownPreview)
		.Content(EditBuffer.ToString());

	// 保留弱引用，编辑时用它刷新预览
	PreviewWidgetPtr = Preview;

	return SNew(SDockTab)
		.Label(LOCTEXT("PreviewTab", "Preview"))
		[
			Preview
		];
}

void FMarkdownAssetEditor::RefreshPreview()
{
	if (TSharedPtr<SMarkdownPreview> Pinned = PreviewWidgetPtr.Pin())
	{
		Pinned->SetContent(EditBuffer.ToString());
	}

	if (StatusTextBlock.IsValid())
	{
		StatusTextBlock->SetText(GetStatusText());
	}
}

// ============================================================================
// 文本回调
// ============================================================================

FText FMarkdownAssetEditor::GetEditText() const
{
	return EditBuffer;
}

void FMarkdownAssetEditor::OnEditTextChanged(const FText& NewText)
{
	EditBuffer = NewText;

	// 每次改动都同步进资产并标脏。
	// 标准 Save 命令、关闭提示与「保存全部」只认资产内容，
	// 早先只把新文本留在缓冲区里，保存后写盘的仍是旧内容。
	if (MarkdownFile)
	{
		MarkdownFile->SetContent(NewText.ToString());
	}

	RefreshPreview();
}

#undef LOCTEXT_NAMESPACE

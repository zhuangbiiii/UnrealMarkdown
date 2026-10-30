// UnrealMarkdown - MIT License, see LICENSE in the plugin root.
// 本文件已按 ZBSStudio 的本地集成需要重写：修复段落不换行，补齐表格 / 任务列表 / 嵌套 / 链接 / 删除线。

#include "MarkdownPreviewWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"
#include "MarkdownStyleCompat.h"

namespace MarkdownPreviewPrivate
{
	/** 预览最大渲染行数；超出只提示剩余行数，避免超大文档拖死编辑器。 */
	static constexpr int32 MaxPreviewLines = 2000;

	/** 单个纯文本片段超过这个词数就整体当一个槽位，防止一屏造出上万个控件。 */
	static constexpr int32 MaxPlainWords = 400;

	static const FLinearColor BodyColor(0.85f, 0.85f, 0.85f);
	static const FLinearColor DimColor(0.62f, 0.62f, 0.62f);
	static const FLinearColor CodeColor(0.92f, 0.66f, 0.34f);

	/** 行内样式。 */
	enum class ERunStyle : uint8
	{
		Plain,
		Bold,
		Italic,
		Code,
		Strike
	};
}

void SMarkdownPreview::Construct(const FArguments& InArgs)
{
	ContentBox = SNew(SVerticalBox);

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(MARKDOWN_STYLE::GetBrush("ToolPanel.GroupBorder"))
		.Padding(10.0f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				ContentBox.ToSharedRef()
			]
		]
	];

	Rebuild(InArgs._Content);
}

void SMarkdownPreview::SetContent(const FString& InContent)
{
	PendingContent = InContent;
	if (!bUpdatePending)
	{
		bUpdatePending = true;
		RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SMarkdownPreview::DeferredRebuild));
	}
}

EActiveTimerReturnType SMarkdownPreview::DeferredRebuild(double, float)
{
	bUpdatePending = false;
	Rebuild(PendingContent);
	return EActiveTimerReturnType::Stop;
}

void SMarkdownPreview::Rebuild(const FString& InContent)
{
	if (!ContentBox.IsValid()) return;

	ContentBox->ClearChildren();

	if (InContent.IsEmpty())
	{
		ContentBox->AddSlot().AutoHeight().Padding(4.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("(empty document)")))
			.ColorAndOpacity(FLinearColor(0.4f, 0.4f, 0.4f))
			.Font(FCoreStyle::GetDefaultFontStyle("Italic", 10))
		];
		return;
	}

	TArray<FString> Lines;
	InContent.ParseIntoArrayLines(Lines);

	// 超出上限只提示剩余行数：超大文档不能把编辑器拖死。
	const int32 MaxLines = FMath::Min(Lines.Num(), MarkdownPreviewPrivate::MaxPreviewLines);
	for (int32 i = 0; i < MaxLines; )
	{
		i += ParseBlock(Lines, i, ContentBox);
	}

	if (Lines.Num() > MaxLines)
	{
		ContentBox->AddSlot().AutoHeight().Padding(4.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(FString::Printf(TEXT("... (%d more lines)"), Lines.Num() - MaxLines)))
			.ColorAndOpacity(FLinearColor(0.4f, 0.4f, 0.4f))
			.Font(FCoreStyle::GetDefaultFontStyle("Italic", 10))
		];
	}
}

// ============================================================================

int32 SMarkdownPreview::ParseBlock(const TArray<FString>& Lines, int32 StartIndex, TSharedPtr<SVerticalBox> Box)
{
	if (StartIndex >= Lines.Num()) return 1;

	const FString& Line = Lines[StartIndex];
	const FString Trimmed = Line.TrimStartAndEnd();

	if (Trimmed.IsEmpty())
	{
		Box->AddSlot().AutoHeight().Padding(1.0f)[SNew(STextBlock).Text(FText::GetEmpty())];
		return 1;
	}

	// ---- Headings: # through ###### followed by space ----
	for (int32 H = 6; H >= 1; --H)
	{
		FString Prefix;
		for (int32 k = 0; k < H; ++k) Prefix += TEXT("#");
		Prefix += TEXT(" ");

		if (Trimmed.StartsWith(Prefix))
		{
			FString HeadingText = Trimmed.RightChop(H + 1);
			int32 HeadingSize = FMath::Max(12, 28 - H * 3);
			Box->AddSlot().AutoHeight().Padding(0, H <= 2 ? 10 : 4, 0, 2)
			[
				SNew(STextBlock)
				.Text(FText::FromString(HeadingText))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", HeadingSize))
				.ColorAndOpacity(FLinearColor(0.95f, 0.95f, 0.95f))
			];
			return 1;
		}
	}

	// ---- Horizontal rule ----
	if (Trimmed == TEXT("---") || Trimmed == TEXT("***") || Trimmed == TEXT("___"))
	{
		Box->AddSlot().AutoHeight().Padding(0, 8)
		[
			SNew(SBorder)
			.BorderImage(MARKDOWN_STYLE::GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.3f, 0.3f, 0.3f))
			.DesiredSizeScale(FVector2D(1.0f, 1.0f))
		];
		return 1;
	}

	// ---- Code block ----
	if (Trimmed.StartsWith(TEXT("```")))
	{
		TSharedRef<SVerticalBox> CodeBox = SNew(SVerticalBox);
		int32 Consumed = 1;
		for (int32 j = StartIndex + 1; j < Lines.Num(); ++j)
		{
			if (Lines[j].TrimStartAndEnd().StartsWith(TEXT("```"))) { Consumed = j - StartIndex + 1; break; }
			CodeBox->AddSlot().AutoHeight()
			[
				SNew(STextBlock).Text(FText::FromString(Lines[j]))
				.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
				.ColorAndOpacity(FLinearColor(0.8f, 0.85f, 0.9f))
			];
			if (j == Lines.Num() - 1) Consumed = Lines.Num() - StartIndex;
		}
		Box->AddSlot().AutoHeight().Padding(4, 2, 4, 8)
		[
			SNew(SBorder)
			.BorderImage(MARKDOWN_STYLE::GetBrush("ToolPanel.DarkGroupBorder"))
			.Padding(8.0f)
			[CodeBox]
		];
		return Consumed;
	}

	// ---- Blockquote ----
	if (Trimmed.StartsWith(TEXT("> ")))
	{
		Box->AddSlot().AutoHeight().Padding(8, 2, 0, 2)
		[
			SNew(SBorder)
			.BorderImage(MARKDOWN_STYLE::GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.3f, 0.5f, 0.9f, 0.6f))
			.Padding(FMargin(8, 2, 2, 2))
			[
				SNew(STextBlock).Text(FText::FromString(Trimmed.RightChop(2)))
				.Font(FCoreStyle::GetDefaultFontStyle("Italic", 10))
				.ColorAndOpacity(FLinearColor(0.7f, 0.8f, 0.95f))
			]
		];
		return 1;
	}

	// ---- 列表（无序 / 任务 / 有序）----
	if (IsListLine(Trimmed))
	{
		int32 Consumed = 0;
		int32 ItemNumber = 1;
		for (int32 j = StartIndex; j < Lines.Num(); ++j)
		{
			const FString SubTrimmed = Lines[j].TrimStartAndEnd();
			if (SubTrimmed.IsEmpty())
			{
				++Consumed;
				break;
			}
			if (!IsListLine(SubTrimmed))
			{
				break;
			}

			// 缩进按两空格一层换算，嵌套层级用左侧留白表达。
			const int32 IndentLevel = GetIndentLevel(Lines[j]);

			FString Marker = TEXT("•");
			FString Body = SubTrimmed;
			if (SubTrimmed.StartsWith(TEXT("- [ ]")) || SubTrimmed.StartsWith(TEXT("* [ ]")))
			{
				Marker = TEXT("[ ]");
				Body = SubTrimmed.RightChop(5).TrimStart();
			}
			else if (SubTrimmed.StartsWith(TEXT("- [x]")) || SubTrimmed.StartsWith(TEXT("- [X]"))
				|| SubTrimmed.StartsWith(TEXT("* [x]")) || SubTrimmed.StartsWith(TEXT("* [X]")))
			{
				Marker = TEXT("[x]");
				Body = SubTrimmed.RightChop(5).TrimStart();
			}
			else if (SubTrimmed.StartsWith(TEXT("- ")) || SubTrimmed.StartsWith(TEXT("* "))
				|| SubTrimmed.StartsWith(TEXT("+ ")))
			{
				Marker = TEXT("•");
				Body = SubTrimmed.RightChop(2);
			}
			else
			{
				const int32 DotIndex = SubTrimmed.Find(TEXT(". "));
				Marker = FString::Printf(TEXT("%d."), ItemNumber);
				++ItemNumber;
				Body = (DotIndex != INDEX_NONE) ? SubTrimmed.RightChop(DotIndex + 2) : SubTrimmed;
			}

			Box->AddSlot().AutoHeight().Padding(FMargin(16 + IndentLevel * 14, 1, 0, 1))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Marker + TEXT("  ")))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
					.ColorAndOpacity(MarkdownPreviewPrivate::DimColor)
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					BuildInlineText(Body, FCoreStyle::GetDefaultFontStyle("Regular", 10), MarkdownPreviewPrivate::BodyColor)
				]
			];
			++Consumed;
		}
		return FMath::Max(1, Consumed);
	}

	// ---- 表格 ----
	if (Trimmed.StartsWith(TEXT("|")) && StartIndex + 1 < Lines.Num() && IsTableSeparator(Lines[StartIndex + 1]))
	{
		int32 TableConsumed = 0;
		Box->AddSlot().AutoHeight().Padding(0, 4, 0, 8)
		[
			BuildTable(Lines, StartIndex, TableConsumed)
		];
		return FMath::Max(1, TableConsumed);
	}

	// ---- 段落 ----
	Box->AddSlot().AutoHeight().Padding(0, 1, 0, 4)
	[
		BuildInlineText(Trimmed, FCoreStyle::GetDefaultFontStyle("Regular", 10), MarkdownPreviewPrivate::BodyColor)
	];
	return 1;
}

// ============================================================================

TSharedRef<SWidget> SMarkdownPreview::BuildInlineText(const FString& Line, const FSlateFontInfo& BaseFont, const FLinearColor& Color)
{
	// 不能像早先那样横向拼 AutoWidth 槽位：那样长段落永远不会换行，只会一直往右撑。
	// 现在把纯文本片段按词拆开交给 SWrapBox，样式片段整体占一个槽位，换行点落在词之间。
	TSharedRef<SWrapBox> WrapBox = SNew(SWrapBox)
		.UseAllottedSize(true)
		.InnerSlotPadding(FVector2D::ZeroVector);

	const int32 Len = Line.Len();
	int32 Cursor = 0;

	// 追加一段文本；只有纯文本才按词拆。
	auto AddRun = [&WrapBox, &BaseFont, &Color](const FString& Text, int32 Style)
	{
		if (Text.IsEmpty())
		{
			return;
		}

		using MarkdownPreviewPrivate::ERunStyle;

		const ERunStyle RunStyle = static_cast<ERunStyle>(Style);
		const FSlateFontInfo Font = RunStyle == ERunStyle::Code
			? FCoreStyle::GetDefaultFontStyle(TEXT("Mono"), FMath::Max(8, static_cast<int32>(BaseFont.Size) - 1))
			: (RunStyle == ERunStyle::Bold
				? FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), BaseFont.Size)
				: (RunStyle == ERunStyle::Italic
					? FCoreStyle::GetDefaultFontStyle(TEXT("Italic"), BaseFont.Size)
					: BaseFont));

		const FLinearColor RunColor = RunStyle == ERunStyle::Code
			? MarkdownPreviewPrivate::CodeColor
			: (RunStyle == ERunStyle::Strike ? Color * 0.55f : Color);

		TArray<FString> Words;
		if (RunStyle == ERunStyle::Plain)
		{
			Text.ParseIntoArray(Words, TEXT(" "), true);
		}

		// 超长纯文本不拆词，避免一屏造出上万个控件。
		if (RunStyle != ERunStyle::Plain || Words.Num() == 0 || Words.Num() > MarkdownPreviewPrivate::MaxPlainWords)
		{
			WrapBox->AddSlot()
			[
				SNew(STextBlock)
				.Text(FText::FromString(Text + TEXT(" ")))
				.Font(Font)
				.ColorAndOpacity(RunColor)
			];
			return;
		}

		for (int32 WordIndex = 0; WordIndex < Words.Num(); ++WordIndex)
		{
			const FString Token = (WordIndex + 1 < Words.Num()) ? Words[WordIndex] + TEXT(" ") : Words[WordIndex];
			WrapBox->AddSlot()
			[
				SNew(STextBlock)
				.Text(FText::FromString(Token))
				.Font(Font)
				.ColorAndOpacity(RunColor)
			];
		}
	};

	while (Cursor < Len)
	{
		// ---- `code` ----
		if (Line[Cursor] == TEXT('`'))
		{
			const int32 Close = Line.Find(TEXT("`"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Cursor + 1);
			if (Close != INDEX_NONE)
			{
				AddRun(Line.Mid(Cursor + 1, Close - Cursor - 1),
					static_cast<int32>(MarkdownPreviewPrivate::ERunStyle::Code));
				Cursor = Close + 1;
				continue;
			}
		}

		// ---- ~~strike~~ ----
		if (Line[Cursor] == TEXT('~') && Cursor + 1 < Len && Line[Cursor + 1] == TEXT('~'))
		{
			const int32 Close = Line.Find(TEXT("~~"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Cursor + 2);
			if (Close != INDEX_NONE)
			{
				AddRun(Line.Mid(Cursor + 2, Close - Cursor - 2),
					static_cast<int32>(MarkdownPreviewPrivate::ERunStyle::Strike));
				Cursor = Close + 2;
				continue;
			}
		}

		// ---- **bold** ----
		if (Line[Cursor] == TEXT('*') && Cursor + 1 < Len && Line[Cursor + 1] == TEXT('*'))
		{
			const int32 Close = Line.Find(TEXT("**"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Cursor + 2);
			if (Close != INDEX_NONE)
			{
				AddRun(Line.Mid(Cursor + 2, Close - Cursor - 2),
					static_cast<int32>(MarkdownPreviewPrivate::ERunStyle::Bold));
				Cursor = Close + 2;
				continue;
			}
		}

		// ---- *italic* ----
		if (Line[Cursor] == TEXT('*'))
		{
			const int32 Close = Line.Find(TEXT("*"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Cursor + 1);
			if (Close != INDEX_NONE)
			{
				AddRun(Line.Mid(Cursor + 1, Close - Cursor - 1),
					static_cast<int32>(MarkdownPreviewPrivate::ERunStyle::Italic));
				Cursor = Close + 1;
				continue;
			}
		}

		// ---- [text](url)：文字正常显示，网址暗一档，免得长链接把整行撑开 ----
		if (Line[Cursor] == TEXT('['))
		{
			const int32 TextEnd = Line.Find(TEXT("]"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Cursor + 1);
			if (TextEnd != INDEX_NONE && TextEnd + 1 < Len && Line[TextEnd + 1] == TEXT('('))
			{
				const int32 UrlEnd = Line.Find(TEXT(")"), ESearchCase::CaseSensitive, ESearchDir::FromStart, TextEnd + 2);
				if (UrlEnd != INDEX_NONE)
				{
					AddRun(Line.Mid(Cursor + 1, TextEnd - Cursor - 1),
						static_cast<int32>(MarkdownPreviewPrivate::ERunStyle::Plain));
					AddRun(FString::Printf(TEXT("(%s)"), *Line.Mid(TextEnd + 2, UrlEnd - TextEnd - 2)),
						static_cast<int32>(MarkdownPreviewPrivate::ERunStyle::Strike));
					Cursor = UrlEnd + 1;
					continue;
				}
			}
		}

		// ---- 普通字符 ----
		const int32 RunStart = Cursor;
		++Cursor;
		while (Cursor < Len)
		{
			const TCHAR Char = Line[Cursor];
			if (Char == TEXT('`') || Char == TEXT('*') || Char == TEXT('~') || Char == TEXT('['))
			{
				break;
			}
			++Cursor;
		}
		AddRun(Line.Mid(RunStart, Cursor - RunStart),
			static_cast<int32>(MarkdownPreviewPrivate::ERunStyle::Plain));
	}

	return WrapBox;
}

bool SMarkdownPreview::IsListLine(const FString& Line)
{
	if (Line.StartsWith(TEXT("- ")) || Line.StartsWith(TEXT("* ")) || Line.StartsWith(TEXT("+ ")))
	{
		return true;
	}

	// 有序列表：至少一位数字，紧跟 ". "。
	int32 DigitEnd = 0;
	while (DigitEnd < Line.Len() && FChar::IsDigit(Line[DigitEnd]))
	{
		++DigitEnd;
	}
	return DigitEnd > 0 && Line.Len() > DigitEnd + 1
		&& Line[DigitEnd] == TEXT('.') && Line[DigitEnd + 1] == TEXT(' ');
}

int32 SMarkdownPreview::GetIndentLevel(const FString& Line)
{
	int32 Spaces = 0;
	for (int32 Index = 0; Index < Line.Len(); ++Index)
	{
		if (Line[Index] == TEXT(' '))
		{
			++Spaces;
		}
		else if (Line[Index] == TEXT('\t'))
		{
			Spaces += 2;
		}
		else
		{
			break;
		}
	}
	return Spaces / 2;
}

bool SMarkdownPreview::IsTableSeparator(const FString& Line)
{
	const FString Trimmed = Line.TrimStartAndEnd();
	if (!Trimmed.StartsWith(TEXT("|")) || Trimmed.Len() < 2)
	{
		return false;
	}

	// 只允许 |、-、: 和空格，且必须至少有一个 '-'。
	bool bHasDash = false;
	for (const TCHAR Char : Trimmed)
	{
		if (Char == TEXT('-'))
		{
			bHasDash = true;
		}
		else if (Char != TEXT('|') && Char != TEXT(':') && Char != TEXT(' '))
		{
			return false;
		}
	}
	return bHasDash;
}

void SMarkdownPreview::SplitTableRow(const FString& Line, TArray<FString>& OutCells)
{
	OutCells.Reset();

	FString Trimmed = Line.TrimStartAndEnd();
	// 去掉首尾竖线，否则会多出两个空单元格。
	if (Trimmed.StartsWith(TEXT("|")))
	{
		Trimmed.RightChopInline(1, EAllowShrinking::No);
	}
	if (Trimmed.EndsWith(TEXT("|")))
	{
		Trimmed.LeftChopInline(1, EAllowShrinking::No);
	}

	Trimmed.ParseIntoArray(OutCells, TEXT("|"), false);
	for (FString& Cell : OutCells)
	{
		Cell = Cell.TrimStartAndEnd();
	}
}

TSharedRef<SWidget> SMarkdownPreview::BuildTable(const TArray<FString>& Lines, int32 StartIndex, int32& OutConsumed)
{
	// 首行是表头，第二行是分隔行，之后到第一个非表格行为止都属于这张表。
	TArray<FString> HeaderCells;
	SplitTableRow(Lines[StartIndex], HeaderCells);

	TArray<TArray<FString>> Rows;
	int32 Index = StartIndex + 2;
	while (Index < Lines.Num())
	{
		const FString RowTrimmed = Lines[Index].TrimStartAndEnd();
		if (!RowTrimmed.StartsWith(TEXT("|")))
		{
			break;
		}
		TArray<FString> Cells;
		SplitTableRow(Lines[Index], Cells);
		Rows.Add(MoveTemp(Cells));
		++Index;
	}

	OutConsumed = Index - StartIndex;

	const int32 ColumnCount = FMath::Max(1, HeaderCells.Num());

	TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel)
		.SlotPadding(FMargin(6.0f, 3.0f));

	for (int32 Column = 0; Column < ColumnCount; ++Column)
	{
		Grid->AddSlot(Column, 0)
		[
			SNew(STextBlock)
			.Text(FText::FromString(HeaderCells.IsValidIndex(Column) ? HeaderCells[Column] : FString()))
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 10))
			.ColorAndOpacity(FLinearColor(0.95f, 0.95f, 0.95f))
		];
	}

	for (int32 RowIndex = 0; RowIndex < Rows.Num(); ++RowIndex)
	{
		for (int32 Column = 0; Column < ColumnCount; ++Column)
		{
			Grid->AddSlot(Column, RowIndex + 1)
			[
				BuildInlineText(Rows[RowIndex].IsValidIndex(Column) ? Rows[RowIndex][Column] : FString(),
					FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 10),
					MarkdownPreviewPrivate::BodyColor)
			];
		}
	}

	return SNew(SBorder)
		.BorderImage(MARKDOWN_STYLE::GetBrush("ToolPanel.GroupBorder"))
		.Padding(6.0f)
		[
			Grid
		];
}

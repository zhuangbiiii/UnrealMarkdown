// Copyright Epic Games, Inc. All Rights Reserved.

#include "MarkdownPreviewWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "MarkdownStyleCompat.h"

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

	// Safety limit: max 1000 lines in preview
	const int32 MaxLines = FMath::Min(Lines.Num(), 1000);
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

	// ---- Unordered list ----
	if (Trimmed.StartsWith(TEXT("- ")) || Trimmed.StartsWith(TEXT("* ")))
	{
		int32 Consumed = 0;
		for (int32 j = StartIndex; j < Lines.Num(); ++j)
		{
			const FString SubTrimmed = Lines[j].TrimStartAndEnd();
			if (SubTrimmed.StartsWith(TEXT("- ")) || SubTrimmed.StartsWith(TEXT("* ")))
			{
				Box->AddSlot().AutoHeight().Padding(20, 1, 0, 1)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("  •  ")))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
						.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						BuildInlineText(SubTrimmed.RightChop(2), FCoreStyle::GetDefaultFontStyle("Regular", 10), FLinearColor(0.85f, 0.85f, 0.85f))
					]
				];
				++Consumed;
			}
			else if (SubTrimmed.IsEmpty()) { ++Consumed; break; }
			else break;
		}
		return FMath::Max(1, Consumed);
	}

	// ---- Ordered list ----
	if (Trimmed.Len() > 2 && FChar::IsDigit(Trimmed[0]) && Trimmed.Contains(TEXT(". ")))
	{
		int32 Consumed = 0, ItemNum = 1;
		for (int32 j = StartIndex; j < Lines.Num(); ++j)
		{
			const FString SubTrimmed = Lines[j].TrimStartAndEnd();
			int32 DotIdx = SubTrimmed.Find(TEXT(". "));
			if (SubTrimmed.Len() > 2 && FChar::IsDigit(SubTrimmed[0]) && DotIdx > 0)
			{
				Box->AddSlot().AutoHeight().Padding(20, 1, 0, 1)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT(" %d.  "), ItemNum++)))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
						.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						BuildInlineText(SubTrimmed.RightChop(DotIdx + 2), FCoreStyle::GetDefaultFontStyle("Regular", 10), FLinearColor(0.85f, 0.85f, 0.85f))
					]
				];
				++Consumed;
			}
			else { if (SubTrimmed.IsEmpty()) ++Consumed; break; }
		}
		return FMath::Max(1, Consumed);
	}

	// ---- Paragraph ----
	Box->AddSlot().AutoHeight().Padding(0, 1, 0, 4)
	[
		BuildInlineText(Trimmed, FCoreStyle::GetDefaultFontStyle("Regular", 10), FLinearColor(0.85f, 0.85f, 0.85f))
	];
	return 1;
}

// ============================================================================

TSharedRef<SWidget> SMarkdownPreview::BuildInlineText(const FString& Line, const FSlateFontInfo& BaseFont, const FLinearColor& Color)
{
	TSharedRef<SHorizontalBox> HBox = SNew(SHorizontalBox);

	const int32 Len = Line.Len();
	int32 i = 0;

	while (i < Len)
	{
		// ---- `code` ----
		if (Line[i] == TEXT('`'))
		{
			int32 j = i + 1;
			while (j < Len && Line[j] != TEXT('`')) ++j;
			if (j < Len)
			{
				FString Code = Line.Mid(i + 1, j - i - 1);
				HBox->AddSlot().AutoWidth()
				[
					SNew(SBorder)
					.BorderImage(MARKDOWN_STYLE::GetBrush("ToolPanel.DarkGroupBorder"))
					.Padding(FMargin(2, 0))
					[
						SNew(STextBlock).Text(FText::FromString(Code))
						.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
						.ColorAndOpacity(FLinearColor(0.9f, 0.6f, 0.3f))
					]
				];
				i = j + 1;
				continue;
			}
		}

		// ---- **bold** ----
		if (Line[i] == TEXT('*') && i + 1 < Len && Line[i + 1] == TEXT('*'))
		{
			int32 j = i + 2;
			while (j + 1 < Len && !(Line[j] == TEXT('*') && Line[j + 1] == TEXT('*'))) ++j;
			if (j + 1 < Len)
			{
				FString BoldText = Line.Mid(i + 2, j - i - 2);
				HBox->AddSlot().AutoWidth()
				[
					SNew(STextBlock).Text(FText::FromString(BoldText))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", BaseFont.Size))
					.ColorAndOpacity(Color)
				];
				i = j + 2;
				continue;
			}
		}

		// ---- *italic* (single *, not followed by another *) ----
		if (Line[i] == TEXT('*') && (i + 1 >= Len || Line[i + 1] != TEXT('*')))
		{
			int32 j = i + 1;
			while (j < Len && Line[j] != TEXT('*')) ++j;
			if (j < Len)
			{
				FString ItalicText = Line.Mid(i + 1, j - i - 1);
				HBox->AddSlot().AutoWidth()
				[
					SNew(STextBlock).Text(FText::FromString(ItalicText))
					.Font(FCoreStyle::GetDefaultFontStyle("Italic", BaseFont.Size))
					.ColorAndOpacity(Color)
				];
				i = j + 1;
				continue;
			}
		}

		// ---- Regular char ----
		int32 Start = i++;
		while (i < Len && Line[i] != TEXT('`') && Line[i] != TEXT('*')) ++i;
		HBox->AddSlot().AutoWidth()
		[
			SNew(STextBlock).Text(FText::FromString(Line.Mid(Start, i - Start)))
			.Font(BaseFont).ColorAndOpacity(Color)
		];
	}

	return HBox;
}

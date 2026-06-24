// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class SVerticalBox;

/**
 * A Slate widget that renders Markdown text as styled widgets.
 *
 * Rebuild is deferred to the next Slate frame via active timer,
 * preventing widget-tree modification during input event processing.
 */
class SMarkdownPreview : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMarkdownPreview) {}
		SLATE_ARGUMENT(FString, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Request a preview refresh (deferred to next frame). */
	void SetContent(const FString& InContent);

private:
	TSharedPtr<SVerticalBox> ContentBox;
	FString PendingContent;
	bool bUpdatePending = false;

	void Rebuild(const FString& InContent);
	EActiveTimerReturnType DeferredRebuild(double, float);

	int32 ParseBlock(const TArray<FString>& Lines, int32 StartIndex, TSharedPtr<SVerticalBox> Box);
	TSharedRef<SWidget> BuildInlineText(const FString& Line, const FSlateFontInfo& BaseFont, const FLinearColor& Color);
};

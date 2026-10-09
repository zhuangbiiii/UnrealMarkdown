#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class SVerticalBox;

/** 可供其他编辑器模块复用的 Markdown 预览；内容更新延迟到下一 Slate 帧。 */
class UNREALMARKDOWNEDITOR_API SMarkdownPreview : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMarkdownPreview) {}
		SLATE_ARGUMENT(FString, Content)
	SLATE_END_ARGS()
	void Construct(const FArguments& InArgs);
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

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class SVerticalBox;

/**
 * 可供其他编辑器模块复用的 Markdown 预览。
 *
 * 手写 Slate 渲染，不依赖 UMG 资产，所以编辑器里打开就能用。
 * 内容更新延迟到主动计时器，连续输入不会每敲一个键就重建整棵控件树。
 */
class UNREALMARKDOWNEDITOR_API SMarkdownPreview : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMarkdownPreview) {}
		SLATE_ARGUMENT(FString, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** 换内容；真正的重建推迟到下一帧。 */
	void SetContent(const FString& InContent);

private:
	TSharedPtr<SVerticalBox> ContentBox;
	FString PendingContent;
	bool bUpdatePending = false;

	void Rebuild(const FString& InContent);
	EActiveTimerReturnType DeferredRebuild(double, float);

	/** 解析一个块，返回消耗掉的行数。 */
	int32 ParseBlock(const TArray<FString>& Lines, int32 StartIndex, TSharedPtr<SVerticalBox> Box);

	/**
	 * 行内标记 → 可自动换行的控件。
	 *
	 * 段落必须能换行，因此纯文本片段会按词拆开交给 SWrapBox；
	 * 带样式的片段通常很短，整体占一个槽位。
	 */
	TSharedRef<SWidget> BuildInlineText(const FString& Line, const FSlateFontInfo& BaseFont, const FLinearColor& Color);

	/** 渲染管道表格，OutConsumed 返回消耗掉的行数。 */
	TSharedRef<SWidget> BuildTable(const TArray<FString>& Lines, int32 StartIndex, int32& OutConsumed);

	/** 是否为表格分隔行，形如 |---|:--:|。 */
	static bool IsTableSeparator(const FString& Line);

	/** 把 |a|b| 拆成 a、b。 */
	static void SplitTableRow(const FString& Line, TArray<FString>& OutCells);

	/** 是否为列表项（无序 / 有序 / 任务）。 */
	static bool IsListLine(const FString& Line);

	/** 行首缩进对应的层级，两空格一层。 */
	static int32 GetIndentLevel(const FString& Line);
};

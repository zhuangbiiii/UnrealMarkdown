// UnrealMarkdown - MIT License, see LICENSE in the plugin root.
// 新增文件：供编辑器格式工具栏、预览与 MCP 工具共用的 Markdown 语法工具。

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "MarkdownSyntax.generated.h"

/** 大纲里的一条标题。 */
USTRUCT(BlueprintType)
struct FMarkdownOutlineEntry
{
	GENERATED_BODY()

	/** 标题所在行号，从 0 开始。 */
	UPROPERTY(BlueprintReadOnly, Category = "Markdown")
	int32 Line = 0;

	/** 标题层级，1 到 6。 */
	UPROPERTY(BlueprintReadOnly, Category = "Markdown")
	int32 Level = 0;

	/** 去掉井号后的标题文本。 */
	UPROPERTY(BlueprintReadOnly, Category = "Markdown")
	FString Text;
};

/**
 * Markdown 的纯文本处理工具。
 *
 * 只做字符串操作，不建语法树、不依赖 Slate，因此运行时、编辑器与 MCP 三处共用同一份实现，
 * 避免工具栏与工具集各写一套、行为慢慢漂移。
 */
UCLASS()
class UNREALMARKDOWN_API UMarkdownSyntaxLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 抽取全部 ATX 标题（# 到 ######）。
	 *
	 * 井号超过三个的空格缩进、井号后不跟空格（形如 #标签）都不算标题。
	 *
	 * @return 按行号升序的标题列表。
	 */
	UFUNCTION(BlueprintPure, Category = "Markdown|Syntax")
	static TArray<FMarkdownOutlineEntry> ExtractOutline(const FString& Source);

	/**
	 * 计算回车后应自动续上的行前缀。
	 *
	 * 覆盖无序列表、有序列表、任务列表与引用块；有序列表的序号会加一。
	 *
	 * @return 续行前缀，含原有的行首缩进；空串表示这一行不需要续。
	 */
	UFUNCTION(BlueprintPure, Category = "Markdown|Syntax")
	static FString GetLineContinuation(const FString& Line);

	/**
	 * 给选区加前后缀。
	 *
	 * 选区为 [SelectionStart, SelectionEnd)。空选区时把前后缀一起插在光标处，
	 * 结果光标落在两个标记之间，可以直接开始输入。
	 *
	 * @param OutText 处理后的全文。
	 * @param OutSelectionStart 处理后应恢复的选区起点。
	 * @param OutSelectionEnd 处理后应恢复的选区终点。
	 */
	UFUNCTION(BlueprintCallable, Category = "Markdown|Syntax")
	static void WrapSelection(
		const FString& Source,
		int32 SelectionStart,
		int32 SelectionEnd,
		const FString& Prefix,
		const FString& Suffix,
		FString& OutText,
		int32& OutSelectionStart,
		int32& OutSelectionEnd);

	/**
	 * 对选区覆盖的每一行加上或去掉同一前缀。
	 *
	 * 有内容的行全部已带该前缀时执行移除，否则统一加上，所以连按两次等于撤销。
	 *
	 * @param OutText 处理后的全文。
	 */
	UFUNCTION(BlueprintCallable, Category = "Markdown|Syntax")
	static void ToggleLinePrefix(
		const FString& Source,
		int32 SelectionStart,
		int32 SelectionEnd,
		const FString& Prefix,
		FString& OutText,
		int32& OutSelectionStart,
		int32& OutSelectionEnd);
};

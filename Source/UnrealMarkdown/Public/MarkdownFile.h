// UnrealMarkdown - MIT License, see LICENSE in the plugin root.
// 本文件已按 ZBSStudio 的本地集成需要扩充。

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "MarkdownFile.generated.h"

/**
 * 以资产生存的 Markdown 文档。
 *
 * 只承载文本，不做解析与渲染：段落解析在编辑器的预览控件里，
 * 语法层面的字符串操作在 UMarkdownSyntaxLibrary 里。
 */
UCLASS(BlueprintType, hidecategories = (Object))
class UNREALMARKDOWN_API UMarkdownFile : public UObject
{
	GENERATED_BODY()

public:
	/** 原始 Markdown 文本。 */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Markdown")
	FString Content;

	/**
	 * 导入来源文件。
	 * 为空表示资产是新建的，没有可以重导入的源文件。
	 */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Markdown")
	FString SourceFilePath;

	/** 覆盖全文。内容没变时不标记脏包。 */
	UFUNCTION(BlueprintCallable, Category = "Markdown")
	void SetContent(const FString& InContent);

	/** 行数；空文档为 0。 */
	UFUNCTION(BlueprintPure, Category = "Markdown")
	int32 GetLineCount() const;

	/** 字符数，包含换行符。 */
	UFUNCTION(BlueprintPure, Category = "Markdown")
	int32 GetCharCount() const;

	UFUNCTION(BlueprintPure, Category = "Markdown")
	bool IsEmpty() const;

	/** 在末尾追加文本。 */
	UFUNCTION(BlueprintCallable, Category = "Markdown")
	void AppendText(const FString& InText);

	/**
	 * 在指定行之前插入文本。
	 *
	 * @param LineIndex 从 0 开始；等于行数时追加到末尾，越界返回 false。
	 * @param InText 插入的文本，可含换行；没有结尾换行时会按文档现有换行符补一个。
	 * @return 是否插入成功。
	 */
	UFUNCTION(BlueprintCallable, Category = "Markdown")
	bool InsertAtLine(int32 LineIndex, const FString& InText);

	/**
	 * 文本替换。
	 *
	 * @param OldText 被替换的文本；空串直接返回 0。
	 * @param NewText 替换成的文本。
	 * @param bReplaceAll true 替换全部，false 只替换第一处。
	 * @return 实际替换处数。
	 */
	UFUNCTION(BlueprintCallable, Category = "Markdown")
	int32 ReplaceInContent(const FString& OldText, const FString& NewText, bool bReplaceAll);

	/** 内容的 MD5（按 UTF-8 计算），用于廉价地判断内容是否变化。 */
	UFUNCTION(BlueprintPure, Category = "Markdown")
	FString GetContentHash() const;

	/**
	 * 从磁盘读入内容，并把来源记入 SourceFilePath。
	 *
	 * @return 读取成功时为 true；失败时资产内容保持不变。
	 */
	UFUNCTION(BlueprintCallable, Category = "Markdown")
	bool ImportFromFile(const FString& Filename);

	/**
	 * 把当前内容写到磁盘文件。
	 *
	 * @param Filename 目标绝对路径，父目录必须已存在。
	 * @param bUtf8 true 写带 BOM 的 UTF-8，外部编辑器才能正确识别编码。
	 * @return 写入成功时为 true。
	 */
	UFUNCTION(BlueprintCallable, Category = "Markdown")
	bool ExportToFile(const FString& Filename, bool bUtf8 = true) const;

#if WITH_EDITOR
	// ---- UObject ----
	// PostEditChangeProperty 只在编辑器构建的 UObject 上存在，此处必须同样只在
	// 编辑器构建里声明，否则 Game 目标会因为「override 没有覆写任何基类成员」编不过。
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	/** 内容变更后的统一收尾：标记脏包，让保存与关闭提示都能看到。 */
	void MarkContentChanged();
};

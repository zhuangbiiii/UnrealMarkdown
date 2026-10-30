// UnrealMarkdown - MIT License, see LICENSE in the plugin root.
// 本文件已按 ZBSStudio 的本地集成需要重写：修复保存丢失，加入格式工具栏与状态栏。

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/AssetEditorToolkit.h"

class SMultiLineEditableText;
class SMarkdownPreview;
class STextBlock;
class UMarkdownFile;

/**
 * UMarkdownFile 的资产编辑器。
 *
 * 左侧源码、右侧预览。每次文本改动都立刻写回资产并标脏，因此标准保存命令、
 * 关闭提示与「保存全部」拿到的永远是最新内容。
 */
class FMarkdownAssetEditor : public FAssetEditorToolkit
{
public:
	FMarkdownAssetEditor();
	virtual ~FMarkdownAssetEditor();

	void InitMarkdownEditor(EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UMarkdownFile* InMarkdownFile);

	// ---- FAssetEditorToolkit ----
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;

private:
	UMarkdownFile* MarkdownFile = nullptr;

	/** 控件文本的镜像；预览与状态栏都读它。 */
	FText EditBuffer;

	TSharedPtr<SMultiLineEditableText> TextEditorWidget;
	TWeakPtr<SMarkdownPreview> PreviewWidgetPtr;
	TSharedPtr<STextBlock> StatusTextBlock;

	// ---- Slate 回调 ----
	FText GetEditText() const;
	void OnEditTextChanged(const FText& NewText);
	void RefreshPreview();
	FReply OnEditorKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent);

	// ---- Markdown 编辑动作 ----
	/** 给选区加前后缀，并把选区恢复到标记内侧。 */
	void ApplyWrap(const FString& Prefix, const FString& Suffix);
	/** 给选区覆盖的每一行加/去同一个前缀。 */
	void ApplyLinePrefix(const FString& Prefix);
	/** 在光标处插入一整块文本，自动补齐前后换行。 */
	void InsertBlock(const FString& Text);

	/** 取选区的扁平字符偏移；没有选区时两者都等于光标位置。 */
	void GetSelectionOffsets(int32& OutStart, int32& OutEnd) const;
	/** 把新全文写回控件并恢复选区。 */
	void ApplyText(const FString& NewText, int32 SelectionStart, int32 SelectionEnd);

	// ---- 工具栏与按钮 ----
	void ExtendToolbar();
	TSharedRef<SWidget> BuildFormatBar();
	TSharedRef<SWidget> MakeWrapButton(const FText& Label, const FText& Tooltip, const FString& Prefix, const FString& Suffix);
	TSharedRef<SWidget> MakePrefixButton(const FText& Label, const FText& Tooltip, const FString& Prefix);

	FText GetStatusText() const;

	// ---- 资产级动作 ----
	void OnSave();
	void OnExport();
	void OnReimport();

	// ---- 标签页 ----
	TSharedRef<SDockTab> SpawnTab_Edit(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Preview(const FSpawnTabArgs& Args);

	static const FName EditTabId;
	static const FName PreviewTabId;
};


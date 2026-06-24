// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/AssetEditorToolkit.h"

class UMarkdownFile;
class SMultiLineEditableText;
class SMarkdownPreview;

/**
 * Custom asset editor for UMarkdownFile.
 *
 * Provides split-pane layout: Edit tab + Preview tab.
 * Edit buffer is separate from file content — only written on Save.
 */
class FMarkdownAssetEditor : public FAssetEditorToolkit
{
public:
	FMarkdownAssetEditor();
	virtual ~FMarkdownAssetEditor();

	void InitMarkdownEditor(EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UMarkdownFile* InMarkdownFile);

	static void RegisterToolkit();

	// ---- FAssetEditorToolkit ----
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;

private:
	UMarkdownFile* MarkdownFile = nullptr;
	FText EditBuffer;
	TSharedPtr<SMultiLineEditableText> TextEditorWidget;
	TWeakPtr<SMarkdownPreview> PreviewWidgetPtr;

	// ---- Slate callbacks ----
	FText GetEditText() const;
	void OnEditTextChanged(const FText& NewText);
	void RefreshPreview();

	// ---- Commands ----
	void BindCommands();
	void ExtendToolbar();
	void OnSave();
	void OnExport();

	// ---- Tab spawners ----
	TSharedRef<SDockTab> SpawnTab_Edit(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Preview(const FSpawnTabArgs& Args);

	static const FName EditTabId;
	static const FName PreviewTabId;
};

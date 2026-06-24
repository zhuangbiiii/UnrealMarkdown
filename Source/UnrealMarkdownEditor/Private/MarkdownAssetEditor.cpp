// Copyright Epic Games, Inc. All Rights Reserved.

#include "MarkdownAssetEditor.h"
#include "MarkdownFile.h"
#include "MarkdownPreviewWidget.h"

#include "DesktopPlatformModule.h"
#include "FileHelpers.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IDesktopPlatform.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Styling/AppStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Layout/SScrollBox.h"

#define LOCTEXT_NAMESPACE "MarkdownAssetEditor"

const FName FMarkdownAssetEditor::EditTabId(TEXT("MarkdownAssetEditor_Edit"));
const FName FMarkdownAssetEditor::PreviewTabId(TEXT("MarkdownAssetEditor_Preview"));

// ============================================================================

void FMarkdownAssetEditor::RegisterToolkit() {}

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

	BindCommands();

	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_MarkdownAssetEditor_Layout_v2")
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
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Viewports"));

	InTabManager->RegisterTabSpawner(PreviewTabId,
		FOnSpawnTab::CreateSP(this, &FMarkdownAssetEditor::SpawnTab_Preview))
		.SetDisplayName(LOCTEXT("PreviewTab", "Preview"))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));
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

void FMarkdownAssetEditor::BindCommands()
{
}

void FMarkdownAssetEditor::ExtendToolbar()
{
	struct Local
	{
		static void FillToolbar(FToolBarBuilder& ToolbarBuilder, FMarkdownAssetEditor* Editor)
		{
			ToolbarBuilder.BeginSection("File");
			{
				ToolbarBuilder.AddToolBarButton(
					FUIAction(FExecuteAction::CreateRaw(Editor, &FMarkdownAssetEditor::OnSave)),
					NAME_None,
					LOCTEXT("SaveBtn", "Save"),
					LOCTEXT("SaveBtnTip", "Save content to asset"),
					FSlateIcon(FAppStyle::GetAppStyleSetName(), "AssetEditor.SaveAsset"));
			}
			ToolbarBuilder.EndSection();

			ToolbarBuilder.BeginSection("Export");
			{
				ToolbarBuilder.AddToolBarButton(
					FUIAction(FExecuteAction::CreateRaw(Editor, &FMarkdownAssetEditor::OnExport)),
					NAME_None,
					LOCTEXT("ExportBtn", "Export"),
					LOCTEXT("ExportBtnTip", "Export to .md file on disk"),
					FSlateIcon(FAppStyle::GetAppStyleSetName(), "AssetEditor.SaveAsset.Greyscale"));
			}
			ToolbarBuilder.EndSection();
		}
	};

	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);
	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateStatic(&Local::FillToolbar, this));
	AddToolbarExtender(ToolbarExtender);
}

// ============================================================================
// Save / Export
// ============================================================================

void FMarkdownAssetEditor::OnSave()
{
	if (!MarkdownFile) return;

	// Flush edit buffer to asset
	MarkdownFile->Content = EditBuffer.ToString();
	MarkdownFile->MarkPackageDirty();

	TArray<UPackage*> PackagesToSave;
	PackagesToSave.Add(MarkdownFile->GetOutermost());
	FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, false, false);
}

void FMarkdownAssetEditor::OnExport()
{
	if (!MarkdownFile) return;

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform) return;

	TArray<FString> OutFiles;
	const FString DefaultName = MarkdownFile->GetName() + TEXT(".md");
	if (DesktopPlatform->SaveFileDialog(
		nullptr, TEXT("Export Markdown File"), FPaths::ProjectDir(), DefaultName,
		TEXT("Markdown Files (*.md)|*.md|Text Files (*.txt)|*.txt|All Files (*.*)|*.*"),
		EFileDialogFlags::None, OutFiles))
	{
		if (OutFiles.Num() > 0)
		{
			// Export current edit buffer (saving first is optional)
			FFileHelper::SaveStringToFile(EditBuffer.ToString(), *OutFiles[0]);
		}
	}
}

// ============================================================================
// Tabs
// ============================================================================

TSharedRef<SDockTab> FMarkdownAssetEditor::SpawnTab_Edit(const FSpawnTabArgs& Args)
{
	TextEditorWidget = SNew(SMultiLineEditableText)
		.Text_Raw(this, &FMarkdownAssetEditor::GetEditText)
		.OnTextChanged_Raw(this, &FMarkdownAssetEditor::OnEditTextChanged)
		.Font(FCoreStyle::GetDefaultFontStyle("Mono", 10))
		.IsReadOnly(false);

	return SNew(SDockTab)
		.Label(LOCTEXT("EditTab", "Edit"))
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				TextEditorWidget.ToSharedRef()
			]
		];
}

TSharedRef<SDockTab> FMarkdownAssetEditor::SpawnTab_Preview(const FSpawnTabArgs& Args)
{
	TSharedRef<SMarkdownPreview> Preview = SNew(SMarkdownPreview)
		.Content(EditBuffer.ToString());

	// Keep a weak reference so we can refresh it on edit
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
}

// ============================================================================
// Text callbacks
// ============================================================================

FText FMarkdownAssetEditor::GetEditText() const
{
	return EditBuffer;
}

void FMarkdownAssetEditor::OnEditTextChanged(const FText& NewText)
{
	EditBuffer = NewText;
	RefreshPreview();
}

#undef LOCTEXT_NAMESPACE

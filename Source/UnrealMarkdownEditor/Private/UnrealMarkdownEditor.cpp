// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealMarkdownEditor.h"
#include "MarkdownFile.h"
#include "MarkdownAssetEditor.h"
#include "MarkdownToolset.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Misc/MessageDialog.h"
#include "Styling/AppStyle.h"
#include "UObject/Class.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "Misc/CoreDelegates.h"

#define LOCTEXT_NAMESPACE "FUnrealMarkdownEditorModule"

// ============================================================================
// Asset Type Actions
// ============================================================================

class FAssetTypeActions_MarkdownFile : public FAssetTypeActions_Base
{
public:
	virtual FText GetName() const override
	{
		return LOCTEXT("AssetTypeActions_MarkdownFile", "Markdown File");
	}

	virtual FColor GetTypeColor() const override
	{
		return FColor(66, 133, 244);
	}

	virtual UClass* GetSupportedClass() const override
	{
		return UMarkdownFile::StaticClass();
	}

	virtual uint32 GetCategories() override
	{
		return EAssetTypeCategories::Misc;
	}

	virtual bool CanFilter() override { return true; }

	virtual FText GetAssetDescription(const FAssetData& AssetData) const override
	{
		return LOCTEXT("MarkdownAssetDescription", "A plain-text Markdown document that can be rendered with UnrealMarkdown widgets.");
	}

	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor) override
	{
		for (UObject* Obj : InObjects)
		{
			if (UMarkdownFile* MdFile = Cast<UMarkdownFile>(Obj))
			{
				TSharedRef<FMarkdownAssetEditor> Editor = MakeShareable(new FMarkdownAssetEditor());
				Editor->InitMarkdownEditor(EToolkitMode::Standalone, EditWithinLevelEditor, MdFile);
			}
		}
	}
};

// ============================================================================
// Module
// ============================================================================

void FUnrealMarkdownEditorModule::StartupModule()
{
	RegisterAssetTypeActions();
	FMarkdownAssetEditor::RegisterToolkit();

	// Defer toolset registration until the editor and ToolsetRegistry are fully ready.
	// In "Default" loading phase, some dependent subsystems may not be initialized yet.
	FCoreDelegates::GetOnPostEngineInit().AddLambda([this]()
	{
		if (UToolsetRegistry::IsAvailable())
		{
			UToolsetRegistry::RegisterToolsetClass(UMarkdownToolset::StaticClass());
			UE_LOG(LogTemp, Log, TEXT("UnrealMarkdownEditor: MarkdownToolset registered with ToolsetRegistry."));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("UnrealMarkdownEditor: ToolsetRegistry not available, Markdown MCP tools will not be exposed."));
		}
	});
}

void FUnrealMarkdownEditorModule::ShutdownModule()
{
	if (UToolsetRegistry::IsAvailable())
	{
		UToolsetRegistry::UnregisterToolsetClass(UMarkdownToolset::StaticClass());
	}
	UnregisterAssetTypeActions();
}

void FUnrealMarkdownEditorModule::RegisterAssetTypeActions()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	MarkdownAssetTypeActions = MakeShareable(new FAssetTypeActions_MarkdownFile());
	AssetTools.RegisterAssetTypeActions(MarkdownAssetTypeActions.ToSharedRef());
}

void FUnrealMarkdownEditorModule::UnregisterAssetTypeActions()
{
	if (MarkdownAssetTypeActions.IsValid())
	{
		if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
		{
			IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
			AssetTools.UnregisterAssetTypeActions(MarkdownAssetTypeActions.ToSharedRef());
		}
		MarkdownAssetTypeActions.Reset();
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUnrealMarkdownEditorModule, UnrealMarkdownEditor)

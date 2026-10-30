// UnrealMarkdown - MIT License, see LICENSE in the plugin root.
// 本文件已按 ZBSStudio 的本地集成需要扩充：内容浏览器右键菜单补齐保存 / 导出 / 重导入。

#include "UnrealMarkdownEditor.h"
#include "MarkdownFile.h"
#include "MarkdownAssetEditor.h"
#include "MarkdownFileReimportHandler.h"

#include "AssetToolsModule.h"
#include "DesktopPlatformModule.h"
#include "EditorReimportHandler.h"
#include "FileHelpers.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IAssetTools.h"
#include "IDesktopPlatform.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "MarkdownStyleCompat.h"
#include "UObject/Class.h"

#define LOCTEXT_NAMESPACE "FUnrealMarkdownEditorModule"

namespace MarkdownAssetActionsPrivate
{
	/** 从内容浏览器的选中项里挑出 Markdown 资产。 */
	static TArray<UMarkdownFile*> CastMarkdownFiles(const TArray<UObject*>& InObjects)
	{
		TArray<UMarkdownFile*> Files;
		for (UObject* Object : InObjects)
		{
			if (UMarkdownFile* File = Cast<UMarkdownFile>(Object))
			{
				Files.Add(File);
			}
		}
		return Files;
	}

	static void SaveMarkdownFiles(const TArray<UMarkdownFile*>& Files)
	{
		TArray<UPackage*> Packages;
		for (UMarkdownFile* File : Files)
		{
			if (File && File->GetOutermost())
			{
				Packages.AddUnique(File->GetOutermost());
			}
		}

		if (Packages.Num() > 0)
		{
			FEditorFileUtils::PromptForCheckoutAndSave(Packages, false, false);
		}
	}

	static void ExportMarkdownFiles(const TArray<UMarkdownFile*>& Files)
	{
		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
		if (!DesktopPlatform || Files.Num() == 0)
		{
			return;
		}

		// 单选给文件对话框；多选给目录对话框，然后按资产名各写一份 .md。
		if (Files.Num() == 1)
		{
			TArray<FString> OutFiles;
			const FString DefaultName = Files[0]->GetName() + TEXT(".md");
			if (DesktopPlatform->SaveFileDialog(
				nullptr, TEXT("Export Markdown File"), FPaths::ProjectDir(), DefaultName,
				TEXT("Markdown Files (*.md)|*.md|Text Files (*.txt)|*.txt|All Files (*.*)|*.*"),
				EFileDialogFlags::None, OutFiles) && OutFiles.Num() > 0)
			{
				Files[0]->ExportToFile(OutFiles[0]);
			}
			return;
		}

		FString OutDirectory;
		if (DesktopPlatform->OpenDirectoryDialog(nullptr, TEXT("Export Markdown Files"), FPaths::ProjectDir(), OutDirectory))
		{
			for (UMarkdownFile* File : Files)
			{
				if (File)
				{
					File->ExportToFile(FPaths::Combine(OutDirectory, File->GetName() + TEXT(".md")));
				}
			}
		}
	}

	static void ReimportMarkdownFiles(const TArray<UMarkdownFile*>& Files)
	{
		TArray<UObject*> Objects;
		for (UMarkdownFile* File : Files)
		{
			if (File && !File->SourceFilePath.IsEmpty())
			{
				Objects.Add(File);
			}
		}

		if (Objects.Num() > 0)
		{
			// 单选走 Reimport，多选走 ReimportMultiple，两个重载的参数列表不同。
			if (Objects.Num() == 1)
			{
				FReimportManager::Instance()->Reimport(Objects[0], /*bAskForNewFileIfMissing*/ false, /*bShowNotification*/ true);
			}
			else
			{
				FReimportManager::Instance()->ReimportMultiple(Objects, /*bAskForNewFileIfMissing*/ false, /*bShowNotification*/ true);
			}
		}
	}

	/** 是否存在可以重导入的资产（记录了源文件且源文件确实在磁盘上）。 */
	static bool CanReimportAny(const TArray<UMarkdownFile*>& Files)
	{
		for (UMarkdownFile* File : Files)
		{
			if (File && !File->SourceFilePath.IsEmpty() && FPaths::FileExists(File->SourceFilePath))
			{
				return true;
			}
		}
		return false;
	}
}

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

	virtual void GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder) override
	{
		const TArray<UMarkdownFile*> Files = MarkdownAssetActionsPrivate::CastMarkdownFiles(InObjects);
		if (Files.Num() == 0)
		{
			return;
		}

		MenuBuilder.BeginSection("MarkdownFile", LOCTEXT("MarkdownSection", "Markdown"));

		MenuBuilder.AddMenuEntry(
			LOCTEXT("SaveMarkdown", "Save"),
			LOCTEXT("SaveMarkdownTip", "把资产内容写盘"),
			FSlateIcon(MARKDOWN_STYLE::GetAppStyleSetName(), "AssetEditor.SaveAsset"),
			FUIAction(FExecuteAction::CreateLambda([Files]()
			{
				MarkdownAssetActionsPrivate::SaveMarkdownFiles(Files);
			})));

		MenuBuilder.AddMenuEntry(
			LOCTEXT("ExportMarkdown", "Export To .md..."),
			LOCTEXT("ExportMarkdownTip", "导出为磁盘上的 .md 文件；多选时选一个目录，按资产名各写一份"),
			FSlateIcon(MARKDOWN_STYLE::GetAppStyleSetName(), "AssetEditor.SaveAsset.Greyscale"),
			FUIAction(FExecuteAction::CreateLambda([Files]()
			{
				MarkdownAssetActionsPrivate::ExportMarkdownFiles(Files);
			})));

		if (MarkdownAssetActionsPrivate::CanReimportAny(Files))
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("ReimportMarkdown", "Reimport"),
				LOCTEXT("ReimportMarkdownTip", "从导入时的 .md 源文件重新读取，覆盖当前内容"),
				FSlateIcon(MARKDOWN_STYLE::GetAppStyleSetName(), "ContentBrowser.ImportIcon"),
				FUIAction(FExecuteAction::CreateLambda([Files]()
				{
					MarkdownAssetActionsPrivate::ReimportMarkdownFiles(Files);
				})));
		}

		MenuBuilder.EndSection();
	}
};

// ============================================================================
// Module
// ============================================================================

void FUnrealMarkdownEditorModule::StartupModule()
{
	RegisterAssetTypeActions();

	// FReimportHandler 在构造时自己注册给 FReimportManager，析构时注销。
	ReimportHandler = MakeUnique<FMarkdownFileReimportHandler>();
}

void FUnrealMarkdownEditorModule::ShutdownModule()
{
	ReimportHandler.Reset();
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

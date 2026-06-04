// Copyright (c) 2025 zhuangbiiii. All Rights Reserved.

#include "Modules/ModuleManager.h"

#include "AssetRegistry/AssetData.h"
#include "AssetToolsModule.h"
#include "ContentBrowserModule.h"
#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "IContentBrowserSingleton.h"
#include "IDesktopPlatform.h"
#include "MarkdownAsset.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "FUnrealMarkdownEditorModule"

namespace
{
    void ShowModuleNotification(const FText& Message, SNotificationItem::ECompletionState State)
    {
        if (!FSlateApplication::IsInitialized())
        {
            return;
        }

        FNotificationInfo Info(Message);
        Info.ExpireDuration = 4.0f;
        Info.bFireAndForget = true;
        Info.bUseLargeFont = false;

        TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
        if (Notification.IsValid())
        {
            Notification->SetCompletionState(State);
        }
    }

    FString EnsureMarkdownExtension(FString Path)
    {
        if (!Path.EndsWith(TEXT(".md"), ESearchCase::IgnoreCase))
        {
            Path += TEXT(".md");
        }

        return Path;
    }
}

class FUnrealMarkdownEditorModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        UToolMenus::RegisterStartupCallback(
            FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FUnrealMarkdownEditorModule::RegisterMenus));
    }

    virtual void ShutdownModule() override
    {
        if (UToolMenus::IsAvailable())
        {
            UToolMenus::UnRegisterStartupCallback(this);
            UToolMenus::UnregisterOwner(this);
        }
    }

private:
    void RegisterMenus()
    {
        if (!UToolMenus::IsToolMenuUIEnabled())
        {
            return;
        }

        FToolMenuOwnerScoped OwnerScoped(this);

        UToolMenu* MainMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
        FToolMenuSection& MainMenuSection = MainMenu->FindOrAddSection("UnrealMarkdown");
        MainMenuSection.AddMenuEntry(
            "UnrealMarkdownImportMarkdown",
            LOCTEXT("ImportMarkdownMenuLabel", "Import Markdown"),
            LOCTEXT("ImportMarkdownMenuTooltip", "Import one or more .md files into Markdown assets."),
            FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Import"),
            FUIAction(FExecuteAction::CreateRaw(this, &FUnrealMarkdownEditorModule::ImportMarkdownFiles)));
        MainMenuSection.AddMenuEntry(
            "UnrealMarkdownExportMarkdown",
            LOCTEXT("ExportMarkdownMenuLabel", "Export Markdown"),
            LOCTEXT("ExportMarkdownMenuTooltip", "Export the selected Markdown assets to .md files."),
            FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Export"),
            FUIAction(
                FExecuteAction::CreateRaw(this, &FUnrealMarkdownEditorModule::ExportMarkdownFiles),
                FCanExecuteAction::CreateRaw(this, &FUnrealMarkdownEditorModule::HasSelectedMarkdownAssets)));

        UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar");
        FToolMenuSection& ToolbarSection = ToolbarMenu->FindOrAddSection("UnrealMarkdown");
        ToolbarSection.AddEntry(FToolMenuEntry::InitToolBarButton(
            "UnrealMarkdownImportToolbar",
            FUIAction(FExecuteAction::CreateRaw(this, &FUnrealMarkdownEditorModule::ImportMarkdownFiles)),
            LOCTEXT("ImportMarkdownToolbarLabel", "Import Markdown"),
            LOCTEXT("ImportMarkdownToolbarTooltip", "Import .md files into the selected Content Browser folder."),
            FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Import")));
        ToolbarSection.AddEntry(FToolMenuEntry::InitToolBarButton(
            "UnrealMarkdownExportToolbar",
            FUIAction(
                FExecuteAction::CreateRaw(this, &FUnrealMarkdownEditorModule::ExportMarkdownFiles),
                FCanExecuteAction::CreateRaw(this, &FUnrealMarkdownEditorModule::HasSelectedMarkdownAssets)),
            LOCTEXT("ExportMarkdownToolbarLabel", "Export Markdown"),
            LOCTEXT("ExportMarkdownToolbarTooltip", "Export the selected Markdown assets to .md files."),
            FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Export")));
    }

    FString GetSelectedImportPath() const
    {
        FString DestinationPath = TEXT("/Game");

        if (!FModuleManager::Get().IsModuleLoaded("ContentBrowser"))
        {
            return DestinationPath;
        }

        FContentBrowserModule& ContentBrowserModule =
            FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

        TArray<FString> SelectedFolders;
        ContentBrowserModule.Get().GetSelectedPathViewFolders(SelectedFolders);
        if (SelectedFolders.Num() > 0)
        {
            DestinationPath = SelectedFolders[0];
        }

        return DestinationPath;
    }

    TArray<UMarkdownAsset*> GetSelectedMarkdownAssets() const
    {
        TArray<UMarkdownAsset*> MarkdownAssets;

        if (!FModuleManager::Get().IsModuleLoaded("ContentBrowser"))
        {
            return MarkdownAssets;
        }

        FContentBrowserModule& ContentBrowserModule =
            FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

        TArray<FAssetData> SelectedAssets;
        ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);

        for (const FAssetData& AssetData : SelectedAssets)
        {
            if (UMarkdownAsset* MarkdownAsset = Cast<UMarkdownAsset>(AssetData.GetAsset()))
            {
                MarkdownAssets.Add(MarkdownAsset);
            }
        }

        return MarkdownAssets;
    }

    bool HasSelectedMarkdownAssets() const
    {
        return GetSelectedMarkdownAssets().Num() > 0;
    }

    void ImportMarkdownFiles()
    {
        IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
        if (!DesktopPlatform)
        {
            ShowModuleNotification(
                LOCTEXT("DesktopPlatformUnavailable", "Desktop file dialogs are unavailable."),
                SNotificationItem::CS_Fail);
            return;
        }

        TArray<FString> Filenames;
        const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
        const bool bOpened = DesktopPlatform->OpenFileDialog(
            ParentWindowHandle,
            LOCTEXT("ImportMarkdownDialogTitle", "Import Markdown Files").ToString(),
            TEXT(""),
            TEXT(""),
            TEXT("Markdown files (*.md)|*.md"),
            EFileDialogFlags::Multiple,
            Filenames);

        if (!bOpened || Filenames.Num() == 0)
        {
            return;
        }

        FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
        const FString DestinationPath = GetSelectedImportPath();
        const TArray<UObject*> ImportedAssets = AssetToolsModule.Get().ImportAssets(Filenames, DestinationPath);

        if (ImportedAssets.Num() == 0)
        {
            ShowModuleNotification(
                LOCTEXT("ImportMarkdownFailed", "No Markdown assets were imported."),
                SNotificationItem::CS_Fail);
            return;
        }

        ShowModuleNotification(
            FText::Format(
                LOCTEXT("ImportMarkdownSucceeded", "Imported {0} Markdown asset(s) to {1}."),
                FText::AsNumber(ImportedAssets.Num()),
                FText::FromString(DestinationPath)),
            SNotificationItem::CS_Success);
    }

    void ExportMarkdownFiles()
    {
        IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
        if (!DesktopPlatform)
        {
            ShowModuleNotification(
                LOCTEXT("DesktopPlatformUnavailableExport", "Desktop file dialogs are unavailable."),
                SNotificationItem::CS_Fail);
            return;
        }

        const TArray<UMarkdownAsset*> MarkdownAssets = GetSelectedMarkdownAssets();
        if (MarkdownAssets.Num() == 0)
        {
            ShowModuleNotification(
                LOCTEXT("NoMarkdownAssetsSelected", "Select at least one Markdown asset to export."),
                SNotificationItem::CS_None);
            return;
        }

        if (MarkdownAssets.Num() == 1)
        {
            ExportSingleMarkdownAsset(DesktopPlatform, MarkdownAssets[0]);
            return;
        }

        ExportMultipleMarkdownAssets(DesktopPlatform, MarkdownAssets);
    }

    void ExportSingleMarkdownAsset(IDesktopPlatform* DesktopPlatform, UMarkdownAsset* MarkdownAsset)
    {
        check(DesktopPlatform);
        check(MarkdownAsset);

        const FString DefaultDirectory = MarkdownAsset->SourceFilePath.IsEmpty()
            ? FPaths::ProjectSavedDir()
            : FPaths::GetPath(MarkdownAsset->SourceFilePath);

        TArray<FString> SaveFilenames;
        const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
        const bool bSaved = DesktopPlatform->SaveFileDialog(
            ParentWindowHandle,
            LOCTEXT("ExportMarkdownDialogTitle", "Export Markdown Asset").ToString(),
            DefaultDirectory,
            MarkdownAsset->GetName() + TEXT(".md"),
            TEXT("Markdown files (*.md)|*.md"),
            EFileDialogFlags::None,
            SaveFilenames);

        if (!bSaved || SaveFilenames.Num() == 0)
        {
            return;
        }

        const FString ExportFilename = EnsureMarkdownExtension(SaveFilenames[0]);
        if (!FFileHelper::SaveStringToFile(MarkdownAsset->MarkdownText, *ExportFilename))
        {
            ShowModuleNotification(
                FText::Format(
                    LOCTEXT("ExportMarkdownSingleFailed", "Failed to export Markdown asset to {0}."),
                    FText::FromString(ExportFilename)),
                SNotificationItem::CS_Fail);
            return;
        }

        ShowModuleNotification(
            FText::Format(
                LOCTEXT("ExportMarkdownSingleSucceeded", "Exported Markdown asset to {0}."),
                FText::FromString(ExportFilename)),
            SNotificationItem::CS_Success);
    }

    void ExportMultipleMarkdownAssets(IDesktopPlatform* DesktopPlatform, const TArray<UMarkdownAsset*>& MarkdownAssets)
    {
        check(DesktopPlatform);

        FString ExportDirectory;
        const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
        const bool bSelectedDirectory = DesktopPlatform->OpenDirectoryDialog(
            ParentWindowHandle,
            LOCTEXT("ExportMarkdownDirectoryTitle", "Choose Export Folder").ToString(),
            FPaths::ProjectSavedDir(),
            ExportDirectory);

        if (!bSelectedDirectory || ExportDirectory.IsEmpty())
        {
            return;
        }

        int32 ExportedCount = 0;
        for (const UMarkdownAsset* MarkdownAsset : MarkdownAssets)
        {
            if (!MarkdownAsset)
            {
                continue;
            }

            const FString ExportFilename =
                EnsureMarkdownExtension(FPaths::Combine(ExportDirectory, MarkdownAsset->GetName()));
            if (FFileHelper::SaveStringToFile(MarkdownAsset->MarkdownText, *ExportFilename))
            {
                ++ExportedCount;
            }
        }

        if (ExportedCount == MarkdownAssets.Num())
        {
            ShowModuleNotification(
                FText::Format(
                    LOCTEXT("ExportMarkdownMultipleSucceeded", "Exported {0} Markdown asset(s)."),
                    FText::AsNumber(ExportedCount)),
                SNotificationItem::CS_Success);
            return;
        }

        ShowModuleNotification(
            FText::Format(
                LOCTEXT("ExportMarkdownMultiplePartial", "Exported {0} of {1} Markdown asset(s)."),
                FText::AsNumber(ExportedCount),
                FText::AsNumber(MarkdownAssets.Num())),
            SNotificationItem::CS_None);
    }
};

IMPLEMENT_MODULE(FUnrealMarkdownEditorModule, UnrealMarkdownEditor)

#undef LOCTEXT_NAMESPACE

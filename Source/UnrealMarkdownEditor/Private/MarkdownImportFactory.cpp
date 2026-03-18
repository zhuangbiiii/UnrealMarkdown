// Copyright (c) 2025 zhuangbiiii. All Rights Reserved.

#include "MarkdownImportFactory.h"
#include "MarkdownAsset.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Logging/LogMacros.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

DEFINE_LOG_CATEGORY_STATIC(LogMarkdownImport, Log, All)

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

UMarkdownImportFactory::UMarkdownImportFactory()
{
    // This factory creates assets from files on disk, not from scratch.
    bCreateNew   = false;
    bEditAfterNew = false;
    bEditorImport = true;

    SupportedClass = UMarkdownAsset::StaticClass();

    // Register the file format that this factory handles.
    Formats.Add(TEXT("md;Markdown File"));
}

// ---------------------------------------------------------------------------
// UFactory interface
// ---------------------------------------------------------------------------

bool UMarkdownImportFactory::FactoryCanImport(const FString& Filename)
{
    return FPaths::GetExtension(Filename).ToLower() == TEXT("md");
}

UObject* UMarkdownImportFactory::FactoryCreateFile(
    UClass* InClass,
    UObject* InParent,
    FName InName,
    EObjectFlags Flags,
    const FString& Filename,
    const TCHAR* Parms,
    FFeedbackContext* Warn,
    bool& bOutOperationCanceled)
{
    bOutOperationCanceled = false;

    // ------------------------------------------------------------------
    // 1. Read the file from disk
    // ------------------------------------------------------------------
    FString FileContent;
    if (!FFileHelper::LoadFileToString(FileContent, *Filename))
    {
        const FText ErrorMsg = FText::Format(
            NSLOCTEXT("UnrealMarkdown", "ImportErrorRead",
                      "Failed to read Markdown file:\n{0}"),
            FText::FromString(Filename));

        Warn->Logf(ELogVerbosity::Error,
                   TEXT("UnrealMarkdown: Failed to read file '%s'"), *Filename);
        UE_LOG(LogMarkdownImport, Error,
               TEXT("Failed to read Markdown file: %s"), *Filename);

        ShowNotification(ErrorMsg, ENotificationSeverity::Error);
        return nullptr;
    }

    // ------------------------------------------------------------------
    // 2. Warn about empty files (still imported, but user should know)
    // ------------------------------------------------------------------
    if (FileContent.IsEmpty())
    {
        const FText WarnMsg = FText::Format(
            NSLOCTEXT("UnrealMarkdown", "ImportWarnEmpty",
                      "Markdown file is empty: {0}"),
            FText::FromString(FPaths::GetCleanFilename(Filename)));

        Warn->Logf(ELogVerbosity::Warning,
                   TEXT("UnrealMarkdown: File is empty: '%s'"), *Filename);
        UE_LOG(LogMarkdownImport, Warning,
               TEXT("Markdown file is empty: %s"), *Filename);

        ShowNotification(WarnMsg, ENotificationSeverity::Warning);
    }

    // ------------------------------------------------------------------
    // 3. Create the asset
    // ------------------------------------------------------------------
    UMarkdownAsset* Asset = NewObject<UMarkdownAsset>(InParent, InName, Flags);
    if (!Asset)
    {
        const FText ErrorMsg = NSLOCTEXT("UnrealMarkdown", "ImportErrorCreate",
                                         "Failed to create Markdown asset object.");

        Warn->Logf(ELogVerbosity::Error,
                   TEXT("UnrealMarkdown: NewObject failed for '%s'"), *InName.ToString());
        UE_LOG(LogMarkdownImport, Error,
               TEXT("NewObject<UMarkdownAsset> returned null for: %s"), *InName.ToString());

        ShowNotification(ErrorMsg, ENotificationSeverity::Error);
        return nullptr;
    }

    Asset->MarkdownText   = FileContent;
    Asset->SourceFilePath = Filename;

    // ------------------------------------------------------------------
    // 4. Notify the user of success
    // ------------------------------------------------------------------
    const FText SuccessMsg = FText::Format(
        NSLOCTEXT("UnrealMarkdown", "ImportSuccess",
                  "Markdown file imported successfully: {0}"),
        FText::FromString(FPaths::GetCleanFilename(Filename)));

    UE_LOG(LogMarkdownImport, Log,
           TEXT("Markdown file imported: %s"), *Filename);

    ShowNotification(SuccessMsg, ENotificationSeverity::Success);

    return Asset;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

namespace
{
    constexpr float ErrorNotificationDuration   = 6.0f;
    constexpr float WarningNotificationDuration = 5.0f;
    constexpr float SuccessNotificationDuration = 3.0f;
}

void UMarkdownImportFactory::ShowNotification(const FText& Message, ENotificationSeverity Severity)
{
    // Notifications are only meaningful when the Slate UI is running.
    if (!FSlateApplication::IsInitialized())
    {
        return;
    }

    float Duration = SuccessNotificationDuration;
    SNotificationItem::ECompletionState State = SNotificationItem::CS_Success;

    switch (Severity)
    {
        case ENotificationSeverity::Error:
            Duration = ErrorNotificationDuration;
            State    = SNotificationItem::CS_Fail;
            break;
        case ENotificationSeverity::Warning:
            Duration = WarningNotificationDuration;
            State    = SNotificationItem::CS_None;
            break;
        default: // Success
            break;
    }

    FNotificationInfo Info(Message);
    Info.ExpireDuration = Duration;
    Info.bUseLargeFont  = false;
    Info.bFireAndForget = true;

    TSharedPtr<SNotificationItem> Notification =
        FSlateNotificationManager::Get().AddNotification(Info);

    if (Notification.IsValid())
    {
        Notification->SetCompletionState(State);
    }
}

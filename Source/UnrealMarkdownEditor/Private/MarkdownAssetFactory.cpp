// Copyright (c) 2025 zhuangbiiii. All Rights Reserved.

#include "MarkdownAssetFactory.h"

#include "MarkdownAsset.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Logging/LogMacros.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Widgets/Notifications/SNotificationList.h"

DEFINE_LOG_CATEGORY_STATIC(LogMarkdownAssetFactory, Log, All)

namespace
{
    constexpr float ErrorNotificationDuration = 6.0f;
    constexpr float WarningNotificationDuration = 5.0f;
    constexpr float SuccessNotificationDuration = 3.0f;
}

UMarkdownAssetFactory::UMarkdownAssetFactory()
{
    bCreateNew = true;
    bEditAfterNew = true;
    bEditorImport = true;

    SupportedClass = UMarkdownAsset::StaticClass();
    Formats.Add(TEXT("md;Markdown File"));
}

UObject* UMarkdownAssetFactory::FactoryCreateNew(
    UClass* InClass,
    UObject* InParent,
    FName InName,
    EObjectFlags Flags,
    UObject* Context,
    FFeedbackContext* Warn)
{
    return NewObject<UMarkdownAsset>(InParent, InClass ? InClass : UMarkdownAsset::StaticClass(), InName, Flags);
}

UObject* UMarkdownAssetFactory::FactoryCreateFile(
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

    FString FileContent;
    if (!FFileHelper::LoadFileToString(FileContent, *Filename))
    {
        const FText ErrorMessage = FText::Format(
            NSLOCTEXT("UnrealMarkdown", "ImportReadError", "Failed to read Markdown file:\n{0}"),
            FText::FromString(Filename));

        if (Warn)
        {
            Warn->Logf(ELogVerbosity::Error, TEXT("UnrealMarkdown: Failed to read file '%s'"), *Filename);
        }

        UE_LOG(LogMarkdownAssetFactory, Error, TEXT("Failed to read Markdown file: %s"), *Filename);
        ShowNotification(ErrorMessage, ENotificationSeverity::Error);
        return nullptr;
    }

    UMarkdownAsset* Asset = NewObject<UMarkdownAsset>(
        InParent,
        InClass ? InClass : UMarkdownAsset::StaticClass(),
        InName,
        Flags);

    if (!Asset)
    {
        const FText ErrorMessage = NSLOCTEXT("UnrealMarkdown", "ImportCreateError", "Failed to create Markdown asset.");

        if (Warn)
        {
            Warn->Logf(ELogVerbosity::Error, TEXT("UnrealMarkdown: Failed to create asset '%s'"), *InName.ToString());
        }

        UE_LOG(LogMarkdownAssetFactory, Error, TEXT("Failed to create Markdown asset: %s"), *InName.ToString());
        ShowNotification(ErrorMessage, ENotificationSeverity::Error);
        return nullptr;
    }

    Asset->MarkdownText = FileContent;
    Asset->SourceFilePath = FPaths::ConvertRelativePathToFull(Filename);

    if (FileContent.IsEmpty())
    {
        const FText WarningMessage = FText::Format(
            NSLOCTEXT("UnrealMarkdown", "ImportEmptyWarning", "Imported empty Markdown file: {0}"),
            FText::FromString(FPaths::GetCleanFilename(Filename)));

        if (Warn)
        {
            Warn->Logf(ELogVerbosity::Warning, TEXT("UnrealMarkdown: Imported empty file '%s'"), *Filename);
        }

        UE_LOG(LogMarkdownAssetFactory, Warning, TEXT("Imported empty Markdown file: %s"), *Filename);
        ShowNotification(WarningMessage, ENotificationSeverity::Warning);
        return Asset;
    }

    const FText SuccessMessage = FText::Format(
        NSLOCTEXT("UnrealMarkdown", "ImportSuccess", "Imported Markdown file: {0}"),
        FText::FromString(FPaths::GetCleanFilename(Filename)));

    UE_LOG(LogMarkdownAssetFactory, Log, TEXT("Imported Markdown file: %s"), *Filename);
    ShowNotification(SuccessMessage, ENotificationSeverity::Success);
    return Asset;
}

bool UMarkdownAssetFactory::FactoryCanImport(const FString& Filename)
{
    return FPaths::GetExtension(Filename).Equals(TEXT("md"), ESearchCase::IgnoreCase);
}

bool UMarkdownAssetFactory::ShouldShowInNewMenu() const
{
    return true;
}

void UMarkdownAssetFactory::ShowNotification(const FText& Message, ENotificationSeverity Severity)
{
    if (!FSlateApplication::IsInitialized())
    {
        return;
    }

    float ExpireDuration = SuccessNotificationDuration;
    SNotificationItem::ECompletionState CompletionState = SNotificationItem::CS_Success;

    switch (Severity)
    {
    case ENotificationSeverity::Error:
        ExpireDuration = ErrorNotificationDuration;
        CompletionState = SNotificationItem::CS_Fail;
        break;
    case ENotificationSeverity::Warning:
        ExpireDuration = WarningNotificationDuration;
        CompletionState = SNotificationItem::CS_None;
        break;
    default:
        break;
    }

    FNotificationInfo Info(Message);
    Info.ExpireDuration = ExpireDuration;
    Info.bFireAndForget = true;
    Info.bUseLargeFont = false;

    TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
    if (Notification.IsValid())
    {
        Notification->SetCompletionState(CompletionState);
    }
}

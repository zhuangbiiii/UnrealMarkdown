// Copyright (c) 2025 zhuangbiiii. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "MarkdownAssetFactory.generated.h"

/**
 * Factory used both for creating blank Markdown assets and importing .md files.
 */
UCLASS()
class UNREALMARKDOWNEDITOR_API UMarkdownAssetFactory : public UFactory
{
    GENERATED_BODY()

public:
    UMarkdownAssetFactory();

    virtual UObject* FactoryCreateNew(
        UClass* InClass,
        UObject* InParent,
        FName InName,
        EObjectFlags Flags,
        UObject* Context,
        FFeedbackContext* Warn) override;

    virtual UObject* FactoryCreateFile(
        UClass* InClass,
        UObject* InParent,
        FName InName,
        EObjectFlags Flags,
        const FString& Filename,
        const TCHAR* Parms,
        FFeedbackContext* Warn,
        bool& bOutOperationCanceled) override;

    virtual bool FactoryCanImport(const FString& Filename) override;
    virtual bool ShouldShowInNewMenu() const override;

private:
    enum class ENotificationSeverity : uint8
    {
        Success,
        Warning,
        Error,
    };

    static void ShowNotification(const FText& Message, ENotificationSeverity Severity);
};

// Copyright (c) 2025 zhuangbiiii. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "MarkdownImportFactory.generated.h"

/**
 * Factory for importing .md (Markdown) files into Unreal Engine as
 * UMarkdownAsset objects. The factory is automatically discovered by
 * the editor and handles drag-and-drop or "Import" actions in the
 * Content Browser for any file with the ".md" extension.
 *
 * On failure the factory shows a toast notification and logs an error so
 * the user is immediately informed of the problem.
 */
UCLASS()
class UNREALMARKDOWNEDITOR_API UMarkdownImportFactory : public UFactory
{
    GENERATED_BODY()

public:
    UMarkdownImportFactory();

    // UFactory interface
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
    // End of UFactory interface

private:
    /** Severity level used when showing an editor toast notification. */
    enum class ENotificationSeverity : uint8
    {
        Success,
        Warning,
        Error,
    };

    /** Show a toast notification in the editor (no-op outside the editor). */
    static void ShowNotification(const FText& Message, ENotificationSeverity Severity);
};

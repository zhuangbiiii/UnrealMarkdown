// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "MarkdownFileFactory.generated.h"

/**
 * Factory for importing .md / .txt files as UMarkdownFile assets.
 * Also supports creating new blank MarkdownFile assets from the Content Browser.
 */
UCLASS()
class UMarkdownFileFactory : public UFactory
{
	GENERATED_BODY()

public:
	UMarkdownFileFactory();

	// ---- Import from file (.md / .txt) ----
	virtual bool FactoryCanImport(const FString& Filename) override;
	virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled) override;

	// ---- Create new empty asset ----
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;

	virtual bool ShouldShowInNewMenu() const override { return true; }
};

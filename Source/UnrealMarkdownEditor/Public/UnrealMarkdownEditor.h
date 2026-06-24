// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "AssetTypeActions_Base.h"

class FUnrealMarkdownEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TSharedPtr<IAssetTypeActions> MarkdownAssetTypeActions;

	void RegisterAssetTypeActions();
	void UnregisterAssetTypeActions();
};

// UnrealMarkdown - MIT License, see LICENSE in the plugin root.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "AssetTypeActions_Base.h"

class FMarkdownFileReimportHandler;

class FUnrealMarkdownEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TSharedPtr<IAssetTypeActions> MarkdownAssetTypeActions;

	/** 重导入处理器需要在模块存活期间常驻，构造时自动注册到 FReimportManager。 */
	TUniquePtr<FMarkdownFileReimportHandler> ReimportHandler;

	void RegisterAssetTypeActions();
	void UnregisterAssetTypeActions();
};

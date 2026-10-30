// UnrealMarkdown - MIT License, see LICENSE in the plugin root.
// 本文件已按 ZBSStudio 的本地集成需要扩充。

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"

#include "MarkdownFileFactory.generated.h"

/**
 * UMarkdownFile 的导入工厂：拖入 .md / .txt，以及在内容浏览器新建空白资产。
 *
 * 重导入不在这里：UFactory 本身不实现重导入接口，
 * 重导入由 MarkdownFileReimportHandler 这个 FReimportHandler 负责。
 */
UCLASS()
class UMarkdownFileFactory : public UFactory
{
	GENERATED_BODY()

public:
	UMarkdownFileFactory();

	// ---- 从文件导入（.md / .txt）----
	virtual bool FactoryCanImport(const FString& Filename) override;
	virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled) override;

	// ---- 新建空白资产 ----
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;

	virtual bool ShouldShowInNewMenu() const override { return true; }
};

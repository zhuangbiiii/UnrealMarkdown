// UnrealMarkdown - MIT License, see LICENSE in the plugin root.
// 本文件已按 ZBSStudio 的本地集成需要扩充（重导入）。

#pragma once

#include "CoreMinimal.h"
#include "EditorReimportHandler.h"

/**
 * UMarkdownFile 的重导入处理器。
 *
 * 重导入接口属于 FReimportHandler，不属于 UFactory，所以必须单独注册一个处理器，
 * 由它把磁盘上的 .md 内容重新读回同名资产。
 * 只有记录了 SourceFilePath 的资产才允许重导入。
 */
class FMarkdownFileReimportHandler final : public FReimportHandler
{
public:
	FMarkdownFileReimportHandler() = default;
	virtual ~FMarkdownFileReimportHandler() override = default;

	//~ Begin FReimportHandler Interface
	virtual bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
	virtual void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;
	virtual EReimportResult::Type Reimport(UObject* Obj) override;
	virtual int32 GetPriority() const override;
	//~ End FReimportHandler Interface
};

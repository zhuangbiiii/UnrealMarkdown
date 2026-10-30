// UnrealMarkdown - MIT License, see LICENSE in the plugin root.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * 插件统一日志分类。
 *
 * 声明带 DLL 导出，因为编辑器模块与 MCP 模块都要写同一份日志；
 * 定义仍在本模块的 cpp 里，否则跨模块链接会找不到符号。
 */
UNREALMARKDOWN_API DECLARE_LOG_CATEGORY_EXTERN(LogUnrealMarkdown, Log, All);

class FUnrealMarkdownModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

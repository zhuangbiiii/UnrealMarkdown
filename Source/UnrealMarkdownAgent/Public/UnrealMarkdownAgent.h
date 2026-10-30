// UnrealMarkdown - MIT License, see LICENSE in the plugin root.
// 本文件已按 ZBSStudio 的本地集成需要新增：ZBS Agent 侧的 Markdown 工具模块入口。

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * 面向编辑器内 ZBS Agent 的 Markdown 工具模块。
 *
 * 只做一件事：把 MarkdownAgentTools 注册成 IZBSAgentToolProvider，
 * 关模块时注销。工具能力本身在 MarkdownAgentTools 里，本类不参与业务。
 */
class FUnrealMarkdownAgentModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

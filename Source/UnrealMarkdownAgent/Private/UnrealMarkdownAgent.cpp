// UnrealMarkdown - MIT License, see LICENSE in the plugin root.
// 本文件已按 ZBSStudio 的本地集成需要新增。

#include "UnrealMarkdownAgent.h"

#include "MarkdownAgentTools.h"
#include "UnrealMarkdown.h"

#include "Features/IModularFeatures.h"
#include "ZBSAgentToolProvider.h"

namespace UnrealMarkdownAgentPrivate
{
	/** 提供者要活到模块卸载，因此不能建在启动函数的栈上。 */
	FMarkdownAgentTools MarkdownTools;
}

void FUnrealMarkdownAgentModule::StartupModule()
{
	// 注册只做能力登记，不碰资产；工具是否可用由宿主按权限等级与写入总闸判定。
	IModularFeatures::Get().RegisterModularFeature(
		IZBSAgentToolProvider::FeatureName(),
		&UnrealMarkdownAgentPrivate::MarkdownTools);

	UE_LOG(LogUnrealMarkdown, Log, TEXT("Markdown Agent 工具已注册：markdown_query / markdown_edit / markdown_asset。"));
}

void FUnrealMarkdownAgentModule::ShutdownModule()
{
	IModularFeatures::Get().UnregisterModularFeature(
		IZBSAgentToolProvider::FeatureName(),
		&UnrealMarkdownAgentPrivate::MarkdownTools);
}

IMPLEMENT_MODULE(FUnrealMarkdownAgentModule, UnrealMarkdownAgent)

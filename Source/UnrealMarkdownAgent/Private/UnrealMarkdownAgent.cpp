// UnrealMarkdown - MIT License, see LICENSE in the plugin root.
// 本文件已按 ZBSStudio 的本地集成需要新增。

#include "UnrealMarkdownAgent.h"

#include "UnrealMarkdown.h"

// 没装 ZBSAgent 插件时这个宏是 0，模块只有一句日志而没有别的实现。
#if UE_ZBS_MARKDOWN_AGENT_TOOLS

#include "MarkdownAgentTools.h"

#include "Features/IModularFeatures.h"
#include "ZBSAgentToolProvider.h"

namespace UnrealMarkdownAgentPrivate
{
	/** 提供者要活到模块卸载，因此不能建在启动函数的栈上。 */
	FMarkdownAgentTools MarkdownTools;
}

#endif // UE_ZBS_MARKDOWN_AGENT_TOOLS

void FUnrealMarkdownAgentModule::StartupModule()
{
#if UE_ZBS_MARKDOWN_AGENT_TOOLS
	// 注册只做能力登记，不碰资产；工具是否可用由宿主按权限等级与写入总闸判定。
	IModularFeatures::Get().RegisterModularFeature(
		IZBSAgentToolProvider::FeatureName(),
		&UnrealMarkdownAgentPrivate::MarkdownTools);

	UE_LOG(LogUnrealMarkdown, Log, TEXT("Markdown Agent 工具已注册：markdown_query / markdown_edit / markdown_asset。"));
#else
	// 静默不注册会让使用者以为插件坏了，所以留一条可查的说明。
	UE_LOG(LogUnrealMarkdown, Log,
		TEXT("未检测到可用的 ZBSAgent 插件，Markdown Agent 工具未注册；Markdown 资产与编辑器功能不受影响。"));
#endif
}

void FUnrealMarkdownAgentModule::ShutdownModule()
{
#if UE_ZBS_MARKDOWN_AGENT_TOOLS
	IModularFeatures::Get().UnregisterModularFeature(
		IZBSAgentToolProvider::FeatureName(),
		&UnrealMarkdownAgentPrivate::MarkdownTools);
#endif
}

IMPLEMENT_MODULE(FUnrealMarkdownAgentModule, UnrealMarkdownAgent)

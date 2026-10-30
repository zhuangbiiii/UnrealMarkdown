// UnrealMarkdown - MIT License, see LICENSE in the plugin root.
// 本文件已按 ZBSStudio 的本地集成需要新增。

#pragma once

#include "CoreMinimal.h"
#include "ZBSAgentToolProvider.h"

/**
 * 面向编辑器内 ZBS Agent 的 Markdown 资产工具提供者。
 *
 * 与 MCP 侧的 UMarkdownToolset 是两条互不相干的通路：
 * MCP 工具按 UFUNCTION 逐个暴露给外部客户端，这里按「一个工具多个 action」收敛成三个，
 * 让模型的工具清单保持短，参数固定。两者共用 UMarkdownFile 与 UMarkdownSyntaxLibrary，
 * 不各自实现一套 Markdown 规则。
 *
 * 三个工具都不驱动运行中的世界，因此都不是运行时工具，
 * 权限等级按「读 2 / 改文本 3 / 动资产 4」划分，与宿主按等级推导写入总闸的规则一致。
 */
class FMarkdownAgentTools : public IZBSAgentToolProvider
{
public:
	/** 返回 markdown_query / markdown_edit / markdown_asset 三个工具的定义。 */
	virtual FString GetSchema() const override;

	/** 返回工具名对应的最低权限等级；不属于本提供者的工具返回 0。 */
	virtual int32 GetRequiredPermissionLevel(const FString& ToolName) const override;

	/** 三个工具都在编辑器内操作资产，不参与运行时进程路由。 */
	virtual bool IsRuntimeTool(const FString& ToolName) const override;

	/** 按工具名分派到查询、文本编辑或资产生命周期实现。 */
	virtual FString Execute(const FZBSAgentToolCall& Call, bool& bOutSucceeded) override;
};

// UnrealMarkdown - MIT License, see LICENSE in the plugin root.
// 新增模块：面向编辑器内 ZBS Agent 的 Markdown 工具集，与 MCP 侧的 UMarkdownToolset 相互独立。

using UnrealBuildTool;

public class UnrealMarkdownAgent : ModuleRules
{
	public UnrealMarkdownAgent(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"UnrealMarkdown",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"UnrealEd",
				"AssetRegistry",
				"AssetTools",
				// FJsonObject / FJsonSerializer / LogJson 都在 Json 模块里，
				// 不能靠 UnrealEd 的传递依赖。
				"Json",
				// IZBSAgentToolProvider 与 FZBSAgentToolCall 只在本模块实现里用到，
				// 按最小暴露放私有依赖。
				"ZBSAgentRuntime",
			}
		);
	}
}

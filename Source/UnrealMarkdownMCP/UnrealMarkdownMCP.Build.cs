// Copyright Epic Games, Inc. All Rights Reserved.
// UnrealMarkdownMCP — MCP toolset (UE 5.8+ only)

using UnrealBuildTool;

public class UnrealMarkdownMCP : ModuleRules
{
	public UnrealMarkdownMCP(ReadOnlyTargetRules Target) : base(Target)
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
				// 导入任意文件、删除与复制资产都要走 IAssetTools。
				"AssetTools",
				"ToolsetRegistry",
			}
		);
	}
}

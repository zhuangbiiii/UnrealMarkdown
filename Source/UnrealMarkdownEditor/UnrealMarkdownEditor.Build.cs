// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealMarkdownEditor : ModuleRules
{
	public UnrealMarkdownEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"UnrealMarkdown",
				"Slate",
				"SlateCore",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"EditorStyle",
				"InputCore",
				"UnrealEd",
				"AssetTools",
				"AssetRegistry",
				"PropertyEditor",
				"ToolMenus",
				"MainFrame",
				"ApplicationCore",
				"AppFramework",
			}
		);

	}
}

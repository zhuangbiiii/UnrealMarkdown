// Copyright (c) 2025 zhuangbiiii. All Rights Reserved.

using UnrealBuildTool;

public class UnrealMarkdownEditor : ModuleRules
{
    public UnrealMarkdownEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "UnrealMarkdown",
        });

        PrivateDependencyModuleNames.AddRange(new[]
        {
            "AssetTools",
            "ContentBrowser",
            "DesktopPlatform",
            "EditorFramework",
            "InputCore",
            "LevelEditor",
            "Slate",
            "SlateCore",
            "ToolMenus",
            "UnrealEd",
        });
    }
}

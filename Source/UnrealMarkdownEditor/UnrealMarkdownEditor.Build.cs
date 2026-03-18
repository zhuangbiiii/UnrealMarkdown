// Copyright (c) 2025 zhuangbiiii. All Rights Reserved.

using UnrealBuildTool;

public class UnrealMarkdownEditor : ModuleRules
{
    public UnrealMarkdownEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Slate",
            "SlateCore",
            "UnrealEd",
            "EditorFramework",
        });
    }
}

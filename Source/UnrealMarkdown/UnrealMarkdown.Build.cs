// Copyright (c) 2025 zhuangbiiii. All Rights Reserved.

using UnrealBuildTool;

public class UnrealMarkdown : ModuleRules
{
    public UnrealMarkdown(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
        });
    }
}

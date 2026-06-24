// Copyright Epic Games, Inc. All Rights Reserved.

#include "MarkdownToolset.h"

#include "Modules/ModuleManager.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "Misc/CoreDelegates.h"

#define LOCTEXT_NAMESPACE "FUnrealMarkdownMCPModule"

class FUnrealMarkdownMCPModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 5
		FCoreDelegates::GetOnPostEngineInit().AddLambda([this]()
#else
		FCoreDelegates::OnPostEngineInit.AddLambda([this]()
#endif
		{
			if (UToolsetRegistry::IsAvailable())
			{
				UToolsetRegistry::RegisterToolsetClass(UMarkdownToolset::StaticClass());
				UE_LOG(LogTemp, Log, TEXT("UnrealMarkdownMCP: MarkdownToolset registered."));
			}
		});
	}

	virtual void ShutdownModule() override
	{
		if (UToolsetRegistry::IsAvailable())
		{
			UToolsetRegistry::UnregisterToolsetClass(UMarkdownToolset::StaticClass());
		}
	}
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUnrealMarkdownMCPModule, UnrealMarkdownMCP)

// Copyright Epic Games, Inc. All Rights Reserved.

#include "MarkdownToolset.h"
#include "MarkdownFile.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"

#define LOCTEXT_NAMESPACE "MarkdownToolset"

// ============================================================================
// Helpers
// ============================================================================

static UMarkdownFile* LoadMarkdownAsset(const FString& AssetPath)
{
	return LoadObject<UMarkdownFile>(nullptr, *AssetPath, nullptr, LOAD_Quiet);
}

static TArray<FAssetData> GetAllMarkdownAssets()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(UMarkdownFile::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;
	Filter.bRecursivePaths = true;

	TArray<FAssetData> Assets;
	AssetRegistry.GetAssets(Filter, Assets);
	return Assets;
}

// ============================================================================
// Read Operations
// ============================================================================

TArray<FString> UMarkdownToolset::ListMarkdownFiles()
{
	TArray<FString> Paths;
	for (const FAssetData& Data : GetAllMarkdownAssets())
	{
		Paths.Add(Data.GetSoftObjectPath().ToString());
	}
	Paths.Sort();
	return Paths;
}

FString UMarkdownToolset::ReadMarkdownContent(const FString& AssetPath)
{
	UMarkdownFile* Md = LoadMarkdownAsset(AssetPath);
	if (!Md)
	{
		return FString::Printf(TEXT("Error: Asset not found: %s"), *AssetPath);
	}
	return Md->Content;
}

FString UMarkdownToolset::GetMarkdownInfo(const FString& AssetPath)
{
	UMarkdownFile* Md = LoadMarkdownAsset(AssetPath);
	if (!Md)
	{
		return FString::Printf(TEXT("{\"error\":\"Asset not found: %s\"}"), *AssetPath);
	}

	return FString::Printf(
		TEXT("{\"name\":\"%s\",\"path\":\"%s\",\"contentLength\":%d,\"lineCount\":%d,\"sourceFile\":\"%s\"}"),
		*Md->GetName(),
		*AssetPath,
		Md->Content.Len(),
		Md->GetLineCount(),
		*Md->SourceFilePath);
}

// ============================================================================
// Write Operations
// ============================================================================

FString UMarkdownToolset::SetMarkdownContent(const FString& AssetPath, const FString& NewContent)
{
	UMarkdownFile* Md = LoadMarkdownAsset(AssetPath);
	if (!Md)
	{
		return FString::Printf(TEXT("Error: Asset not found: %s"), *AssetPath);
	}

	Md->SetContent(NewContent);
	Md->MarkPackageDirty();

	// Save the package
	UPackage* Package = Md->GetOutermost();
	if (Package)
	{
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		UPackage::SavePackage(Package, Md, *Package->GetLoadedPath().GetLocalFullPath(), SaveArgs);
	}

	return FString::Printf(TEXT("OK: Updated content of %s (%d chars)"), *AssetPath, NewContent.Len());
}

FString UMarkdownToolset::CreateMarkdownFile(const FString& AssetPath, const FString& InitialContent)
{
	// Parse path: /Game/Folder/AssetName
	FString PackagePath = AssetPath;
	FString AssetName;
	int32 LastSlash;
	if (PackagePath.FindLastChar(TEXT('/'), LastSlash))
	{
		AssetName = PackagePath.RightChop(LastSlash + 1);
		PackagePath = PackagePath.Left(LastSlash);
	}
	else
	{
		AssetName = PackagePath;
		PackagePath = TEXT("/Game");
	}

	if (AssetName.IsEmpty())
	{
		return TEXT("Error: Invalid asset name.");
	}

	// Check if already exists
	if (LoadMarkdownAsset(AssetPath))
	{
		return FString::Printf(TEXT("Error: Asset already exists: %s"), *AssetPath);
	}

	// Create the package
	UPackage* Package = CreatePackage(*(PackagePath / AssetName));
	if (!Package)
	{
		return TEXT("Error: Failed to create package.");
	}

	// Create the object
	UMarkdownFile* NewAsset = NewObject<UMarkdownFile>(Package, UMarkdownFile::StaticClass(), *AssetName, RF_Public | RF_Standalone);
	if (!NewAsset)
	{
		return TEXT("Error: Failed to create UMarkdownFile object.");
	}

	NewAsset->Content = InitialContent;
	NewAsset->MarkPackageDirty();

	// Notify asset registry
	FAssetRegistryModule::AssetCreated(NewAsset);

	// Save
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	UPackage::SavePackage(Package, NewAsset, *Package->GetLoadedPath().GetLocalFullPath(), SaveArgs);

	return FString::Printf(TEXT("OK: Created %s (%d chars)"), *AssetPath, InitialContent.Len());
}

FString UMarkdownToolset::ExportMarkdownFile(const FString& AssetPath, const FString& OutputFilePath)
{
	UMarkdownFile* Md = LoadMarkdownAsset(AssetPath);
	if (!Md)
	{
		return FString::Printf(TEXT("Error: Asset not found: %s"), *AssetPath);
	}

	if (FFileHelper::SaveStringToFile(Md->Content, *OutputFilePath))
	{
		return FString::Printf(TEXT("OK: Exported %s → %s"), *AssetPath, *OutputFilePath);
	}
	return FString::Printf(TEXT("Error: Failed to write to %s"), *OutputFilePath);
}

TArray<FString> UMarkdownToolset::SearchMarkdownFiles(const FString& SearchTerm)
{
	TArray<FString> Matches;
	FString LowerTerm = SearchTerm.ToLower();

	for (const FAssetData& Data : GetAllMarkdownAssets())
	{
		UMarkdownFile* Md = LoadMarkdownAsset(Data.GetSoftObjectPath().ToString());
		if (Md && Md->Content.ToLower().Contains(LowerTerm))
		{
			Matches.Add(Data.GetSoftObjectPath().ToString());
		}
	}
	Matches.Sort();
	return Matches;
}

#undef LOCTEXT_NAMESPACE

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "MarkdownToolset.generated.h"

/**
 * MCP tools for UnrealMarkdown plugin operations.
 *
 * Registered with the Toolset Registry on editor startup.
 * All UFUNCTIONs marked AICallable become discoverable MCP tools
 * when the Unreal MCP server is running.
 */
UCLASS(BlueprintType, MinimalAPI)
class UMarkdownToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	// ========================================================================
	// Read Operations
	// ========================================================================

	/**
	 * List all UMarkdownFile assets in the project.
	 * @return Array of asset paths like "/Game/MyMarkdownFile".
	 */
	UFUNCTION(meta = (AICallable), Category = "Markdown")
	static TArray<FString> ListMarkdownFiles();

	/**
	 * Read the full content of a UMarkdownFile asset.
	 * @param AssetPath The asset path, e.g. "/Game/MyDoc".
	 * @return The markdown text content.
	 */
	UFUNCTION(meta = (AICallable), Category = "Markdown")
	static FString ReadMarkdownContent(const FString& AssetPath);

	/**
	 * Get metadata about a UMarkdownFile asset.
	 * @param AssetPath The asset path.
	 * @return JSON string with name, path, content length, line count, source file path.
	 */
	UFUNCTION(meta = (AICallable), Category = "Markdown")
	static FString GetMarkdownInfo(const FString& AssetPath);

	// ========================================================================
	// Write Operations
	// ========================================================================

	/**
	 * Update the content of a UMarkdownFile asset.
	 * Saves the package after modification.
	 * @param AssetPath The asset path.
	 * @param NewContent The new markdown text.
	 * @return True on success.
	 */
	UFUNCTION(meta = (AICallable), Category = "Markdown")
	static FString SetMarkdownContent(const FString& AssetPath, const FString& NewContent);

	/**
	 * Create a new UMarkdownFile asset in the Content Browser.
	 * @param AssetPath Full asset path, e.g. "/Game/NewDoc".
	 * @param InitialContent Optional initial markdown text.
	 * @return Success/error message.
	 */
	UFUNCTION(meta = (AICallable), Category = "Markdown")
	static FString CreateMarkdownFile(const FString& AssetPath, const FString& InitialContent);

	/**
	 * Export a UMarkdownFile to a .md file on disk.
	 * @param AssetPath The asset path.
	 * @param OutputFilePath Absolute path for the output .md file.
	 * @return Success/error message.
	 */
	UFUNCTION(meta = (AICallable), Category = "Markdown")
	static FString ExportMarkdownFile(const FString& AssetPath, const FString& OutputFilePath);

	/**
	 * Search markdown files whose content contains the given string.
	 * @param SearchTerm Term to search for (case-insensitive).
	 * @return Array of matching asset paths.
	 */
	UFUNCTION(meta = (AICallable), Category = "Markdown")
	static TArray<FString> SearchMarkdownFiles(const FString& SearchTerm);
};

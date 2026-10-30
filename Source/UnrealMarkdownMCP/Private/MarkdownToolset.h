// Copyright Epic Games, Inc. All Rights Reserved.
// MCP toolset — only compiled on UE 5.8+

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

	/**
	 * Get the heading outline of a markdown asset.
	 * @param AssetPath The asset path.
	 * @return One entry per heading, formatted "Line|Level|Text"; empty when the asset has no headings.
	 */
	UFUNCTION(meta = (AICallable), Category = "Markdown")
	static TArray<FString> GetMarkdownOutline(const FString& AssetPath);

	// ========================================================================
	// 增删查改的剩余部分
	// ========================================================================

	/**
	 * Append text to the end of a markdown asset and save it.
	 * @param AssetPath The asset path.
	 * @param Text Text to append; a newline is inserted first when the asset does not end with one.
	 * @return Success/error message.
	 */
	UFUNCTION(meta = (AICallable), Category = "Markdown")
	static FString AppendMarkdownContent(const FString& AssetPath, const FString& Text);

	/**
	 * Replace text inside a markdown asset and save it.
	 * @param AssetPath The asset path.
	 * @param OldText Text to look for.
	 * @param NewText Replacement text.
	 * @param bReplaceAll Replace every occurrence when true, the first one otherwise.
	 * @return Success/error message including the number of replacements.
	 */
	UFUNCTION(meta = (AICallable), Category = "Markdown")
	static FString ReplaceInMarkdownContent(const FString& AssetPath, const FString& OldText, const FString& NewText, bool bReplaceAll);

	/**
	 * Save a markdown asset to disk without changing its content.
	 * @param AssetPath The asset path.
	 * @return Success/error message.
	 */
	UFUNCTION(meta = (AICallable), Category = "Markdown")
	static FString SaveMarkdownFile(const FString& AssetPath);

	/**
	 * Rename or move a markdown asset.
	 * @param AssetPath Current asset path.
	 * @param NewAssetPath Destination asset path, e.g. "/Game/Docs/Renamed".
	 * @return Success/error message.
	 */
	UFUNCTION(meta = (AICallable), Category = "Markdown")
	static FString RenameMarkdownFile(const FString& AssetPath, const FString& NewAssetPath);

	/**
	 * Delete a markdown asset and its package file.
	 * @param AssetPath The asset path.
	 * @return Success/error message.
	 */
	UFUNCTION(meta = (AICallable), Category = "Markdown")
	static FString DeleteMarkdownFile(const FString& AssetPath);

	/**
	 * Import a .md / .txt file on disk as a new markdown asset.
	 * @param SourceFile Absolute path of the file to import.
	 * @param AssetPath Destination asset path, e.g. "/Game/Docs/MyDoc".
	 * @return Success/error message.
	 */
	UFUNCTION(meta = (AICallable), Category = "Markdown")
	static FString ImportMarkdownFile(const FString& SourceFile, const FString& AssetPath);

	/**
	 * Re-read a markdown asset from the source file it was imported from.
	 * @param AssetPath The asset path.
	 * @return Success/error message; reports NoChange when the file content is identical.
	 */
	UFUNCTION(meta = (AICallable), Category = "Markdown")
	static FString ReimportMarkdownFile(const FString& AssetPath);
};

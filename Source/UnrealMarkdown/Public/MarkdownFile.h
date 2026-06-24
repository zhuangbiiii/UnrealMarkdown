// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "MarkdownFile.generated.h"

/**
 * A Unreal Engine asset that stores Markdown content.
 *
 * Like a UStaticMesh holds mesh data, this asset holds markdown text.
 * Supports import from .md / .txt files, in-editor editing, and export.
 */
UCLASS(BlueprintType, hidecategories = (Object))
class UNREALMARKDOWN_API UMarkdownFile : public UObject
{
	GENERATED_BODY()

public:
	/** The raw markdown text content. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Markdown")
	FString Content;

	/** Original source file path (populated on import). */
	UPROPERTY(BlueprintReadOnly, Category = "Markdown")
	FString SourceFilePath;

	/**
	 * Set the markdown content.
	 * @param InContent The markdown text.
	 */
	UFUNCTION(BlueprintCallable, Category = "Markdown")
	void SetContent(const FString& InContent);

	/**
	 * Get a line count.
	 * @return Number of lines in the markdown content.
	 */
	UFUNCTION(BlueprintPure, Category = "Markdown")
	int32 GetLineCount() const;

	/**
	 * Append text to the end of the content.
	 * @param InText Text to append.
	 */
	UFUNCTION(BlueprintCallable, Category = "Markdown")
	void AppendText(const FString& InText);

	// ---- UObject ----
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
};

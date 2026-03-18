// Copyright (c) 2025 zhuangbiiii. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "MarkdownAsset.generated.h"

/**
 * Asset class that stores the content of an imported Markdown (.md) file.
 * Instances of this class are created automatically when .md files are
 * dragged into the Unreal Engine Content Browser.
 */
UCLASS(BlueprintType)
class UNREALMARKDOWNEDITOR_API UMarkdownAsset : public UObject
{
    GENERATED_BODY()

public:
    /** The raw Markdown text content read from the imported file. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Markdown")
    FString MarkdownText;

    /** The absolute path of the source file this asset was imported from. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Markdown")
    FString SourceFilePath;
};

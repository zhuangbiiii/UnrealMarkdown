// Copyright (c) 2025 zhuangbiiii. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MarkdownAsset.generated.h"

/**
 * Unreal asset used to store Markdown text inside the editor and at runtime.
 */
UCLASS(BlueprintType)
class UNREALMARKDOWN_API UMarkdownAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    /** Raw Markdown text. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Markdown", meta = (MultiLine = true))
    FString MarkdownText;

    /** Source file path (stored as an absolute path) when this asset was imported. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Markdown")
    FString SourceFilePath;
};

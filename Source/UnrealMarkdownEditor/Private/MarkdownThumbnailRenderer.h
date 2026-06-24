// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ThumbnailRendering/ThumbnailRenderer.h"
#include "MarkdownThumbnailRenderer.generated.h"

/**
 * Custom thumbnail renderer for UMarkdownFile assets.
 * Renders the first few lines of text content as the thumbnail.
 */
UCLASS()
class UMarkdownThumbnailRenderer : public UThumbnailRenderer
{
	GENERATED_BODY()

public:
	virtual bool CanVisualizeAsset(UObject* Object) override;
	virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* RenderTarget, FCanvas* Canvas, bool bAdditionalViewFamily) override;
};

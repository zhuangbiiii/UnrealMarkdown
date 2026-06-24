// Copyright Epic Games, Inc. All Rights Reserved.

#include "MarkdownThumbnailRenderer.h"
#include "MarkdownFile.h"

#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Engine/Engine.h"

bool UMarkdownThumbnailRenderer::CanVisualizeAsset(UObject* Object)
{
	return Object && Object->IsA<UMarkdownFile>();
}

void UMarkdownThumbnailRenderer::Draw(
	UObject* Object,
	int32 X,
	int32 Y,
	uint32 Width,
	uint32 Height,
	FRenderTarget* RenderTarget,
	FCanvas* Canvas,
	bool bAdditionalViewFamily)
{
	UMarkdownFile* MdFile = Cast<UMarkdownFile>(Object);
	if (!MdFile)
	{
		return;
	}

	// Draw a subtle background
	FCanvasTileItem BgItem(
		FVector2D(X, Y),
		FVector2D(Width, Height),
		FLinearColor(0.08f, 0.08f, 0.1f, 1.0f));
	BgItem.BlendMode = SE_BLEND_Opaque;
	Canvas->DrawItem(BgItem);

	// Show the first few lines of markdown content
	FString PreviewText = MdFile->Content;
	if (PreviewText.Len() > 500)
	{
		PreviewText = PreviewText.Left(500) + TEXT("...");
	}
	if (PreviewText.IsEmpty())
	{
		PreviewText = TEXT("[Empty Markdown File]");
	}

	// Draw title
	FCanvasTextItem TitleItem(
		FVector2D(X + 8, Y + 4),
		FText::FromString(TEXT("MD")),
		UEngine::GetSmallFont(),
		FLinearColor(0.3f, 0.5f, 1.0f, 1.0f));
	Canvas->DrawItem(TitleItem);

	// Draw preview text
	FCanvasTextItem TextItem(
		FVector2D(X + 8, Y + 24),
		FText::FromString(PreviewText),
		UEngine::GetSmallFont(),
		FLinearColor(0.7f, 0.7f, 0.7f, 1.0f));
	Canvas->DrawItem(TextItem);
}

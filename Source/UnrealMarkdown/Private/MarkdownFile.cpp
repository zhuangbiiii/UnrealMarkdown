// Copyright Epic Games, Inc. All Rights Reserved.

#include "MarkdownFile.h"

void UMarkdownFile::SetContent(const FString& InContent)
{
	Content = InContent;
	Modify();
}

int32 UMarkdownFile::GetLineCount() const
{
	int32 Count = 1;
	for (const TCHAR& Ch : Content)
	{
		if (Ch == TEXT('\n'))
		{
			++Count;
		}
	}
	return Count;
}

void UMarkdownFile::AppendText(const FString& InText)
{
	Content += InText;
	Modify();
}

void UMarkdownFile::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar << Content;
	Ar << SourceFilePath;
}

void UMarkdownFile::PostLoad()
{
	Super::PostLoad();
}

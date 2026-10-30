// UnrealMarkdown - MIT License, see LICENSE in the plugin root.
// 本文件已按 ZBSStudio 的本地集成需要扩充（重导入）。

#include "MarkdownFileReimportHandler.h"

#include "MarkdownFile.h"
#include "UnrealMarkdown.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

bool FMarkdownFileReimportHandler::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	const UMarkdownFile* Asset = Cast<UMarkdownFile>(Obj);
	if (!Asset || Asset->SourceFilePath.IsEmpty())
	{
		return false;
	}

	OutFilenames.Add(Asset->SourceFilePath);
	return true;
}

void FMarkdownFileReimportHandler::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	UMarkdownFile* Asset = Cast<UMarkdownFile>(Obj);
	if (Asset && NewReimportPaths.Num() > 0)
	{
		Asset->Modify();
		Asset->SourceFilePath = FPaths::ConvertRelativePathToFull(NewReimportPaths[0]);
	}
}

EReimportResult::Type FMarkdownFileReimportHandler::Reimport(UObject* Obj)
{
	UMarkdownFile* Asset = Cast<UMarkdownFile>(Obj);
	if (!Asset || Asset->SourceFilePath.IsEmpty())
	{
		return EReimportResult::Failed;
	}

	FString Loaded;
	if (!FFileHelper::LoadFileToString(Loaded, *Asset->SourceFilePath))
	{
		UE_LOG(LogUnrealMarkdown, Error, TEXT("重导入失败，读不到 %s"), *Asset->SourceFilePath);
		return EReimportResult::Failed;
	}

	// 内容没变就不调 SetContent，免得把包无谓标脏后内容浏览器反复提示保存。
	if (!Asset->Content.Equals(Loaded, ESearchCase::CaseSensitive))
	{
		Asset->SetContent(Loaded);
		UE_LOG(LogUnrealMarkdown, Log, TEXT("%s 已从 %s 重导入（%d 字符）"),
			*Asset->GetName(), *Asset->SourceFilePath, Asset->Content.Len());
	}

	return EReimportResult::Succeeded;
}

int32 FMarkdownFileReimportHandler::GetPriority() const
{
	// 高于引擎默认处理器，避免其它文本导入器把 .md 抢走。
	return 100;
}

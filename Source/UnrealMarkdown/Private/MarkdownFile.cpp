// UnrealMarkdown - MIT License, see LICENSE in the plugin root.
// 本文件已按 ZBSStudio 的本地集成需要扩充。

#include "MarkdownFile.h"

#include "Containers/StringConv.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"

namespace MarkdownFilePrivate
{
	/** 取文档实际使用的换行符：导入自 Windows 的是 CRLF，新建的是 LF。 */
	static FString GetLineEnding(const FString& Source)
	{
		return Source.Contains(TEXT("\r\n")) ? FString(TEXT("\r\n")) : FString(TEXT("\n"));
	}

	/** 收集每行起始偏移；最后一项指向内容长度之后，便于判断是否越界。 */
	static void CollectLineStarts(const FString& Source, TArray<int32>& OutStarts)
	{
		OutStarts.Reset();
		OutStarts.Add(0);
		for (int32 Index = 0; Index < Source.Len(); ++Index)
		{
			if (Source[Index] == TEXT('\n'))
			{
				OutStarts.Add(Index + 1);
			}
		}
	}

	/** 非重叠地数出现次数，用于报告替换处数。 */
	static int32 CountOccurrences(const FString& Source, const FString& Needle)
	{
		if (Needle.IsEmpty())
		{
			return 0;
		}

		int32 Count = 0;
		int32 SearchFrom = 0;
		while (true)
		{
			const int32 Found = Source.Find(Needle, ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchFrom);
			if (Found == INDEX_NONE)
			{
				break;
			}
			++Count;
			SearchFrom = Found + Needle.Len();
		}
		return Count;
	}
}

void UMarkdownFile::SetContent(const FString& InContent)
{
	// 编辑器每敲一个字符都会调到这里，内容没变就别把包弄脏。
	if (Content.Equals(InContent, ESearchCase::CaseSensitive))
	{
		return;
	}

	Content = InContent;
	MarkContentChanged();
}

int32 UMarkdownFile::GetLineCount() const
{
	if (Content.IsEmpty())
	{
		return 0;
	}

	int32 Count = 1;
	for (const TCHAR Char : Content)
	{
		if (Char == TEXT('\n'))
		{
			++Count;
		}
	}
	return Count;
}

int32 UMarkdownFile::GetCharCount() const
{
	return Content.Len();
}

bool UMarkdownFile::IsEmpty() const
{
	return Content.IsEmpty();
}

void UMarkdownFile::AppendText(const FString& InText)
{
	if (InText.IsEmpty())
	{
		return;
	}

	Content += InText;
	MarkContentChanged();
}

bool UMarkdownFile::InsertAtLine(int32 LineIndex, const FString& InText)
{
	if (LineIndex < 0 || InText.IsEmpty())
	{
		return false;
	}

	TArray<int32> LineStarts;
	MarkdownFilePrivate::CollectLineStarts(Content, LineStarts);
	if (LineIndex > LineStarts.Num())
	{
		return false;
	}

	const int32 Offset = (LineIndex == LineStarts.Num()) ? Content.Len() : LineStarts[LineIndex];
	const bool bAppendingAtEnd = (Offset == Content.Len());

	FString Payload = InText;
	if (bAppendingAtEnd)
	{
		// 追加到末尾：只在原末尾缺换行时补一个分隔，新文本之后不再留空行。
		if (!Content.IsEmpty() && !Content.EndsWith(TEXT("\n")))
		{
			Payload = MarkdownFilePrivate::GetLineEnding(Content) + Payload;
		}
	}
	else if (!Payload.EndsWith(TEXT("\n")))
	{
		// 插在中间：必须自带换行，否则会和原来那一行粘在一起。
		Payload += MarkdownFilePrivate::GetLineEnding(Content);
	}

	Content = Content.Left(Offset) + Payload + Content.RightChop(Offset);
	MarkContentChanged();
	return true;
}

int32 UMarkdownFile::ReplaceInContent(const FString& OldText, const FString& NewText, bool bReplaceAll)
{
	if (OldText.IsEmpty())
	{
		return 0;
	}

	if (!bReplaceAll)
	{
		const int32 Found = Content.Find(OldText, ESearchCase::CaseSensitive);
		if (Found == INDEX_NONE)
		{
			return 0;
		}
		Content = Content.Left(Found) + NewText + Content.RightChop(Found + OldText.Len());
		MarkContentChanged();
		return 1;
	}

	const int32 Replaced = MarkdownFilePrivate::CountOccurrences(Content, OldText);
	if (Replaced == 0)
	{
		return 0;
	}

	Content.ReplaceInline(*OldText, *NewText, ESearchCase::CaseSensitive);
	MarkContentChanged();
	return Replaced;
}

FString UMarkdownFile::GetContentHash() const
{
	// FMD5::HashAnsiString 会把 FString 当单字节串，非 ASCII 会算错，所以先转 UTF-8。
	// FMD5::HashBytes 直接返回 32 位小写十六进制串，不需要自己再把摘要转字符。
	FTCHARToUTF8 Utf8(*Content);

	return FMD5::HashBytes(reinterpret_cast<const uint8*>(Utf8.Get()), static_cast<uint64>(Utf8.Length()));
}

bool UMarkdownFile::ImportFromFile(const FString& Filename)
{
	FString Loaded;
	if (!FFileHelper::LoadFileToString(Loaded, *Filename))
	{
		return false;
	}

	Content = MoveTemp(Loaded);
	SourceFilePath = FPaths::ConvertRelativePathToFull(Filename);
	MarkContentChanged();
	return true;
}

bool UMarkdownFile::ExportToFile(const FString& Filename, bool bUtf8) const
{
	// EEncodingOptions 是 enum class，类型名就是它自身。
	const FFileHelper::EEncodingOptions Encoding = bUtf8
		? FFileHelper::EEncodingOptions::ForceUTF8
		: FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM;

	return FFileHelper::SaveStringToFile(Content, *Filename, Encoding);
}

void UMarkdownFile::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UMarkdownFile, Content))
	{
		// 详情面板直接改 Content 时不会走 SetContent，这里补上脏包标记。
		MarkContentChanged();
	}
}

void UMarkdownFile::MarkContentChanged()
{
	Modify();
	MarkPackageDirty();
}


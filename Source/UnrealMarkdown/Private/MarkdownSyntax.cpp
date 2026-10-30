// UnrealMarkdown - MIT License, see LICENSE in the plugin root.
// 新增文件：Markdown 语法工具的实现。

#include "MarkdownSyntax.h"

namespace MarkdownSyntaxPrivate
{
	/** 返回行首空白（空格与制表符）的长度。 */
	static int32 GetIndentLength(const FString& Line)
	{
		int32 Indent = 0;
		while (Indent < Line.Len() && (Line[Indent] == TEXT(' ') || Line[Indent] == TEXT('\t')))
		{
			++Indent;
		}
		return Indent;
	}

	/** 把 [From, To) 扩到完整的行边界。 */
	static void ExpandToLineBounds(const FString& Source, int32 From, int32 To, int32& OutBlockStart, int32& OutBlockEnd)
	{
		OutBlockStart = From;
		while (OutBlockStart > 0 && Source[OutBlockStart - 1] != TEXT('\n'))
		{
			--OutBlockStart;
		}

		OutBlockEnd = To;
		while (OutBlockEnd < Source.Len() && Source[OutBlockEnd] != TEXT('\n'))
		{
			++OutBlockEnd;
		}
	}
}

TArray<FMarkdownOutlineEntry> UMarkdownSyntaxLibrary::ExtractOutline(const FString& Source)
{
	TArray<FMarkdownOutlineEntry> Result;
	if (Source.IsEmpty())
	{
		return Result;
	}

	TArray<FString> Lines;
	Source.ParseIntoArrayLines(Lines, false);

	for (int32 Index = 0; Index < Lines.Num(); ++Index)
	{
		const FString& Line = Lines[Index];

		// CommonMark 允许 ATX 标题前最多三个空格。
		int32 Indent = 0;
		while (Indent < Line.Len() && Indent < 3 && Line[Indent] == TEXT(' '))
		{
			++Indent;
		}

		int32 Level = 0;
		while (Indent + Level < Line.Len() && Level < 6 && Line[Indent + Level] == TEXT('#'))
		{
			++Level;
		}
		if (Level == 0)
		{
			continue;
		}

		// 井号后必须是空格或行尾；否则是 #标签 这类普通文本，不是标题。
		const int32 AfterHashes = Indent + Level;
		if (AfterHashes < Line.Len() && Line[AfterHashes] != TEXT(' '))
		{
			continue;
		}

		FMarkdownOutlineEntry Entry;
		Entry.Line = Index;
		Entry.Level = Level;
		Entry.Text = Line.RightChop(AfterHashes).TrimStartAndEnd();
		Result.Add(MoveTemp(Entry));
	}

	return Result;
}

FString UMarkdownSyntaxLibrary::GetLineContinuation(const FString& Line)
{
	const FString Trimmed = Line.TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		return FString();
	}

	// 保留缩进，嵌套列表回车后才能继续往下一层走。
	const FString Indent = Line.Left(MarkdownSyntaxPrivate::GetIndentLength(Line));

	if (Trimmed.StartsWith(TEXT(">")))
	{
		return Indent + TEXT("> ");
	}

	// 任务列表放前面：它同时满足无序列表的判定，先匹配才能保留勾选框。
	static const TCHAR* TaskOpeners[] = { TEXT("- [ ]"), TEXT("- [x]"), TEXT("- [X]"),
		TEXT("* [ ]"), TEXT("* [x]"), TEXT("* [X]") };
	for (const TCHAR* Opener : TaskOpeners)
	{
		if (Trimmed.StartsWith(Opener))
		{
			return Indent + TEXT("- [ ] ");
		}
	}

	if (Trimmed.StartsWith(TEXT("- ")) || Trimmed.StartsWith(TEXT("* ")) || Trimmed.StartsWith(TEXT("+ ")))
	{
		return Indent + TEXT("- ");
	}

	int32 DigitEnd = 0;
	while (DigitEnd < Trimmed.Len() && FChar::IsDigit(Trimmed[DigitEnd]))
	{
		++DigitEnd;
	}
	if (DigitEnd > 0 && Trimmed.Len() > DigitEnd + 1
		&& Trimmed[DigitEnd] == TEXT('.') && Trimmed[DigitEnd + 1] == TEXT(' '))
	{
		const int32 Number = FCString::Atoi(*Trimmed.Left(DigitEnd));
		return FString::Printf(TEXT("%s%d. "), *Indent, Number + 1);
	}

	return FString();
}

void UMarkdownSyntaxLibrary::WrapSelection(
	const FString& Source,
	int32 SelectionStart,
	int32 SelectionEnd,
	const FString& Prefix,
	const FString& Suffix,
	FString& OutText,
	int32& OutSelectionStart,
	int32& OutSelectionEnd)
{
	const int32 Length = Source.Len();
	const int32 From = FMath::Clamp(FMath::Min(SelectionStart, SelectionEnd), 0, Length);
	const int32 To = FMath::Clamp(FMath::Max(SelectionStart, SelectionEnd), 0, Length);
	const FString Selected = Source.Mid(From, To - From);

	OutText = Source.Left(From) + Prefix + Selected + Suffix + Source.RightChop(To);
	OutSelectionStart = From + Prefix.Len();
	OutSelectionEnd = OutSelectionStart + Selected.Len();
}

void UMarkdownSyntaxLibrary::ToggleLinePrefix(
	const FString& Source,
	int32 SelectionStart,
	int32 SelectionEnd,
	const FString& Prefix,
	FString& OutText,
	int32& OutSelectionStart,
	int32& OutSelectionEnd)
{
	const int32 Length = Source.Len();
	const int32 From = FMath::Clamp(FMath::Min(SelectionStart, SelectionEnd), 0, Length);
	const int32 To = FMath::Clamp(FMath::Max(SelectionStart, SelectionEnd), 0, Length);

	int32 BlockStart = 0;
	int32 BlockEnd = 0;
	MarkdownSyntaxPrivate::ExpandToLineBounds(Source, From, To, BlockStart, BlockEnd);

	const FString Block = Source.Mid(BlockStart, BlockEnd - BlockStart);

	TArray<FString> Lines;
	Block.ParseIntoArrayLines(Lines, false);

	// 先判断是加还是减：所有非空行都已带前缀才执行移除。
	bool bAllPrefixed = true;
	bool bHasContent = false;
	for (const FString& Line : Lines)
	{
		if (Line.TrimStartAndEnd().IsEmpty())
		{
			continue;
		}

		bHasContent = true;
		const FString Body = Line.RightChop(MarkdownSyntaxPrivate::GetIndentLength(Line));
		if (!Body.StartsWith(Prefix, ESearchCase::CaseSensitive))
		{
			bAllPrefixed = false;
			break;
		}
	}
	if (!bHasContent)
	{
		bAllPrefixed = false;
	}

	FString Rebuilt;
	for (int32 Index = 0; Index < Lines.Num(); ++Index)
	{
		FString Line = Lines[Index];
		if (!Line.TrimStartAndEnd().IsEmpty())
		{
			const int32 Indent = MarkdownSyntaxPrivate::GetIndentLength(Line);
			FString Body = Line.RightChop(Indent);
			if (bAllPrefixed)
			{
				if (Body.StartsWith(Prefix, ESearchCase::CaseSensitive))
				{
					Body = Body.RightChop(Prefix.Len());
				}
			}
			else
			{
				Body = Prefix + Body;
			}
			Line = Line.Left(Indent) + Body;
		}

		if (Index > 0)
		{
			Rebuilt += TEXT("\n");
		}
		Rebuilt += Line;
	}

	OutText = Source.Left(BlockStart) + Rebuilt + Source.RightChop(BlockEnd);
	OutSelectionStart = BlockStart;
	OutSelectionEnd = BlockStart + Rebuilt.Len();
}

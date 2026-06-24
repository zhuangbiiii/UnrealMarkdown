// Copyright Epic Games, Inc. All Rights Reserved.

#include "MarkdownFileFactory.h"
#include "MarkdownFile.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "MarkdownFileFactory"

UMarkdownFileFactory::UMarkdownFileFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	bEditorImport = true;
	bText = true;

	SupportedClass = UMarkdownFile::StaticClass();

	Formats.Add(TEXT("md;Markdown Document"));
	Formats.Add(TEXT("txt;Text File (import as Markdown)"));
	Formats.Add(TEXT("markdown;Markdown Document"));
}

bool UMarkdownFileFactory::FactoryCanImport(const FString& Filename)
{
	const FString Extension = FPaths::GetExtension(Filename).ToLower();
	return Extension == TEXT("md") || Extension == TEXT("txt") || Extension == TEXT("markdown");
}

UObject* UMarkdownFileFactory::FactoryCreateFile(
	UClass* InClass,
	UObject* InParent,
	FName InName,
	EObjectFlags Flags,
	const FString& Filename,
	const TCHAR* Parms,
	FFeedbackContext* Warn,
	bool& bOutOperationCanceled)
{
	bOutOperationCanceled = false;

	const FString Extension = FPaths::GetExtension(Filename).ToLower();

	// ---- .txt files: ask user whether to import as Markdown ----
	if (Extension == TEXT("txt"))
	{
		const FText Title = LOCTEXT("ImportTxtTitle", "Import Text File");
		const FText Message = FText::Format(
			LOCTEXT("ImportTxtMessage", "The file '{0}' has a .txt extension.\n\nDo you want to import it as a MarkdownFile asset?\n\nThe raw text will be stored verbatim; use Markdown rendering widgets to display it."),
			FText::FromString(FPaths::GetCleanFilename(Filename)));

		EAppReturnType::Type Choice = FMessageDialog::Open(
			EAppMsgType::YesNoCancel,
			Message,
			Title);

		if (Choice == EAppReturnType::Cancel)
		{
			bOutOperationCanceled = true;
			return nullptr;
		}
		if (Choice == EAppReturnType::No)
		{
			// User declined — return nullptr so the default text importer can handle it
			bOutOperationCanceled = true;
			return nullptr;
		}
		// Yes: fall through to import
	}

	// ---- Read the file ----
	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *Filename))
	{
		bOutOperationCanceled = true;
		FMessageDialog::Open(
			EAppMsgType::Ok,
			FText::Format(LOCTEXT("ReadError", "Failed to read file:\n{0}"), FText::FromString(Filename)),
			LOCTEXT("ReadErrorTitle", "Import Error"));
		return nullptr;
	}

	// ---- Create the asset ----
	UMarkdownFile* NewAsset = NewObject<UMarkdownFile>(InParent, InClass, InName, Flags);
	if (!NewAsset)
	{
		bOutOperationCanceled = true;
		return nullptr;
	}

	NewAsset->Content = Content;
	NewAsset->SourceFilePath = Filename;

	return NewAsset;
}

UObject* UMarkdownFileFactory::FactoryCreateNew(
	UClass* InClass,
	UObject* InParent,
	FName InName,
	EObjectFlags Flags,
	UObject* Context,
	FFeedbackContext* Warn)
{
	UMarkdownFile* NewAsset = NewObject<UMarkdownFile>(InParent, InClass, InName, Flags);
	if (NewAsset)
	{
		// Initialize with a helpful template
		NewAsset->Content = FString::Printf(TEXT("# %s\n\nStart writing your markdown here.\n\n## Features\n- **Bold** and *italic* text\n- `inline code`\n- Code blocks\n- Lists and headings"), *InName.ToString());
		NewAsset->SourceFilePath = FString();
		NewAsset->MarkPackageDirty();

		// Notify the asset registry
		FAssetRegistryModule::AssetCreated(NewAsset);
	}
	return NewAsset;
}

#undef LOCTEXT_NAMESPACE

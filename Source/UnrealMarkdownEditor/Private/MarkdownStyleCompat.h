// Copyright Epic Games, Inc. All Rights Reserved.
// Compatibility header: FAppStyle (5.1+) vs FEditorStyle (5.0)

#pragma once

#include "CoreMinimal.h"

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
	#include "Styling/AppStyle.h"
	#define MARKDOWN_STYLE FAppStyle
#else
	#include "EditorStyleSet.h"
	#define MARKDOWN_STYLE FEditorStyle
#endif

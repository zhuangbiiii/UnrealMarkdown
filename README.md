# UnrealMarkdown v2

A Markdown asset system for Unreal Engine 5.8 — import, create, edit, preview, and export `.md` files as native engine assets.

## v2 New Features

| Feature | Description |
| --- | --- |
| **Markdown Assets** | `.md` files are now first-class engine assets (`UMarkdownFile`), like `UStaticMesh` |
| **Import .md / .txt** | Drag `.md` files into Content Browser to import; `.txt` files prompt to confirm import as Markdown |
| **Create New** | Right-click in Content Browser → Miscellaneous → Markdown File |
| **Custom Editor** | Double-click any MarkdownFile asset opens a dedicated editor with Edit + Preview tabs |
| **Export** | Toolbar button exports back to `.md` or `.txt` on disk |
| **Thumbnail** | Content Browser shows text preview thumbnails |
| **Blueprint API** | `UMarkdownFile` accessible in Blueprint: `SetContent`, `GetLineCount`, `AppendText` |

## Architecture

```text
Plugins/UnrealMarkdown/
├── UnrealMarkdown.uplugin          # v2 plugin descriptor
├── Source/
│   ├── UnrealMarkdown/             # Runtime module
│   │   ├── Public/
│   │   │   ├── UnrealMarkdown.h    # Module interface
│   │   │   └── MarkdownFile.h      # UMarkdownFile asset class
│   │   └── Private/
│   │       ├── UnrealMarkdown.cpp
│   │       └── MarkdownFile.cpp
│   └── UnrealMarkdownEditor/       # Editor module
│       ├── Public/
│       │   └── UnrealMarkdownEditor.h
│       └── Private/
│           ├── UnrealMarkdownEditor.cpp         # Asset type actions registration
│           ├── MarkdownFileFactory.h/.cpp       # Import / Create factory
│           ├── MarkdownAssetEditor.h/.cpp       # Custom editor with Edit+Preview
│           └── MarkdownThumbnailRenderer.h/.cpp # Content Browser thumbnails
└── Content/                        # v1 Blueprint widgets (still available)
```

## Usage

### Import a .md file

1. Drag a `.md` file into the Content Browser
2. The factory reads the file and creates a `UMarkdownFile` asset
3. For `.txt` files: a dialog asks whether to import as MarkdownFile

### Create a new MarkdownFile

1. Content Browser → right-click → Miscellaneous → **Markdown File**
2. Name your asset
3. Double-click to open the editor

### Edit a MarkdownFile

Double-click the asset to open the **Markdown Editor**:

- **Edit tab** (left): Raw markdown text editor
- **Preview tab** (right): Raw text preview (v2 shows plain text; v1's RichText renderer is available via Blueprint widgets)
- **Save** button: Saves the asset package
- **Export** button: Writes content to a `.md` file on disk

### Blueprint API

```text
// Get content
Content = MarkdownFile->Content

// Set content at runtime
MarkdownFile->SetContent("# Hello World")

// Append text
MarkdownFile->AppendText("\n## New Section")

// Line count
int32 Lines = MarkdownFile->GetLineCount()
```

### C++ API

```cpp
#include "MarkdownFile.h"

// Load an asset
UMarkdownFile* Md = LoadObject<UMarkdownFile>(nullptr, TEXT("/Game/MyDoc"));

// Read content
FString Text = Md->Content;

// Modify
Md->SetContent(TEXT("# New Content"));
Md->MarkPackageDirty();
```

## Extension Points

| Interface | Purpose |
| --- | --- |
| `FAssetTypeActions_MarkdownFile` | Customize right-click menu, asset color, categories |
| `FMarkdownAssetEditor` | Override `RegisterTabSpawners` to add custom tabs (e.g., rendered HTML preview) |
| `UMarkdownFileFactory` | Extend `FactoryCreateFile` for additional file formats |
| `UMarkdownFile` | Add properties (author, tags, modified date) — UHT-ready |

## Compatibility

- **Engine:** 5.8.0+
- **Platforms:** Windows, Mac, Linux (runtime module is platform-agnostic)
- **License:** MIT

## v1 Legacy

The original v1 Blueprint widgets (`EUW_Markdown_Editor`, `EUW_Markdown_Readme`, `BPFL_MarkdownStatic`) remain in the `Content/` folder and are compatible with v2 assets. You can render `UMarkdownFile::Content` through the existing RichTextBlock-based pipeline.

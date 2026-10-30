# UnrealMarkdown

第三方 Markdown 资产插件（原作者 tzexeo，MIT License，见插件根目录 `LICENSE`）的本地集成说明。
**本文件只描述 ZBSStudio 侧的改造点与署名归属**，插件原有能力与用法见 `README.md`。

## 署名与归属

| 项 | 内容 |
| --- | --- |
| 上游 | `tzexeo` / UnrealMarkdown，MIT License |
| 本地版本 | `Version: 3`，`VersionName: 2.1`（`UnrealMarkdown.uplugin`） |
| 本地改动范围 | 运行时资产能力、编辑器工具栏与预览、内容浏览器右键菜单、MCP 工具集、自动化测试 |
| 代码头 | 改动过的源文件改用 `// UnrealMarkdown - MIT License, see LICENSE in the plugin root.`，不套用 Epic 版权 |

## 模块

| 模块 | 类型 | 职责 |
| --- | --- | --- |
| `UnrealMarkdown` | Runtime | `UMarkdownFile` 资产、`UMarkdownSyntaxLibrary` 纯字符串语法工具、`LogUnrealMarkdown` 日志分类 |
| `UnrealMarkdownEditor` | Editor | 导入工厂、重导入处理器、资产编辑器（源码 + 预览）、内容浏览器右键菜单、缩略图、自动化测试 |
| `UnrealMarkdownMCP` | Editor | `UMarkdownToolset`，向 MCP 暴露 Markdown 资产的增删查改、导入导出、重导入 |
| `UnrealMarkdownAgent` | Editor | `FMarkdownAgentTools`，向编辑器内 ZBS Agent 暴露三个 `markdown_*` 工具 |

## 本地改造点

### 1. 资产能力（`MarkdownFile`）

`UMarkdownFile` 只承载文本，不做解析与渲染。除原有的 `Content` / `SetContent` / `AppendText` 外新增：

- `SourceFilePath`：导入来源的绝对路径，重导入据此回读磁盘文件
- `InsertAtLine` / `ReplaceInContent`：按行插入与文本替换，返回实际处理条数
- `GetContentHash`：内容 MD5（先转 UTF-8 再求摘要，避免 `HashAnsiString` 把中文算错）
- `ImportFromFile` / `ExportToFile`：磁盘读写，导出可选带 BOM 的 UTF-8
- `PostEditChangeProperty`：详情面板直接改 `Content` 时补上标脏

`SetContent` 在内容未变化时直接返回，不再标脏——编辑器每次按键都会调用它，
否则包会一直处于已修改状态。

### 2. 语法工具（`MarkdownSyntax`）

`UMarkdownSyntaxLibrary` 是蓝图函数库，供编辑器工具栏、预览与 MCP 三方共用，
避免同一套 Markdown 规则在三处各写一遍：

- `ExtractOutline`：抽取标题大纲（允许最多 3 个前导空格；`#` 后必须跟空格或行尾）
- `GetLineContinuation`：回车续行，保留缩进并延续列表、引用、任务列表编号
- `WrapSelection` / `ToggleLinePrefix`：成对标记包裹与行前缀开关，回传新的选区范围

### 3. 重导入

重导入接口属于 `FReimportHandler` 而不属于 `UFactory`，因此由
`MarkdownFileReimportHandler`（`Source/UnrealMarkdownEditor/Private/`）单独实现，
在编辑器模块 `StartupModule` 中构造、`ShutdownModule` 中析构：

- `CanReimport` 仅在资产记录了 `SourceFilePath` 时返回 true
- `Reimport` 回读源文件，内容一致时不调用 `SetContent`，避免无谓标脏

### 4. 资产编辑器

`SMarkdownAssetEditor` 左侧源码、右侧预览。**每次文本改动都立刻写回资产并标脏**，
所以标准保存命令、关闭提示与「保存全部」拿到的永远是最新内容。
工具栏提供加粗 / 斜体 / 行内代码 / 链接、H1–H3、无序 / 有序 / 任务 / 引用、代码块与分隔线；
快捷键 `Ctrl+B`、`Ctrl+I`、`Ctrl+K`、`Tab`。

`SMarkdownPreview` 内置轻量渲染：标题、无序 / 有序 / 任务列表（带缩进层级）、
引用、围栏代码块、分隔线、管道表格，以及行内的粗体 / 斜体 / 删除线 / 行内代码 / 链接。
布局用 `SWrapBox` 实现长段落自动换行，重建由活动计时器延迟合批。

### 5. 内容浏览器

`FAssetTypeActions_MarkdownFile::GetActions` 增加「Markdown」分组：
保存、导出为 `.md`、重导入（仅在源文件存在时出现）。

### 6. MCP 工具集

`UMarkdownToolset` 在原有用具基础上补齐增删查改与导入导出。
保存资产走 `FPackageName::LongPackageNameToFilename`——新建包的 `GetLoadedPath()`
是空的，早先按它取路径会导致保存静默失败。

`RenameMarkdownFile` 是「复制 + 删除」而非引擎原生重命名，**不会重写引用关系**，
调用方需要自行处理引用。

### 7. Agent 工具（`UnrealMarkdownAgent`）

与 MCP 侧是两条互不相干的通路：MCP 工具按 `UFUNCTION` 逐个暴露给外部客户端，
Agent 工具收敛成三个「一个工具多个 action」的入口，让模型的工具清单保持短、参数固定。
两者共用 `UMarkdownFile` 与 `UMarkdownSyntaxLibrary`，不各自实现一套 Markdown 规则。

| 工具 | 等级 | 动作 |
| --- | --- | --- |
| `markdown_query` | 2 | `list` / `read` / `info` / `outline` / `search` |
| `markdown_edit` | 3 | `set` / `append` / `insert_line` / `replace` / `save` |
| `markdown_asset` | 4 | `create` / `import` / `reimport` / `export` / `delete` |

几条刻意的设计：

- **等级按「读 2 / 改文本 3 / 动资产 4」划分**，与 ZBSAgent 宿主按等级推导写入总闸的
  规则（3 文本 / 4 资产）对齐，不需要宿主为本插件特判。
- **`markdown_edit` 默认立刻落盘**（`autoSave` 默认 true）。Agent 一轮里常常写完就不再回来，
  留一个脏包在内存里会让后续 `read` 与磁盘上的文件不一致。
- **`expectedHash` 守卫**：`markdown_query` 回传 `contentHash`，写入时带上它，
  期间若有人改过同一个资产，写入会被拒（`errorCode=stale_asset`），避免覆盖别人的改动。
- **失败一律给稳定的 `errorCode`**（`asset_not_found` / `asset_exists` / `invalid_line_index` /
  `no_source_file` / `stale_asset` / `save_failed` 等），模型据此能判断是参数错了还是状态变了。
- 缩写靠 `DELETE` 走 `ObjectTools::ForceDeleteObjects` 而不是 `DeleteSingleObject`：
  后者只清内存，会留下磁盘上还在、编辑器里看不到的孤儿包。

### 8. 自动化测试

`Source/UnrealMarkdownEditor/Private/Tests/MarkdownAssetTests.cpp`：

| 用例 | 覆盖 |
| --- | --- |
| `UnrealMarkdown.Syntax` | 包裹选区、行前缀开关、续行、大纲抽取 |
| `UnrealMarkdown.AssetContent` | 行数 / 字符数、标脏与幂等、hash、插入、替换 |
| `UnrealMarkdown.FileIO` | 导出 → 导入往返、中文与 emoji、源路径、重导入 |

`Source/UnrealMarkdownAgent/Private/Tests/MarkdownAgentToolsTests.cpp`：

| 用例 | 覆盖 |
| --- | --- |
| `UnrealMarkdown.AgentToolContract` | schema 合法、三个工具齐全、等级 2/3/4、坏参数给稳定 errorCode |
| `UnrealMarkdown.AgentToolRoundTrip` | 建 → 读 → 大纲 → 搜索 → 替换（含 hash 守卫）→ 追加 / 插入 → 导出 → 重导入拒绝 → 删除 |

测试资产建在独立普通包里而不是 `NewObject<>()` 的默认瞬态外层——
`MarkPackageDirty` 遇到 `RF_Transient` 链会直接返回、不置脏标志。

运行方式（不要用 UAT，直接起 `UnrealEditor-Cmd.exe`）：

```bat
"%UE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%PROJ%\ZBSStudio.uproject" ^
 -ExecCmds="Automation RunTests UnrealMarkdown;Quit" ^
 -unattended -nopause -nosplash -nullrhi -NoSound -stdout -FullStdOutLogOutput ^
 -ABSLOG="%PROJ%\.Agent\md_tests.log" -ReportOutputPath="%PROJ%\.Agent\md_report"
```

现成脚本是 `Tools/RunMarkdownTests.bat`。

**最近一次全量结果（2026-10-29）**：`Found 5 automation tests`，
`AgentToolContract` / `AgentToolRoundTrip` / `AssetContent` / `FileIO` / `Syntax` **5/5 `{Success}`**，
`GIsCriticalError=0`。

## 已知限制

- 预览是自绘的轻量渲染，不追求完整 CommonMark；引用嵌套、脚注、内嵌 HTML 不渲染。
- 重导入不重新解析资产引用，重命名资产不会同步改源文件名。
- 插件未登记在 `ZBSStudio.uproject` 的 `Plugins` 数组里，靠「项目插件默认启用」加载。
- `markdown_query` 的 `list` / `search` 会加载工程里**每一个** Markdown 资产。
  引擎在 `LinkerLoad` 里对「序列化字节数与标签不符」的包是 `Fatal`，所以只要工程里存在一个
  被截断的 `.uasset`，这两个动作就会把编辑器带下去。已做的防护是先把「注册表里有、磁盘上没了」
  的条目剔掉（见 `CollectAssets`），并在结果里回传 `skipped`；但**损坏但存在**的文件仍需人工清理。

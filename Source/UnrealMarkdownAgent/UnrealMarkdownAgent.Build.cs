// UnrealMarkdown - MIT License, see LICENSE in the plugin root.
// 面向编辑器内 ZBS Agent 的 Markdown 工具集模块。ZBSAgent 是可选依赖：
// 没装那个插件时本模块照常加载，只是不注册任何工具。

using System;
using System.IO;
using UnrealBuildTool;

public class UnrealMarkdownAgent : ModuleRules
{
	/// <summary>ZBSAgent 插件的名字，同时用作目录名与描述文件名。</summary>
	private const string AgentPluginName = "ZBSAgent";

	public UnrealMarkdownAgent(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"UnrealMarkdown",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"UnrealEd",
				"AssetRegistry",
				"AssetTools",
				// FJsonObject / FJsonSerializer / LogJson 都在 Json 模块里，
				// 不能靠 UnrealEd 的传递依赖。
				"Json",
			}
		);

		// ZBSAgent 只在装了它的工程里才有意义，因此不能写成硬依赖：
		// 硬依赖会让发布包在别人的机器上直接 UBT 报 Unable to find module。
		bool bHasAgentPlugin = IsAgentPluginUsable(Target);
		if (bHasAgentPlugin)
		{
			// IZBSAgentToolProvider 与 FZBSAgentToolCall 只在本模块实现里用到，
			// 按最小暴露放私有依赖。
			PrivateDependencyModuleNames.Add("ZBSAgentRuntime");
		}

		// 工具实现与测试整段包在这个宏里，取值 0 时本模块退化成空壳。
		PrivateDefinitions.Add("UE_ZBS_MARKDOWN_AGENT_TOOLS=" + (bHasAgentPlugin ? "1" : "0"));
	}

	/// <summary>
	/// 判断 ZBSAgent 插件能否参与本次构建。
	///
	/// 只找描述文件不够：工程可以在 .uproject 里把它显式关掉，那时插件目录还在，
	/// 但它的模块不会被编译，依赖它的模块依然会链接失败。因此两步都要看。
	/// </summary>
	/// <param name="Target">本次构建的目标，用来拿工程文件与引擎目录。</param>
	/// <returns>描述文件存在且没有被工程显式禁用时为 true。</returns>
	private bool IsAgentPluginUsable(ReadOnlyTargetRules Target)
	{
		// 插件可能装在工程的 Plugins 下，也可能装在引擎的 Plugins 下。
		string ProjectPluginsRoot = null;
		if (Target.ProjectFile != null)
		{
			string ProjectRoot = Path.GetDirectoryName(Target.ProjectFile.FullName);
			if (!String.IsNullOrEmpty(ProjectRoot))
			{
				ProjectPluginsRoot = Path.Combine(ProjectRoot, "Plugins");
			}
		}

		// EngineDirectory 是 ModuleRules 上的字符串属性，不在 ReadOnlyTargetRules 上。
		string EnginePluginsRoot = Path.Combine(EngineDirectory, "Plugins");

		bool bDescriptorFound = false;
		foreach (string Root in new string[] { ProjectPluginsRoot, EnginePluginsRoot })
		{
			if (String.IsNullOrEmpty(Root))
			{
				continue;
			}
			if (File.Exists(Path.Combine(Root, AgentPluginName, AgentPluginName + ".uplugin")))
			{
				bDescriptorFound = true;
				break;
			}
		}

		if (!bDescriptorFound)
		{
			return false;
		}

		return !IsAgentPluginDisabledInProject(Target);
	}

	/// <summary>
	/// 在 .uproject 里找 ZBSAgent 的插件条目，看它是否被显式写成 Enabled: false。
	///
	/// 读文件失败或解析失败一律当作「没被禁用」：宁可让 UBT 在链接阶段报出明确的
	/// 缺失模块，也不要因为读不懂工程文件就把功能悄悄关掉。
	/// </summary>
	/// <param name="Target">本次构建的目标。</param>
	/// <returns>工程里明确禁用该插件时为 true。</returns>
	private static bool IsAgentPluginDisabledInProject(ReadOnlyTargetRules Target)
	{
		if (Target.ProjectFile == null)
		{
			return false;
		}

		string Text;
		try
		{
			Text = File.ReadAllText(Target.ProjectFile.FullName);
		}
		catch (IOException)
		{
			return false;
		}

		string Quoted = "\"" + AgentPluginName + "\"";
		int SearchFrom = 0;
		while (true)
		{
			int At = Text.IndexOf(Quoted, SearchFrom, StringComparison.Ordinal);
			if (At < 0)
			{
				return false;
			}
			SearchFrom = At + Quoted.Length;

			string Entry = ReadEnclosingObject(Text, At);
			if (Entry != null && IsExplicitlyDisabled(Entry))
			{
				return true;
			}
		}
	}

	/// <summary>取包含 Position 的那个最内层花括号块；找不到闭合时返回 null。</summary>
	private static string ReadEnclosingObject(string Text, int Position)
	{
		int Open = Text.LastIndexOf('{', Position);
		if (Open < 0)
		{
			return null;
		}

		int Depth = 0;
		for (int Index = Open; Index < Text.Length; ++Index)
		{
			if (Text[Index] == '{')
			{
				++Depth;
			}
			else if (Text[Index] == '}')
			{
				--Depth;
				if (Depth == 0)
				{
					return Text.Substring(Open, Index - Open + 1);
				}
			}
		}
		return null;
	}

	/// <summary>条目里写了 Enabled 且值为 false 时为 true。</summary>
	private static bool IsExplicitlyDisabled(string Entry)
	{
		int At = Entry.IndexOf("\"Enabled\"", StringComparison.Ordinal);
		if (At < 0)
		{
			return false;
		}

		int Value = Entry.IndexOf(':', At);
		if (Value < 0)
		{
			return false;
		}

		string Rest = Entry.Substring(Value + 1).TrimStart();
		return Rest.StartsWith("false", StringComparison.OrdinalIgnoreCase);
	}
}

// Fill out your copyright notice in the Description page of Project Settings.

using System.IO;
using UnrealBuildTool;

public class RevulyticsLibrary : ModuleRules
{
	const string RevulyticsVersionString = "5.5.1";
	
	public RevulyticsLibrary(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// Add the import library
			PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "x64", "Release", "ruiSDK_" + RevulyticsVersionString + ".x64.lib"));

			// Delay-load the DLL, so we can load it from the right place first
			PublicDelayLoadDLLs.Add("ruiSDK_" + RevulyticsVersionString + ".x64.dll");

			// Ensure that the DLL is staged along with the executable
			RuntimeDependencies.Add("$(PluginDir)/Binaries/ThirdParty/RevulyticsLibrary/Win64/ruiSDK_" + RevulyticsVersionString + ".x64.dll");
		}

		PublicDefinitions.Add(string.Format("REVULYTICS_VER=\"{0}\"", RevulyticsVersionString));
	}
}

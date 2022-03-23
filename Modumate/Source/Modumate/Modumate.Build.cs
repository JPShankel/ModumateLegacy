// Copyright 2021 Modumate, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class Modumate : ModuleRules
{
	public Modumate(ReadOnlyTargetRules Target) : base(Target)
	{
		// Exceptions enabled for third party libraries that rely on our code catching their exceptions.
		bEnableExceptions = true;

		PrivatePCHHeaderFile = "Private/ModumatePCH.h";
		OptimizeCode = CodeOptimization.InShippingBuildsOnly;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"EngineSettings",
			"InputCore",
			"ProceduralMeshComponent",
			"Json",
			"JsonUtilities",
			"Slate",
			"SlateCore",
			"ApplicationCore",
			"ImageWrapper",
			"ImageWriteQueue",
			"RenderCore",
			"HTTP",
			"Analytics",
			"Slate",
			"SlateCore",
			"UMG",
			"Cbor",
			"Serialization",
			"GeometryAlgorithms",
			"GeometricObjects",
			"SunPosition",
			"RHI",
			"MediaAssets",
			"WebBrowserWidget",
			"WebBrowser",
			"DatasmithRuntime"
		});

		if (Target.Platform == UnrealTargetPlatform.Win64 ||
			Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicDependencyModuleNames.Add("VivoxCore");
		}

		PrivateDependencyModuleNames.AddRange(new string[] { });

		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			LoadWinLibs();
		}
	}

	public bool LoadWinLibs()
	{
		PublicAdditionalLibraries.Add(Path.GetFullPath(Path.Combine(ModuleDirectory, "../../WinLib/ShLwApi.lib")));
		PublicDefinitions.Add(string.Format("WITH_WINLIB_BINDING={0}", 1));
		return true;
	}
}

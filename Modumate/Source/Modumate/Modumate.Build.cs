// Fill out your copyright notice in the Description page of Project Settings.

using System.IO;
using UnrealBuildTool;

public class Modumate : ModuleRules
{
	private string ModulePath
	{
		get { return ModuleDirectory; }
	}

	private string BinariesPath
	{
		get { return Path.GetFullPath(Path.Combine(ModulePath, "../../Binaries/Win64")); }
	}

	private string WinLibPath
	{
		get { return Path.GetFullPath(Path.Combine(ModulePath, "../../WinLib")); }
	}

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
			"MediaAssets"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {  });

		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}

		LoadWinLibs();
	}

	public bool LoadWinLibs()
	{
		PublicAdditionalLibraries.Add(Path.Combine(WinLibPath, "ShLwApi.lib"));
		PublicDefinitions.Add(string.Format("WITH_WINLIB_BINDING={0}", 1));
		return true;
	}
}

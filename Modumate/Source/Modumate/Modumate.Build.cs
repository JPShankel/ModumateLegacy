// Fill out your copyright notice in the Description page of Project Settings.

using System.IO;
using UnrealBuildTool;

public class Modumate : ModuleRules
{
	private string ModulePath
	{
		get { return ModuleDirectory; }
	}

	private string PDFLibraryPath
	{
		get { return Path.GetFullPath(Path.Combine(ModulePath, "../../Source/ThirdParty/PDFTron/Lib/")); }
	}

	private string PDFIncludePath
	{
		get { return Path.GetFullPath(Path.Combine(ModulePath, "../../Source/ThirdParty/PDFTron/Headers/")); }
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
		bEnforceIWYU = true;
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivatePCHHeaderFile = "Private/ModumatePCH.h";

		bEnableExceptions = true;
        bLegacyPublicIncludePaths = false;

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
		});

		PrivateDependencyModuleNames.AddRange(new string[] {  });

		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}

		LoadPDF(Target);
		LoadWinLibs();
	}

	public bool LoadPDF(ReadOnlyTargetRules Target)
	{
		PublicAdditionalLibraries.Add(Path.Combine(PDFLibraryPath, "PDFNetC.lib"));
		PublicIncludePaths.Add(PDFIncludePath);

        RuntimeDependencies.Add(Path.Combine(BinariesPath, "PDFNetC.dll"));
        RuntimeDependencies.Add(Path.Combine(BinariesPath, "PDFNetDotNetCore.dll"));

        PublicDelayLoadDLLs.Add("PDFNetC.dll");
        PublicDelayLoadDLLs.Add("PDFNetDotNetCore.dll");

        PublicDefinitions.Add(string.Format("WITH_PDFTRON_BINDING={0}", 1));
		return true;
	}

	public bool LoadWinLibs()
	{
		PublicAdditionalLibraries.Add(Path.Combine(WinLibPath, "ShLwApi.lib"));
		PublicDefinitions.Add(string.Format("WITH_WINLIB_BINDING={0}", 1));
		return true;
	}
}

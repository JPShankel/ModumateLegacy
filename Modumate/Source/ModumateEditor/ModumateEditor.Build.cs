// Fill out your copyright notice in the Description page of Project Settings.

using System.IO;
using UnrealBuildTool;

public class ModumateEditor : ModuleRules
{
	private string ModulePath
	{
		get { return ModuleDirectory; }
	}

	public ModumateEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;
		OptimizeCode = CodeOptimization.Never;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Modumate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"AssetTools",
			"Core",
			"CoreUObject",
			"Engine",
			"DesktopPlatform",
			"Modumate",
			"SourceControl",
			"UnrealEd",
		});
	}
}

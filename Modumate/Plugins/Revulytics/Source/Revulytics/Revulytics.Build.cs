// Copyright 2019 Modumate, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class Revulytics : ModuleRules
	{
		public Revulytics( ReadOnlyTargetRules Target ) : base(Target)
		{
			PCHUsage = PCHUsageMode.NoPCHs;

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"EngineSettings",
					"Projects",
					"RevulyticsLibrary",
					// ... add other public dependencies that you statically link with here ...
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Analytics",
					// ... add private dependencies that you statically link with here ...
				}
				);

			PublicIncludePathModuleNames.Add("Analytics");
		}
	}
}

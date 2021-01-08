// Copyright 2019 Modumate, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ModumateAnalytics : ModuleRules
	{
		public ModumateAnalytics( ReadOnlyTargetRules Target ) : base(Target)
		{
			PCHUsage = PCHUsageMode.NoPCHs;

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					// ... add other public dependencies that you statically link with here ...
					"Json",
					"JsonUtilities",
					"Modumate",
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Analytics",
					"HTTP",
					// ... add private dependencies that you statically link with here ...
				}
				);

			PublicIncludePathModuleNames.Add("Analytics");
			PublicIncludePathModuleNames.Add("Modumate");
		}
	}
}

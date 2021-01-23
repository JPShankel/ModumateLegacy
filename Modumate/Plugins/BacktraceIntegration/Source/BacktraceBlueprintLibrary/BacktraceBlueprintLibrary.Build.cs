// Copyright 2017-2018 Backtrace I/O. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class BacktraceBlueprintLibrary : ModuleRules
    {
        public BacktraceBlueprintLibrary(ReadOnlyTargetRules Target) : base(Target)
        {
            PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

            PublicIncludePaths.AddRange(
                new string[] {
                }
                );

            PrivateIncludePaths.AddRange(
                new string[] {
                    "BacktraceBlueprintLibrary/Private",
                }
                );

            PublicDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core",
                    "CoreUObject",
                    "Engine",
                    "Projects",
                    "BacktraceSettingsLibrary",
                }
                );

            //@third party BEGIN MODUMATE - remove circular reference
            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    //"BacktraceBlueprintLibrary",
                }
                );
            //@third party END MODUMATE

            DynamicallyLoadedModuleNames.AddRange(
                new string[]
                {
                }
                );

            PublicIncludePathModuleNames.Add("BacktraceBlueprintLibrary");
		}
	}
}

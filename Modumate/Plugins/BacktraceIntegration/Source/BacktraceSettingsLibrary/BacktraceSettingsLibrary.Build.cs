// Copyright 2017-2018 Backtrace I/O. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class BacktraceSettingsLibrary : ModuleRules
    {
        public BacktraceSettingsLibrary(ReadOnlyTargetRules Target) : base(Target)
        {
            PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

            //@third party BEGIN MODUMATE - remove reference to missing directory
#if false
            PublicIncludePaths.AddRange(
                new string[] {
                    "BacktraceSettingsLibrary/Public"
                }
                );
#endif
            //@third party END MODUMATE

            PrivateIncludePaths.AddRange(
                new string[] {
                }
                );

            //@third party BEGIN MODUMATE - remove circular reference
            PublicDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core",
                    "CoreUObject",
                    "Engine",
                    "InputCore",
                    "Projects",
                    "DeveloperSettings",
                    //"BacktraceSettingsLibrary",
                }
                );

            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    //"BacktraceSettingsLibrary",
                }
                );
            //@third party END MODUMATE

            DynamicallyLoadedModuleNames.AddRange(
                new string[]
                {
                }
                );

            PublicIncludePathModuleNames.Add("BacktraceSettingsLibrary");
        }
    }
}

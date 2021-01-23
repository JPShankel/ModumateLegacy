// Copyright 2017-2018 Backtrace I/O. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class BacktraceIntegration : ModuleRules
    {
        public BacktraceIntegration(ReadOnlyTargetRules Target) : base(Target)
        {
            PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

            PublicIncludePaths.AddRange(
                new string[] {
                }
                );

            PrivateIncludePaths.AddRange(
                new string[] {
                    "BacktraceIntegration/Private",
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
                    "Json",
                    "JsonUtilities",
                    "BacktraceSettingsLibrary",
                    //"BacktraceIntegration",
                }
                );

            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "Slate",
                    "SlateCore",
                    "UnrealEd",
                    "XmlParser",
                    //"BacktraceIntegration",
                }
                );
            //@third party END MODUMATE

            DynamicallyLoadedModuleNames.AddRange(
                new string[]
                {
                }
                );

            PublicIncludePathModuleNames.Add("BacktraceIntegration");
        }
    }
}

// Copyright 2021 Modumate, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;
using System.Collections.Generic;

public class ModumateTarget : TargetRules
{
	public ModumateTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		ExtraModuleNames.AddRange( new string[] { "Modumate" } );
		bUseLoggingInShipping = true;
	    if (Target.Platform == UnrealTargetPlatform.Mac)
	    {
			string frameworkCopy = "sh $(ProjectDir)/Scripts/MacPostBuild.sh $(ProjectDir) $(TargetName) $(TargetConfiguration)";
			string sign = "sh $(ProjectDir)/Scripts/MacPostBuildSign.sh $(ProjectDir) $(TargetName) $(TargetConfiguration)";
			PostBuildSteps.Add(frameworkCopy);
			PostBuildSteps.Add(sign);
	    }
	}
}

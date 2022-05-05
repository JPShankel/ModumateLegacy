// Copyright 2021 Modumate, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;
using System.Collections.Generic;

public class ModumateEditorTarget : TargetRules
{
	public ModumateEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		ExtraModuleNames.AddRange( new string[] { "ModumateEditor" } );
		if (Target.Platform == UnrealTargetPlatform.Mac)
	    {
			string postBuildScript = "sh $(ProjectDir)/Scripts/MacPostBuild.sh $(ProjectDir) $(TargetName) $(TargetConfiguration)";
			PostBuildSteps.Add(postBuildScript);
	    }
	}
}

// Copyright 2021 Modumate, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class ModumateEditorTarget : TargetRules
{
	public ModumateEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		ExtraModuleNames.AddRange( new string[] { "ModumateEditor" } );
	}
}

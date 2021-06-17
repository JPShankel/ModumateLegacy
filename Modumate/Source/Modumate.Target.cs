// Copyright 2021 Modumate, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class ModumateTarget : TargetRules
{
	public ModumateTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		ExtraModuleNames.AddRange( new string[] { "Modumate" } );
		bUseLoggingInShipping = true;
	}
}

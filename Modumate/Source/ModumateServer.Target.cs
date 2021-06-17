// Copyright 2021 Modumate, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class ModumateServerTarget : TargetRules
{
	public ModumateServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		ExtraModuleNames.AddRange( new string[] { "Modumate" } );
		bUseLoggingInShipping = true;
	}
}

// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelRailTool.h"
#include "UnrealClasses/EditModelPlayerController.h"

URailTool::URailTool()
	: Super()
{
	ObjectType = EObjectType::OTRailSegment;
}

float URailTool::GetDefaultPlaneHeight() const
{
	return Controller->GetDefaultRailingsHeightFromDoc();
}

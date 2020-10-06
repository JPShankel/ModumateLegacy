// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelRailTool.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"

URailTool::URailTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ObjectType = EObjectType::OTRailSegment;
	SetAxisConstraint(EAxisConstraint::AxisZ);
}

float URailTool::GetDefaultPlaneHeight() const
{
	return Controller->GetDefaultRailingsHeightFromDoc();
}

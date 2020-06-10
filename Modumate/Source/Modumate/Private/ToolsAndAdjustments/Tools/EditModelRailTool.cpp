// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelRailTool.h"
#include "DocumentManagement/ModumateObjectInstanceRails.h"

URailTool::URailTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

bool URailTool::Activate()
{
	return UEditModelToolBase::Activate();
}

bool URailTool::BeginUse()
{
	return Super::BeginUse();
}

bool URailTool::EnterNextStage()
{
	return Super::EnterNextStage();
}

bool URailTool::FrameUpdate()
{
	return UEditModelToolBase::FrameUpdate();
}

bool URailTool::EndUse()
{
	return UEditModelToolBase::EndUse();
}

bool URailTool::AbortUse()
{
	return UEditModelToolBase::AbortUse();
}

bool URailTool::HandleInputNumber(double n)
{
	return true;
}



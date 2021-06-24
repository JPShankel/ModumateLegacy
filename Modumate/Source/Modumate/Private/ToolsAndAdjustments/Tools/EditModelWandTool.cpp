// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelWandTool.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "DocumentManagement/ModumateSnappingView.h"
#include "Runtime/Engine/Classes/Engine/Engine.h"



UWandTool::UWandTool()
	: Super()
{}

bool UWandTool::Activate()
{
	Super::Activate();

	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Object;
	return true;
}

bool UWandTool::Deactivate()
{
	return UEditModelToolBase::Deactivate();
}


bool UWandTool::BeginUse()
{
	Super::BeginUse();

	return true;
}

bool UWandTool::HandleMouseUp()
{

	AEditModelGameState *gameState = Controller->GetWorld()->GetGameState<AEditModelGameState>();
	UModumateDocument* doc = gameState->Document;

	AModumateObjectInstance *newTarget = Controller->EMPlayerState->HoveredObject;
	if (newTarget)
	{
		UModumateFunctionLibrary::SetMOIAndDescendentsHidden(TArray<AModumateObjectInstance*> {newTarget});
	}
	EndUse();

	return true;
}

bool UWandTool::FrameUpdate()
{
	Super::FrameUpdate();
	return true;
}

bool UWandTool::EndUse()
{
	Super::EndUse();
	return true;
}



// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelWandTool.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "DocumentManagement/ModumateSnappingView.h"
#include "Runtime/Engine/Classes/Engine/Engine.h"

using namespace Modumate;

UWandTool::UWandTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
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

	AEditModelGameState_CPP *gameState = Controller->GetWorld()->GetGameState<AEditModelGameState_CPP>();
	UModumateDocument *doc = &gameState->Document;

	AModumateObjectInstance *newTarget = Controller->EMPlayerState->HoveredObject;
	if (newTarget)
	{
		UModumateFunctionLibrary::DocAddHideMoiActors(TArray<AActor*> {newTarget->GetActor()});
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



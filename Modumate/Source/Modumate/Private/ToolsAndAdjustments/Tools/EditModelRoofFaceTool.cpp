// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelRoofFaceTool.h"

#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/LineActor.h"
#include "DocumentManagement/ModumateCommands.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "DocumentManagement/ModumateObjectInstanceCabinets.h"

using namespace Modumate;

URoofFaceTool::URoofFaceTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ObjectType = EObjectType::OTRoofFace;
	SetAxisConstraint(EAxisConstraint::None);
	bRequireHoverMetaPlane = true;
}

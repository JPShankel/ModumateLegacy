// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Handles/EditModelPortalAdjustmentHandles.h"

#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "DocumentManagement/ModumateCommands.h"
#include "Runtime/Engine/Classes/Kismet/KismetMathLibrary.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"
#include "UI/AdjustmentHandleAssetData.h"
#include "UI/AdjustmentHandleWidget.h"

using namespace Modumate;
bool AAdjustPortalInvertHandle::BeginUse()
{
	if (!Super::BeginUse())
	{
		return false;
	}

	TArray<int32> ids = { TargetMOI->ID };

	if (bShouldTransvert)
	{
		Controller->ModumateCommand(
			FModumateCommand(Commands::kTransverseObjects)
			.Param(Parameters::kObjectIDs, ids));
	}
	else
	{
		Controller->ModumateCommand(
			FModumateCommand(Commands::kInvertObjects)
			.Param(Parameters::kObjectIDs, ids));
	}

	EndUse();
	return false;
}

FVector AAdjustPortalInvertHandle::GetHandlePosition() const
{
	FVector bottomCorner0, bottomCorner1;
	if (!GetBottomCorners(bottomCorner0, bottomCorner1))
	{
		return FVector::ZeroVector;
	}

	if (bShouldTransvert)
	{
		return FMath::Lerp(bottomCorner0, bottomCorner1, 0.25f);
	}
	else
	{
		return FMath::Lerp(bottomCorner0, bottomCorner1, 0.75f);
	}
}

FVector AAdjustPortalInvertHandle::GetHandleDirection() const
{
	if (bShouldTransvert)
	{
		return ensure(TargetMOI) ? TargetMOI->GetNormal() : FVector::ZeroVector;
	}
	else
	{
		return ensure(TargetMOI) ? (FVector::UpVector ^ TargetMOI->GetNormal()).GetSafeNormal() : FVector::ZeroVector;
	}
}

void AAdjustPortalInvertHandle::SetTransvert(bool bInShouldTransvert)
{
	bShouldTransvert = bInShouldTransvert;
}

bool AAdjustPortalInvertHandle::GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const
{
	OutButtonStyle = PlayerHUD->HandleAssets->InvertStyle;
	OutWidgetSize = FVector2D(48.0f, 48.0f);
	return true;
}

bool AAdjustPortalInvertHandle::GetBottomCorners(FVector &OutCorner0, FVector &OutCorner1) const
{
	if (!ensure(TargetMOI))
	{
		return false;
	}

	FVector bottomFrontCorner0 = TargetMOI->GetCorner(0);
	FVector bottomFrontCorner1 = TargetMOI->GetCorner(3);
	FVector bottomBackCorner0 = TargetMOI->GetCorner(4);
	FVector bottomBackCorner1 = TargetMOI->GetCorner(7);

	OutCorner0 = 0.5f * (bottomFrontCorner0 + bottomBackCorner0);
	OutCorner1 = 0.5f * (bottomFrontCorner1 + bottomBackCorner1);
	return true;
}

// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Handles/AdjustBackgroundImageExtentsHandle.h"

#include "Objects/BackgroundImage.h"
#include "DocumentManagement/ModumateDocument.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"



AAdjustBackgroundImageExtentsHandle::AAdjustBackgroundImageExtentsHandle(const FObjectInitializer& ObjectInitializer /*= FObjectInitializer::Get()*/)
	: Super(ObjectInitializer)
{ }

bool AAdjustBackgroundImageExtentsHandle::BeginUse()
{
	if (!AAdjustmentHandleActor::BeginUse() || (TargetMOI == nullptr) || (TargetMOI->GetObjectType() != EObjectType::OTBackgroundImage))
	{
		return false;
	}

	LastValidLocation = FVector(ForceInitToZero);
	LastValidExtents = FVector2D(ForceInitToZero);
	LastSign = 1.0f;

	int32 numCorners = TargetMOI->GetNumCorners();

	FVector targetNormal = TargetMOI->GetNormal();
	if (!targetNormal.IsNormalized() || numCorners == 0)
	{
		return false;
	}
	FPlane PolyPlane = FPlane(TargetMOI->GetCorner(0), targetNormal);

	AnchorLoc = FVector::PointPlaneProject(GetHandlePosition(), PolyPlane);
	Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(AnchorLoc, FVector(PolyPlane));

	const AMOIBackgroundImage* backgroundImageMOI = Cast<AMOIBackgroundImage>(TargetMOI);
	if (!ensure(backgroundImageMOI))
	{
		return false;
	}

	FMOIStateData targetModifiedState = TargetOriginalState;
	FMOIBackgroundImageData imageData;
	OriginalImageSize = backgroundImageMOI->GetImageSize();
	if (OriginalImageSize.IsZero())
	{
		return false;
	}
	if (ensure(targetModifiedState.CustomData.LoadStructData(imageData)))
	{
		LastValidLocation = imageData.Location;
		OriginalLocation = LastValidLocation;
		LastValidExtents = OriginalImageSize * imageData.Scale;
		OriginalExtents = LastValidExtents;
		AspectRatio = OriginalImageSize.X / OriginalImageSize.Y;
	}

	return true;
}

void AAdjustBackgroundImageExtentsHandle::ApplyExtents(bool bIsPreview)
{
	auto delta = MakeShared<FMOIDelta>();

	FMOIStateData targetModifiedState = TargetOriginalState;
	FMOIBackgroundImageData imageData;
	if (ensure(targetModifiedState.CustomData.LoadStructData(imageData)))
	{
		imageData.Location = LastValidLocation;
		imageData.Scale = LastValidExtents.X / OriginalImageSize.X;
		targetModifiedState.CustomData.SaveStructData<FMOIBackgroundImageData>(imageData);
		delta->AddMutationState(TargetMOI, TargetOriginalState, targetModifiedState);
		if (bIsPreview)
		{
			Controller->GetDocument()->ApplyPreviewDeltas({ delta }, GetWorld());
		}
		else
		{
			Controller->GetDocument()->ApplyDeltas({ delta }, GetWorld());
		}
	}
}

void AAdjustBackgroundImageExtentsHandle::UpdateTransform(float Offset)
{
	const FVector handleDirection = GetHandleDirection();
	const FVector secondDirection = (TargetMOI->GetNormal() ^ handleDirection).GetSafeNormal();
	if (TargetIndex % 2 == 0)
	{
		LastValidExtents.X = OriginalExtents.X + Offset * AspectRatio;
		LastValidExtents.Y = OriginalExtents.Y + Offset;
		LastValidLocation = OriginalLocation + handleDirection * Offset * 0.5 + secondDirection * Offset * 0.5 * AspectRatio;
	}
	else
	{
		LastValidExtents.X = OriginalExtents.X + Offset;
		LastValidExtents.Y = OriginalExtents.Y + Offset / AspectRatio;
		LastValidLocation = OriginalLocation + handleDirection * Offset * 0.5f - secondDirection * Offset * 0.5 / AspectRatio;

	}
}

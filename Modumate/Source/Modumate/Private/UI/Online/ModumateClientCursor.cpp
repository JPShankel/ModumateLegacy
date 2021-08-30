// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/Online/ModumateClientCursor.h"
#include "Components/Image.h"
#include "UnrealClasses/EditModelPlayerController.h"

bool UModumateClientCursor::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!IconImage)
	{
		return false;
	}

	return true;
}

void UModumateClientCursor::InitCursor(const FLinearColor& InClientColor, const FLinearColor& InBackgroundColor)
{
	ClientColor = InClientColor;
	BackgroundColor = InBackgroundColor;

	DynamicCursorMaterial = UMaterialInstanceDynamic::Create(CursorMaterial, this);
	IconImage->SetBrushFromMaterial(DynamicCursorMaterial);
	DynamicCursorMaterial->SetVectorParameterValue(MaterialClientColorParam, ClientColor);
	DynamicCursorMaterial->SetVectorParameterValue(MaterialBackgroundColorParam, BackgroundColor);

	ULocalPlayer* localPlayer = GetWorld() ? GetWorld()->GetFirstLocalPlayerFromController() : nullptr;
	APlayerController* localController = localPlayer ? localPlayer->GetPlayerController(GetWorld()) : nullptr;
	EditModelPlayerController = localController ? Cast<AEditModelPlayerController>(localController) : nullptr;
}

bool UModumateClientCursor::UpdateCursorOcclusion(const FVector& WorldLocation)
{
	if (!EditModelPlayerController)
	{
		return false;
	}

	// Line trace to detect any block from the camera to cursor world location
	FVector camPos = EditModelPlayerController->PlayerCameraManager->GetCameraLocation();
	FHitResult lineHitResult;
	bool bLineHit = EditModelPlayerController->LineTraceSingleAgainstMOIs(lineHitResult, camPos, WorldLocation);

	// Ignore hit with thin objects
	if (bLineHit)
	{
		float hitDist = FVector::Distance(lineHitResult.Location, WorldLocation);
		bLineHit = hitDist > OcclusionThreshold;
	}

	IconImage->SetRenderOpacity(bLineHit ? OccludedColorAlpha : 1.f);

	return true;
}

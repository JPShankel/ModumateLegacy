// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "ModumateCore/ModumateBrowserStatics.h"

#include "Objects/ModumateObjectInstance.h"
#include "Online/ModumateAnalyticsStatics.h"
#include "UnrealClasses/EditModelGameState.h"
#include "Objects/CameraView.h"

bool UModumateBrowserStatics::CreateCameraViewAsMoi(UObject* WorldContextObject, UCameraComponent *CameraComp, const FString &CameraViewName, const FDateTime &TimeOfDay, int32 CameraViewIndex /*= INDEX_NONE*/)
{
	UWorld *world = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	AEditModelGameState *gameState = world ? Cast<AEditModelGameState>(world->GetGameState()) : nullptr;
	if (gameState == nullptr || CameraComp == nullptr)
	{
		return false;
	}

	FMOICameraViewData newCameraViewData;
	newCameraViewData.Position = CameraComp->GetComponentLocation();
	newCameraViewData.Rotation = CameraComp->GetComponentQuat();
	newCameraViewData.FOV = CameraComp->FieldOfView;
	newCameraViewData.AspectRatio = CameraComp->AspectRatio;
	newCameraViewData.Name = CameraViewName;
	newCameraViewData.bOrthoView = CameraComp->ProjectionMode == ECameraProjectionMode::Orthographic;
	newCameraViewData.MoiId = gameState->Document->GetNextAvailableID();
	newCameraViewData.CameraViewIndex = CameraViewIndex;
	newCameraViewData.SavedTime = TimeOfDay.ToString();

	FMOIStateData stateData(gameState->Document->GetNextAvailableID(), EObjectType::OTCameraView);
	stateData.CustomData.SaveStructData(newCameraViewData);

	auto delta = MakeShared<FMOIDelta>();
	delta->AddCreateDestroyState(stateData, EMOIDeltaType::Create);
	bool bSuccess = gameState->Document->ApplyDeltas({ delta }, WorldContextObject->GetWorld());

	static const FString analyticsEventName(TEXT("SaveCameraView"));
	UModumateAnalyticsStatics::RecordEventSimple(WorldContextObject, EModumateAnalyticsCategory::View, analyticsEventName);

	return bSuccess;
}

bool UModumateBrowserStatics::UpdateCameraViewAsMoi(UObject* WorldContextObject, UCameraComponent* CameraComp, int32 CameraMoiId, const FDateTime& TimeOfDay)
{
	UWorld* world = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	AEditModelGameState* gameState = world ? Cast<AEditModelGameState>(world->GetGameState()) : nullptr;
	if (gameState == nullptr || CameraComp == nullptr)
	{
		return false;
	}

	if (gameState)
	{
		AModumateObjectInstance* moi = gameState->Document->GetObjectById(CameraMoiId);
		FMOIStateData oldStateData = moi->GetStateData();
		FMOIStateData newStateData = oldStateData;

		FMOICameraViewData newCameraViewData;
		newStateData.CustomData.LoadStructData(newCameraViewData);
		newCameraViewData.Position = CameraComp->GetComponentLocation();
		newCameraViewData.Rotation = CameraComp->GetComponentQuat();
		newCameraViewData.FOV = CameraComp->FieldOfView;
		newCameraViewData.AspectRatio = CameraComp->AspectRatio;
		newCameraViewData.bOrthoView = CameraComp->ProjectionMode == ECameraProjectionMode::Orthographic;
		newCameraViewData.SavedTime = TimeOfDay.ToString();

		newStateData.CustomData.SaveStructData<FMOICameraViewData>(newCameraViewData);

		auto delta = MakeShared<FMOIDelta>();
		delta->AddMutationState(moi, oldStateData, newStateData);
		gameState->Document->ApplyDeltas({ delta }, world);

		return true;
	}

	return false;
}

bool UModumateBrowserStatics::RemoveCameraViewMoi(UObject* WorldContextObject, int32 CameraMoiId)
{
	UWorld *world = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	AEditModelGameState *gameState = world ? Cast<AEditModelGameState>(world->GetGameState()) : nullptr;
	if (gameState == nullptr)
	{
		return false;
	}

	const AModumateObjectInstance* cameraMoi = gameState->Document->GetObjectById(CameraMoiId);
	if (cameraMoi && cameraMoi->GetObjectType() == EObjectType::OTCameraView)
	{
		gameState->Document->DeleteObjects(TArray<int32>{ CameraMoiId });
		return true;
	}
	return false;
}

bool UModumateBrowserStatics::EditCameraViewName(UObject* WorldContextObject, int32 CameraMoiId, const FString &NewCameraViewName)
{
	UWorld *world = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	AEditModelGameState *gameState = world ? Cast<AEditModelGameState>(world->GetGameState()) : nullptr;
	if (gameState == nullptr)
	{
		return false;
	}

	if (gameState)
	{
		AModumateObjectInstance* moi = gameState->Document->GetObjectById(CameraMoiId);
		FMOIStateData oldStateData = moi->GetStateData();
		FMOIStateData newStateData = oldStateData;

		FMOICameraViewData newCameraViewData;
		newStateData.CustomData.LoadStructData(newCameraViewData);
		newCameraViewData.Name = NewCameraViewName;
		newStateData.CustomData.SaveStructData<FMOICameraViewData>(newCameraViewData);

		auto delta = MakeShared<FMOIDelta>();
		delta->AddMutationState(moi, oldStateData, newStateData);
		gameState->Document->ApplyDeltas({ delta }, world);

		return true;
	}

	return false;
}

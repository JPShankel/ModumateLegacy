// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "ModumateCore/ModumateBrowserStatics.h"

#include "Objects/ModumateObjectInstance.h"
#include "Online/ModumateAnalyticsStatics.h"
#include "DocumentManagement/ModumateDocument.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/AxesActor.h"
#include "UI/EditModelUserWidget.h"
#include "UI/ViewCubeWidget.h"

bool UModumateBrowserStatics::CreateCameraViewAsMoi(UObject* WorldContextObject, UCameraComponent *CameraComp, const FString &CameraViewName, const FDateTime &TimeOfDay, int32 CameraViewIndex /*= INDEX_NONE*/)
{
	UWorld *world = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	auto controller = world->GetFirstPlayerController<AEditModelPlayerController>();
	if (controller == nullptr || CameraComp == nullptr)
	{
		return false;
	}

	FMOICameraViewData newCameraViewData;
	UpdateCameraViewData(WorldContextObject, CameraComp, newCameraViewData, TimeOfDay);
	newCameraViewData.Name = CameraViewName;
	newCameraViewData.MoiId = controller->GetDocument()->GetNextAvailableID();
	newCameraViewData.CameraViewIndex = CameraViewIndex;

	FMOIStateData stateData(controller->GetDocument()->GetNextAvailableID(), EObjectType::OTCameraView);
	stateData.CustomData.SaveStructData(newCameraViewData);

	auto delta = MakeShared<FMOIDelta>();
	delta->AddCreateDestroyState(stateData, EMOIDeltaType::Create);
	bool bSuccess = controller->GetDocument()->ApplyDeltas({ delta }, WorldContextObject->GetWorld());

	static const FString analyticsEventName(TEXT("SaveCameraView"));
	UModumateAnalyticsStatics::RecordEventSimple(WorldContextObject, EModumateAnalyticsCategory::View, analyticsEventName);

	return bSuccess;
}

bool UModumateBrowserStatics::UpdateCameraViewAsMoi(UObject* WorldContextObject, UCameraComponent* CameraComp, int32 CameraMoiId, const FDateTime& TimeOfDay)
{
	UWorld* world = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	auto controller = world->GetFirstPlayerController<AEditModelPlayerController>();
	if (controller == nullptr || CameraComp == nullptr)
	{
		return false;
	}

	if (controller)
	{
		AModumateObjectInstance* moi = controller->GetDocument()->GetObjectById(CameraMoiId);
		FMOIStateData oldStateData = moi->GetStateData();
		FMOIStateData newStateData = oldStateData;

		FMOICameraViewData newCameraViewData;
		newStateData.CustomData.LoadStructData(newCameraViewData);
		UpdateCameraViewData(WorldContextObject, CameraComp, newCameraViewData, TimeOfDay);
		newStateData.CustomData.SaveStructData<FMOICameraViewData>(newCameraViewData);

		auto delta = MakeShared<FMOIDelta>();
		delta->AddMutationState(moi, oldStateData, newStateData);
		controller->GetDocument()->ApplyDeltas({ delta }, world);

		return true;
	}

	return false;
}

bool UModumateBrowserStatics::RemoveCameraViewMoi(UObject* WorldContextObject, int32 CameraMoiId)
{
	UWorld *world = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	auto controller = world->GetFirstPlayerController<AEditModelPlayerController>();
	if (controller == nullptr)
	{
		return false;
	}

	const AModumateObjectInstance* cameraMoi = controller->GetDocument()->GetObjectById(CameraMoiId);
	if (cameraMoi && cameraMoi->GetObjectType() == EObjectType::OTCameraView)
	{
		controller->GetDocument()->DeleteObjects(TArray<int32>{ CameraMoiId });
		return true;
	}
	return false;
}

bool UModumateBrowserStatics::EditCameraViewName(UObject* WorldContextObject, int32 CameraMoiId, const FString &NewCameraViewName)
{
	UWorld *world = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	auto controller = world->GetFirstPlayerController<AEditModelPlayerController>();
	if (controller == nullptr)
	{
		return false;
	}

	if (controller)
	{
		AModumateObjectInstance* moi = controller->GetDocument()->GetObjectById(CameraMoiId);
		FMOIStateData oldStateData = moi->GetStateData();
		FMOIStateData newStateData = oldStateData;

		FMOICameraViewData newCameraViewData;
		newStateData.CustomData.LoadStructData(newCameraViewData);
		newCameraViewData.Name = NewCameraViewName;
		newStateData.CustomData.SaveStructData<FMOICameraViewData>(newCameraViewData);

		auto delta = MakeShared<FMOIDelta>();
		delta->AddMutationState(moi, oldStateData, newStateData);
		controller->GetDocument()->ApplyDeltas({ delta }, world);

		return true;
	}

	return false;
}

bool UModumateBrowserStatics::UpdateCameraViewData(UObject* WorldContextObject, UCameraComponent* CameraComp, FMOICameraViewData& CameraViewData, const FDateTime& TimeOfDay)
{
	UWorld* world = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	auto controller = world->GetFirstPlayerController<AEditModelPlayerController>();
	if (controller == nullptr || CameraComp == nullptr)
	{
		return false;
	}

	bool bNewAxisVisibility = true;
	if (controller->AxesActor)
	{
		bNewAxisVisibility = !controller->AxesActor->IsHidden();
	}

	bool bNewViewCubeVisibility = true;
	if (controller->EditModelUserWidget->ViewCubeUserWidget && controller->EditModelUserWidget->ViewCubeUserWidget)
	{
		bNewViewCubeVisibility = controller->EditModelUserWidget->ViewCubeUserWidget->IsVisible();
	}

	CameraViewData.Position = CameraComp->GetComponentLocation();
	CameraViewData.Rotation = CameraComp->GetComponentQuat();
	CameraViewData.FOV = CameraComp->FieldOfView;
	CameraViewData.AspectRatio = CameraComp->AspectRatio;
	CameraViewData.bOrthoView = CameraComp->ProjectionMode == ECameraProjectionMode::Orthographic;
	CameraViewData.SavedTime = TimeOfDay.ToString();
	CameraViewData.bAxesActorVisibility = bNewAxisVisibility;
	CameraViewData.bViewCubeVisibility = bNewViewCubeVisibility;

	return true;
}

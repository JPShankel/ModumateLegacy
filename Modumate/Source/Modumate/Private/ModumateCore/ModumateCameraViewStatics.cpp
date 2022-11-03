// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "ModumateCore/ModumateCameraViewStatics.h"

#include "Objects/ModumateObjectInstance.h"
#include "Online/ModumateAnalyticsStatics.h"
#include "DocumentManagement/ModumateDocument.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/AxesActor.h"
#include "Objects/DesignOption.h"
#include "UI/EditModelUserWidget.h"
#include "UI/ViewCubeWidget.h"
#include "Objects/ModumateObjectStatics.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "ModumateCore/ModumateRayTracingSettings.h"
#include "UnrealClasses/EditModelPlayerPawn.h"
#include "UnrealClasses/SkyActor.h"
#include "UnrealClasses/EditModelGameState.h"

bool UModumateCameraViewStatics::CreateCameraViewAsMoi(UObject* WorldContextObject, UCameraComponent *CameraComp, const FString &CameraViewName, const FDateTime &TimeOfDay, int32& NextID, int32 CameraViewIndex /*= INDEX_NONE*/)
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
	newCameraViewData.MoiId = NextID++;
	newCameraViewData.CameraViewIndex = CameraViewIndex;

	FMOIStateData stateData(newCameraViewData.MoiId, EObjectType::OTCameraView);
	stateData.CustomData.SaveStructData(newCameraViewData);

	auto delta = MakeShared<FMOIDelta>();
	delta->AddCreateDestroyState(stateData, EMOIDeltaType::Create);
	bool bSuccess = controller->GetDocument()->ApplyDeltas({ delta }, WorldContextObject->GetWorld());

	static const FString analyticsEventName(TEXT("SaveCameraView"));
	UModumateAnalyticsStatics::RecordEventSimple(WorldContextObject, EModumateAnalyticsCategory::View, analyticsEventName);

	return bSuccess;
}

bool UModumateCameraViewStatics::UpdateCameraViewAsMoi(UObject* WorldContextObject, UCameraComponent* CameraComp, int32 CameraMoiId, const FDateTime& TimeOfDay)
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

bool UModumateCameraViewStatics::RemoveCameraViewMoi(UObject* WorldContextObject, int32 CameraMoiId)
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

bool UModumateCameraViewStatics::EditCameraViewName(UObject* WorldContextObject, int32 CameraMoiId, const FString &NewCameraViewName)
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

bool UModumateCameraViewStatics::UpdateCameraViewData(UObject* WorldContextObject, UCameraComponent* CameraComp, FMOICameraViewData& CameraViewData, const FDateTime& TimeOfDay)
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

	CameraViewData.SavedCullingCutPlane = controller->CurrentCullingCutPlaneID;
	CameraViewData.Position = CameraComp->GetComponentLocation();
	CameraViewData.Rotation = CameraComp->GetComponentQuat();
	CameraViewData.FOV = CameraComp->FieldOfView;
	CameraViewData.AspectRatio = CameraComp->AspectRatio;
	CameraViewData.bOrthoView = CameraComp->ProjectionMode == ECameraProjectionMode::Orthographic;
	CameraViewData.OrthoWidth = CameraComp->OrthoWidth;
	CameraViewData.SavedTime = TimeOfDay.ToIso8601();
	CameraViewData.bAxesActorVisibility = bNewAxisVisibility;
	CameraViewData.bViewCubeVisibility = bNewViewCubeVisibility;

	CameraViewData.SavedVisibleDesignOptions.Empty();
	TArray<AMOIDesignOption*> designOptions;
	Algo::Transform(controller->GetDocument()->GetObjectsOfType(EObjectType::OTDesignOption),
		designOptions,
		[](AModumateObjectInstance* MOI) {return Cast<AMOIDesignOption>(MOI); }
	);
	for (auto* designOption : designOptions)
	{
		if (ensure(designOption) && designOption->InstanceData.isShowing)
		{
			CameraViewData.SavedVisibleDesignOptions.Add(designOption->ID);
		}
	}

	CameraViewData.SavedCutPlaneVisibilities.Empty();
	TArray<AModumateObjectInstance*> cpMois = controller->GetDocument()->GetObjectsOfType(EObjectType::OTCutPlane);
	for (const auto& curCp : cpMois)
	{
		CameraViewData.SavedCutPlaneVisibilities.Add(curCp->ID, curCp->IsVisible());
	}

	auto bRTEnabledCVAR = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing.Shadows"));
	if (bRTEnabledCVAR != nullptr && bRTEnabledCVAR->GetInt() == 1)
	{
		CameraViewData.bRTEnabled = true;
	}
	else {
		CameraViewData.bRTEnabled = false;
	}
	AEditModelPlayerState* playerState = controller->GetPlayerState<AEditModelPlayerState>();
	if (playerState == nullptr)
	{
		return false;
	}
	CameraViewData.rayTracingQuality = playerState->RayTracingQuality;
	CameraViewData.rayTracingExposure = playerState->RayTracingExposure;
	CameraViewData.bShowLights = playerState->bShowLights;

	UModumateObjectStatics::UpdateDesignOptionVisibility(controller->GetDocument());
	return true;
}
bool UModumateCameraViewStatics::ActivateCameraView(UObject* WorldContextObject, FMOICameraViewData CameraView)
{
	UWorld* world = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (world == nullptr)
	{
		return false;
	}
	auto controller = world->GetFirstPlayerController<AEditModelPlayerController>();
	if (controller == nullptr)
	{
		return false;
	}
	APawn* pawn = controller->GetPawn();
	if (pawn)
	{
		// Position and rotation
		pawn->SetActorLocationAndRotation(CameraView.Position, CameraView.Rotation);

		// Projection mode
		controller->EMPlayerPawn->SetCameraOrtho(CameraView.bOrthoView);
		
		// FOV and ortho width
		UCameraComponent* cameraComp = controller->GetViewTarget()->FindComponentByClass<UCameraComponent>();
		if (cameraComp)
		{
			controller->SetFieldOfViewCommand(CameraView.FOV);
			cameraComp->SetOrthoWidth(CameraView.OrthoWidth);
		}

		//Ray Tracing Enabled
		UModumateRayTracingSettings* RTSettings = NewObject<UModumateRayTracingSettings>();
		APostProcessVolume* ppv = Cast<APostProcessVolume>(UGameplayStatics::GetActorOfClass(world, APostProcessVolume::StaticClass()));
		AEditModelPlayerState* playerState = controller->GetPlayerState<AEditModelPlayerState>();
		if (playerState == nullptr)
		{
			return false;
		}
		if (ppv != nullptr && RTSettings != nullptr)
		{
			RTSettings->Init();
			RTSettings->SetRayTracingEnabled(ppv, CameraView.bRTEnabled);
			RTSettings->ApplyRayTraceQualitySettings(ppv, CameraView.rayTracingQuality);
			RTSettings->SetExposure(ppv, CameraView.rayTracingExposure);
			playerState->RayTracingExposure = CameraView.rayTracingExposure;
			playerState->RayTracingQuality = CameraView.rayTracingQuality;
		}
		playerState->bShowLights = CameraView.bShowLights;
		DirtyLightMOIs(world);
		// Time
		FDateTime newDateTime;
		if (FDateTime::ParseIso8601(*CameraView.SavedTime, newDateTime))
		{
			controller->SkyActor->SetCurrentDateTime(newDateTime);
		}

		// Axis actor
		if (controller->AxesActor)
		{
			controller->AxesActor->SetActorHiddenInGame(!CameraView.bAxesActorVisibility);
		}

		// Viewcube
		if (controller->EditModelUserWidget->ViewCubeUserWidget && controller->EditModelUserWidget->ViewCubeUserWidget)
		{
			controller->EditModelUserWidget->ViewCubeUserWidget->SetVisibility(CameraView.bViewCubeVisibility ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
		}

		// Update cutplane culling
		controller->SetCurrentCullingCutPlane(CameraView.SavedCullingCutPlane, false);

		// Update cutplane visibility
		TArray<int32> visibleCPs;
		TArray<int32> hiddenCPs;
		for (const auto& kvp : CameraView.SavedCutPlaneVisibilities)
		{
			if (kvp.Value)
			{
				visibleCPs.Add(kvp.Key);
			}
			else
			{
				hiddenCPs.Add(kvp.Key);
			}
		}

		playerState->AddHideObjectsById(hiddenCPs);
		playerState->UnhideObjectsById(visibleCPs);
		controller->GetDocument()->OnCameraViewSelected(CameraView.MoiId);
	}
	return true;
}

void UModumateCameraViewStatics::DirtyLightMOIs(UObject* WorldContextObject)
{
	UWorld* world = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	AEditModelGameState* gameState = world != nullptr ? world->GetGameState<AEditModelGameState>() : nullptr;
	
	if (gameState == nullptr)
	{
		return;
	}
	UModumateDocument* doc = gameState->Document;
	if (doc == nullptr)
	{
		return;
	}
	TArray<AModumateObjectInstance*> mois = doc->GetObjectsOfType(EObjectType::OTFurniture);
	for (AModumateObjectInstance* moi : mois)
	{
		if (moi != nullptr)
		{
			const FBIMPresetInstance* preset = doc->GetPresetCollection().PresetFromGUID(moi->GetStateData().AssemblyGUID);
			if (preset && preset->HasCustomData<FLightConfiguration>())
			{
				moi->MarkDirty(EObjectDirtyFlags::Structure);
				moi->CleanObject(EObjectDirtyFlags::Structure, nullptr);
			}
		}
	}
}
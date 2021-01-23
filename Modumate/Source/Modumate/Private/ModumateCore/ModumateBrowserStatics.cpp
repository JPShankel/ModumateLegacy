// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "ModumateCore/ModumateBrowserStatics.h"

#include "Objects/ModumateObjectInstance.h"
#include "Online/ModumateAnalyticsStatics.h"
#include "UnrealClasses/EditModelGameState.h"

using namespace Modumate;

bool UModumateBrowserStatics::GetCutPlanesFromProject(UObject* WorldContextObject, TArray<FCutPlaneParamBlueprint> &OutCutPlaneParams)
{
	OutCutPlaneParams.Reset();

	UWorld *world = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	AEditModelGameState *gameState = world ? Cast<AEditModelGameState>(world->GetGameState()) : nullptr;
	if (gameState == nullptr)
	{
		return false;
	}

	bool bSuccess = true;
	TArray<AModumateObjectInstance*> cutPlaneObjs = gameState->Document->GetObjectsOfType(EObjectType::OTCutPlane);
	for (auto *cutPlaneObj : cutPlaneObjs)
	{
		auto &cutPlaneParam = OutCutPlaneParams.AddDefaulted_GetRef();
		cutPlaneParam.ObjectID = cutPlaneObj->ID;
		cutPlaneParam.bVisiblity = cutPlaneObj->IsVisible();
		cutPlaneParam.Location = cutPlaneObj->GetLocation();
		cutPlaneParam.Normal = cutPlaneObj->GetNormal();
		
		// TODO: Copy other parameters?
		//cutPlaneParam.DisplayName;
		//cutPlaneParam.Key;
		//cutPlaneParam.Size;
	}

	return bSuccess;
}

bool UModumateBrowserStatics::GetScopeBoxesFromProject(UObject* WorldContextObject, TArray<FScopeBoxParamBlueprint> &OutScopeBoxParams)
{
	OutScopeBoxParams.Reset();

	UWorld *world = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	AEditModelGameState *gameState = world ? Cast<AEditModelGameState>(world->GetGameState()) : nullptr;
	if (gameState == nullptr)
	{
		return false;
	}

	bool bSuccess = true;
	TArray<AModumateObjectInstance*> scopeBoxObjs = gameState->Document->GetObjectsOfType(EObjectType::OTScopeBox);
	for (auto *scopeBoxObj : scopeBoxObjs)
	{
		auto &scopeBoxParam = OutScopeBoxParams.AddDefaulted_GetRef();
		scopeBoxParam.ObjectID = scopeBoxObj->ID;
		scopeBoxParam.bVisiblity = scopeBoxObj->IsVisible();
		// TODO: re-implement scope box data
		//scopeBoxParam.Location = scopeBoxObj->GetLocation();
		//scopeBoxParam.Extent = scopeBoxObj->GetExtents();

		// TODO: Copy other parameters?
		//scopeBoxParam.DisplayName;
		//scopeBoxParam.Key;
	}

	return bSuccess;
}

bool UModumateBrowserStatics::SetDrawingObjectName(UObject* WorldContextObject, int32 ObjectID, const FString &NewName)
{
	// TODO: Set name here
	return true;
}

bool UModumateBrowserStatics::SetDrawingObjectVisibility(UObject* WorldContextObject, int32 ObjectID, bool Visible)
{
	UWorld *world = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	AEditModelGameState *gameState = world ? Cast<AEditModelGameState>(world->GetGameState()) : nullptr;
	if (gameState == nullptr)
	{
		return false;
	}

	AModumateObjectInstance *drawingObj = gameState->Document->GetObjectById(ObjectID);
	if (drawingObj == nullptr)
	{
		return false;
	}

	// TODO: JI, you can place the hide cut plane or scrope box logic below
	return true;
}

bool UModumateBrowserStatics::ToggleAllCutPlaneVisibility(UObject* WorldContextObject)
{
	UWorld *world = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	AEditModelGameState *gameState = world ? Cast<AEditModelGameState>(world->GetGameState()) : nullptr;
	if (gameState == nullptr)
	{
		return false;
	}

	// TODO: Toggle all cut planes here

	return true;
}

bool UModumateBrowserStatics::ToggleAllScopeBoxesVisibility(UObject* WorldContextObject)
{
	UWorld *world = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	AEditModelGameState *gameState = world ? Cast<AEditModelGameState>(world->GetGameState()) : nullptr;
	if (gameState == nullptr)
	{
		return false;
	}

	// TODO: Toggle all scope boxes here

	return true;
}

FVector UModumateBrowserStatics::GetDrawingOriginFromProject(UObject* WorldContextObject)
{
	// TODO: Get project origin from document
	return FVector::ZeroVector;
}

FVector UModumateBrowserStatics::GetDrawingXDirectionFromProject(UObject* WorldContextObject)
{
	// TODO: Get project direction from document
	return FVector(0.f, -1.f, 0.f);
}

bool UModumateBrowserStatics::GetCameraViewsFromDoc(UObject* WorldContextObject, TArray<FModumateCameraView> &OutCameraViews)
{
	UWorld *world = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	AEditModelGameState *gameState = world ? Cast<AEditModelGameState>(world->GetGameState()) : nullptr;
	if (gameState == nullptr)
	{
		return false;
	}
	OutCameraViews = gameState->Document->SavedCameraViews;
	return true;
}

bool UModumateBrowserStatics::SaveCameraView(UObject* WorldContextObject, UCameraComponent *CameraComp, const FString &CameraViewName, const FDateTime &TimeOfDay, int32 CameraViewIndex /*= -1*/)
{
	UWorld *world = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	AEditModelGameState *gameState = world ? Cast<AEditModelGameState>(world->GetGameState()) : nullptr;
	if (gameState == nullptr || CameraComp == nullptr)
	{
		return false;
	}

	FModumateCameraView newCameraView;
	newCameraView.Position = CameraComp->GetComponentLocation();
	newCameraView.Rotation = CameraComp->GetComponentQuat();
	newCameraView.FOV = CameraComp->FieldOfView;
	newCameraView.AspectRatio = CameraComp->AspectRatio;
	newCameraView.Name = CameraViewName;
	newCameraView.TimeOfDay = TimeOfDay;

	if (gameState->Document->SavedCameraViews.IsValidIndex(CameraViewIndex))
	{
		gameState->Document->SavedCameraViews[CameraViewIndex] = newCameraView;
	}
	else
	{
		gameState->Document->SavedCameraViews.Add(newCameraView);
	}

	static const FString analyticsEventName(TEXT("SaveCameraView"));
	UModumateAnalyticsStatics::RecordEventSimple(WorldContextObject, UModumateAnalyticsStatics::EventCategoryView, analyticsEventName);

	return true;
}

bool UModumateBrowserStatics::RemoveCameraView(UObject* WorldContextObject, int32 CameraViewIndex)
{
	UWorld *world = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	AEditModelGameState *gameState = world ? Cast<AEditModelGameState>(world->GetGameState()) : nullptr;
	if (gameState == nullptr)
	{
		return false;
	}

	if (gameState->Document->SavedCameraViews.IsValidIndex(CameraViewIndex))
	{
		gameState->Document->SavedCameraViews.RemoveAt(CameraViewIndex);
		return true;
	}
	return false;
}

bool UModumateBrowserStatics::EditCameraViewName(UObject* WorldContextObject, int32 CameraViewIndex, const FString &NewCameraViewName)
{
	UWorld *world = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	AEditModelGameState *gameState = world ? Cast<AEditModelGameState>(world->GetGameState()) : nullptr;
	if (gameState == nullptr)
	{
		return false;
	}

	if (gameState->Document->SavedCameraViews.IsValidIndex(CameraViewIndex))
	{
		gameState->Document->SavedCameraViews[CameraViewIndex].Name = NewCameraViewName;
		return true;
	}
	return false;
}

bool UModumateBrowserStatics::ReorderCameraViews(UObject* WorldContextObject, int32 From, int32 To)
{
	UWorld *world = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	AEditModelGameState *gameState = world ? Cast<AEditModelGameState>(world->GetGameState()) : nullptr;
	if (gameState == nullptr)
	{
		return false;
	}

	if (gameState->Document->SavedCameraViews.IsValidIndex(From) &&
		(gameState->Document->SavedCameraViews.IsValidIndex(To)))
	{
		FModumateCameraView moveCV = gameState->Document->SavedCameraViews[From];
		gameState->Document->SavedCameraViews.RemoveAt(From);
		gameState->Document->SavedCameraViews.Insert(moveCV, To);
		return true;
	}
	return false;
}

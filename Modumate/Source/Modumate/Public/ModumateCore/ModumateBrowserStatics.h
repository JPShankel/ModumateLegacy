// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UObject/Object.h"
#include "Objects/CameraView.h"
#include "ModumateBrowserStatics.generated.h"

// Helper functions for accessing / editing browser menu related data.
UCLASS(BlueprintType)
class MODUMATE_API UModumateBrowserStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// Save CameraView to Doc. Default CameraViewIndex will save CameraView to the end of the list
	UFUNCTION()
	static bool CreateCameraViewAsMoi(UObject* WorldContextObject, UCameraComponent *CameraComp, const FString &CameraViewName, const FDateTime &TimeOfDay, int32 CameraViewIndex = INDEX_NONE);
	
	UFUNCTION()
	static bool UpdateCameraViewAsMoi(UObject* WorldContextObject, UCameraComponent* CameraComp, int32 CameraMoiId, const FDateTime& TimeOfDay);
	
	UFUNCTION()
	static bool RemoveCameraViewMoi(UObject* WorldContextObject, int32 CameraMoiId);
	
	UFUNCTION()
	static bool EditCameraViewName(UObject* WorldContextObject, int32 CameraMoiId, const FString &NewCameraViewName);

	UFUNCTION()
	static bool UpdateCameraViewData(UObject* WorldContextObject, UCameraComponent* CameraComp, FMOICameraViewData& CameraViewData, const FDateTime& TimeOfDay);
};

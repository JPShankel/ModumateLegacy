// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UObject/Object.h"
#include "DocumentManagement/ModumateCameraView.h"
#include "ModumateBrowserStatics.generated.h"

USTRUCT(BlueprintType)
struct FCutPlaneParamBlueprint
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CutPlane")
	FName Key;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CutPlane")
	int32 ObjectID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CutPlane")
	FString DisplayName = FString(TEXT("New Cut Plane"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CutPlane")
	bool bVisiblity = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CutPlane")
	FVector Location;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CutPlane")
	FVector Normal;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CutPlane")
	FVector Size;

	// If true, menu will start initial behavior such as user naming process
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CutPlane")
	bool bRecentlyCreated = false;
};

USTRUCT(BlueprintType)
struct FScopeBoxParamBlueprint
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ScopeBox")
	FName Key;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ScopeBox")
	int32 ObjectID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ScopeBox")
	FString DisplayName = FString(TEXT("New Scope Box"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ScopeBox")
	bool bVisiblity = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ScopeBox")
	FVector Location;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ScopeBox")
	FVector Extent;

	// If true, menu will start initial behavior such as user naming process
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ScopeBox")
	bool bRecentlyCreated = false;
};

// Helper functions for accessing / editing browser menu related data.
UCLASS(BlueprintType)
class MODUMATE_API UModumateBrowserStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "Modumate | Browser")
	static bool GetCutPlanesFromProject(UObject* WorldContextObject, TArray<FCutPlaneParamBlueprint> &OutCutPlaneParams);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "Modumate | Browser")
	static bool GetScopeBoxesFromProject(UObject* WorldContextObject, TArray<FScopeBoxParamBlueprint> &OutScopeBoxParams);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "Modumate | Browser")
	static bool SetDrawingObjectName(UObject* WorldContextObject, int32 ObjectID, const FString &NewName);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "Modumate | Browser")
	static bool SetDrawingObjectVisibility(UObject* WorldContextObject, int32 ObjectID, bool Visible);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "Modumate | Browser")
	static bool ToggleAllCutPlaneVisibility(UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "Modumate | Browser")
	static bool ToggleAllScopeBoxesVisibility(UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"), Category = "Modumate | Browser")
	static FVector GetDrawingOriginFromProject(UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"), Category = "Modumate | Browser")
	static FVector GetDrawingXDirectionFromProject(UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "Modumate | Browser")
	static	bool GetCameraViewsFromDoc(UObject* WorldContextObject, TArray<FModumateCameraView> &OutCameraViews);

	// Save CameraView to Doc. Default CameraViewIndex will save CameraView to the end of the list
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "Modumate | Browser")
	static bool SaveCameraView(UObject* WorldContextObject, UCameraComponent *CameraComp, const FString &CameraViewName, const FDateTime &TimeOfDay, int32 CameraViewIndex = -1);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "Modumate | Browser")
	static bool RemoveCameraView(UObject* WorldContextObject, int32 CameraViewIndex);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "Modumate | Browser")
	static bool EditCameraViewName(UObject* WorldContextObject, int32 CameraViewIndex, const FString &NewCameraViewName);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "Modumate | Browser")
	static bool ReorderCameraViews(UObject* WorldContextObject, int32 From, int32 To);
};

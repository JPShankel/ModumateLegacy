// Copyright 2021 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Objects/PlaneBase.h"

#include "CameraView.generated.h"

USTRUCT(BlueprintType)
struct MODUMATE_API FMOICameraViewData
{
	GENERATED_BODY()

	UPROPERTY()
	int32 MoiId = INDEX_NONE;

	UPROPERTY()
	int32 CameraViewIndex = INDEX_NONE;

	UPROPERTY()
	int32 SavedCullingCutPlane = INDEX_NONE;

	UPROPERTY()
	TArray<int32> SavedVisibleDesignOptions;

	UPROPERTY()
	TMap<int32, bool> SavedCutPlaneVisibilities; // MoiID, IsVisible

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CameraView")
	FVector Position = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CameraView")
	FQuat Rotation = FQuat::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CameraView")
	float FOV = 50.f; // Default Modumate camera fov

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CameraView")
	float AspectRatio = 1.778f; // Default UE4 camera aspect ratio

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CameraView")
	float OrthoWidth = 512;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CameraView")
	FString Name = FString(TEXT("NewCameraView"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CameraView")
	bool bOrthoView = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CameraView")
	FString SavedTime;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CameraView")
	bool bAxesActorVisibility = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CameraView")
	bool bViewCubeVisibility = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CameraView")
	bool bCutPlanesColorVisibility = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CameraView")
	bool bGraphDirectionVisibility = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CameraView")
	bool bRTEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CameraView")
	int32 rayTracingExposure = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CameraView")
	int32 rayTracingQuality = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CameraView")
	bool bShowLights = false;
};

UCLASS()
class MODUMATE_API AMOICameraView : public AMOIPlaneBase
{
	GENERATED_BODY()

public:
	AMOICameraView();

	virtual AActor* CreateActor(const FVector& loc, const FQuat& rot) override;
	virtual void PostCreateObject(bool bNewObject) override;
	virtual bool GetUpdatedVisuals(bool& bOutVisible, bool& bOutCollisionEnabled) override;
	virtual bool ToWebMOI(FWebMOI& OutMOI) const override;
	virtual bool FromWebMOI(const FString& InJson) override;

	UPROPERTY()
	FMOICameraViewData InstanceData;

	void UpdateViewMenu();
	void UpdateCameraPosition();

protected:

	UFUNCTION()
	void OnCameraActorDestroyed(AActor* DestroyedActor);
};

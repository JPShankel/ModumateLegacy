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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CameraView")
	FVector Position = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CameraView")
	FQuat Rotation = FQuat::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CameraView")
	float FOV = 50.f; // Default Modumate camera fov

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CameraView")
	float AspectRatio = 1.778f; // Default UE4 camera aspect ratio

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CameraView")
	FString Name = FString(TEXT("NewCameraView"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CameraView")
	bool bOrthoView = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CameraView")
	FString SavedTime;
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

	UPROPERTY()
	FMOICameraViewData InstanceData;

	void UpdateViewMenu();

protected:

	UFUNCTION()
	void OnCameraActorDestroyed(AActor* DestroyedActor);
};

// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "EditModelCameraController.generated.h"

UENUM()
enum class ECameraMovementState : uint8
{
	Default,
	Orbiting,
	Panning,
	Flying,
};

UCLASS()
class MODUMATE_API UEditModelCameraController : public UActorComponent
{
	GENERATED_BODY()

public:
	UEditModelCameraController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	void Init();

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float RotateSpeed;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float OrbitZoomPercentSpeed;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float OrbitZoomMinStepDist;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float OrbitZoomMinDistance;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	class UCurveFloat* OrbitMovementLerpCurve;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float OrbitMaxPitch;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float FreeZoomMinStepDist;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float FreeZoomInteriorDist;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float FreeZoomExponent;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bUseSmoothZoom;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float SmoothZoomSpeed;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	class UStaticMesh *OrbitAnchorMesh;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float OrbitAnchorScreenSize;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float FlyingSpeed;

	UFUNCTION(BlueprintPure)
	ECameraMovementState GetMovementState() const { return CurMovementState; }

protected:
	// Begin input binding functions

	UFUNCTION()
	void OnActionOrbitPressed();

	UFUNCTION()
	void OnActionOrbitReleased();

	UFUNCTION()
	void OnActionPanPressed();

	UFUNCTION()
	void OnActionPanReleased();

	UFUNCTION()
	void OnActionZoomIn();

	UFUNCTION()
	void OnActionZoomOut();

	UFUNCTION()
	void OnAxisRotateYaw(float RotateYawValue);

	UFUNCTION()
	void OnAxisRotatePitch(float RotatePitchValue);

	UFUNCTION()
	void OnAxisMoveForward(float PanForwardValue);

	UFUNCTION()
	void OnAxisMoveRight(float PanRightValue);

	UFUNCTION()
	void OnAxisMoveUp(float PanUpValue);

	UFUNCTION()
	void OnMouseLeave(FIntPoint MousePos);

	// End input binding functions

	bool SetMovementState(ECameraMovementState NewMovementState);
	void SetOrbiting(bool bNewOrbiting);
	void SetPanning(bool bNewPanning);
	void OnZoom(float ZoomSign);
	void OnMoveValue(FVector LocalMoveValue);
	void UpdateFreeZooming(float DeltaTime);
	void UpdateRotating(float DeltaTime);
	void UpdateOrbiting(float DeltaTime);
	void UpdatePanning(float DeltaTime);
	void UpdateFlying(float DeltaTime);
	bool GetRealMouseCursorInViewport(FIntPoint &OutPosition);
	bool TryWrapCursor(FIntPoint CurCursorPos);
	void UpdateOrbitAnchorScale();

	UPROPERTY()
	class AEditModelPlayerController_CPP *Controller;

	UPROPERTY()
	class UCameraComponent *Camera;

	UPROPERTY()
	class UModumateViewportClient *ViewportClient;

	UPROPERTY()
	class AStaticMeshActor *OrbitAnchorActor;

	ECameraMovementState CurMovementState;

	FVector OrbitTarget, OrbitStartProxyTarget, PanOriginTarget, FreeZoomTargetDelta;
	FIntPoint PanLastMousePos;
	FVector2D PanFactors;
	float OrbitZoomDistance, OrbitZoomTargetDistance, OrbitZoomDeltaAccumulated;
	float OrbitMovementElapsed;
	FVector2D RotationDeltasAccumulated;
	FVector FlyingDeltasAccumulated;
	FTransform CamTransform;
};

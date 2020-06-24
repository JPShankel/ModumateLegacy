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
	Retargeting,
};

UCLASS()
class MODUMATE_API UEditModelCameraController : public UActorComponent
{
	GENERATED_BODY()

public:
	UEditModelCameraController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ToolTip = "The speed of rotation, in degrees / second, during Orbiting and Flying that's applied by the yaw and pitch input axes"))
	float RotateSpeed;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ToolTip = "Whether to smoothly approach the intended zoom distance during Orbiting and Default movement, rather than snap directly to it"))
	bool bUseSmoothZoom;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ToolTip = "If bUseSmoothZoom is enabled, the % / second of desired zoom distance to travel each frame"))
	float SmoothZoomSpeed;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ToolTip = "For each zoom in/out action, how far do we want to zoom as a percent of the distance to the zoom target"))
	float ZoomPercentSpeed;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ToolTip = "The minimum desired zoom distance, in cm, if possible"))
	float ZoomMinStepDist;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ToolTip = "The minimum distance to the zoom target allowed, after which we are no longer able to zoom closer"))
	float ZoomMinDistance;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ToolTip = "The maximum total distance from the origin we are allowed to be zoomed out"))
	float ZoomMaxTotalDistance;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ToolTip = "The function of OrbitMovementElapsed to how close to OrbitTarget from OrbitStartProxyTarget we should be orbiting"))
	class UCurveFloat* OrbitMovementLerpCurve;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ToolTip = "The maximum pitch, in degrees, that we allow the camera to reach while orbiting"))
	float OrbitMaxPitch;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ToolTip = "The range of (absolute value of) camera pitch values between which the orbit center will choose between the center and bottom of the screen"))
	FVector2D OrbitDriftPitchRange;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ToolTip = "When orbiting around a position towards the bottom of the screen, the percentage of the viewport height we should clamp to"))
	float OrbitCursorMaxHeightPCT;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ToolTip = "The static mesh that is used to show the orbit target anchor"))
	class UStaticMesh *OrbitAnchorMesh;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ToolTip = "The desired screen size of the orbit anchor"))
	float OrbitAnchorScreenSize;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ToolTip = "A multiplier on flight speed, subject to the flying movement variables in EditModelPlayerPawn"))
	float FlyingSpeed;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ToolTip = "While retargeting, the new transform that the camera should lerp to"))
	FTransform NewTargetTransform;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ToolTip = "The amount of time that it takes for the camera to reach its destination while in the Retargeting movement state"))
	float RetargetingDuration;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ToolTip = "The exponent of the retargeting easing function while retargeting"))
	float RetargetingEaseExp;

	UFUNCTION(BlueprintPure)
	ECameraMovementState GetMovementState() const { return CurMovementState; }

	bool ZoomToProjectExtents();
	bool ZoomToSelection();

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
	void OnAxisMoveForward(float MoveForwardValue);

	UFUNCTION()
	void OnAxisMoveRight(float MoveRightValue);

	UFUNCTION()
	void OnAxisMoveUp(float MoveUpValue);

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
	void UpdateRetargeting(float DeltaTime);
	bool GetRealMouseCursorInViewport(FIntPoint &OutPosition);
	bool TryWrapCursor(FIntPoint CurCursorPos);
	void UpdateOrbitAnchorScale();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	class AEditModelPlayerController_CPP *Controller;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	class UModumateViewportClient *ViewportClient;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	class AStaticMeshActor *OrbitAnchorActor;

	ECameraMovementState CurMovementState;

	// The world-space position about which we intend to orbit after clicking on a target, if we're in ECameraMovementState::Orbiting
	FVector OrbitTarget;

	// The world-space position about which we are actually rotating, which approaches OrbitTarget in order to smooth the camera
	FVector OrbitStartProxyTarget;

	// Between each frame, the amount of positional offset in world space that we want to zoom, while in ECameraMovementState::Default
	FVector FreeZoomDeltaAccumulated;

	// The previous position of the mouse, in viewport-space
	FIntPoint PanLastMousePos;

	// The X and Y cm/px factors to us while panning, which are established when panning starts
	// These factors would need to be re-calculated if the screen size, FOV, or zoom level changed while panning
	FVector2D PanFactors;

	// The current actual distance from OrbitTarget that we are placing the camera at, if we're in ECameraMovementState::Orbiting
	float OrbitZoomDistance;

	// The target distance from OrbitTarget that we are placing the camera at, which is approached by OrbitZoomDistance if bUseSmoothZoom is true
	float OrbitZoomTargetDistance;

	// The amount of rotational movement that has elapsed during the start of the current orbit, which allows OrbitStartProxyTarget to approach OrbitTarget
	float OrbitMovementElapsed;

	// Between each frame, the amount of distance (in percent of distance to the orbit target) that we want to zoom, while in ECameraMovementState::Orbiting
	float OrbitZoomDeltaAccumulated;

	// While orbiting, this is the relative direction from the camera's position to the orbit target. If the orbit target is intended for the center of the screen,
	// then it would be FVector::ForwardVector, but if the orbit target is intended below the center, for example, then the Z value would be < 0.
	FVector OrbitRelativeDir;

	// Between each frame, the amount of yaw/pitch (in degrees) of movement that we want to apply to the camera, while in ECameraMovementState::Orbiting
	FVector2D RotationDeltasAccumulated;

	// Between each frame, the amount of movement input that we want to apply to the camera's location while in ECameraMovementState::Flying
	// X and Y are relative to the camera's forward and right vectors, Z is in world space
	FVector FlyingDeltasAccumulated;

	// The camera transform when we start to set a new target to interpolate to
	FTransform PreRetargetTransform;

	// The time that we have spent so far in the Retargeting movement state
	float RetargetingTimeElapsed;

	// The current world-space transform of the camera (and pawn, meant to be identical), which is modified during a frame to be the new intended transform
	FTransform CamTransform;
};

// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/EditModelCameraController.h"

#include "Framework/Application/SlateApplication.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelInputHandler.h"
#include "UnrealClasses/EditModelPlayerPawn_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "Engine/StaticMeshActor.h"
#include "Kismet/GameplayStatics.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "UnrealClasses/ModumateViewportClient.h"
#include "Slate/SceneViewport.h"
#include "UnrealClient.h"

UEditModelCameraController::UEditModelCameraController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, RotateSpeed(5.0f)
	, bUseSmoothZoom(true)
	, SmoothZoomSpeed(25.0f)
	, ZoomPercentSpeed(0.2f)
	, ZoomMinStepDist(10.0f)
	, ZoomMinDistance(20.0f)
	, ZoomMaxTotalDistance(525600.0f)
	, OrbitMovementLerpCurve(nullptr)
	, OrbitMaxPitch(88.0f)
	, OrbitDriftPitchRange(20.0f, 70.0f)
	, OrbitCursorMaxHeightPCT(0.9f)
	, OrbitAnchorScreenSize(0.25f)
	, FlyingSpeed(15.0f)
	, Controller(nullptr)
	, CurMovementState(ECameraMovementState::Default)
	, OrbitTarget(ForceInitToZero)
	, OrbitStartProxyTarget(ForceInitToZero)
	, FreeZoomDeltaAccumulated(ForceInitToZero)
	, PanLastMousePos(ForceInitToZero)
	, PanFactors(ForceInitToZero)
	, OrbitZoomDistance(0.0f)
	, OrbitZoomTargetDistance(0.0f)
	, OrbitMovementElapsed(0.0f)
	, OrbitZoomDeltaAccumulated(0.0f)
	, OrbitRelativeDir(FVector::ForwardVector)
	, RotationDeltasAccumulated(ForceInitToZero)
	, FlyingDeltasAccumulated(ForceInitToZero)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UEditModelCameraController::BeginPlay()
{
	Super::BeginPlay();

	UWorld *world = GetWorld();
	Controller = Cast<AEditModelPlayerController_CPP>(GetOwner());
	ViewportClient = Cast<UModumateViewportClient>(GetWorld()->GetGameViewport());

	if (ensure(ViewportClient))
	{
		ViewportClient->OnMouseLeaveDelegate.AddUObject(this, &UEditModelCameraController::OnMouseLeave);
	}

	if (ensure(world && OrbitAnchorMesh))
	{
		FActorSpawnParameters spawnParams;
		spawnParams.Name = FName(TEXT("OrbitAnchorActor"));
		spawnParams.Owner = GetOwner();
		OrbitAnchorActor = world->SpawnActor<AStaticMeshActor>(spawnParams);

		OrbitAnchorActor->GetStaticMeshComponent()->SetStaticMesh(OrbitAnchorMesh);
		OrbitAnchorActor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
		OrbitAnchorActor->SetActorEnableCollision(false);
		OrbitAnchorActor->SetActorHiddenInGame(true);
	}
}

void UEditModelCameraController::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!Controller || !Controller->EMPlayerPawn)
	{
		return;
	}

	const FTransform oldTransform = Controller->EMPlayerPawn->GetActorTransform();
	CamTransform = oldTransform;

	switch (CurMovementState)
	{
	case ECameraMovementState::Default:
		UpdateFreeZooming(DeltaTime);
		break;
	case ECameraMovementState::Orbiting:
		UpdateOrbiting(DeltaTime);
		break;
	case ECameraMovementState::Panning:
		UpdatePanning(DeltaTime);
		break;
	case ECameraMovementState::Flying:
		UpdateFlying(DeltaTime);
		break;
	default:
		break;
	}

	// Clamp the camera position based on the maximum total zoom distance from the origin
	FVector curCamPos = CamTransform.GetLocation();
	FVector clampedCamPos = curCamPos.GetClampedToMaxSize(ZoomMaxTotalDistance);
	CamTransform.SetLocation(clampedCamPos);

	if (!CamTransform.EqualsNoScale(oldTransform))
	{
		Controller->EMPlayerPawn->SetActorLocationAndRotation(CamTransform.GetLocation(), CamTransform.GetRotation());
	}
}

void UEditModelCameraController::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	if (ensure(PlayerInputComponent))
	{
		static const FName orbitActionName(TEXT("CameraOrbit"));
		PlayerInputComponent->BindAction(orbitActionName, EInputEvent::IE_Pressed, this, &UEditModelCameraController::OnActionOrbitPressed);
		PlayerInputComponent->BindAction(orbitActionName, EInputEvent::IE_Released, this, &UEditModelCameraController::OnActionOrbitReleased);

		static const FName panActionName(TEXT("CameraPan"));
		PlayerInputComponent->BindAction(panActionName, EInputEvent::IE_Pressed, this, &UEditModelCameraController::OnActionPanPressed);
		PlayerInputComponent->BindAction(panActionName, EInputEvent::IE_Released, this, &UEditModelCameraController::OnActionPanReleased);

		PlayerInputComponent->BindAction(FName(TEXT("CameraZoomIn")), EInputEvent::IE_Pressed, this, &UEditModelCameraController::OnActionZoomIn);
		PlayerInputComponent->BindAction(FName(TEXT("CameraZoomOut")), EInputEvent::IE_Pressed, this, &UEditModelCameraController::OnActionZoomOut);

		PlayerInputComponent->BindAxis(FName(TEXT("MoveYaw")), this, &UEditModelCameraController::OnAxisRotateYaw);
		PlayerInputComponent->BindAxis(FName(TEXT("MovePitch")), this, &UEditModelCameraController::OnAxisRotatePitch);

		PlayerInputComponent->BindAxis(FName(TEXT("MoveForward")), this, &UEditModelCameraController::OnAxisMoveForward);
		PlayerInputComponent->BindAxis(FName(TEXT("MoveRight")), this, &UEditModelCameraController::OnAxisMoveRight);
		PlayerInputComponent->BindAxis(FName(TEXT("MoveUp")), this, &UEditModelCameraController::OnAxisMoveUp);
	}
}

void UEditModelCameraController::OnActionOrbitPressed()
{
	SetOrbiting(true);
}

void UEditModelCameraController::OnActionOrbitReleased()
{
	SetOrbiting(false);
}

void UEditModelCameraController::OnActionPanPressed()
{
	SetPanning(true);
}

void UEditModelCameraController::OnActionPanReleased()
{
	SetPanning(false);
}

void UEditModelCameraController::OnActionZoomIn()
{
	OnZoom(-1.0f);
}

void UEditModelCameraController::OnActionZoomOut()
{
	OnZoom(1.0f);
}

void UEditModelCameraController::OnAxisRotateYaw(float RotateYawValue)
{
	if ((RotateYawValue != 0.0f) &&
		((CurMovementState == ECameraMovementState::Orbiting) || (CurMovementState == ECameraMovementState::Flying)))
	{
		RotationDeltasAccumulated.X += RotateSpeed * RotateYawValue;
	}
}

void UEditModelCameraController::OnAxisRotatePitch(float RotatePitchValue)
{
	if ((RotatePitchValue != 0.0f) &&
		((CurMovementState == ECameraMovementState::Orbiting) || (CurMovementState == ECameraMovementState::Flying)))
	{
		RotationDeltasAccumulated.Y += RotateSpeed * RotatePitchValue;
	}
}

void UEditModelCameraController::OnAxisMoveForward(float MoveForwardValue)
{
	OnMoveValue(MoveForwardValue * FVector::ForwardVector);
}

void UEditModelCameraController::OnAxisMoveRight(float MoveRightValue)
{
	OnMoveValue(MoveRightValue * FVector::RightVector);
}

void UEditModelCameraController::OnAxisMoveUp(float MoveUpValue)
{
	OnMoveValue(MoveUpValue * FVector::UpVector);
}

void UEditModelCameraController::OnMouseLeave(FIntPoint MousePos)
{
	if (CurMovementState == ECameraMovementState::Panning)
	{
		TryWrapCursor(MousePos);
	}
}

bool UEditModelCameraController::SetMovementState(ECameraMovementState NewMovementState)
{
	if (CurMovementState == NewMovementState)
	{
		return false;
	}

	FVector camPos = CamTransform.GetLocation();
	FQuat camRot = CamTransform.GetRotation();
	bool bShouldInputHaveBeenDisabled = true;

	// End the previous movement state
	switch (CurMovementState)
	{
	case ECameraMovementState::Default:
	{
		bShouldInputHaveBeenDisabled = false;
		break;
	}
	case ECameraMovementState::Orbiting:
	{
		// Restore the mouse cursor to the screen position of the orbit target,
		// rather than its original screen position when orbiting started.
		FVector2D orbitTargetScreenPos;
		if (UGameplayStatics::ProjectWorldToScreen(Controller, OrbitTarget, orbitTargetScreenPos))
		{
			Controller->SetMouseLocation(orbitTargetScreenPos.X, orbitTargetScreenPos.Y);
		}

		OrbitTarget = FVector::ZeroVector;
		OrbitStartProxyTarget = FVector::ZeroVector;
		OrbitRelativeDir = FVector::ForwardVector;

		if (OrbitAnchorActor)
		{
			OrbitAnchorActor->SetActorHiddenInGame(true);
		}
		break;
	}
	case ECameraMovementState::Panning:
	{
		CurMovementState = ECameraMovementState::Default;
		PanLastMousePos = FIntPoint::ZeroValue;

		Controller->CurrentMouseCursor = EMouseCursor::Default;
		break;
	}
	case ECameraMovementState::Flying:
		break;
	}

	if (bShouldInputHaveBeenDisabled && Controller->InputHandlerComponent)
	{
		Controller->InputHandlerComponent->RequestInputDisabled(StaticClass()->GetFName(), false);
	}

	// Begin the new movement state
	const FSnappedCursor &cursor = Controller->EMPlayerState->SnappedCursor;
	bool bShouldInputBeDisabled = true;

	switch (NewMovementState)
	{
	case ECameraMovementState::Default:
	{
		bShouldInputBeDisabled = false;
		OrbitZoomDeltaAccumulated = 0.0f;
		break;
	}
	case ECameraMovementState::Orbiting:
	{
		if (Controller->IsCursorOverWidget() || !cursor.bValid)
		{
			return false;
		}

		OrbitTarget = cursor.WorldPosition;

		// Calculate the direction from the camera that points at or below the center of the screen, horizontally centered, towards the orbit target.
		// This direction is the one that can be used for orbiting around a point, rather than the forward vector, so it can be fixed about
		// a point in screen space that's not only the center of the screen.
		FVector2D viewportSize(ViewportClient->Viewport->GetSizeXY());

		// At the time that we start orbiting, we will cap the cursor height based on the pitch of the camera.
		// Below a given minimum pitch, we will cap it to a given percentage of the viewport height,
		// Below a given maximum pitch, we will cap it linearly towards the center of the viewport,
		// And above a maximum pitch, it will be set to the center of the viewport.
		FVector2D maxCursorHeightRange(FMath::Min(OrbitCursorMaxHeightPCT * viewportSize.Y, cursor.ScreenPosition.Y), viewportSize.Y / 2);
		float curCamPitch = FMath::Abs(camRot.Rotator().Pitch);
		float maxCursorHeight = FMath::GetMappedRangeValueClamped(OrbitDriftPitchRange, maxCursorHeightRange, curCamPitch);
		FVector2D centeredCursorPos(viewportSize.X / 2, FMath::Max(viewportSize.Y / 2, maxCursorHeight));

		FVector orbitCamWorldOrigin, orbitCamWorldDir;
		Controller->DeprojectScreenPositionToWorld(centeredCursorPos.X, centeredCursorPos.Y, orbitCamWorldOrigin, orbitCamWorldDir);
		OrbitRelativeDir = camRot.UnrotateVector(orbitCamWorldDir);

		// Determine the proxy target, from which we will determine the intended orbit distance and interpolate towards the true OrbitTarget,
		// based on the intersection of the centered orbit direction and the plane on which the orbit target lies.
		OrbitStartProxyTarget = FMath::RayPlaneIntersection(camPos, orbitCamWorldDir, FPlane(OrbitTarget, -orbitCamWorldDir));

		OrbitZoomDistance = OrbitZoomTargetDistance = FVector::Dist(OrbitStartProxyTarget, camPos);
		OrbitMovementElapsed = 0.0f;
		OrbitZoomDeltaAccumulated = 0.0f;
		RotationDeltasAccumulated = FVector2D::ZeroVector;

		if (OrbitAnchorActor)
		{
			OrbitAnchorActor->SetActorTransform(FTransform(FQuat::Identity, OrbitTarget));
			OrbitAnchorActor->SetActorHiddenInGame(false);
			UpdateOrbitAnchorScale();
		}
		break;
	}
	case ECameraMovementState::Panning:
	{
		if (Controller->IsCursorOverWidget() || !cursor.bValid)
		{
			return false;
		}

		FVector panOriginTarget = cursor.WorldPosition;
		PanLastMousePos = cursor.ScreenPosition.IntPoint();

		FVector2D screenPosOrigin, offsetScreenPosRight, offsetScreenPosUp, screenOffsets;
		if (Controller->ProjectWorldLocationToScreen(panOriginTarget, screenPosOrigin) &&
			Controller->ProjectWorldLocationToScreen(panOriginTarget + camRot.GetRightVector(), offsetScreenPosRight) &&
			Controller->ProjectWorldLocationToScreen(panOriginTarget + camRot.GetUpVector(), offsetScreenPosUp))
		{
			screenOffsets.X = offsetScreenPosRight.X - screenPosOrigin.X;
			screenOffsets.Y = offsetScreenPosUp.Y - screenPosOrigin.Y;
			PanFactors = FVector2D(1.0f, 1.0f) / screenOffsets;
		}

		Controller->CurrentMouseCursor = EMouseCursor::GrabHandClosed;
		break;
	}
	case ECameraMovementState::Flying:
	{
		FlyingDeltasAccumulated = FVector::ZeroVector;
		RotationDeltasAccumulated = FVector2D::ZeroVector;
		break;
	}
	default:
		break;
	}

	CurMovementState = NewMovementState;

	if (bShouldInputBeDisabled && Controller->InputHandlerComponent)
	{
		Controller->InputHandlerComponent->RequestInputDisabled(StaticClass()->GetFName(), true);
	}

	return true;
}

void UEditModelCameraController::SetOrbiting(bool bNewOrbiting)
{
	if (bNewOrbiting)
	{
		SetMovementState(ECameraMovementState::Orbiting);
	}
	else if ((CurMovementState == ECameraMovementState::Orbiting) || (CurMovementState == ECameraMovementState::Flying))
	{
		SetMovementState(ECameraMovementState::Default);
	}
}

void UEditModelCameraController::SetPanning(bool bNewPanning)
{
	if (bNewPanning && (CurMovementState == ECameraMovementState::Default))
	{
		SetMovementState(ECameraMovementState::Panning);
	}
	else if (CurMovementState == ECameraMovementState::Panning)
	{
		SetMovementState(ECameraMovementState::Default);
	}
}

void UEditModelCameraController::OnZoom(float ZoomSign)
{
	if (!ensure(Controller))
	{
		return;
	}

	switch (CurMovementState)
	{
	case ECameraMovementState::Default:
	{
		if (!Controller->EMPlayerState)
		{
			return;
		}

		// Don't allow zooming when hovering over a widget, or without a valid world-space target
		const FSnappedCursor &cursor = Controller->EMPlayerState->SnappedCursor;
		if (Controller->IsCursorOverWidget() || !cursor.bValid)
		{
			return;
		}

		// Don't allow zooming closer than MinOrbitDistance
		// Zoom as if the current camera transform with the intended zoom delta is the origin,
		// to be consistent between smooth zooming and not-smooth zooming with quick zoom actions.
		const FVector origin = CamTransform.GetLocation() + FreeZoomDeltaAccumulated;
		const FVector target = cursor.WorldPosition;
		const FVector deltaToTarget = target - origin;
		const float distToTarget = deltaToTarget.Size();
		if (FMath::IsNearlyZero(distToTarget))
		{
			return;
		}
		const FVector dirToTarget = deltaToTarget / distToTarget;

		// Zoom closer to/further from the target by ZoomDeltaAccumulated percent
		// Rather than adding/subtracting ZoomPercent directly from the current distance, invert the percentage change
		// so that zooming in and out cover the same distance.
		float distPercentChange = ((ZoomSign < 0.0f) ? (1.0f / (1.0f + ZoomPercentSpeed)) : (1.0f + ZoomPercentSpeed)) - 1.0f;
		float zoomDistDelta = distPercentChange * distToTarget;

		// Zoom in distances at least ZoomMinStepDist in size
		zoomDistDelta = FMath::Sign(zoomDistDelta) * FMath::Max(FMath::Abs(zoomDistDelta), ZoomMinStepDist);

		// Accumulate the free zoom target delta
		FVector newZoomDelta = FreeZoomDeltaAccumulated - (zoomDistDelta * dirToTarget);

		// But don't zoom closer than ZoomMinDistance to the target if we're zooming in
		const FVector accumulatedGoalPos = origin + newZoomDelta;
		const FVector accumulatedDeltaFromTarget = target - accumulatedGoalPos;
		const float accumulatedDistFromTarget = accumulatedDeltaFromTarget | dirToTarget;
		if ((accumulatedDistFromTarget < ZoomMinDistance) && (ZoomSign < 0.0f))
		{
			newZoomDelta = (target - ZoomMinDistance * dirToTarget) - origin;
			float newZoomForwardDist = newZoomDelta | dirToTarget;
			if (newZoomForwardDist < 0.0f)
			{
				newZoomDelta = FVector::ZeroVector;
			}
		}

		FreeZoomDeltaAccumulated = newZoomDelta;

		break;
	}
	case ECameraMovementState::Orbiting:
	{
		OrbitZoomDeltaAccumulated += ZoomSign * ZoomPercentSpeed;
		break;
	}
	default:
		break;
	}
}

void UEditModelCameraController::OnMoveValue(FVector LocalMoveValue)
{
	if (!LocalMoveValue.IsNearlyZero())
	{
		if (CurMovementState == ECameraMovementState::Orbiting)
		{
			SetMovementState(ECameraMovementState::Flying);
		}

		FlyingDeltasAccumulated += LocalMoveValue;
	}
}

void UEditModelCameraController::UpdateFreeZooming(float DeltaTime)
{
	if (FreeZoomDeltaAccumulated.IsNearlyZero())
	{
		return;
	}

	// If smooth zooming, use the desired zoom delta over time; otherwise use it all immediately
	float zoomLerpAlpha = bUseSmoothZoom ? FMath::Clamp(SmoothZoomSpeed * DeltaTime, 0.0f, 1.0f) : 1.0f;
	FVector curZoomDelta = FMath::Lerp(FVector::ZeroVector, FreeZoomDeltaAccumulated, zoomLerpAlpha);
	if (curZoomDelta.IsNearlyZero())
	{
		return;
	}

	CamTransform.SetLocation(CamTransform.GetLocation() + curZoomDelta);
	FreeZoomDeltaAccumulated *= (1.0f - zoomLerpAlpha);
}

void UEditModelCameraController::UpdateRotating(float DeltaTime)
{
	if (RotationDeltasAccumulated.IsZero())
	{
		return;
	}

	const FRotator camRotator = CamTransform.GetRotation().Rotator();

	// Accumulate camera yaw
	FRotator newCamRotator = camRotator;
	newCamRotator.Yaw = camRotator.Yaw + RotationDeltasAccumulated.X;

	// Accumulate camera pitch
	newCamRotator.Pitch = FMath::Clamp(camRotator.Pitch + RotationDeltasAccumulated.Y, -OrbitMaxPitch, OrbitMaxPitch);

	// Constrain camera roll, in case it became non-zero during Rotator <-> Quat conversions
	newCamRotator.Roll = 0.0f;

	CamTransform.SetRotation(FQuat(newCamRotator));
	RotationDeltasAccumulated = FVector2D::ZeroVector;
}

void UEditModelCameraController::UpdateOrbiting(float DeltaTime)
{
	float newOrbitMovement = RotationDeltasAccumulated.Size();

	// Update rotation so that we can adjust the target based on that
	UpdateRotating(DeltaTime);

	const FVector camPos = CamTransform.GetLocation();
	const FVector orbitDir = CamTransform.GetRotation().RotateVector(OrbitRelativeDir);
	
	// Accumulate camera distance

	// Make the zoom distance delta at least MinOrbitZoomStepDist in size,
	// but don't allow zooming closer than MinOrbitDistance

	if (!FMath::IsNearlyZero(OrbitZoomDeltaAccumulated))
	{
		float distPercentChange = ((OrbitZoomDeltaAccumulated < 0.0f) ? (1.0f / (1.0f - OrbitZoomDeltaAccumulated)) : (1.0f + OrbitZoomDeltaAccumulated)) - 1.0f;
		float distDelta = distPercentChange * OrbitZoomTargetDistance;

		distDelta = FMath::Sign(distDelta) * FMath::Max(FMath::Abs(distDelta), ZoomMinStepDist);
		OrbitZoomTargetDistance = FMath::Max(OrbitZoomTargetDistance + distDelta, ZoomMinDistance);
		OrbitZoomDeltaAccumulated = 0.0f;
	}

	// If smooth zooming, approach the desired zoom distance over time; otherwise snap to it
	float zoomLerpAlpha = bUseSmoothZoom ? SmoothZoomSpeed * DeltaTime : 1.0f;
	OrbitZoomDistance = FMath::Lerp(OrbitZoomDistance, OrbitZoomTargetDistance, zoomLerpAlpha);

	// Accumulate orbit lerp distance progress
	float orbitLerpProgress = OrbitMovementLerpCurve ? FMath::Clamp(OrbitMovementLerpCurve->GetFloatValue(OrbitMovementElapsed), 0.0f, 1.0f) : 1.0f;
	OrbitMovementElapsed += newOrbitMovement;

	// Lerp camera position based on original position and orbiting position
	FVector lerpedOrbitTarget = FMath::Lerp(OrbitStartProxyTarget, OrbitTarget, orbitLerpProgress);
	FVector newCamPos = lerpedOrbitTarget - (OrbitZoomDistance * orbitDir);

	// Set the final orbit camera transform
	CamTransform.SetLocation(newCamPos);

	// And don't forget, update the orbit anchor to be the right size
	UpdateOrbitAnchorScale();
}

void UEditModelCameraController::UpdatePanning(float DeltaTime)
{
	FIntPoint curMousePos;
	if (!GetRealMouseCursorInViewport(curMousePos))
	{
		return;
	}

	EWindowMode::Type curWindowMode = ViewportClient->Viewport->GetWindowMode();
	if ((curWindowMode != EWindowMode::Windowed) || ViewportClient->GetWindow()->IsWindowMaximized())
	{
		if (TryWrapCursor(curMousePos))
		{
			return;
		}
	}

	if (PanLastMousePos != curMousePos)
	{
		FIntPoint mouseDelta = curMousePos - PanLastMousePos;
		FVector2D worldCamDelta = -FVector2D(mouseDelta) * PanFactors;

		const FVector camPos = CamTransform.GetLocation();
		const FQuat camRot = CamTransform.GetRotation();

		FVector newCamPos = camPos + (worldCamDelta.X * camRot.GetRightVector()) + (worldCamDelta.Y * camRot.GetUpVector());
		CamTransform.SetLocation(newCamPos);

		PanLastMousePos = curMousePos;
	}
}

void UEditModelCameraController::UpdateFlying(float DeltaTime)
{
	UpdateRotating(DeltaTime);

	if (!FlyingDeltasAccumulated.IsZero())
	{
		FVector camPos = CamTransform.GetLocation();
		FQuat camRot = CamTransform.GetRotation();

		FVector flyingInputX = camRot.GetAxisX() * FlyingDeltasAccumulated.X;
		FVector flyingInputY = camRot.GetAxisY() * FlyingDeltasAccumulated.Y;
		FVector flyingInputZ = FVector::UpVector * FlyingDeltasAccumulated.Z;
		FVector flyingInput = flyingInputX + flyingInputY + flyingInputZ;
		Controller->EMPlayerPawn->AddMovementInput(flyingInput, FlyingSpeed);

		FlyingDeltasAccumulated = FVector::ZeroVector;
	}
}

bool UEditModelCameraController::GetRealMouseCursorInViewport(FIntPoint &OutPosition)
{
	// Ask Slate (who in turn asks the OS) for the actual mouse cursor position,
	// since the viewport's cursor position is delayed.
	// The delay breaks deltas while the mouse cursor wraps around the edges of the viewport.
	FVector2D realCurMousePos = FSlateApplication::Get().GetCursorPos();
	FSceneViewport *sceneViewport = static_cast<FSceneViewport*>(ViewportClient->Viewport);
	FVector2D localCursorPos = sceneViewport->GetCachedGeometry().AbsoluteToLocal(realCurMousePos);
	OutPosition = localCursorPos.IntPoint();

	return true;
}

bool UEditModelCameraController::TryWrapCursor(FIntPoint CurCursorPos)
{
	FIntPoint viewportSize = ViewportClient->Viewport->GetSizeXY();
	FIntPoint newMousePos = CurCursorPos;

	for (int32 axisIdx = 0; axisIdx < 2; ++axisIdx)
	{
		int32 axisSize = viewportSize(axisIdx);
		int32 curPosValue = CurCursorPos(axisIdx);

		if ((curPosValue <= 0) || (curPosValue >= (axisSize - 1)))
		{
			newMousePos(axisIdx) = (curPosValue + axisSize) % axisSize;
		}
	}

	if (newMousePos != CurCursorPos)
	{
		Controller->SetMouseLocation(newMousePos.X, newMousePos.Y);
		PanLastMousePos = newMousePos;
		return true;
	}

	return false;
}

void UEditModelCameraController::UpdateOrbitAnchorScale()
{
	if (OrbitAnchorActor)
	{
		UStaticMeshComponent *orbitAnchorMeshComp = OrbitAnchorActor->GetStaticMeshComponent();
		FVector adjustedScale = UModumateFunctionLibrary::GetWorldComponentToScreenSizeScale(orbitAnchorMeshComp, FVector(OrbitAnchorScreenSize));
		orbitAnchorMeshComp->SetWorldScale3D(adjustedScale);
	}
}

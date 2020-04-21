// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "EditModelCameraController.h"

#include "Application/SlateApplication.h"
#include "Components/StaticMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "EditModelPlayerController_CPP.h"
#include "EditModelPlayerPawn_CPP.h"
#include "EditModelPlayerState_CPP.h"
#include "Engine/StaticMeshActor.h"
#include "Kismet/GameplayStatics.h"
#include "ModumateFunctionLibrary.h"
#include "ModumateViewportClient.h"
#include "Slate/SceneViewport.h"
#include "UnrealClient.h"

UEditModelCameraController::UEditModelCameraController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, RotateSpeed(5.0f)
	, OrbitZoomPercentSpeed(0.2f)
	, OrbitZoomMinStepDist(10.0f)
	, OrbitZoomMinDistance(10.0f)
	, OrbitMovementLerpCurve(nullptr)
	, OrbitMaxPitch(88.0f)
	, FreeZoomMinStepDist(50.0f)
	, FreeZoomInteriorDist(1000.0f)
	, FreeZoomExponent(1.2f)
	, bUseSmoothZoom(true)
	, SmoothZoomSpeed(100.0f)
	, OrbitAnchorScreenSize(0.25f)
	, FlyingSpeed(15.0f)
	, Controller(nullptr)
	, CurMovementState(ECameraMovementState::Default)
	, OrbitTarget(ForceInitToZero)
	, OrbitStartProxyTarget(ForceInitToZero)
	, PanOriginTarget(ForceInitToZero)
	, FreeZoomTargetDelta(ForceInitToZero)
	, PanLastMousePos(ForceInitToZero)
	, PanFactors(ForceInitToZero)
	, OrbitZoomDistance(0.0f)
	, OrbitZoomTargetDistance(0.0f)
	, OrbitZoomDeltaAccumulated(0.0f)
	, OrbitMovementElapsed(0.0f)
	, RotationDeltasAccumulated(ForceInitToZero)
	, FlyingDeltasAccumulated(ForceInitToZero)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UEditModelCameraController::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// TODO: only update this when it's changed, but it's owned by Blueprint at the moment so we don't know when that can happen.
	Camera = Controller->EMPlayerPawn->GetEditCameraComponent();
	if (!ensure(Camera))
	{
		return;
	}

	const FTransform oldTransform = Camera->GetComponentToWorld();
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

	if (!CamTransform.EqualsNoScale(oldTransform))
	{
		Camera->SetWorldLocationAndRotation(CamTransform.GetLocation(), CamTransform.GetRotation());
	}
}

void UEditModelCameraController::Init()
{
	UWorld *world = GetWorld();
	Controller = Cast<AEditModelPlayerController_CPP>(GetOwner());
	ViewportClient = Cast<UModumateViewportClient>(GetWorld()->GetGameViewport());

	if (ensureAlways(Controller && Controller->InputComponent && ViewportClient))
	{
		static const FName orbitActionName(TEXT("CameraOrbit"));
		Controller->InputComponent->BindAction(orbitActionName, EInputEvent::IE_Pressed, this, &UEditModelCameraController::OnActionOrbitPressed);
		Controller->InputComponent->BindAction(orbitActionName, EInputEvent::IE_Released, this, &UEditModelCameraController::OnActionOrbitReleased);

		static const FName panActionName(TEXT("CameraPan"));
		Controller->InputComponent->BindAction(panActionName, EInputEvent::IE_Pressed, this, &UEditModelCameraController::OnActionPanPressed);
		Controller->InputComponent->BindAction(panActionName, EInputEvent::IE_Released, this, &UEditModelCameraController::OnActionPanReleased);

		Controller->InputComponent->BindAction(FName(TEXT("CameraZoomIn")), EInputEvent::IE_Pressed, this, &UEditModelCameraController::OnActionZoomIn);
		Controller->InputComponent->BindAction(FName(TEXT("CameraZoomOut")), EInputEvent::IE_Pressed, this, &UEditModelCameraController::OnActionZoomOut);

		Controller->InputComponent->BindAxis(FName(TEXT("CameraOrbitYaw")), this, &UEditModelCameraController::OnAxisRotateYaw);
		Controller->InputComponent->BindAxis(FName(TEXT("CameraOrbitPitch")), this, &UEditModelCameraController::OnAxisRotatePitch);

		Controller->InputComponent->BindAxis(FName(TEXT("MoveForward")), this, &UEditModelCameraController::OnAxisMoveForward);
		Controller->InputComponent->BindAxis(FName(TEXT("MoveRight")), this, &UEditModelCameraController::OnAxisMoveRight);
		Controller->InputComponent->BindAxis(FName(TEXT("MoveUp")), this, &UEditModelCameraController::OnAxisMoveUp);

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
	if ((CurMovementState == NewMovementState) || !ensure(Camera))
	{
		return false;
	}

	FVector camPos = CamTransform.GetLocation();
	FQuat camRot = CamTransform.GetRotation();

	// End the previous movement state
	switch (CurMovementState)
	{
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

		if (OrbitAnchorActor)
		{
			OrbitAnchorActor->SetActorHiddenInGame(true);
		}
		break;
	}
	case ECameraMovementState::Panning:
	{
		CurMovementState = ECameraMovementState::Default;
		PanOriginTarget = FVector::ZeroVector;
		PanLastMousePos = FIntPoint::ZeroValue;

		Controller->CurrentMouseCursor = EMouseCursor::Default;
		break;
	}
	case ECameraMovementState::Flying:
		break;
	}

	// Begin the new movement state
	const FSnappedCursor &cursor = Controller->EMPlayerState->SnappedCursor;

	switch (NewMovementState)
	{
	case ECameraMovementState::Default:
		break;
	case ECameraMovementState::Orbiting:
	{
		if (Controller->IsCursorOverWidget() || !cursor.bValid)
		{
			return false;
		}

		OrbitTarget = cursor.WorldPosition;

		FVector camFwd = camRot.GetForwardVector();
		if (FMath::IsNearlyZero(camFwd.Z))
		{
			OrbitStartProxyTarget = OrbitTarget;
		}
		else
		{
			OrbitStartProxyTarget = FMath::RayPlaneIntersection(camPos, camFwd, FPlane(FVector::UpVector, OrbitTarget.Z));
		}

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

		PanOriginTarget = cursor.WorldPosition;
		ViewportClient->Viewport->GetMousePos(PanLastMousePos, true);

		FVector2D screenPosOrigin, offsetScreenPosRight, offsetScreenPosUp, screenOffsets;
		if (Controller->ProjectWorldLocationToScreen(PanOriginTarget, screenPosOrigin) &&
			Controller->ProjectWorldLocationToScreen(PanOriginTarget + camRot.GetRightVector(), offsetScreenPosRight) &&
			Controller->ProjectWorldLocationToScreen(PanOriginTarget + camRot.GetUpVector(), offsetScreenPosUp))
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
	if (!ensure(Camera))
	{
		return;
	}

	switch (CurMovementState)
	{
	case ECameraMovementState::Orbiting:
	{
		// For orbiting, zooming just changes the distance 
		OrbitZoomDeltaAccumulated += ZoomSign * OrbitZoomPercentSpeed;
		break;
	}
	case ECameraMovementState::Default:
	{
		// For free-zooming, accumulate remaining distance to travel along the axis pointed to by the cursor,
		// increasing the zoom step exponentially as the distance from the project increases.
		FVector camPos = CamTransform.GetLocation();

		FVector cursorWorldPos, cursorWorldDir;
		if (!Controller->DeprojectMousePositionToWorld(cursorWorldPos, cursorWorldDir))
		{
			return;
		}

		// TODO: have a cached project bounds, so we can use zoom distance relative to the outside of the project, rather than the origin.
		float zoomTotalDist = FVector::Distance(camPos, FVector::ZeroVector);
		float zoomExteriorDist = zoomTotalDist - FreeZoomInteriorDist;
		float zoomExpBase = FMath::Max(1.0f, zoomExteriorDist / FreeZoomInteriorDist);
		float zoomStepDist = -ZoomSign * FreeZoomMinStepDist * FMath::Pow(zoomExpBase, FreeZoomExponent);

		FreeZoomTargetDelta += zoomStepDist * cursorWorldDir;
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
	if (FreeZoomTargetDelta.IsNearlyZero())
	{
		return;
	}

	// If smooth zooming, use the desired zoom delta over time; otherwise use it all immediately
	float zoomLerpAlpha = bUseSmoothZoom ? SmoothZoomSpeed * DeltaTime : 1.0f;
	FVector curZoomDelta = FMath::Lerp(FVector::ZeroVector, FreeZoomTargetDelta, zoomLerpAlpha);
	if (curZoomDelta.IsNearlyZero())
	{
		return;
	}

	CamTransform.SetLocation(CamTransform.GetLocation() + curZoomDelta);
	FreeZoomTargetDelta -= curZoomDelta;
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
	// First, figure out how much orbit movement has just happen, so we can use it to lerp towards the real target
	const float zoomProgressFactor = 20.0f;
	float newOrbitMovement = RotationDeltasAccumulated.Size() + zoomProgressFactor * FMath::Abs(OrbitZoomDeltaAccumulated);

	// Update rotation so that we can adjust the target based on that
	UpdateRotating(DeltaTime);

	const FVector camPos = CamTransform.GetLocation();
	const FVector newCamFwd = CamTransform.GetRotation().GetForwardVector();
	
	// Accumulate camera distance

	// Make the zoom distance delta at least MinOrbitZoomStepDist in size,
	// but don't allow zooming closer than MinOrbitDistance
	float distDelta = OrbitZoomDeltaAccumulated * OrbitZoomTargetDistance;
	distDelta = FMath::Sign(distDelta) * FMath::Max(FMath::Abs(distDelta), OrbitZoomMinStepDist);
	OrbitZoomTargetDistance = FMath::Max(OrbitZoomTargetDistance + distDelta, OrbitZoomMinDistance);
	OrbitZoomDeltaAccumulated = 0.0f;

	// If smooth zooming, approach the desired zoom distance over time; otherwise snap to it
	float zoomLerpAlpha = bUseSmoothZoom ? SmoothZoomSpeed * DeltaTime : 1.0f;
	OrbitZoomDistance = FMath::Lerp(OrbitZoomDistance, OrbitZoomTargetDistance, zoomLerpAlpha);

	// Accumulate orbit lerp distance progress
	float orbitLerpProgress = OrbitMovementLerpCurve ? FMath::Clamp(OrbitMovementLerpCurve->GetFloatValue(OrbitMovementElapsed), 0.0f, 1.0f) : 1.0f;
	OrbitMovementElapsed += newOrbitMovement;

	// Lerp camera position based on original position and orbiting position
	FVector lerpedOrbitTarget = FMath::Lerp(OrbitStartProxyTarget, OrbitTarget, orbitLerpProgress);
	FVector newCamPos = lerpedOrbitTarget - (OrbitZoomDistance * newCamFwd);

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
		FVector worldFlyingDelta = FlyingSpeed * camRot.RotateVector(FlyingDeltasAccumulated);

		CamTransform.SetLocation(camPos + worldFlyingDelta);
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

		if (CurCursorPos(axisIdx) <= 0)
		{
			newMousePos(axisIdx) = axisSize - 1;
		}
		else if (CurCursorPos(axisIdx) >= (axisSize - 1))
		{
			newMousePos(axisIdx) = 0;
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

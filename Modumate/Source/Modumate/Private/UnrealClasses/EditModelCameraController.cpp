// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/EditModelCameraController.h"

#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Engine/StaticMeshActor.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/GameplayStatics.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "Online/ModumateAnalyticsStatics.h"
#include "Slate/SceneViewport.h"
#include "UnrealClasses/EditModelInputHandler.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerPawn.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/ModumateViewportClient.h"
#include "UnrealClient.h"

UEditModelCameraController::UEditModelCameraController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bUpdateCameraTransform(true)
	, RotateSpeed(5.0f)
	, bUseSmoothZoom(true)
	, SmoothZoomSpeed(25.0f)
	, ZoomPercentSpeed(0.2f)
	, ZoomMinStepDist(10.0f)
	, ZoomMinDistance(20.0f)
	, bEnforceFreeZoomMinDist(false)
	, ZoomMaxTotalDistance(525600.0f)
	, OrbitMovementLerpCurve(nullptr)
	, OrbitMaxPitch(90.0f)
	, OrbitDriftPitchRange(20.0f, 70.0f)
	, OrbitCursorMaxHeightPCT(0.9f)
	, OrbitAnchorScreenSize(0.25f)
	, FlyingSpeed(15.0f)
	, RetargetingDuration(0.25f)
	, RetargetingEaseExp(4.0f)
	, SnapAxisPitchLimit(60.0f)
	, SnapAxisYawDelta(54.0f)
	, Controller(nullptr)
	, CurMovementState(ECameraMovementState::Default)
	, OrbitTarget(ForceInitToZero)
	, OrbitStartProxyTarget(ForceInitToZero)
	, DefaultSphere(FVector::ZeroVector, 100.0f)
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
	, RetargetingTimeElapsed(0.0f)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UEditModelCameraController::BeginPlay()
{
	Super::BeginPlay();

#if UE_SERVER
	return;
#endif

	UWorld *world = GetWorld();
	Controller = Cast<AEditModelPlayerController>(GetOwner());
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

		UStaticMeshComponent* staticMeshComp = OrbitAnchorActor->GetStaticMeshComponent();
		staticMeshComp->SetStaticMesh(OrbitAnchorMesh);
		staticMeshComp->SetMobility(EComponentMobility::Movable);
		staticMeshComp->SetCastShadow(false);

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
	case ECameraMovementState::Retargeting:
		UpdateRetargeting(DeltaTime);
		break;
	default:
		break;
	}

	// Clamp the camera position based on the maximum total zoom distance from the origin
	FVector curCamPos = CamTransform.GetLocation();
	FVector clampedCamPos = curCamPos.GetClampedToMaxSize(ZoomMaxTotalDistance);
	CamTransform.SetLocation(clampedCamPos);

	if (!CamTransform.EqualsNoScale(oldTransform) && bUpdateCameraTransform)
	{
		Controller->EMPlayerPawn->SetActorLocationAndRotation(CamTransform.GetLocation(), CamTransform.GetRotation());
	}
}

void UEditModelCameraController::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	if (ensure(PlayerInputComponent))
	{
		PlayerInputComponent->BindAction(GetEnumValueShortName(EInputCameraActions::CameraOrbit), EInputEvent::IE_Pressed, this, &UEditModelCameraController::OnActionOrbitPressed);
		PlayerInputComponent->BindAction(GetEnumValueShortName(EInputCameraActions::CameraOrbit), EInputEvent::IE_Released, this, &UEditModelCameraController::OnActionOrbitReleased);

		PlayerInputComponent->BindAction(GetEnumValueShortName(EInputCameraActions::CameraPan), EInputEvent::IE_Pressed, this, &UEditModelCameraController::OnActionPanPressed);
		PlayerInputComponent->BindAction(GetEnumValueShortName(EInputCameraActions::CameraPan), EInputEvent::IE_Released, this, &UEditModelCameraController::OnActionPanReleased);

		PlayerInputComponent->BindAction(GetEnumValueShortName(EInputCameraActions::CameraZoomIn), EInputEvent::IE_Pressed, this, &UEditModelCameraController::OnActionZoomIn);
		PlayerInputComponent->BindAction(GetEnumValueShortName(EInputCameraActions::CameraZoomOut), EInputEvent::IE_Pressed, this, &UEditModelCameraController::OnActionZoomOut);

		PlayerInputComponent->BindAxis(GetEnumValueShortName(EInputMovementAxes::MoveYaw), this, &UEditModelCameraController::OnAxisRotateYaw);
		PlayerInputComponent->BindAxis(GetEnumValueShortName(EInputMovementAxes::MovePitch), this, &UEditModelCameraController::OnAxisRotatePitch);

		PlayerInputComponent->BindAxis(GetEnumValueShortName(EInputMovementAxes::MoveForward), this, &UEditModelCameraController::OnAxisMoveForward);
		PlayerInputComponent->BindAxis(GetEnumValueShortName(EInputMovementAxes::MoveRight), this, &UEditModelCameraController::OnAxisMoveRight);
		PlayerInputComponent->BindAxis(GetEnumValueShortName(EInputMovementAxes::MoveUp), this, &UEditModelCameraController::OnAxisMoveUp);
	}
}

bool UEditModelCameraController::ZoomToProjectExtents(const FVector& NewViewForward, const FVector& NewViewUp)
{
	return ZoomToTargetSphere(Controller->GetDocument()->CalculateProjectBounds().GetSphere(), NewViewForward, NewViewUp);
}

bool UEditModelCameraController::ZoomToSelection(const FVector& NewViewForward, const FVector& NewViewUp)
{
	return ZoomToTargetSphere(UModumateFunctionLibrary::GetSelectedExtents(Controller).GetSphere(), NewViewForward, NewViewUp);
}

bool UEditModelCameraController::ZoomToNextAxis(FVector2D NextAxisDirection, bool bUseSelection)
{
	if (NextAxisDirection.IsNearlyZero())
	{
		return false;
	}

	// If we're already retargeting, chain the subsequent move as if we've already finished the retarget
	if (CurMovementState == ECameraMovementState::Retargeting)
	{
		CamTransform = NewTargetTransform;
		SetMovementState(ECameraMovementState::Default);
	}

	const float normalizedYawDelta = SnapAxisYawDelta / 90.0f;

	FQuat curViewQuat = CamTransform.GetRotation();
	FRotator curViewRot = curViewQuat.Rotator();
	FVector curViewForward = curViewQuat.GetForwardVector();
	FVector curViewUp = curViewQuat.GetUpVector();
	FVector newViewForward(ForceInitToZero), newViewUp(ForceInitToZero);

	// The current view is within the horizontal pitch limit
	if (FMath::IsWithin(curViewRot.Pitch, -SnapAxisPitchLimit, SnapAxisPitchLimit))
	{
		// Find the nearest axis-aligned yaw, with an optional delta from horizontal movement
		float curYawIndex = curViewRot.Yaw / 90.0f;
		float newYawIndex = FMath::RoundToFloat(curYawIndex - NextAxisDirection.X * normalizedYawDelta);
		float newYawRad = HALF_PI * newYawIndex;
		FVector2D newProjectedYaw(FMath::Cos(newYawRad), FMath::Sin(newYawRad));

		// Without vertical movement, only adjust the view forward
		if (NextAxisDirection.Y == 0)
		{
			newViewForward.Set(newProjectedYaw.X, newProjectedYaw.Y, 0.0f);
		}
		// With vertical movement, we also need to calculate the corresponding new view up
		else
		{
			newViewForward = -NextAxisDirection.Y * FVector::UpVector;
			newViewUp = FVector(NextAxisDirection.Y * newProjectedYaw, 0.0f);
		}
	}
	// Otherwise, treat the view as vertical
	else
	{
		float curViewSignZ = FMath::Sign(curViewForward.Z);

		// Find the nearest axis-aligned "yaw" from the view up, with an optional delta from horizontal movement
		FVector2D curPojectedViewUp = FVector2D(curViewUp.X, curViewUp.Y).GetSafeNormal();
		float curYawIndex = FMath::Atan2(curPojectedViewUp.Y, curPojectedViewUp.X) / HALF_PI;
		float newYawIndex = FMath::RoundToFloat(curYawIndex + curViewSignZ * NextAxisDirection.X * normalizedYawDelta);
		float newYawRad = HALF_PI * newYawIndex;
		FVector2D newProjectedYaw(FMath::Cos(newYawRad), FMath::Sin(newYawRad));

		// With vertical movement, we also need to calculate the corresponding new view forward
		if (NextAxisDirection.Y == curViewSignZ)
		{
			newViewForward = FVector(-curViewSignZ * newProjectedYaw, 0.0f);
		}
		// Without vertical movement, only adjust the view up
		else
		{
			newViewForward.Set(0.0f, 0.0f, curViewSignZ);
			newViewUp.Set(newProjectedYaw.X, newProjectedYaw.Y, 0.0f);
		}
	}

	if (newViewForward.IsNormalized())
	{
		static const FString eventName(TEXT("QuickViewNext"));
		UModumateAnalyticsStatics::RecordEventSimple(this, EModumateAnalyticsCategory::View, eventName);

		if (bUseSelection)
		{
			return ZoomToSelection(newViewForward, newViewUp);
		}
		else
		{
			return ZoomToProjectExtents(newViewForward, newViewUp);
		}
	}

	return false;
}

void UEditModelCameraController::OnActionOrbitPressed()
{
	SetOrbiting(true);
	Controller->OnHandledInputAction(EInputCameraActions::CameraOrbit, EInputEvent::IE_Pressed);
}

void UEditModelCameraController::OnActionOrbitReleased()
{
	SetOrbiting(false);
	Controller->OnHandledInputAction(EInputCameraActions::CameraOrbit, EInputEvent::IE_Released);
}

void UEditModelCameraController::OnActionPanPressed()
{
	SetPanning(true);
	Controller->OnHandledInputAction(EInputCameraActions::CameraPan, EInputEvent::IE_Pressed);
}

void UEditModelCameraController::OnActionPanReleased()
{
	SetPanning(false);
	Controller->OnHandledInputAction(EInputCameraActions::CameraPan, EInputEvent::IE_Released);
}

void UEditModelCameraController::OnActionZoomIn()
{
	OnZoom(-1.0f);
	Controller->OnHandledInputAction(EInputCameraActions::CameraZoomIn, EInputEvent::IE_Pressed);
}

void UEditModelCameraController::OnActionZoomOut()
{
	OnZoom(1.0f);
	Controller->OnHandledInputAction(EInputCameraActions::CameraZoomOut, EInputEvent::IE_Pressed);
}

void UEditModelCameraController::OnAxisRotateYaw(float RotateYawValue)
{
	if ((RotateYawValue != 0.0f) &&
		((CurMovementState == ECameraMovementState::Orbiting) || (CurMovementState == ECameraMovementState::Flying)))
	{
		RotationDeltasAccumulated.X += RotateSpeed * RotateYawValue;
		Controller->OnHandledInputAxis(EInputMovementAxes::MoveYaw, RotateYawValue);
	}
}

void UEditModelCameraController::OnAxisRotatePitch(float RotatePitchValue)
{
	if ((RotatePitchValue != 0.0f) &&
		((CurMovementState == ECameraMovementState::Orbiting) || (CurMovementState == ECameraMovementState::Flying)))
	{
		RotationDeltasAccumulated.Y += RotateSpeed * RotatePitchValue;
		Controller->OnHandledInputAxis(EInputMovementAxes::MovePitch, RotatePitchValue);
	}
}

void UEditModelCameraController::OnAxisMoveForward(float MoveForwardValue)
{
	OnMoveValue(MoveForwardValue * FVector::ForwardVector);

	if (MoveForwardValue != 0.0f)
	{
		Controller->OnHandledInputAxis(EInputMovementAxes::MoveForward, MoveForwardValue);
	}
}

void UEditModelCameraController::OnAxisMoveRight(float MoveRightValue)
{
	OnMoveValue(MoveRightValue * FVector::RightVector);

	if (MoveRightValue != 0.0f)
	{
		Controller->OnHandledInputAxis(EInputMovementAxes::MoveRight, MoveRightValue);
	}
}

void UEditModelCameraController::OnAxisMoveUp(float MoveUpValue)
{
	OnMoveValue(MoveUpValue * FVector::UpVector);

	if (MoveUpValue != 0.0f)
	{
		Controller->OnHandledInputAxis(EInputMovementAxes::MoveUp, MoveUpValue);
	}
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
	bool bShouldAxisInputHaveBeenPrioritized = true;

	// End the previous movement state
	switch (CurMovementState)
	{
	case ECameraMovementState::Default:
	{
		bShouldAxisInputHaveBeenPrioritized = false;
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
	case ECameraMovementState::Retargeting:
		NewTargetTransform = CamTransform;
		break;
	}

	if (bShouldAxisInputHaveBeenPrioritized && Controller->InputHandlerComponent)
	{
		Controller->InputHandlerComponent->RequestAxisInputPriority(StaticClass()->GetFName(), false);
	}

	// Begin the new movement state
	const FSnappedCursor &cursor = Controller->EMPlayerState->SnappedCursor;
	bool bShouldPrioritizeAxisInputs = true;

	switch (NewMovementState)
	{
	case ECameraMovementState::Default:
	{
		bShouldPrioritizeAxisInputs = false;
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
			PanFactors = FVector2D::UnitVector / screenOffsets;
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
	case ECameraMovementState::Retargeting:
		PreRetargetTransform = CamTransform;
		RetargetingTimeElapsed = 0.0f;
		break;
	default:
		break;
	}

	CurMovementState = NewMovementState;

	if (bShouldPrioritizeAxisInputs && Controller->InputHandlerComponent)
	{
		Controller->InputHandlerComponent->RequestAxisInputPriority(StaticClass()->GetFName(), true);
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

		// But don't zoom closer than ZoomMinDistance to the target if we're zooming in and it's enforced during free-zooming
		if (bEnforceFreeZoomMinDist)
		{
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

	FQuat camRotation = CamTransform.GetRotation();

	// First, consume accumulated yaw by rotating about the Z axis
	if (!FMath::IsNearlyZero(RotationDeltasAccumulated.X))
	{
		FQuat deltaYaw(FVector::UpVector, FMath::DegreesToRadians(RotationDeltasAccumulated.X));
		camRotation = deltaYaw * camRotation;
	}

	// Next, consume accumulated pitch by rotating about the local Y axis, to an extent
	if (!FMath::IsNearlyZero(RotationDeltasAccumulated.Y))
	{
		float curPitch = camRotation.Rotator().Pitch;
		float newPitch = FMath::Clamp(curPitch + RotationDeltasAccumulated.Y, -OrbitMaxPitch, OrbitMaxPitch);
		float pitchDeltaDegrees = (newPitch - curPitch);

		if (!FMath::IsNearlyZero(pitchDeltaDegrees))
		{
			FVector camAxisY = camRotation.GetAxisY();
			FQuat deltaPitch(camAxisY, FMath::DegreesToRadians(-pitchDeltaDegrees));
			camRotation = deltaPitch * camRotation;
		}
	}

	CamTransform.SetRotation(camRotation);
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

void UEditModelCameraController::UpdateRetargeting(float DeltaTime)
{
	if (RetargetingTimeElapsed >= RetargetingDuration)
	{
		CamTransform = NewTargetTransform;
		SetMovementState(ECameraMovementState::Default);
	}
	else
	{
		RetargetingTimeElapsed += DeltaTime;
		float lerpAlpha = FMath::InterpEaseOut(0.0f, 1.0f, FMath::Clamp(RetargetingTimeElapsed / RetargetingDuration, 0.0f, 1.0f), RetargetingEaseExp);

		FVector newLocation = FMath::Lerp(PreRetargetTransform.GetLocation(), NewTargetTransform.GetLocation(), lerpAlpha);
		FQuat newRotation = FQuat::Slerp(PreRetargetTransform.GetRotation(), NewTargetTransform.GetRotation(), lerpAlpha);

		// Ensure that the interpolated rotation has no roll
		FVector newRightVector = newRotation.GetRightVector();
		FVector flatRightVector(newRightVector.X, newRightVector.Y, 0.0f);
		if (!FMath::IsNearlyZero(newRightVector.Z) && flatRightVector.Normalize())
		{
			FQuat cancelRollDelta = FQuat::FindBetweenNormals(newRightVector, flatRightVector);
			newRotation = cancelRollDelta * newRotation;
		}

		CamTransform.SetLocation(newLocation);
		CamTransform.SetRotation(newRotation);
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

bool UEditModelCameraController::ZoomToTargetSphere(const FSphere& TargetSphere, const FVector& NewViewForward, const FVector& NewViewUp, bool bSnapVerticalViewToAxis)
{
	if ((CurMovementState != ECameraMovementState::Default))
	{
		return false;
	}

	FQuat curViewRotation = CamTransform.GetRotation();
	FVector curViewForward = curViewRotation.GetForwardVector();
	FVector curViewUp = curViewRotation.GetUpVector();
	FQuat newViewRotation = curViewRotation;
	FVector fixedViewForward = NewViewForward;

	// If the new view direction is unspecified, or close to the current one, use the current view forward.
	if (!fixedViewForward.IsNormalized() || (FVector::Coincident(fixedViewForward, curViewForward) && FVector::Coincident(NewViewUp, curViewUp)))
	{
		fixedViewForward = CamTransform.GetRotation().GetForwardVector();
	}
	// Otherwise, if the target view direction isn't in the Z axis, then compute the default view rotation (with a maximally-Z-oriented up vector).
	else if (!FVector::Parallel(fixedViewForward, FVector::UpVector))
	{
		newViewRotation = FRotationMatrix::MakeFromX(fixedViewForward).ToQuat();
	}
	// Otherwise, find the view rotation that has the smallest change in up-vector, to prevent unnecessary rolling of camera view while transitioning to up/down views.
	else if (NewViewUp.IsNormalized() && FVector::Orthogonal(fixedViewForward, NewViewUp))
	{
		newViewRotation = FRotationMatrix::MakeFromXZ(fixedViewForward, NewViewUp).ToQuat();
	}
	else
	{
		FVector newViewRight = curViewRotation.GetRightVector();

		if (bSnapVerticalViewToAxis)
		{
			newViewRight = (FMath::Abs(newViewRight.X) > FMath::Abs(newViewRight.Y)) ?
				FVector(FMath::Sign(newViewRight.X), 0.0f, 0.0f) :
				FVector(0.0f, FMath::Sign(newViewRight.Y), 0.0f);
		}

		newViewRotation = FRotationMatrix::MakeFromXY(fixedViewForward, newViewRight).ToQuat();
	}

	FVector newViewOrigin = Controller->CalculateViewLocationForSphere(
		TargetSphere.W <= 1.f ? DefaultSphere : TargetSphere,
		fixedViewForward,
		Controller->EMPlayerPawn->CameraComponent->AspectRatio,
		Controller->EMPlayerPawn->CameraComponent->FieldOfView);

	NewTargetTransform.SetComponents(newViewRotation, newViewOrigin, CamTransform.GetScale3D());
	return SetMovementState(ECameraMovementState::Retargeting);
}

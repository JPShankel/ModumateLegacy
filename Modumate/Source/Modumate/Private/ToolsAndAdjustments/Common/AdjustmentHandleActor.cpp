// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/EditableTextBox.h"
#include "Graph/Graph3D.h"
#include "Kismet/KismetMathLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "ModumateCore/ModumateObjectDeltaStatics.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "UI/AdjustmentHandleAssetData.h"
#include "UI/AdjustmentHandleWidget.h"
#include "UI/DimensionManager.h"
#include "UI/EditModelPlayerHUD.h"
#include "UI/PendingSegmentActor.h"
#include "UI/HUDDrawWidget.h"
#include "UI/WidgetClassAssetData.h"
#include "UnrealClasses/DimensionWidget.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/LineActor.h"
#include "UnrealClasses/ModumateGameInstance.h"


using namespace Modumate;
const FName AAdjustmentHandleActor::StateRequestTag(TEXT("AdjustmentHandle"));

// Sets default values
AAdjustmentHandleActor::AAdjustmentHandleActor(const FObjectInitializer &ObjectInitializer)
	: Super(ObjectInitializer)
	, HoveredScale(1.2f)
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
}


// Called when the game starts or when spawned
void AAdjustmentHandleActor::BeginPlay()
{
	Super::BeginPlay();

	UWorld *world = GetWorld();
	if (ensure(world))
	{
		GameState = world->GetGameState<AEditModelGameState_CPP>();
		Controller = world->GetFirstPlayerController<AEditModelPlayerController_CPP>();
		PlayerHUD = Controller->GetEditModelHUD();
	}
}

void AAdjustmentHandleActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// If the adjust handle actor is destroyed while it's in use, then abort the handle usage.
	if (Controller && Controller->EMPlayerState && (Controller->InteractionHandle == this))
	{
		UE_LOG(LogTemp, Warning, TEXT("Adjustment handle was destroyed while it was being used!"));
		AbortUse();
	}

	if (Widget)
	{
		if (bEnabled)
		{
			Widget->RemoveFromViewport();
		}

		if (Controller && Controller->HUDDrawWidget)
		{
			Controller->HUDDrawWidget->UserWidgetPool.Release(Widget);
		}
	}

	bEnabled = false;

	Super::EndPlay(EndPlayReason);
}

void AAdjustmentHandleActor::Initialize()
{
	if (!ensure(!bInitialized))
	{
		return;
	}

	// Spawn the handle's widget from the standard adjustment handle class, from the HUD's widget pool
	Widget = PlayerHUD->GetOrCreateWidgetInstance<UAdjustmentHandleWidget>(PlayerHUD->WidgetClasses->AdjustmentHandleClass);
	Widget->HandleActor = this;
	ApplyWidgetStyle();

	bInitialized = true;
}

void AAdjustmentHandleActor::ApplyWidgetStyle()
{
	// If common style properties of the handle widget are customized by a derived class, then set those up now
	const USlateWidgetStyleAsset* overrideMainButtonStyle = nullptr;
	FVector2D overrideWidgetSize(ForceInitToZero), overrideMainButtonOffset(ForceInitToZero);
	if (ensure(PlayerHUD && PlayerHUD->HandleAssets && Widget) &&
		GetHandleWidgetStyle(overrideMainButtonStyle, overrideWidgetSize, overrideMainButtonOffset))
	{
		if (overrideMainButtonStyle)
		{
			Widget->SetMainButtonStyle(overrideMainButtonStyle);
		}

		// TODO: who is supposed to own these variables?
		float defaultScale = 1.0f;
		float hoveredScale = Widget->IsButtonHovered() ? HoveredScale : defaultScale;

		// hoveredOffset is the average because the padding is on one side
		float hoveredOffset = Widget->IsButtonHovered() ? (hoveredScale + defaultScale)/2.0f : defaultScale;

		if (overrideWidgetSize.GetMin() > 0.0f)
		{
			Widget->DesiredSize = overrideWidgetSize * hoveredScale;
		}

		Widget->MainButtonOffset = overrideMainButtonOffset * hoveredOffset;
	}
}

bool AAdjustmentHandleActor::GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const
{
	return false;
}

void AAdjustmentHandleActor::PostEndOrAbort()
{
	if (Controller)
	{
		Controller->InteractionHandle = nullptr;
		Controller->EMPlayerState->SnappedCursor.MouseMode = OriginalMouseMode;

		GameState->Document.ClearPreviewDeltas(Controller->GetWorld());
	}

	if (TargetMOI)
	{
		if (TargetMOI->GetIsInPreviewMode())
		{
			TargetMOI->EndPreviewOperation_DEPRECATED();
		}

		TargetMOI->RequestCollisionDisabled(StateRequestTag, false);

		for (auto *descendent : TargetDescendents)
		{
			if (descendent && !descendent->IsDestroyed())
			{
				descendent->RequestCollisionDisabled(StateRequestTag, false);
			}
		}

		for (auto* connectedMOI : TargetConnected)
		{
			connectedMOI->RequestCollisionDisabled(StateRequestTag, false);
		}
	}

	if (GameInstance && GameInstance->DimensionManager)
	{
		UDimensionWidget* dimensionWidget = nullptr;
		auto dimensionActor = GameInstance->DimensionManager->GetDimensionActor(PendingSegmentID);
		if (dimensionActor)
		{
			dimensionWidget = dimensionActor->DimensionText;
		}
		if (dimensionWidget != nullptr)
		{
			dimensionWidget->Measurement->OnTextCommitted.RemoveDynamic(this, &AAdjustmentHandleActor::OnTextCommitted);

			GameInstance->DimensionManager->ReleaseDimensionActor(PendingSegmentID);
			PendingSegmentID = MOD_ID_NONE;
		}
	}

	Controller->EMPlayerState->SnappedCursor.ClearAffordanceFrame();

	if (SourceMOI && !SourceMOI->IsDestroyed())
	{
		SourceMOI->ShowAdjustmentHandles(Controller, true);
	}

	if (ensureAlways(bIsInUse))
	{
		bIsInUse = false;
	}
}

void AAdjustmentHandleActor::UpdateTargetGeometry()
{
	if (TargetMOI)
	{
		TargetMOI->MarkDirty(EObjectDirtyFlags::Structure);
	}
}

void AAdjustmentHandleActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (Widget)
	{
		Widget->UpdateTransform();
	}
}

void AAdjustmentHandleActor::SetEnabled(bool bNewEnabled)
{
	if (bEnabled != bNewEnabled)
	{
		bEnabled = bNewEnabled;

		if (bEnabled && !bInitialized)
		{
			Initialize();
		}

		SetActorHiddenInGame(!bEnabled);
		SetActorEnableCollision(bEnabled);
		SetActorTickEnabled(bEnabled);

		if (ensure(Widget))
		{
			if (bEnabled)
			{
				Widget->AddToViewport();
			}
			else
			{
				Widget->RemoveFromViewport();
			}
		}
	}

	// By default, only recurse into children in order to disable them;
	// leave enabling children to the implementation of the handle class.
	if (!bEnabled)
	{
		for (AAdjustmentHandleActor *handleChild : HandleChildren)
		{
			if (handleChild)
			{
				handleChild->SetEnabled(false);
			}
		}
	}
}

bool AAdjustmentHandleActor::BeginUse()
{
	if (!ensure(Controller))
	{
		return false;
	}

	UWidgetBlueprintLibrary::SetFocusToGameViewport(); // Takes keyboard focus away from any potential widget text field

	OriginalMouseMode = Controller->EMPlayerState->SnappedCursor.MouseMode;
	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;

	// By default, disable collision on the target MOI so that its current modified state doesn't interfere with handle usage
	if (TargetMOI)
	{
		TargetMOI->BeginPreviewOperation_DEPRECATED();
		TargetMOI->RequestCollisionDisabled(StateRequestTag, true);

		TargetDescendents = TargetMOI->GetAllDescendents();
		for (auto *descendent : TargetDescendents)
		{
			descendent->RequestCollisionDisabled(StateRequestTag, true);
		}

		TargetMOI->GetConnectedMOIs(TargetConnected);
		for (auto* connectedMOI : TargetConnected)
		{
			connectedMOI->RequestCollisionDisabled(StateRequestTag, true);
		}

		TargetOriginalState = TargetMOI->GetStateData();
	}

	// By default, hide all adjustment handles while one is in use
	if (SourceMOI)
	{
		SourceMOI->ShowAdjustmentHandles(Controller, false);
	}

	bIsInUse = true;

	AnchorLoc = GetHandlePosition();

	if (auto world = GetWorld())
	{
		GameInstance = Cast<UModumateGameInstance>(GetWorld()->GetGameInstance());

		if (HasDimensionActor())
		{
			auto dimensionActor = GameInstance->DimensionManager->AddDimensionActor(APendingSegmentActor::StaticClass());
			PendingSegmentID = dimensionActor->ID;

			auto dimensionWidget = dimensionActor->DimensionText;
			dimensionWidget->Measurement->SetIsReadOnly(false);
			dimensionWidget->Measurement->OnTextCommitted.AddDynamic(this, &AAdjustmentHandleActor::OnTextCommitted);

			GameInstance->DimensionManager->SetActiveActorID(PendingSegmentID);

			auto pendingSegment = dimensionActor->GetLineActor();
			pendingSegment->Thickness = SegmentThickness;
			pendingSegment->Color = SegmentColor;
			pendingSegment->Point1 = AnchorLoc;
		}
	}

	return true;
}

bool AAdjustmentHandleActor::UpdateUse()
{
	Controller->AddAllOriginAffordances();

	if (HasDimensionActor())
	{
		ALineActor* pendingSegment = nullptr;
		auto dimensionActor = GameInstance->DimensionManager->GetDimensionActor(PendingSegmentID);
		if (ensure(dimensionActor))
		{
			pendingSegment = dimensionActor->GetLineActor();
		}

		if (pendingSegment != nullptr)
		{

			auto cursor = Controller->EMPlayerState->SnappedCursor;
			pendingSegment->Point2 = cursor.SketchPlaneProject(cursor.WorldPosition);
		}
		else
		{
			return false;
		}
	}

	return true;
}

void AAdjustmentHandleActor::EndUse()
{
	TSet<int32> newVertexIDs;
	FModumateObjectDeltaStatics::GetTransformableIDs({ TargetMOI->ID }, Controller->GetDocument(), newVertexIDs);

	TMap<int32, FTransform> newTransforms;
	for (int32 id : newVertexIDs)
	{
		auto obj = GameState->Document.GetObjectById(id);
		if (obj == nullptr)
		{
			continue;
		}

		newTransforms.Add(id, obj->GetWorldTransform());
	}

	// TODO: preview operation is no longer necessary, but removing this could cause ensures
	// until the other handles are refactored
	TargetMOI->EndPreviewOperation_DEPRECATED();

	// Now that we've reverted the target object back to its original state, clean all objects so that
	// deltas can be applied to the original state, and all of its dependent changes.
	GameState->Document.CleanObjects();

	FModumateObjectDeltaStatics::MoveTransformableIDs(newTransforms, Controller->GetDocument(), Controller->GetWorld(), false);

	// TODO: the deltas should be an outparam of EndUse (and EndUse would need to be refactored)
	if (!IsActorBeingDestroyed())
	{
		PostEndOrAbort();
	}
}

void AAdjustmentHandleActor::AbortUse()
{
	PostEndOrAbort();
}

bool AAdjustmentHandleActor::IsInUse() const
{
	return bIsInUse;
}

bool AAdjustmentHandleActor::HandleInputNumber(float number)
{
	return false;
}

bool AAdjustmentHandleActor::HasDimensionActor()
{
	return true;
}

bool AAdjustmentHandleActor::HasDistanceTextInput()
{
	return true;
}

FVector AAdjustmentHandleActor::GetHandlePosition() const
{
	return SourceMOI ? SourceMOI->GetObjectLocation() : FVector::ZeroVector;
}

FVector AAdjustmentHandleActor::GetHandleDirection() const
{
	if (!ensure(TargetMOI))
	{
		return FVector::ZeroVector;
	}

	int32 numPoints = TargetMOI->GetNumCorners();
	if ((numPoints == 0) || (TargetIndex == INDEX_NONE) || (TargetIndex > numPoints))
	{
		return FVector::ZeroVector;
	}

	if (TargetIndex == numPoints)
	{
		return TargetMOI->GetNormal();
	}

	TArray<FVector> targetPoints;
	for (int32 i = 0; i < numPoints; ++i)
	{
		targetPoints.Add(TargetMOI->GetCorner(i));
	}

	// This would be redundant, but the handles need to recalculate the plane in order to determine
	// how the points wind, which may not result in the same plane normal as the reported object normal.
	FPlane targetPlane;
	if (!ensure(UModumateGeometryStatics::GetPlaneFromPoints(targetPoints, targetPlane)))
	{
		return FVector::ZeroVector;
	}

	FVector targetNormal(targetPlane);

	int32 numPolyPoints = targetPoints.Num();
	int32 edgeStartIdx = TargetIndex;
	int32 edgeEndIdx = (TargetIndex + 1) % numPolyPoints;

	const FVector &edgeStartPoint = targetPoints[edgeStartIdx];
	const FVector &edgeEndPoint = targetPoints[edgeEndIdx];
	FVector edgeDir = (edgeEndPoint - edgeStartPoint).GetSafeNormal();
	FVector edgeNormal = (edgeDir ^ targetNormal);

	return edgeNormal;
}

bool AAdjustmentHandleActor::HandleInvert()
{
	return false;
}

void AAdjustmentHandleActor::SetTargetMOI(FModumateObjectInstance *InTargetMOI)
{
	TargetMOI = InTargetMOI;
}

void AAdjustmentHandleActor::SetTargetIndex(int32 InTargetIndex)
{
	TargetIndex = InTargetIndex;
}

void AAdjustmentHandleActor::SetSign(float InSign)
{
	if (ensure(FMath::Sign(InSign) == InSign))
	{
		Sign = InSign;
	}
}

void AAdjustmentHandleActor::OnTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	Controller->OnTextCommitted(Text, CommitMethod);
}

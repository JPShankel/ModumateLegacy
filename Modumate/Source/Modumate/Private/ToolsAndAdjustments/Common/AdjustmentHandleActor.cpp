// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Graph/Graph3D.h"
#include "Kismet/KismetMathLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "ModumateCore/ModumateObjectDeltaStatics.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "ToolsAndAdjustments/Handles/EditModelPortalAdjustmentHandles.h"
#include "UI/AdjustmentHandleAssetData.h"
#include "UI/AdjustmentHandleWidget.h"
#include "UI/EditModelPlayerHUD.h"
#include "UI/HUDDrawWidget.h"
#include "UI/WidgetClassAssetData.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"


using namespace Modumate;
const FName AAdjustmentHandleActor::StateRequestTag(TEXT("AdjustmentHandle"));

// Sets default values
AAdjustmentHandleActor::AAdjustmentHandleActor(const FObjectInitializer &ObjectInitializer)
	: Super(ObjectInitializer)
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

		if (overrideWidgetSize.GetMin() > 0.0f)
		{
			Widget->DesiredSize = overrideWidgetSize;
		}

		Widget->MainButtonOffset = overrideMainButtonOffset;
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
			TargetMOI->EndPreviewOperation();
		}

		TargetMOI->RequestCollisionDisabled(StateRequestTag, false);

		for (auto *descendent : TargetDescendents)
		{
			if (descendent && !descendent->IsDestroyed())
			{
				descendent->RequestCollisionDisabled(StateRequestTag, false);
			}
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

	// TODO: refactor with new dimension string manager
	Controller->DimStringWidgetSelectedObject = nullptr; // Reset reference to object that was selected from widget
	UWidgetBlueprintLibrary::SetFocusToGameViewport(); // Takes keyboard focus away from any potential widget text field

	OriginalMouseMode = Controller->EMPlayerState->SnappedCursor.MouseMode;
	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;

	// By default, disable collision on the target MOI so that its current modified state doesn't interfere with handle usage
	if (TargetMOI)
	{
		TargetMOI->BeginPreviewOperation();
		TargetMOI->RequestCollisionDisabled(StateRequestTag, true);

		TargetDescendents = TargetMOI->GetAllDescendents();
		for (auto *descendent : TargetDescendents)
		{
			descendent->RequestCollisionDisabled(StateRequestTag, true);
		}
	}

	// By default, hide all adjustment handles while one is in use
	if (SourceMOI)
	{
		SourceMOI->ShowAdjustmentHandles(Controller, false);
	}

	bIsInUse = true;

	return true;
}

bool AAdjustmentHandleActor::UpdateUse()
{
	Controller->AddAllOriginAffordances();

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
	TargetMOI->EndPreviewOperation();

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

FVector AAdjustmentHandleActor::GetHandlePosition() const
{
	return SourceMOI ? SourceMOI->GetObjectLocation() : FVector::ZeroVector;
}

FVector AAdjustmentHandleActor::GetHandleDirection() const
{
	return FVector::ZeroVector;
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

// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelPortalTools.h"

#include "DrawDebugHelpers.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "DocumentManagement/ModumateCommands.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/CompoundMeshActor.h"
#include "ModumateCore/ExpressionEvaluator.h"
#include "Database/ModumateObjectDatabase.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "UnrealClasses/PortalFrameActor_CPP.h"

using namespace Modumate;

UPortalToolBase::UPortalToolBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, CursorActor(nullptr)
	, Document(nullptr)
	, HostID(0)
	, bHasBoringError(false)
	, bUseFixedOffset(true)
	, WorldPos(FVector::ZeroVector)
	, RelativePos(FVector2D::ZeroVector)
	, WorldRot(FQuat::Identity)
	, RelativeRot(FQuat::Identity)
	, Assembly()
	, Active(false)
	, Inverted(false)
	, bValidPortalConfig(false)
{
	UWorld *world = Controller ? Controller->GetWorld() : nullptr;
	if (world)
	{
		Document = &world->GetGameState<AEditModelGameState_CPP>()->Document;
	}
}

bool UPortalToolBase::Activate()
{
	HostID = 0;
	Controller->DeselectAll();
	Controller->EMPlayerState->SetHoveredObject(nullptr);
	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;
	SetupCursor();
	Active = true;
	return true;
}

void UPortalToolBase::SetAssembly(const FShoppingItem &key)
{
	Assembly = key;
	if (Active)
	{
		SetupCursor();
	}
}

void UPortalToolBase::SetupCursor()
{
	if (CursorActor != nullptr)
	{
		CursorActor->Destroy();
	}

	if (HostID != 0)
	{
		auto *lastHitHost = Document->GetObjectById(HostID);
		auto *lastWallMeshActor = lastHitHost ? Cast<ADynamicMeshActor>(lastHitHost->GetActor()) : nullptr;
		if (lastWallMeshActor)
		{
			lastWallMeshActor->PreviewHoleActors.Reset();
			lastHitHost->SetupGeometry();
		}
	}

	CursorActor = Controller->GetWorld()->SpawnActor<ACompoundMeshActor>(ACompoundMeshActor::StaticClass());
	CursorActor->SetActorEnableCollision(false);

	auto *gameState = Controller->GetWorld()->GetGameState<AEditModelGameState_CPP>();
	const FModumateObjectAssembly *obAsm = gameState->GetAssemblyByKey(Assembly.Key);
	bValidPortalConfig = false;

	float invMul = Inverted ? 1 : -1;
	if (obAsm && obAsm->PortalConfiguration.IsValid())
	{
		bValidPortalConfig = true;

		CursorActor->MakeFromAssembly(*obAsm, FVector::OneVector, Inverted, false);

		// Store data used for previewing this object as if it were a fully-created MOI
		CursorActor->TempAssemblyKey = Assembly.Key;
		CursorActor->TempObjectToolMode = GetToolMode();
	}
}

bool UPortalToolBase::Deactivate()
{
	if (CursorActor != nullptr)
	{
		CursorActor->Destroy();
		CursorActor = nullptr;
	}

	if (HostID != 0)
	{
		auto *lastHitWall = Document->GetObjectById(HostID);
		auto *lastWallMeshActor = lastHitWall ? Cast<ADynamicMeshActor>(lastHitWall->GetActor()) : nullptr;
		if (lastWallMeshActor)
		{
			lastWallMeshActor->PreviewHoleActors.Reset();
			lastHitWall->SetupGeometry();
		}
	}

	Active = false;
	return true;
}

static bool CalcPortalPositionOnHost(const FModumateObjectInstance *hostObj, const FVector &hitLoc, const FVector &hitNormal,
	bool bUseGroundOffset, const FVector &offsetFromGround, FTransform &outTransform)
{
	const FModumateObjectInstance *hostParent = hostObj->GetParentObject();
	if (!(hostParent && (hostParent->GetObjectType() == EObjectType::OTMetaPlane)))
	{
		return false;
	}

	FVector hostOrigin = hostParent->GetControlPoint(0);
	FVector positionOnHost = hitLoc;
	if (bUseGroundOffset)
	{
		positionOnHost.Z = hostOrigin.Z + offsetFromGround.Z;
	}

	FVector mountedNormal = hitNormal;
	FVector hostNormal = hostObj->GetNormal();
	float hostThickness = hostObj->CalculateThickness();
	if (!FVector::Parallel(hitNormal, hostNormal))
	{
		FVector hitDeltaFromOrigin = positionOnHost - hostOrigin;
		float distFromHostPlaneA = FMath::Abs(hitDeltaFromOrigin | hostNormal);
		float distFromHostPlaneB = FMath::Abs((hitDeltaFromOrigin | hostNormal) - hostThickness);

		if ((distFromHostPlaneA < distFromHostPlaneB) && (distFromHostPlaneA < KINDA_SMALL_NUMBER))
		{
			mountedNormal = -hostNormal;
		}
		else if ((distFromHostPlaneB < distFromHostPlaneA) && (distFromHostPlaneB < KINDA_SMALL_NUMBER))
		{
			mountedNormal = hostNormal;
		}
		else
		{
			return false;
		}
	}

	// mounted transform should be +Z up, +Y away from the wall, and +X along the wall.
	FQuat rot = FRotationMatrix::MakeFromYZ(mountedNormal, FVector::UpVector).ToQuat();

	outTransform = FTransform(rot, positionOnHost, FVector::OneVector);
	return true;
}

bool UPortalToolBase::FrameUpdate()
{
	int32 lastWallID = HostID;
	FModumateObjectInstance *hitMOI = nullptr;
	const auto &snapCursor = Controller->EMPlayerState->SnappedCursor;
	FVector hitLoc = snapCursor.WorldPosition;
	FVector hitNormal = snapCursor.HitNormal;

	HostID = 0;

	if (CursorActor && Controller->EMPlayerState->SnappedCursor.Visible)
	{
		hitMOI = Document->ObjectFromActor(Controller->EMPlayerState->SnappedCursor.Actor);
		FModumateObjectInstance *hitMOIParent = hitMOI ? hitMOI->GetParentObject() : nullptr;

		while (hitMOIParent && (hitMOIParent->GetObjectType() != EObjectType::OTMetaPlane))
		{
			hitMOI = hitMOIParent;
			hitMOIParent = hitMOI->GetParentObject();
		}
	}

	float heightFromBottom = 0.0f;

	// Use default heights from document for doors and windows
	// To be deprecated in favor of instance level overrides
	if (Controller->GetToolMode() == EToolMode::VE_DOOR)
	{
		heightFromBottom = Modumate::Units::FUnitValue::WorldInches(Document->GetDefaultDoorHeight()).AsWorldInches();
	}
	else if (Controller->GetToolMode() == EToolMode::VE_WINDOW)
	{
		heightFromBottom = Modumate::Units::FUnitValue::WorldInches(Document->GetDefaultWindowHeight()).AsWorldInches();
	}

	bool bHasRelativeTransform = hitMOI && UModumateObjectStatics::GetRelativeTransformOnPlaneHostedObj(
		hitMOI, hitLoc, hitNormal, heightFromBottom, bUseFixedOffset, RelativePos, RelativeRot);

	bool bHasWorldTransform = bHasRelativeTransform && UModumateObjectStatics::GetWorldTransformOnPlaneHostedObj(
		hitMOI, RelativePos, RelativeRot, WorldPos, WorldRot);

	if (bHasWorldTransform)
	{
		HostID = hitMOI->ID;

#if UE_BUILD_DEBUG
		DrawDebugCoordinateSystem(Controller->GetWorld(), WorldPos, WorldRot.Rotator(), 20.0f, false, -1.f, 255, 2.0f);
#endif

		CursorActor->SetActorLocation(WorldPos);
		CursorActor->SetActorRotation(WorldRot);
	}
	else
	{
		HostID = 0;
	}

	if (lastWallID != HostID)
	{
		if (hitMOI)
		{
			auto *wallMeshActor = Cast<ADynamicMeshActor>(hitMOI->GetActor());
			if (wallMeshActor && bValidPortalConfig)
			{
				wallMeshActor->PreviewHoleActors.AddUnique(CursorActor);
				hitMOI->MarkDirty(EObjectDirtyFlags::Structure);
			}
		}

		auto *lastHitWall = Document->GetObjectById(lastWallID);
		auto *lastWallMeshActor = lastHitWall ? Cast<ADynamicMeshActor>(lastHitWall->GetActor()) : nullptr;
		if (lastWallMeshActor)
		{
			lastWallMeshActor->PreviewHoleActors.Remove(CursorActor);
			lastHitWall->MarkDirty(EObjectDirtyFlags::Structure);
		}
	}

	if (hitMOI && HostID)
	{
		hitMOI->MarkDirty(EObjectDirtyFlags::Structure);
		CursorActor->SetActorHiddenInGame(false);

		if (auto *hostMeshActor = Cast<ADynamicMeshActor>(hitMOI->GetActor()))
		{
			bHasBoringError = hostMeshActor->ErrorHoleActors.Contains(CursorActor);
		}
	}
	else
	{
		if (CursorActor != nullptr)
		{
			CursorActor->SetActorHiddenInGame(true);
		}
	}

	return true;
}

bool UPortalToolBase::EnterNextStage()
{
	return true;
}

bool UPortalToolBase::EndUse()
{
	return true;
}

bool UPortalToolBase::ScrollToolOption(int32 dir)
{
	return true;
}

bool UPortalToolBase::HandleInputNumber(double n)
{
	return true;
}

bool UPortalToolBase::AbortUse()
{
	return true;
}

bool UPortalToolBase::HandleInvert()
{
	if (!IsInUse())
	{
		return false;
	}
	Inverted = !Inverted;
	SetupCursor();
	return true;
}

bool UPortalToolBase::HandleControlKey(bool pressed)
{
	return true;
}


UDoorTool::UDoorTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UDoorTool::BeginUse()
{
	if (HostID != 0 && !bHasBoringError)
	{
		Controller->ModumateCommand(
			FModumateCommand(Commands::kAddDoor)
			.Param(Parameters::kObjectID, HostID)
			.Param(Parameters::kLocation, RelativePos)
			.Param(Parameters::kQuaternion, RelativeRot)
			.Param(Parameters::kInverted,Inverted)
			.Param(Parameters::kAssembly, Assembly.Key)
		);
	}
	return false;
}

UWindowTool::UWindowTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UWindowTool::BeginUse()
{
	if (HostID != 0 && !bHasBoringError)
	{
		Controller->ModumateCommand(
			FModumateCommand(Commands::kAddWindow)
			.Param(Parameters::kObjectID, HostID)
			.Param(Parameters::kLocation, RelativePos)
			.Param(Parameters::kQuaternion, RelativeRot)
			.Param(Parameters::kInverted,Inverted)
			.Param(Parameters::kAssembly,Assembly.Key)
		);
	}
	return false;
}

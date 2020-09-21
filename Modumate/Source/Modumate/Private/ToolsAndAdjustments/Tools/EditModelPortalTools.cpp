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
	, HostID(MOD_ID_NONE)
	, bUseFixedOffset(true)
	, WorldPos(FVector::ZeroVector)
	, RelativePos(FVector2D::ZeroVector)
	, WorldRot(FQuat::Identity)
	, RelativeRot(FQuat::Identity)
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
	HostID = MOD_ID_NONE;
	Controller->DeselectAll();
	Controller->EMPlayerState->SetHoveredObject(nullptr);
	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;
	SetupCursor();
	Active = true;
	CreateObjectMode = EToolCreateObjectMode::Draw;
	return true;
}

void UPortalToolBase::SetAssemblyKey(const FBIMKey& InAssemblyKey)
{
	UEditModelToolBase::SetAssemblyKey(InAssemblyKey);
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
		if (lastHitHost)
		{
			lastHitHost->MarkDirty(EObjectDirtyFlags::Structure);
		}
	}

	CursorActor = Controller->GetWorld()->SpawnActor<ACompoundMeshActor>(ACompoundMeshActor::StaticClass());
	CursorActor->SetActorEnableCollision(false);

	auto *gameState = Controller->GetWorld()->GetGameState<AEditModelGameState_CPP>();
	const FBIMAssemblySpec *obAsm = gameState->Document.PresetManager.GetAssemblyByKey(GetToolMode(),AssemblyKey);
	bValidPortalConfig = true;

	float invMul = Inverted ? 1 : -1;

	CursorActor->MakeFromAssembly(*obAsm, FVector::OneVector, Inverted, false);

	// Store data used for previewing this object as if it were a fully-created MOI
	CursorActor->TempAssemblyKey = AssemblyKey;
	CursorActor->TempObjectToolMode = GetToolMode();
}


//TODO: Obtain correct assembly native size when it becomes better defined.
FVector UPortalToolBase::AssemblyNativeSize(const FBIMAssemblySpec& assembly) const
{
	if (ensure(assembly.Parts.Num() > 0))
	{
		return assembly.Parts[0].Mesh.NativeSize * Modumate::InchesToCentimeters;
	}
	else
	{
		return FVector::ZeroVector;
	}
}

bool UPortalToolBase::Deactivate()
{
	if (CursorActor != nullptr)
	{
		CursorActor->Destroy();
		CursorActor = nullptr;
	}

	if (HostID != MOD_ID_NONE)
	{
		auto *lastHitWall = Document->GetObjectById(HostID);
		if (lastHitWall)
		{
			lastHitWall->MarkDirty(EObjectDirtyFlags::Structure);
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
	static FName requesterName(TEXT("UPortalToolBase"));

	int32 lastHostID = HostID;
	FModumateObjectInstance *hitMOI = nullptr;
	const auto &snapCursor = Controller->EMPlayerState->SnappedCursor;
	FVector hitLoc = snapCursor.WorldPosition;

	HostID = MOD_ID_NONE;

	if (CursorActor && Controller->EMPlayerState->SnappedCursor.Visible)
	{
		hitMOI = Document->ObjectFromActor(Controller->EMPlayerState->SnappedCursor.Actor);

		FModumateObjectInstance *hitMOIParent = hitMOI ? hitMOI->GetParentObject() : nullptr;

		while (hitMOIParent)
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

	bool bHasRelativeTransform = hitMOI && UModumateObjectStatics::GetRelativeTransformOnPlanarObj(
		hitMOI, hitLoc, heightFromBottom, bUseFixedOffset, RelativePos, RelativeRot);

	bool bHasWorldTransform = bHasRelativeTransform && UModumateObjectStatics::GetWorldTransformOnPlanarObj(
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
		HostID = MOD_ID_NONE;
	}

	if (lastHostID != HostID)
	{
		if (hitMOI && bValidPortalConfig)
		{
			hitMOI->MarkDirty(EObjectDirtyFlags::Structure);
		}

		auto *lastHitHost = Document->GetObjectById(lastHostID);
		if (lastHitHost)
		{
			lastHitHost->MarkDirty(EObjectDirtyFlags::Structure);
		}
	}

	if (hitMOI && HostID)
	{
		CursorActor->SetActorHiddenInGame(false);
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

bool UPortalToolBase::BeginUse()
{
	const FBIMAssemblySpec* assembly = Document->PresetManager.GetAssemblyByKey(GetToolMode(), AssemblyKey);

	FModumateObjectInstance* parent = Document->GetObjectById(HostID);

	if (assembly == nullptr || parent == nullptr)
	{
		return false;
	}

	UWorld* world = parent->GetWorld();

	FVector worldPos(ForceInitToZero);
	FQuat worldRot(ForceInit);
	if (!UModumateObjectStatics::GetWorldTransformOnPlanarObj(parent,
		RelativePos, RelativeRot, worldPos, worldRot))
	{
		return false;
	}

	const bool bPaintTool = CreateObjectMode == EToolCreateObjectMode::Apply;

	TArray<TSharedPtr<FDelta>> deltas;
	Document->BeginUndoRedoMacro();

	if (bPaintTool)
	{
		const TArray<int32>& planeChildren = parent->GetChildIDs();
		if (planeChildren.Num() > 0)
		{
			int32 childId = planeChildren[0];
			const FModumateObjectInstance* childObj = Document->GetObjectById(childId);
			FMOIStateData deleteState = childObj->GetDataState();
			deleteState.StateType = EMOIDeltaType::Destroy;
			auto deleteDelta = MakeShared<FMOIDelta>();
			deleteDelta->AddCreateDestroyStates({ deleteState });

			deltas.Add(deleteDelta);
		}

	}

	int32 newParentId = HostID;
	if (!bPaintTool)
	{   // Create new metaplane to host portal.
		FBox bounds(ForceInit);
		FVector nativeSize(AssemblyNativeSize(*assembly));

		if (nativeSize.IsZero())
		{	// No supplied native size - use meshes:
			for (const auto* mesh : CursorActor->StaticMeshComps)
			{
				if (mesh != nullptr)
				{
					FVector minPoint(ForceInitToZero);
					FVector maxPoint(ForceInitToZero);
					mesh->GetLocalBounds(minPoint, maxPoint);
					FVector localPosition(mesh->GetRelativeTransform().GetTranslation());
					bounds += minPoint + localPosition;
					bounds += maxPoint + localPosition;
				}
			}
		}
		else
		{
			bounds.Max = nativeSize;
		}

		FTransform portalTransform(worldRot, worldPos, FVector::OneVector);

		TArray<FVector> metaPlanePoints =
			{ {bounds.Min.X, 0, bounds.Min.Z}, { bounds.Min.X, 0, bounds.Max.Z },
			{bounds.Max.X, 0, bounds.Max.Z}, { bounds.Max.X, 0, bounds.Min.Z } };


		for (auto& p : metaPlanePoints)
		{
			p = portalTransform.TransformPosition(p);
		}

		TArray<int32> metaGraphIds;
		if (Document->MakeMetaObject(world, metaPlanePoints, TArray<int32>(), EObjectType::OTMetaPlane, 0, metaGraphIds)
			&& metaGraphIds.Num() > 0)
		{
			newParentId = metaGraphIds[0];
			// Check for a single new face.
			if (Document->GetVolumeGraph().FindFace(newParentId) == nullptr
				|| (metaGraphIds.Num() > 1 && Document->GetVolumeGraph().FindFace(metaGraphIds[1]) != nullptr))
			{
				Document->EndUndoRedoMacro();
				return false;
			}
		}
		else
		{
			Document->EndUndoRedoMacro();
			return false;
		}
	}

	auto addPortal = MakeShared<FMOIDelta>();
	FMOIStateData portalData;
	portalData.StateType = EMOIDeltaType::Create;
	portalData.ObjectType = UModumateTypeStatics::ObjectTypeFromToolMode(GetToolMode());
	portalData.ParentID = HostID;
	portalData.Orientation = FQuat::Identity;
	portalData.ObjectAssemblyKey = AssemblyKey;
	portalData.ParentID = newParentId;
	portalData.ObjectID = Document->GetNextAvailableID();

	TArray<FMOIStateData> createStates(&portalData, 1);
	addPortal->AddCreateDestroyStates(createStates);

	deltas.Add(addPortal);

	Document->ApplyDeltas(deltas, world);
	Document->EndUndoRedoMacro();

	return true;
}

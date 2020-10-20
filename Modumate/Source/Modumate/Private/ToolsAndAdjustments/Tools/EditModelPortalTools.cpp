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
#include "Objects/Portal.h"

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
	, InstanceStampSize(FVector::ZeroVector)
	, InstanceBottomOffset(0.0f)
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

TArray<EEditViewModes> UPortalToolBase::GetRequiredEditModes() const
{
	return { EEditViewModes::ObjectEditing, EEditViewModes::MetaPlanes };
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

	const FBIMAssemblySpec *obAsm = GameState->Document.PresetManager.GetAssemblyByKey(GetToolMode(),AssemblyKey);
	bValidPortalConfig = true;

	float invMul = Inverted ? 1 : -1;

	CursorActor->MakeFromAssembly(*obAsm, FVector::OneVector, Inverted, false);

	// Store data used for previewing this object as if it were a fully-created MOI
	CursorActor->TempAssemblyKey = AssemblyKey;
	CursorActor->TempObjectToolMode = GetToolMode();
}

bool UPortalToolBase::CalculateNativeSize()
{
	const FBIMAssemblySpec* assembly = GameState->Document.PresetManager.GetAssemblyByKey(GetToolMode(), AssemblyKey);
	if (assembly == nullptr)
	{
		return false;
	}

	// Calculate the native size of the mesh
	FVector nativeSize(assembly->GetRiggedAssemblyNativeSize());
	if (!nativeSize.IsZero())
	{
		InstanceStampSize = nativeSize;
		return true;
	}
	else if (CursorActor)
	{
		// No supplied native size - use meshes:
		// TODO: don't rely on the CursorActor to compute a property of the assembly
		FBox bounds(ForceInitToZero);
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

		InstanceStampSize = bounds.GetSize();
		return true;
	}

	return false;
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

	bool bHasRelativeTransform = hitMOI && UModumateObjectStatics::GetRelativeTransformOnPlanarObj(
		hitMOI, hitLoc, GetInstanceBottomOffset(), bUseFixedOffset, RelativePos, RelativeRot);

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

	TArray<FDeltaPtr> deltas;
	Document->BeginUndoRedoMacro();

	if (bPaintTool)
	{
		const TArray<int32>& planeChildren = parent->GetChildIDs();
		if (planeChildren.Num() > 0)
		{
			int32 childId = planeChildren[0];
			const FModumateObjectInstance* childObj = Document->GetObjectById(childId);

			auto deleteDelta = MakeShared<FMOIDelta>();
			deleteDelta->AddCreateDestroyState(childObj->GetStateData(), EMOIDeltaType::Destroy);

			deltas.Add(deleteDelta);
		}
	}

	int32 newParentId = HostID;
	if (!bPaintTool)
	{
		FTransform portalTransform(worldRot, worldPos, FVector::OneVector);

		TArray<FVector> metaPlanePoints = {
			{0, 0, 0}, { 0, 0, InstanceStampSize.Z },
			{InstanceStampSize.X, 0, InstanceStampSize.Z}, { InstanceStampSize.X, 0, 0 }
		};

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

	FMOIPortalData portalInstanceData;

	FMOIStateData objectStateData(Document->GetNextAvailableID(), UModumateTypeStatics::ObjectTypeFromToolMode(GetToolMode()), newParentId);
	objectStateData.AssemblyKey = AssemblyKey;
	objectStateData.CustomData.SaveStructData(portalInstanceData);

	auto addPortal = MakeShared<FMOIDelta>();
	addPortal->AddCreateDestroyState(objectStateData, EMOIDeltaType::Create);

	deltas.Add(addPortal);

	Document->ApplyDeltas(deltas, world);
	Document->EndUndoRedoMacro();

	return true;
}

void UPortalToolBase::SetInstanceWidth(const float InWidth)
{
	InstanceStampSize.X = InWidth;
}

void UPortalToolBase::SetInstanceHeight(const float InHeight)
{
	InstanceStampSize.Z = InHeight;
}

void UPortalToolBase::SetInstanceBottomOffset(const float InBottomOffset)
{
	InstanceBottomOffset = InBottomOffset;
}

float UPortalToolBase::GetInstanceWidth() const
{
	return InstanceStampSize.X;
}

float UPortalToolBase::GetInstanceHeight() const
{
	return InstanceStampSize.Z;
}

float UPortalToolBase::GetInstanceBottomOffset() const
{
	return InstanceBottomOffset;
}

void UPortalToolBase::OnAssemblyChanged()
{
	Super::OnAssemblyChanged();

	if (Active)
	{
		SetupCursor();
	}
	CalculateNativeSize();
}


UDoorTool::UDoorTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstanceStampSize = FVector(91.44f, 0.0f, 203.2f);
	InstanceBottomOffset = 0.0f;
}


UWindowTool::UWindowTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstanceStampSize = FVector(50.f, 0.0f, 100.f);
	InstanceBottomOffset = 91.44f;
}

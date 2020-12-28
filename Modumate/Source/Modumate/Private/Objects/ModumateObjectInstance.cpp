// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "Objects/ModumateObjectInstance.h"

#include "Algo/Accumulate.h"
#include "Algo/Transform.h"
#include "Database/ModumateObjectDatabase.h"
#include "DocumentManagement/ModumateCommands.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Objects/MOIFactory.h"
#include "Drafting/ModumateDraftingElements.h"
#include "GameFramework/Actor.h"
#include "Graph/Graph2DTypes.h"
#include "Graph/Graph3DTypes.h"
#include "Materials/Material.h"
#include "Misc/OutputDeviceNull.h"
#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"
#include "UI/HUDDrawWidget.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/LineActor.h"
#include "UnrealClasses/ModumateObjectComponent_CPP.h"
#include "UObject/ConstructorHelpers.h"

using namespace Modumate;
class AEditModelPlayerController_CPP;

AModumateObjectInstance::AModumateObjectInstance()
{
}

void AModumateObjectInstance::BeginPlay()
{
	Super::BeginPlay();

	UWorld* world = GetWorld();
	auto gameState = ensure(world) ? world->GetGameState<AEditModelGameState_CPP>() : nullptr;
	Document = ensure(gameState) ? gameState->Document : nullptr;
}

void AModumateObjectInstance::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyMOI(true);

	Super::EndPlay(EndPlayReason);
}

void AModumateObjectInstance::SetupMOIComponent()
{
	if (!ensure(MeshActor.IsValid()))
	{
		return;
	}

	// (Optionally create) and register a MOI component that can be used for looking up MOIs by ID.
	UModumateObjectComponent_CPP *moiComponent = MeshActor->FindComponentByClass<UModumateObjectComponent_CPP>();
	if (moiComponent == nullptr)
	{
		moiComponent = NewObject<UModumateObjectComponent_CPP>(MeshActor.Get());
		moiComponent->RegisterComponent();
	}

	moiComponent->ObjectID = ID;
}

void AModumateObjectInstance::UpdateAssemblyFromKey()
{
	if ((CachedAssembly.UniqueKey() != StateData.AssemblyGUID) || (CachedAssembly.ObjectType == EObjectType::OTNone))
	{
		// Meta-objects don't have assemblies but we track MOI type in the CachedAssembly
		if (!Document->PresetManager.TryGetProjectAssemblyForPreset(StateData.ObjectType, StateData.AssemblyGUID, CachedAssembly))
		{
			CachedAssembly.ObjectType = StateData.ObjectType;
		}
		else
		{
			// Missing assemblies will get default keys, so CachedAssembly will be different if we had to fallback
			// NOTE: this is a temporary solution to avoid persistent ensures when BIM keys expire
			// Future work will deprecate BIM keys for GUIDs which will be much more stable (they may disappear but won't change)
			StateData.AssemblyGUID = CachedAssembly.UniqueKey();
		}
		bAssemblyLayersReversed = false;
	}
}

EObjectType AModumateObjectInstance::GetObjectType() const
{
	return CachedAssembly.ObjectType;
}

AModumateObjectInstance *AModumateObjectInstance::GetParentObject()
{
	return Document->GetObjectById(GetParentID());
}

const AModumateObjectInstance *AModumateObjectInstance::GetParentObject() const
{
	return Document->GetObjectById(GetParentID());
}

TArray<AModumateObjectInstance *> AModumateObjectInstance::GetChildObjects()
{
	TArray<AModumateObjectInstance *> ret;
	Algo::Transform(CachedChildIDs, ret, [this](int32 id) { return Document->GetObjectById(id); });
	return ret.FilterByPredicate([](AModumateObjectInstance *ob) {return ob != nullptr; });
}

TArray<const AModumateObjectInstance *> AModumateObjectInstance::GetChildObjects() const
{
	TArray<const AModumateObjectInstance * > ret;
	Algo::TransformIf(
		CachedChildIDs, 
		ret, 
		[this](int32 id) { return Document->GetObjectById(id) != nullptr; }, 
		[this](int32 id) { return Document->GetObjectById(id); }
	);

	return ret;
}

bool AModumateObjectInstance::HasChildID(int32 ChildID) const
{
	return CachedChildIDs.Contains(ChildID);
}

void AModumateObjectInstance::AddCachedChildID(int32 ChildID)
{
	const AModumateObjectInstance* childObj = Document->GetObjectById(ChildID);
	if (!bDestroyed && ensure(!HasChildID(ChildID) && childObj && (childObj->GetParentID() == ID)))
	{
		CachedChildIDs.Add(ChildID);
		MarkDirty(EObjectDirtyFlags::Structure);
	}
}

void AModumateObjectInstance::RemoveCachedChildID(int32 ChildID)
{
	if (!bDestroyed && ensure(HasChildID(ChildID)))
	{
		CachedChildIDs.Remove(ChildID);
		MarkDirty(EObjectDirtyFlags::Structure);
	}
}

void AModumateObjectInstance::GetConnectedIDs(TArray<int32> &connectedIDs) const
{
	connectedIDs.Reset();
	EObjectType objectType = GetObjectType();

	EGraph3DObjectType graph3DObjectType = UModumateTypeStatics::Graph3DObjectTypeFromObjectType(objectType);
	if (graph3DObjectType != EGraph3DObjectType::None)
	{
		const FGraph3D &graph = Document->GetVolumeGraph();
		switch (graph3DObjectType)
		{
		case EGraph3DObjectType::Vertex:
			if (const FGraph3DVertex *graphVertex = graph.FindVertex(ID))
			{
				for (FGraphSignedID directedEdgeID : graphVertex->ConnectedEdgeIDs)
				{
					connectedIDs.Add(FMath::Abs(directedEdgeID));
				}
			}
			return;
		case EGraph3DObjectType::Edge:
			if (const FGraph3DEdge *graphEdge = graph.FindEdge(ID))
			{
				connectedIDs.Add(graphEdge->StartVertexID);
				connectedIDs.Add(graphEdge->EndVertexID);

				for (const auto &connectedFace : graphEdge->ConnectedFaces)
				{
					connectedIDs.Add(FMath::Abs(connectedFace.FaceID));
				}
			}
			return;
		case EGraph3DObjectType::Face:
			if (const FGraph3DFace *graphFace = graph.FindFace(ID))
			{
				for (int32 vertexID : graphFace->VertexIDs)
				{
					connectedIDs.Add(vertexID);
				}

				for (FGraphSignedID directedEdgeID : graphFace->EdgeIDs)
				{
					connectedIDs.Add(FMath::Abs(directedEdgeID));
				}
			}
			return;
		default:
			break;
		}
	}

	EGraphObjectType graph2DObjectType = UModumateTypeStatics::Graph2DObjectTypeFromObjectType(objectType);
	if (graph2DObjectType != EGraphObjectType::None)
	{
		if (auto surfaceGraph = Document->FindSurfaceGraph(GetParentID()))
		{
			switch (objectType)
			{
			case EObjectType::OTSurfaceVertex:
				if (const FGraph2DVertex *vertex = surfaceGraph->FindVertex(ID))
				{
					for (FGraphSignedID directedEdgeID : vertex->Edges)
					{
						connectedIDs.Add(FMath::Abs(directedEdgeID));
					}
				}
				return;
			case EObjectType::OTSurfaceEdge:
				if (const FGraph2DEdge *edge = surfaceGraph->FindEdge(ID))
				{
					connectedIDs.Add(edge->StartVertexID);
					connectedIDs.Add(edge->EndVertexID);

					if (edge->LeftPolyID != MOD_ID_NONE)
					{
						connectedIDs.Add(edge->LeftPolyID);
					}
					if (edge->RightPolyID != MOD_ID_NONE)
					{
						connectedIDs.Add(edge->RightPolyID);
					}
				}
				return;
			case EObjectType::OTSurfacePolygon:
				if (const FGraph2DPolygon *polygon = surfaceGraph->FindPolygon(ID))
				{
					for (int32 vertexID : polygon->VertexIDs)
					{
						connectedIDs.Add(vertexID);
					}

					for (FGraphSignedID directedEdgeID : polygon->Edges)
					{
						connectedIDs.Add(FMath::Abs(directedEdgeID));
					}
				}
				return;
			default:
				break;
			}
		}
	}
}

void AModumateObjectInstance::GetConnectedMOIs(TArray<AModumateObjectInstance *> &connectedMOIs) const
{
	TArray<int32> connectedIDs;
	GetConnectedIDs(connectedIDs);

	connectedMOIs.Reset();
	for (int32 connectedID : connectedIDs)
	{
		if (AModumateObjectInstance *connectedMOI = Document->GetObjectById(connectedID))
		{
			connectedMOIs.Add(connectedMOI);
		}
	}
}

TArray<AModumateObjectInstance *> AModumateObjectInstance::GetAllDescendents()
{
	TArray<AModumateObjectInstance *> myKids = GetChildObjects();
	TArray<AModumateObjectInstance *> copyKids = myKids;
	for (auto &kid : copyKids)
	{
		myKids.Append(kid->GetAllDescendents());
	}
	return myKids;
}

TArray<const AModumateObjectInstance *> AModumateObjectInstance::GetAllDescendents() const
{
	TArray<const AModumateObjectInstance *> myKids = GetChildObjects();
	TArray<const AModumateObjectInstance *> copyKids = myKids;
	for (auto &kid : copyKids)
	{
		myKids.Append(kid->GetAllDescendents());
	}
	return myKids;
}

bool AModumateObjectInstance::OnSelected(bool bNewSelected)
{
	if (bSelected == bNewSelected)
	{
		return false;
	}

	bSelected = bNewSelected;
	return true;
}

bool AModumateObjectInstance::OnHovered(AEditModelPlayerController_CPP* Controller, bool bNewHovered)
{
	if (bHovered == bNewHovered)
	{
		return false;
	}

	bHovered = bNewHovered;
	return true;
}

void AModumateObjectInstance::RequestHidden(const FName &Requester, bool bRequestHidden)
{
	bool bWasRequestedHidden = IsRequestedHidden();

	if (bRequestHidden)
	{
		bool bDuplicateRequest = false;
		HideRequests.Add(Requester, &bDuplicateRequest);
		ensureAlwaysMsgf(!bDuplicateRequest, TEXT("%s already requested that MOI %d is hidden!"),
			*Requester.ToString(), ID);
	}
	else
	{
		int32 numRemoved = HideRequests.Remove(Requester);
		ensureAlwaysMsgf(numRemoved == 1, TEXT("%s revoked a hide request for MOI %d that was never set!"),
			*Requester.ToString(), ID);
	}

	if (bWasRequestedHidden != IsRequestedHidden())
	{
		UpdateVisuals();
	}
}

void AModumateObjectInstance::RequestCollisionDisabled(const FName &Requester, bool bRequestCollisionDisabled)
{
	bool bWasCollisionRequestedDisabled = IsCollisionRequestedDisabled();

	if (bRequestCollisionDisabled)
	{
		bool bDuplicateRequest = false;
		CollisionDisabledRequests.Add(Requester, &bDuplicateRequest);
		ensureMsgf(!bDuplicateRequest, TEXT("%s already requested that MOI %d has collision disabled!"),
			*Requester.ToString(), ID);
	}
	else
	{
		int32 numRemoved = CollisionDisabledRequests.Remove(Requester);
		ensureMsgf(numRemoved == 1, TEXT("%s revoked a collision disabled request for MOI %d that was never set!"),
			*Requester.ToString(), ID);
	}

	if (bWasCollisionRequestedDisabled != IsCollisionRequestedDisabled())
	{
		UpdateVisuals();
	}
}

void AModumateObjectInstance::UpdateVisuals()
{
	GetUpdatedVisuals(bVisible, bCollisionEnabled);
}

void AModumateObjectInstance::ClearAdjustmentHandles()
{
	for (auto &ah : AdjustmentHandles)
	{
		if (ah.IsValid())
		{
			ah->Destroy();
		}
	}

	AdjustmentHandles.Reset();
}

const TArray<TWeakObjectPtr<AAdjustmentHandleActor>> &AModumateObjectInstance::GetAdjustmentHandles() const
{
	return AdjustmentHandles;
}

bool AModumateObjectInstance::HasAdjustmentHandles() const
{
	return (AdjustmentHandles.Num() > 0);
}

AAdjustmentHandleActor *AModumateObjectInstance::MakeHandle(TSubclassOf<AAdjustmentHandleActor> HandleClass)
{
	if (!MeshActor.IsValid())
	{
		return nullptr;
	}

	FActorSpawnParameters spawnParams;
	spawnParams.Owner = MeshActor.Get();
	AAdjustmentHandleActor *handleActor = GetWorld()->SpawnActor<AAdjustmentHandleActor>(HandleClass.Get(), FTransform::Identity, spawnParams);

	handleActor->SourceMOI = this;
	handleActor->TargetMOI = this;
	handleActor->AttachToActor(MeshActor.Get(), FAttachmentTransformRules::KeepRelativeTransform);
	AdjustmentHandles.Add(handleActor);

	return handleActor;
}

bool AModumateObjectInstance::CanBeSplit() const
{
	switch (GetObjectType())
	{
	case EObjectType::OTWallSegment:
	case EObjectType::OTRailSegment:
	case EObjectType::OTCeiling:
	case EObjectType::OTFloorSegment:
		//case OTRoofSegment:
	case EObjectType::OTCabinet:
	case EObjectType::OTSystemPanel:
		return true;
	default:
		return false;
	}
}

float AModumateObjectInstance::CalculateThickness() const
{
	return CachedAssembly.CalculateThickness().AsWorldCentimeters();
}

void AModumateObjectInstance::MarkDirty(EObjectDirtyFlags NewDirtyFlags)
{
	for (EObjectDirtyFlags dirtyFlag : UModumateTypeStatics::OrderedDirtyFlags)
	{
		if (((NewDirtyFlags & dirtyFlag) == dirtyFlag) && !IsDirty(dirtyFlag))
		{
			DirtyFlags |= dirtyFlag;
			Document->RegisterDirtyObject(dirtyFlag, this, true);
			SetIsDynamic(true);
		}
	}
}

bool AModumateObjectInstance::IsDirty(EObjectDirtyFlags CheckDirtyFlags) const
{
	return ((DirtyFlags & CheckDirtyFlags) == CheckDirtyFlags);
}

bool AModumateObjectInstance::RouteCleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	bool bSuccess = false;

	// Only one flag being cleaned at a time is supported; check that here
	int32 flagsInt = (int32)DirtyFlag;
	if (!ensureAlwaysMsgf((DirtyFlag != EObjectDirtyFlags::None) && ((flagsInt & (flagsInt - 1)) == 0),
		TEXT("Passed multiple dirty flags to clean at once, invalid operation!")))
	{
		return bSuccess;
	}

	// Make sure that Document is at least valid first.
	if (!ensure(Document != nullptr))
	{
		return bSuccess;
	}

	if (IsDirty(DirtyFlag))
	{
		bool bValidObjectToClean = true;

		// We can't clean objects that were destroyed after they were marked dirty; they should have already cleared their dirty flags.
		if (!ensureAlways(!bDestroyed))
		{
			return false;
		}

		// If this object has a parent assigned, then it needs to have been created and cleaned, and contain this object, before it can clean itself.
		// NOTE: this is expected if we try to clean children before parents, like during loading, or undoing deletion of parented objects.
		if (GetParentID() != MOD_ID_NONE)
		{
			AModumateObjectInstance *parentObj = Document->GetObjectById(GetParentID());
			if (parentObj == nullptr)
			{
				bValidObjectToClean = false;
			}
			else
			{
				if (!parentObj->HasChildID(ID))
				{
					parentObj->AddCachedChildID(ID);
					bValidObjectToClean = false;
				}

				if (parentObj->IsDirty(DirtyFlag))
				{
					bValidObjectToClean = false;
				}
			}
		}

		// Now actually clean the object.
		if (bValidObjectToClean)
		{
			// Some default behavior to ensure that assemblies are up-to-date
			if (DirtyFlag == EObjectDirtyFlags::Structure)
			{
				UpdateAssemblyFromKey();
			}

			// Let the implementation handle all the specific cleaning
			bSuccess = CleanObject(DirtyFlag, OutSideEffectDeltas);
		}

		if (bSuccess)
		{
			// Now mark the children as dirty with the same flag, since this parent just cleaned itself.
			// Also take this opportunity to clean up the cached list of children, in case any had changed parents.
			int32 curNumChildren = CachedChildIDs.Num();
			for (int32 childIdx = curNumChildren - 1; childIdx >= 0; --childIdx)
			{
				int32 childID = CachedChildIDs[childIdx];
				AModumateObjectInstance* childObj = Document->GetObjectById(childID);
				if (childObj && !childObj->IsDestroyed() && (childObj->GetParentID() == ID))
				{
					childObj->MarkDirty(DirtyFlag);
				}
				else
				{
					CachedChildIDs.RemoveAt(childIdx);
				}
			}

			// And finally, register this object as not being dirty anymore.
			DirtyFlags &= ~DirtyFlag;
			Document->RegisterDirtyObject(DirtyFlag, this, false);
		}
	}

	if (DirtyFlags == EObjectDirtyFlags::None && !IsInPreviewMode())
	{
		SetIsDynamic(false);
	}
	return bSuccess;
}

FMOIStateData& AModumateObjectInstance::GetStateData()
{
	return StateData;
}

const FMOIStateData& AModumateObjectInstance::GetStateData() const
{
	return StateData;
}

bool AModumateObjectInstance::SetStateData(const FMOIStateData& NewStateData)
{
	if (ID == MOD_ID_NONE)
	{
		ID = NewStateData.ID;
	}
	// Don't support modifying existing objects' ID or ObjectType (except during initial creation); this would require recreating the entire object for now.
	else if (!ensure((StateData.ID == NewStateData.ID) && (StateData.ObjectType == NewStateData.ObjectType)))
	{
		return false;
	}

	// Skip setting data state that's identical to the current state
	if (NewStateData == StateData)
	{
		return false;
	}

	StateData = NewStateData;
	if (!UpdateInstanceData())
	{
		return false;
	}

	// TODO: selectively dirty the object based on implementation, based on which custom data was modified
	MarkDirty(EObjectDirtyFlags::All);
	return true;
}

bool AModumateObjectInstance::UpdateStateDataFromObject()
{
	// If there's custom data for this object, make sure the serialized representation is up-to-date from the object.
	UScriptStruct* customStructDef;
	const void* customStructPtr;
	if (GetInstanceDataStruct(customStructDef, customStructPtr))
	{
		return StateData.CustomData.SaveStructData(customStructDef, customStructPtr);
	}

	// Otherwise, the serialized representation doesn't need to be updated.
	return true;
}

bool AModumateObjectInstance::UpdateInstanceData()
{
	// If there's custom data for this object, make sure the object is up-to-date from the serialized representation.
	UScriptStruct* customStructDef;
	void* customStructPtr;
	bool bLoadedStructData = false;

	if (GetInstanceDataStruct(customStructDef, customStructPtr))
	{
		bLoadedStructData = StateData.CustomData.LoadStructData(customStructDef, customStructPtr);
	}

	if (bLoadedStructData)
	{
		PostLoadInstanceData();
	}

	return bLoadedStructData;
}

bool AModumateObjectInstance::GetInstanceDataStruct(UScriptStruct*& OutStructDef, void*& OutStructPtr)
{
	OutStructDef = nullptr;
	OutStructPtr = nullptr;

	static const FName instanceDataPropName(TEXT("InstanceData"));
	UClass* moiClass = GetClass();
	FProperty* instanceDataProp = moiClass->FindPropertyByName(instanceDataPropName);
	FStructProperty* structProp = CastField<FStructProperty>(instanceDataProp);
	if (structProp)
	{
		OutStructDef = structProp->Struct;
		OutStructPtr = structProp->ContainerPtrToValuePtr<void>(this);
	}

	return (OutStructDef != nullptr) && (OutStructPtr != nullptr);
}

bool AModumateObjectInstance::GetInstanceDataStruct(UScriptStruct*& OutStructDef, const void*& OutStructPtr) const
{
	void* mutableStructPtr;
	bool bSuccess = const_cast<AModumateObjectInstance*>(this)->GetInstanceDataStruct(OutStructDef, mutableStructPtr);
	OutStructPtr = const_cast<const void*>(mutableStructPtr);
	return bSuccess;
}

bool AModumateObjectInstance::BeginPreviewOperation()
{
	if (IsInPreviewMode())
	{
		return false;
	}

	bPreviewOperationMode = true;
	SetIsDynamic(true);
	return true;
}

bool AModumateObjectInstance::EndPreviewOperation()
{
	if (!IsInPreviewMode())
	{
		return false;
	}

	bPreviewOperationMode = false;
	MarkDirty(EObjectDirtyFlags::Structure);
	return true;
}

bool AModumateObjectInstance::IsInPreviewMode() const
{
	return bPreviewOperationMode;
}

int32 AModumateObjectInstance::GetParentID() const 
{ 
	return StateData.ParentID;
}

void AModumateObjectInstance::SetParentID(int32 NewParentID) 
{
	if (GetParentID() != NewParentID)
	{
		AModumateObjectInstance* oldParentObj = GetParentObject();

		StateData.ParentID = NewParentID;
		MarkDirty(EObjectDirtyFlags::Structure);

		if (oldParentObj)
		{
			oldParentObj->RemoveCachedChildID(ID);
		}

		if (AModumateObjectInstance* newParentObj = GetParentObject())
		{
			newParentObj->AddCachedChildID(ID);
		}
	}
}

const FBIMAssemblySpec &AModumateObjectInstance::GetAssembly() const
{
	return CachedAssembly;
}

void AModumateObjectInstance::SetAssemblyLayersReversed(bool bNewLayersReversed)
{
	if (bNewLayersReversed != bAssemblyLayersReversed)
	{
		CachedAssembly.ReverseLayers();
		bAssemblyLayersReversed = bNewLayersReversed;
	}
}

void AModumateObjectInstance::UpdateGeometry()
{
	const bool origDynamicStatus = GetIsDynamic();
	SetIsDynamic(true);
	UpdateDynamicGeometry();
	SetIsDynamic(origDynamicStatus);
}

void AModumateObjectInstance::RouteGetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
{
	outPoints.Reset();
	outLines.Reset();

	GetStructuralPointsAndLines(outPoints, outLines, bForSnapping, bForSelection);

	// Make sure that the output correctly references this MOI,
	// since some implementations may copy structure from other connected MOIs.
	for (FStructurePoint &point : outPoints)
	{
		point.ObjID = ID;
	}

	for (FStructureLine &line : outLines)
	{
		line.ObjID = ID;
	}
}

static const FName PartialActorDestructionRequest(TEXT("DestroyActorPartial"));

void AModumateObjectInstance::DestroyActor(bool bFullDelete)
{
	if (bFullDelete)
	{
		ClearAdjustmentHandles();

		if (MeshActor.IsValid())
		{
			TArray<AActor*> attachedActors;
			MeshActor->GetAttachedActors(attachedActors);
			for (auto* attachedActor : attachedActors)
			{
				attachedActor->Destroy();
			}

			MeshActor->Destroy();
			MeshActor.Reset();
		}
	}
	else
	{
		auto controller = GetWorld()->GetFirstPlayerController<AEditModelPlayerController_CPP>();
		ShowAdjustmentHandles(controller, false);

		RequestHidden(PartialActorDestructionRequest, true);
		RequestCollisionDisabled(PartialActorDestructionRequest, true);
	}
}

void AModumateObjectInstance::DestroyMOI(bool bFullDelete)
{
	if (!bDestroyed)
	{
		PreDestroy();

		CachedChildIDs.Reset();
		if (AModumateObjectInstance* parentObj = GetParentObject())
		{
			parentObj->RemoveCachedChildID(ID);
		}

		// Clear dirty flags, since we won't be able to clean the object later
		DirtyFlags = EObjectDirtyFlags::None;
		for (EObjectDirtyFlags dirtyFlag : UModumateTypeStatics::OrderedDirtyFlags)
		{
			Document->RegisterDirtyObject(dirtyFlag, this, false);
		}

		bDestroyed = true;
	}

	// DestroyActor is safe to call multiple times, and may be necessary if this was only partially deleted before being fully deleted
	DestroyActor(bFullDelete);
}

void AModumateObjectInstance::RestoreMOI()
{
	if (!ensure(bDestroyed))
	{
		return;
	}

	if (MeshActor.IsValid())
	{
		RequestHidden(PartialActorDestructionRequest, false);
		RequestCollisionDisabled(PartialActorDestructionRequest, false);
	}
	else
	{
		MeshActor = RestoreActor();
		SetupMOIComponent();
	}

	PostCreateObject(false);
}

void AModumateObjectInstance::PostCreateObject(bool bNewObject)
{
	if (bNewObject && ensure(!MeshActor.IsValid() && (StateData.ID != MOD_ID_NONE)))
	{
		UpdateAssemblyFromKey();
		MeshActor = CreateActor(FVector::ZeroVector, FQuat::Identity);
		SetupMOIComponent();
	}

	if (bDestroyed)
	{
		ensureAlways(!bNewObject);
		bDestroyed = false;
	}

	// TODO: distinguish errors due to missing instance data vs. mismatched types or versions
	UpdateInstanceData();

	// This isn't strictly necessary, but it will save some flag cleaning if the parent already exists at the time of creation.
	if (AModumateObjectInstance* parentObj = GetParentObject())
	{
		parentObj->AddCachedChildID(ID);
	}
	MarkDirty(EObjectDirtyFlags::All);
}

// FModumateObjectInstanceImplBase Implementation

FVector AModumateObjectInstance::GetLocation() const
{
	const AActor* moiActor = GetActor();
	return moiActor ? moiActor->GetActorLocation() : FVector::ZeroVector;
}

FQuat AModumateObjectInstance::GetRotation() const
{
	const AActor *moiActor = GetActor();
	return moiActor ? moiActor->GetActorQuat() : FQuat::Identity;
}

FTransform AModumateObjectInstance::GetWorldTransform() const
{
	return FTransform(GetRotation(), GetLocation());
}

FVector AModumateObjectInstance::GetCorner(int32 index) const
{
	return FVector::ZeroVector;
}

int32 AModumateObjectInstance::GetNumCorners() const
{
	return 0;
}

void AModumateObjectInstance::GetUpdatedVisuals(bool &bOutVisible, bool &bOutCollisionEnabled)
{
	AActor *moiActor = GetActor();
	auto *controller = moiActor ? moiActor->GetWorld()->GetFirstPlayerController<AEditModelPlayerController_CPP>() : nullptr;
	if (controller)
	{
		bool bEnabledByViewMode = controller->EMPlayerState->IsObjectTypeEnabledByViewMode(GetObjectType());
		bOutVisible = !IsRequestedHidden() && bEnabledByViewMode;
		bOutCollisionEnabled = !IsCollisionRequestedDisabled() && bEnabledByViewMode;
		moiActor->SetActorHiddenInGame(!bOutVisible);
		moiActor->SetActorEnableCollision(bOutCollisionEnabled);
	}
}

void AModumateObjectInstance::ShowAdjustmentHandles(AEditModelPlayerController_CPP* Controller, bool bShow)
{
	if (bShow && !HasAdjustmentHandles())
	{
		SetupAdjustmentHandles(Controller);
	}

	// By default, only show top-level handles, and allow them to hide or show their children appropriately.
	for (auto &ah : GetAdjustmentHandles())
	{
		if (ah.IsValid() && (ah->HandleParent == nullptr))
		{
			ah->SetEnabled(bShow);
		}
	}
}

void AModumateObjectInstance::OnAssemblyChanged()
{
	MarkDirty(EObjectDirtyFlags::Structure);
}

AActor *AModumateObjectInstance::RestoreActor()
{
	if (UWorld *world = GetWorld())
	{
		return CreateActor(FVector::ZeroVector, FQuat::Identity);
	}

	return nullptr;
}

AActor *AModumateObjectInstance::CreateActor(const FVector &loc, const FQuat &rot)
{
	UWorld* world = GetWorld();
	AEditModelGameMode_CPP* gameMode = world ? world->GetAuthGameMode<AEditModelGameMode_CPP>() : nullptr;
	if (gameMode)
	{
		DynamicMeshActor = world->SpawnActor<ADynamicMeshActor>(gameMode->DynamicMeshActorClass.Get(), FTransform(rot, loc));

		if (DynamicMeshActor.IsValid() && DynamicMeshActor->Mesh)
		{
			ECollisionChannel collisionObjType = UModumateTypeStatics::CollisionTypeFromObjectType(GetObjectType());
			DynamicMeshActor->Mesh->SetCollisionObjectType(collisionObjType);
		}
	}

	return DynamicMeshActor.Get();
}


void AModumateObjectInstance::PreDestroy()
{

}

bool AModumateObjectInstance::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	switch (DirtyFlag)
	{
	case EObjectDirtyFlags::Structure:
		SetupDynamicGeometry();
		break;
	case EObjectDirtyFlags::Visuals:
		UpdateVisuals();
		break;
	default:
		break;
	}

	return true;
}

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
#include "Quantities/QuantitiesManager.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/LineActor.h"
#include "UnrealClasses/ModumateObjectComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "ModumateCore/PrettyJSONWriter.h"


class AEditModelPlayerController;

AModumateObjectInstance::AModumateObjectInstance()
{
}

void AModumateObjectInstance::BeginPlay()
{
	Super::BeginPlay();

	UWorld* world = GetWorld();
	auto gameState = ensure(world) ? world->GetGameState<AEditModelGameState>() : nullptr;
	Document = ensure(gameState) ? gameState->Document : nullptr;
}

void AModumateObjectInstance::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyMOI(true);

	// Clear dirty flags, since we won't be able to clean the object later
	DirtyFlags = EObjectDirtyFlags::None;
	if (Document)
	{
		for (EObjectDirtyFlags dirtyFlag : UModumateTypeStatics::OrderedDirtyFlags)
		{
			Document->RegisterDirtyObject(dirtyFlag, this, false);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AModumateObjectInstance::SetupMOIComponent()
{
	if (!ensure(MeshActor.IsValid()))
	{
		return;
	}

	// (Optionally create) and register a MOI component that can be used for looking up MOIs by ID.
	UModumateObjectComponent *moiComponent = MeshActor->FindComponentByClass<UModumateObjectComponent>();
	if (moiComponent == nullptr)
	{
		moiComponent = NewObject<UModumateObjectComponent>(MeshActor.Get());
		moiComponent->RegisterComponent();
	}

	moiComponent->ObjectID = ID;
}

void AModumateObjectInstance::UpdateAssemblyFromKey()
{
	if ((CachedAssembly.UniqueKey() != StateData.AssemblyGUID) || (CachedAssembly.ObjectType == EObjectType::OTNone))
	{
		// Meta-objects don't have assemblies but we track MOI type in the CachedAssembly
		if (!Document->GetPresetCollection().TryGetProjectAssemblyForPreset(StateData.ObjectType, StateData.AssemblyGUID, CachedAssembly))
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
	AModumateObjectInstance* childObj = Document->GetObjectById(ChildID);
	if (!bDestroyed && ensure(!HasChildID(ChildID) && childObj && (childObj->GetParentID() == ID)))
	{
		CachedChildIDs.Add(ChildID);
		MarkDirty(EObjectDirtyFlags::Structure);

		// Make sure the new child inherits any dirty flags the parent already has, in case it wasn't already dirtied.
		childObj->MarkDirty(DirtyFlags);
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
		const FGraph3D& graph = *Document->FindVolumeGraph(ID);
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

				for (int32 containedFaceID : graphFace->ContainedFaceIDs)
				{
					if (const FGraph3DFace* containedFace = graph.FindFace(containedFaceID))
					{
						for (int32 vertexID : containedFace->VertexIDs)
						{
							connectedIDs.Add(vertexID);
						}

						for (FGraphSignedID directedEdgeID : containedFace->EdgeIDs)
						{
							connectedIDs.Add(FMath::Abs(directedEdgeID));
						}
					}
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
				if (const FGraph2DPolygon* polygon = surfaceGraph->FindPolygon(ID))
				{
					for (int32 vertexID : polygon->VertexIDs)
					{
						connectedIDs.Add(vertexID);
					}

					for (FGraphSignedID directedEdgeID : polygon->Edges)
					{
						connectedIDs.Add(FMath::Abs(directedEdgeID));
					}

					for (int32 containedPolyID : polygon->ContainedPolyIDs)
					{
						if (const FGraph2DPolygon* containedPoly = surfaceGraph->FindPolygon(containedPolyID))
						{
							for (int32 vertexID : containedPoly->VertexIDs)
							{
								connectedIDs.Add(vertexID);
							}

							for (FGraphSignedID directedEdgeID : containedPoly->Edges)
							{
								connectedIDs.Add(FMath::Abs(directedEdgeID));
							}
						}
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

bool AModumateObjectInstance::OnHovered(AEditModelPlayerController* Controller, bool bNewHovered)
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
		MarkDirty(EObjectDirtyFlags::Visuals);
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
		MarkDirty(EObjectDirtyFlags::Visuals);
	}
}

bool AModumateObjectInstance::TryUpdateVisuals()
{
	return GetUpdatedVisuals(bVisible, bCollisionEnabled);
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
	if (bDestroyed || Document == nullptr)
	{
		return;
	}

	for (EObjectDirtyFlags dirtyFlag : UModumateTypeStatics::OrderedDirtyFlags)
	{
		if (((NewDirtyFlags & dirtyFlag) == dirtyFlag) && !IsDirty(dirtyFlag))
		{
			DirtyFlags |= dirtyFlag;
			Document->RegisterDirtyObject(dirtyFlag, this, true);
			SetIsDynamic(true);
		}
	}

	// Recursively propagate dirty flags to children as early as possible,
	// since delaying this until the cleaning stage itself allows the middle of the tree to be initially skipped,
	// and the bottom of the tree to be redundantly cleaned. (i.e. modifying a MetaPlane and a SurfacePolygon in the same frame)
	for (int32 childID : CachedChildIDs)
	{
		if (auto childObj = Document->GetObjectById(childID))
		{
			childObj->MarkDirty(NewDirtyFlags);
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
		if (!ensure(!bDestroyed))
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

			if (bSuccess && !Document->IsPreviewingDeltas() &&
				(DirtyFlag == EObjectDirtyFlags::Structure || DirtyFlag == EObjectDirtyFlags::Mitering))
			{
				// Update use of quantities by this MOI.
				UpdateQuantities();
			}

		}

		if (bSuccess)
		{
			// Clean up the cached list of children, in case any had changed parents.
			int32 curNumChildren = CachedChildIDs.Num();
			for (int32 childIdx = curNumChildren - 1; childIdx >= 0; --childIdx)
			{
				int32 childID = CachedChildIDs[childIdx];
				AModumateObjectInstance* childObj = Document->GetObjectById(childID);
				if ((childObj == nullptr) || childObj->IsDestroyed() || (childObj->GetParentID() != ID))
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

	// TODO: distinguish errors due to missing instance data vs. mismatched types or versions
	UpdateInstanceData();

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
		// Reset the struct here, so that any containers in the instance data are cleared out before they are populated via deserialization.
		bLoadedStructData = StateData.CustomData.LoadStructData(customStructDef, customStructPtr, true);
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

bool AModumateObjectInstance::GetWebMOI(FString& OutJson) const
{
	// Get common data members
	FWebMOI webMOI;
	webMOI.ID = ID;
	webMOI.DisplayName = GetStateData().DisplayName;
	webMOI.Parent = GetParentID();
	webMOI.Type = GetObjectType();
	webMOI.Children = GetChildIDs();
	
	// Get custom data
	UScriptStruct* structDef;
	const void* structPtr;
	if (GetInstanceDataStruct(structDef, structPtr))
	{
		for (TFieldIterator<FProperty> it(structDef); it; ++it)
		{
			// TODO: initially we only support string properties
			FStrProperty* prop = CastField<FStrProperty>(*it);
			if (prop != nullptr)
			{
				// MOI subclasses add properties to WebProperties on construction with type and display name info
				// Properties not included in this map are not visible to the web
				const FWebMOIProperty* formProp = WebProperties.Find(it->GetName());
				if (formProp != nullptr)
				{
					FWebMOIProperty webProp = *formProp;
					webProp.Value = prop->GetPropertyValue_InContainer(structPtr);
					webMOI.Properties.Add(webProp.Name, webProp);
				}
			}
		}
	}

	return WriteJsonGeneric<FWebMOI>(OutJson, &webMOI);
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

void AModumateObjectInstance::RouteGetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines,
	bool bForSnapping, bool bForSelection, const FPlane& CullingPlane)
{
	// First, update the cached points/lines so they can be processed by the culling plane.
	// TODO: could these actually be cached so they don't need to be recomputed until the object is dirty?
	CachedStructurePoints.Reset();
	CachedStructureLines.Reset();
	GetStructuralPointsAndLines(CachedStructurePoints, CachedStructureLines, bForSnapping, bForSelection);

	outPoints.Reset();
	outLines.Reset();

	bool bUseCullingPlane = CullingPlane.IsValid() && (GetObjectType() != EObjectType::OTCutPlane);

	// Now, filter the cached points/lines based on the input culling plane (if any)
	for (FStructurePoint& point : CachedStructurePoints)
	{
		if (!bUseCullingPlane || (CullingPlane.PlaneDot(point.Point) > -PLANAR_DOT_EPSILON))
		{
			point.ObjID = ID;
			outPoints.Add(point);
		}
	}

	for (FStructureLine& line : CachedStructureLines)
	{
		line.ObjID = ID;

		if (!bUseCullingPlane)
		{
			outLines.Add(line);
		}
		else
		{
			// Only check line if either or both points are in front of the culling plane
			bool bP1IsBehind = CullingPlane.PlaneDot(line.P1) < -PLANAR_DOT_EPSILON;
			bool bP2IsBehind = CullingPlane.PlaneDot(line.P2) < -PLANAR_DOT_EPSILON;

			if (!bP1IsBehind && !bP2IsBehind)
			{
				outLines.Add(line);
			}
			else if (!bP1IsBehind || !bP2IsBehind)
			{
				FVector lineDir = (line.P2 - line.P1).GetSafeNormal();
				FVector intersect = FMath::RayPlaneIntersection(line.P1, lineDir, CullingPlane);
				FVector newP1 = bP1IsBehind ? intersect : line.P1;
				FVector newP2 = bP2IsBehind ? intersect : line.P2;

				outPoints.Add(FStructurePoint(intersect, lineDir, bP2IsBehind ? line.CP1 : line.CP2, INDEX_NONE, line.ObjID, EPointType::Corner));
				outLines.Add(FStructureLine(newP1, newP2, line.CP1, line.CP2, line.ObjID));
			}
		}
	}
}

static const FName PartialActorDestructionRequest(TEXT("DestroyActorPartial"));

void AModumateObjectInstance::DestroyActor(bool bFullDelete)
{
	bool bSelfMeshActor = (MeshActor.Get() == this);

	// Don't perform a full deletion if MeshActor is the MOI itself.
	// TODO: support this pattern better, now that MOIs are actors and this type of inheritance makes more sense for lightweight MOIs.
	if (bFullDelete && !bSelfMeshActor)
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
	else if (!bPartiallyDestroyed)
	{
		auto controller = GetWorld()->GetFirstPlayerController<AEditModelPlayerController>();
		ShowAdjustmentHandles(controller, false);

		RequestHidden(PartialActorDestructionRequest, true);
		RequestCollisionDisabled(PartialActorDestructionRequest, true);
		bPartiallyDestroyed = true;

		TryUpdateVisuals();
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
		bPartiallyDestroyed = false;
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

bool AModumateObjectInstance::GetUpdatedVisuals(bool &bOutVisible, bool &bOutCollisionEnabled)
{
	AActor *moiActor = GetActor();
	auto *controller = moiActor ? moiActor->GetWorld()->GetFirstPlayerController<AEditModelPlayerController>() : nullptr;

	// If we're missing the controller or player state, then we're either a dedicated server or are shutting down, so in either case hidden is fine.
	if ((controller == nullptr) || (controller->EMPlayerState == nullptr))
	{
		bOutVisible = false;
		bOutCollisionEnabled = false;
		return true;
	}

	bool bEnabledByViewMode = controller->EMPlayerState->IsObjectTypeEnabledByViewMode(GetObjectType());
	bOutVisible = !IsRequestedHidden() && bEnabledByViewMode;
	bOutCollisionEnabled = !IsCollisionRequestedDisabled() && bEnabledByViewMode;
	moiActor->SetActorHiddenInGame(!bOutVisible);
	moiActor->SetActorEnableCollision(bOutCollisionEnabled);

	return true;
}

void AModumateObjectInstance::ShowAdjustmentHandles(AEditModelPlayerController* Controller, bool bShow)
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

// Can be called when a new assembly is assigned or when an the existing assembly changes structure
void AModumateObjectInstance::OnAssemblyChanged()
{
	CachedAssembly.Reset();
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
	auto* gameMode = world ? world->GetGameInstance<UModumateGameInstance>()->GetEditModelGameMode() : nullptr;
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
	if (CachedQuantities.Num() != 0)
	{
		UModumateGameInstance* gameInstance = GetWorld()->GetGameInstance<UModumateGameInstance>();
		if (gameInstance != nullptr)
		{
			gameInstance->GetQuantitiesManager()->SetDirtyBit();
		}
	}
}

bool AModumateObjectInstance::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	switch (DirtyFlag)
	{
	case EObjectDirtyFlags::Structure:
		SetupDynamicGeometry();
		break;
	case EObjectDirtyFlags::Visuals:
		return TryUpdateVisuals();
	default:
		break;
	}

	return true;
}

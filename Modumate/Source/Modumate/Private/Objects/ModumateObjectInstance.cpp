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

FModumateObjectInstance::FModumateObjectInstance(UWorld *world, FModumateDocument *doc, const FMOIStateData& InStateData)
	: World(world)
	, Document(doc)
	, StateData_V2(InStateData)
	, ID(InStateData.ID)
{
	if (!ensureAlways(world && doc && (StateData_V2.ObjectType != EObjectType::OTNone) && (ID != MOD_ID_NONE)))
	{
		return;
	}

	// TODO: we should neither need to store the assembly by value, nor should we need to get it by ToolMode rather than ObjectType.
	EToolMode assemblyToolMode = UModumateTypeStatics::ToolModeFromObjectType(StateData_V2.ObjectType);
	const FBIMAssemblySpec* existingAssembly = Document->PresetManager.GetAssemblyByKey(assemblyToolMode, StateData_V2.AssemblyKey);
	if (existingAssembly)
	{
		CachedAssembly = *existingAssembly;
	}
	else
	{
		CachedAssembly.ObjectType = StateData_V2.ObjectType;
	}

	Implementation = FMOIFactory::MakeMOIImplementation(StateData_V2.ObjectType, this);
	if (ensureAlways(Implementation))
	{
		MeshActor = Implementation->CreateActor(world, FVector::ZeroVector, FQuat::Identity);
		SetupMOIComponent();
		UpdateAssemblyFromKey();
	}
}

void FModumateObjectInstance::SetupMOIComponent()
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

void FModumateObjectInstance::UpdateAssemblyFromKey()
{
	if (CachedAssembly.UniqueKey() != StateData_V2.AssemblyKey)
	{
		// TODO: we should neither need to store the assembly by value, nor should we need to get it by ToolMode rather than ObjectType.
		EToolMode assemblyToolMode = UModumateTypeStatics::ToolModeFromObjectType(StateData_V2.ObjectType);
		const FBIMAssemblySpec* existingAssembly = Document->PresetManager.GetAssemblyByKey(assemblyToolMode, StateData_V2.AssemblyKey);
		if (existingAssembly)
		{
			CachedAssembly = *existingAssembly;
		}
		else
		{
			CachedAssembly = FBIMAssemblySpec();
			CachedAssembly.RootPreset = StateData_V2.AssemblyKey;
			CachedAssembly.ObjectType = StateData_V2.ObjectType;
		}

		bAssemblyLayersReversed = false;
	}
}

EObjectType FModumateObjectInstance::GetObjectType() const
{
	return CachedAssembly.ObjectType;
}

FModumateObjectInstance::~FModumateObjectInstance()
{
	Destroy();

	if (Implementation)
	{
		delete Implementation;
		Implementation = nullptr;
	}
}

void FModumateObjectInstance::Destroy()
{
	if (!bDestroyed)
	{
		if (Implementation)
		{
			Implementation->Destroy();
		}

		CachedChildIDs.Reset();
		if (FModumateObjectInstance* parentObj = GetParentObject())
		{
			parentObj->RemoveCachedChildID(ID);
		}

		// If we need to do anything else during destruction, like cached its destroyed state, that would go here.
		DestroyActor();

		// Clear dirty flags, since we won't be able to clean the object later
		DirtyFlags = EObjectDirtyFlags::None;
		for (EObjectDirtyFlags dirtyFlag : UModumateTypeStatics::OrderedDirtyFlags)
		{
			Document->RegisterDirtyObject(dirtyFlag, this, false);
		}

		bDestroyed = true;
	}
}

void FModumateObjectInstance::SetParentObject(FModumateObjectInstance *newParentObj, bool bForceUpdate)
{
	int32 newParentID = newParentObj ? newParentObj->ID : 0;
	if ((GetParentID() != newParentID) || bForceUpdate)
	{
		SetParentID(newParentID);
	}
}

FModumateObjectInstance *FModumateObjectInstance::GetParentObject()
{
	return Document->GetObjectById(GetParentID());
}

const FModumateObjectInstance *FModumateObjectInstance::GetParentObject() const
{
	return Document->GetObjectById(GetParentID());
}

TArray<FModumateObjectInstance *> FModumateObjectInstance::GetChildObjects()
{
	TArray<FModumateObjectInstance *> ret;
	Algo::Transform(CachedChildIDs, ret, [this](int32 id) { return Document->GetObjectById(id); });
	return ret.FilterByPredicate([](FModumateObjectInstance *ob) {return ob != nullptr; });
}

TArray<const FModumateObjectInstance *> FModumateObjectInstance::GetChildObjects() const
{
	TArray<const FModumateObjectInstance * > ret;
	Algo::TransformIf(
		CachedChildIDs, 
		ret, 
		[this](int32 id) { return Document->GetObjectById(id) != nullptr; }, 
		[this](int32 id) { return Document->GetObjectById(id); }
	);

	return ret;
}

bool FModumateObjectInstance::HasChildID(int32 ChildID) const
{
	return CachedChildIDs.Contains(ChildID);
}

void FModumateObjectInstance::AddCachedChildID(int32 ChildID)
{
	const FModumateObjectInstance* childObj = Document->GetObjectById(ChildID);
	if (ensure(!HasChildID(ChildID) && childObj && (childObj->GetParentID() == ID)))
	{
		CachedChildIDs.Add(ChildID);
		MarkDirty(EObjectDirtyFlags::Structure);
	}
}

void FModumateObjectInstance::RemoveCachedChildID(int32 ChildID)
{
	if (ensure(HasChildID(ChildID)))
	{
		CachedChildIDs.Remove(ChildID);
		MarkDirty(EObjectDirtyFlags::Structure);
	}
}

bool FModumateObjectInstance::AddChild_DEPRECATED(FModumateObjectInstance *child, bool bUpdateChildHierarchy)
{
	if (child)
	{
		if (child->GetParentID() == ID)
		{
			return false;
		}

		if (FModumateObjectInstance *oldParent = child->GetParentObject())
		{
			oldParent->RemoveChild_DEPRECATED(child, bUpdateChildHierarchy);
		}

		child->SetParentID(ID);

		if (CachedChildIDs.AddUnique(child->ID) != INDEX_NONE)
		{
			MarkDirty(EObjectDirtyFlags::All);
		}

		if (bUpdateChildHierarchy)
		{
			child->MarkDirty(EObjectDirtyFlags::All);
		}

		return true;
	}

	return false;
}

bool FModumateObjectInstance::RemoveChild_DEPRECATED(FModumateObjectInstance *child, bool bUpdateChildHierarchy)
{
	if (child)
	{
		if (child->GetParentID() != ID)
		{
			return false;
		}

		child->SetParentID(MOD_ID_NONE);

		if (CachedChildIDs.Remove(child->ID) > 0)
		{
			MarkDirty(EObjectDirtyFlags::All);
		}

		if (bUpdateChildHierarchy)
		{
			child->MarkDirty(EObjectDirtyFlags::All);
		}

		return true;
	}

	return false;
}

void FModumateObjectInstance::GetConnectedIDs(TArray<int32> &connectedIDs) const
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

void FModumateObjectInstance::GetConnectedMOIs(TArray<FModumateObjectInstance *> &connectedMOIs) const
{
	TArray<int32> connectedIDs;
	GetConnectedIDs(connectedIDs);

	connectedMOIs.Reset();
	for (int32 connectedID : connectedIDs)
	{
		if (FModumateObjectInstance *connectedMOI = Document->GetObjectById(connectedID))
		{
			connectedMOIs.Add(connectedMOI);
		}
	}
}

TArray<FModumateObjectInstance *> FModumateObjectInstance::GetAllDescendents()
{
	TArray<FModumateObjectInstance *> myKids = GetChildObjects();
	TArray<FModumateObjectInstance *> copyKids = myKids;
	for (auto &kid : copyKids)
	{
		myKids.Append(kid->GetAllDescendents());
	}
	return myKids;
}

TArray<const FModumateObjectInstance *> FModumateObjectInstance::GetAllDescendents() const
{
	TArray<const FModumateObjectInstance *> myKids = GetChildObjects();
	TArray<const FModumateObjectInstance *> copyKids = myKids;
	for (auto &kid : copyKids)
	{
		myKids.Append(kid->GetAllDescendents());
	}
	return myKids;
}

void FModumateObjectInstance::OnSelected(bool bNewSelected)
{
	if (bSelected != bNewSelected)
	{
		bSelected = bNewSelected;

		Implementation->OnSelected(bNewSelected);
	}
}

void FModumateObjectInstance::MouseHoverActor(AEditModelPlayerController_CPP *controller, bool EnableHover)
{
	if (bHovered != EnableHover)
	{
		bHovered = EnableHover;

		Implementation->OnCursorHoverActor(controller, EnableHover);
	}
}

bool FModumateObjectInstance::IsSelectableByUser() const
{
	return Implementation && Implementation->IsSelectableByUser();
}

bool FModumateObjectInstance::ShowStructureOnSelection() const
{
	return Implementation && Implementation->ShowStructureOnSelection();
}

bool FModumateObjectInstance::UseStructureDataForCollision() const
{
	return Implementation && Implementation->UseStructureDataForCollision();
}

UMaterialInterface *FModumateObjectInstance::GetMaterial()
{
	return Implementation->GetMaterial();
}

void FModumateObjectInstance::SetMaterial(UMaterialInterface *mat)
{
	Implementation->SetMaterial(mat);
}

void FModumateObjectInstance::RequestHidden(const FName &Requester, bool bRequestHidden)
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
		UpdateVisibilityAndCollision();
	}
}

void FModumateObjectInstance::RequestCollisionDisabled(const FName &Requester, bool bRequestCollisionDisabled)
{
	bool bWasCollisionRequestedDisabled = IsCollisionRequestedDisabled();

	if (bRequestCollisionDisabled)
	{
		bool bDuplicateRequest = false;
		CollisionDisabledRequests.Add(Requester, &bDuplicateRequest);
		ensureAlwaysMsgf(!bDuplicateRequest, TEXT("%s already requested that MOI %d has collision disabled!"),
			*Requester.ToString(), ID);
	}
	else
	{
		int32 numRemoved = CollisionDisabledRequests.Remove(Requester);
		ensureAlwaysMsgf(numRemoved == 1, TEXT("%s revoked a collision disabled request for MOI %d that was never set!"),
			*Requester.ToString(), ID);
	}

	if (bWasCollisionRequestedDisabled != IsCollisionRequestedDisabled())
	{
		UpdateVisibilityAndCollision();
	}
}

void FModumateObjectInstance::UpdateVisibilityAndCollision()
{
	Implementation->UpdateVisibilityAndCollision(bVisible, bCollisionEnabled);
}

void FModumateObjectInstance::ShowAdjustmentHandles(AEditModelPlayerController_CPP *Controller, bool bShow)
{
	if (bShow && !HasAdjustmentHandles() && ensure(Implementation))
	{
		Implementation->SetupAdjustmentHandles(Controller);
	}

	Implementation->ShowAdjustmentHandles(Controller, bShow);
}

void FModumateObjectInstance::ClearAdjustmentHandles()
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

const TArray<TWeakObjectPtr<AAdjustmentHandleActor>> &FModumateObjectInstance::GetAdjustmentHandles() const
{
	return AdjustmentHandles;
}

bool FModumateObjectInstance::HasAdjustmentHandles() const
{
	return (AdjustmentHandles.Num() > 0);
}

AAdjustmentHandleActor *FModumateObjectInstance::MakeHandle(TSubclassOf<AAdjustmentHandleActor> HandleClass)
{
	if (!MeshActor.IsValid() || !World.IsValid())
	{
		return nullptr;
	}

	FActorSpawnParameters spawnParams;
	spawnParams.Owner = MeshActor.Get();
	AAdjustmentHandleActor *handleActor = World->SpawnActor<AAdjustmentHandleActor>(HandleClass.Get(), FTransform::Identity, spawnParams);

	handleActor->SourceMOI = this;
	handleActor->TargetMOI = this;
	handleActor->AttachToActor(MeshActor.Get(), FAttachmentTransformRules::KeepRelativeTransform);
	AdjustmentHandles.Add(handleActor);

	return handleActor;
}

bool FModumateObjectInstance::CanBeSplit() const
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

float FModumateObjectInstance::CalculateThickness() const
{
	return CachedAssembly.CalculateThickness().AsWorldCentimeters();
}

FVector FModumateObjectInstance::GetNormal() const
{
	return Implementation->GetNormal();
}

void FModumateObjectInstance::MarkDirty(EObjectDirtyFlags NewDirtyFlags)
{
	for (EObjectDirtyFlags dirtyFlag : UModumateTypeStatics::OrderedDirtyFlags)
	{
		if (((NewDirtyFlags & dirtyFlag) == dirtyFlag) && !IsDirty(dirtyFlag))
		{
			DirtyFlags |= dirtyFlag;
			Document->RegisterDirtyObject(dirtyFlag, this, true);
			Implementation->SetIsDynamic(true);
		}
	}
}

bool FModumateObjectInstance::IsDirty(EObjectDirtyFlags CheckDirtyFlags) const
{
	return ((DirtyFlags & CheckDirtyFlags) == CheckDirtyFlags);
}

bool FModumateObjectInstance::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
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

		// If we have a parent assigned, then by assume that
		// it needs to be exist and be clean before we can clean ourselves.
		// NOTE: this is expected if we try to clean children before parents, like during loading.
		if (GetParentID() != MOD_ID_NONE)
		{
			FModumateObjectInstance *parentObj = Document->GetObjectById(GetParentID());
			if ((parentObj == nullptr) || parentObj->IsDirty(DirtyFlag))
			{
				bValidObjectToClean = false;
			}
			else if (!parentObj->HasChildID(ID))
			{
				parentObj->AddCachedChildID(ID);
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
			bSuccess = Implementation->CleanObject(DirtyFlag, OutSideEffectDeltas);
		}

		if (bSuccess)
		{
			// Now mark the children as dirty with the same flag, since this parent just cleaned itself.
			// Also take this opportunity to clean up the cached list of children, in case any had changed parents.
			int32 curNumChildren = CachedChildIDs.Num();
			for (int32 childIdx = curNumChildren - 1; childIdx >= 0; --childIdx)
			{
				int32 childID = CachedChildIDs[childIdx];
				FModumateObjectInstance* childObj = Document->GetObjectById(childID);
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

	if (DirtyFlags == EObjectDirtyFlags::None && !GetIsInPreviewMode())
	{
		Implementation->SetIsDynamic(false);
	}
	return bSuccess;
}

bool FModumateObjectInstance::SetDataState_DEPRECATED(const FMOIStateData_DEPRECATED &DataState)
{
	CurrentState_DEPRECATED = DataState;
	PreviewState_DEPRECATED = DataState;
	MarkDirty(EObjectDirtyFlags::Structure);

	return true;
}

FMOIStateData_DEPRECATED &FModumateObjectInstance::GetDataState_DEPRECATED()
{
	return GetIsInPreviewMode() ? PreviewState_DEPRECATED : CurrentState_DEPRECATED;
}

const FMOIStateData_DEPRECATED &FModumateObjectInstance::GetDataState_DEPRECATED() const
{
	return GetIsInPreviewMode() ? PreviewState_DEPRECATED : CurrentState_DEPRECATED;
}

FMOIStateData& FModumateObjectInstance::GetStateData()
{
	return StateData_V2;
}

const FMOIStateData& FModumateObjectInstance::GetStateData() const
{
	return StateData_V2;
}

bool FModumateObjectInstance::SetStateData(const FMOIStateData& NewStateData)
{
	// Don't support modifying existing objects' ID or ObjectType (except during initial creation); this would require recreating the entire object for now.
	if (!ensure((StateData_V2.ID == NewStateData.ID) && (StateData_V2.ObjectType == NewStateData.ObjectType)))
	{
		return false;
	}

	// Skip setting data state that's identical to the current state
	if (NewStateData == StateData_V2)
	{
		return false;
	}

	StateData_V2 = NewStateData;
	if (!UpdateInstanceData())
	{
		return false;
	}

	// TODO: selectively dirty the object based on implementation, based on which custom data was modified
	MarkDirty(EObjectDirtyFlags::All);
	return true;
}

bool FModumateObjectInstance::UpdateStateDataFromObject()
{
	// If there's custom data for this object, make sure the serialized representation is up-to-date from the object.
	UScriptStruct* customStructDef;
	const void* customStructPtr;
	if (GetTypedInstanceData(customStructDef, customStructPtr))
	{
		return StateData_V2.CustomData.SaveStructData(customStructDef, customStructPtr);
	}

	// Otherwise, the serialized representation doesn't need to be updated.
	return true;
}

bool FModumateObjectInstance::UpdateInstanceData()
{
	// If there's custom data for this object, make sure the object is up-to-date from the serialized representation.
	UScriptStruct* customStructDef;
	void* customStructPtr;
	if (GetTypedInstanceData(customStructDef, customStructPtr))
	{
		return StateData_V2.CustomData.LoadStructData(customStructDef, customStructPtr);
	}

	// Otherwise, the object doesn't need to be updated.
	return true;
}

bool FModumateObjectInstance::GetTypedInstanceData(UScriptStruct*& OutStructDef, void*& OutStructPtr)
{
	OutStructDef = nullptr;
	OutStructPtr = nullptr;

	if (Implementation)
	{
		Implementation->GetTypedInstanceData(OutStructDef, OutStructPtr);
	}

	return (OutStructDef != nullptr) && (OutStructPtr != nullptr);
}

bool FModumateObjectInstance::GetTypedInstanceData(UScriptStruct*& OutStructDef, const void*& OutStructPtr) const
{
	void* mutableStructPtr;
	bool bSuccess = const_cast<FModumateObjectInstance*>(this)->GetTypedInstanceData(OutStructDef, mutableStructPtr);
	OutStructPtr = const_cast<const void*>(mutableStructPtr);
	return bSuccess;
}

bool FModumateObjectInstance::BeginPreviewOperation_DEPRECATED()
{
	if (!ensureAlways(!GetIsInPreviewMode()))
	{
		return false;
	}
	PreviewState_DEPRECATED = CurrentState_DEPRECATED;
	bPreviewOperationMode = true;
	Implementation->SetIsDynamic(true);
	return true;
}

bool FModumateObjectInstance::EndPreviewOperation_DEPRECATED()
{
	if (!ensureAlways(GetIsInPreviewMode()))
	{
		return false;
	}
	bPreviewOperationMode = false;
	MarkDirty(EObjectDirtyFlags::Structure);
	return true;
}

bool FModumateObjectInstance::GetIsInPreviewMode() const
{
	return bPreviewOperationMode;
}

int32 FModumateObjectInstance::GetParentID() const 
{ 
	return StateData_V2.ParentID;
}

void FModumateObjectInstance::SetParentID(int32 NewParentID) 
{
	if (GetParentID() != NewParentID)
	{
		FModumateObjectInstance* oldParentObj = GetParentObject();

		StateData_V2.ParentID = NewParentID;
		MarkDirty(EObjectDirtyFlags::Structure);

		if (oldParentObj)
		{
			oldParentObj->RemoveCachedChildID(ID);
		}

		if (FModumateObjectInstance* newParentObj = GetParentObject())
		{
			newParentObj->AddCachedChildID(ID);
		}
	}
}

const FBIMAssemblySpec &FModumateObjectInstance::GetAssembly() const
{
	return CachedAssembly;
}

void FModumateObjectInstance::SetAssemblyLayersReversed(bool bNewLayersReversed)
{
	if (bNewLayersReversed != bAssemblyLayersReversed)
	{
		CachedAssembly.ReverseLayers();
		bAssemblyLayersReversed = bNewLayersReversed;
	}
}

void FModumateObjectInstance::SetControlPointIndex(int32 IndexNum, int32 IndexVal)
{
	if (ensureAlways(GetDataState_DEPRECATED().ControlIndices.Num() > IndexNum))
	{
		GetDataState_DEPRECATED().ControlIndices[IndexNum] = IndexVal;
	}
}

int32 FModumateObjectInstance::GetControlPointIndex(int32 IndexNum) const
{
	ensureAlways(IndexNum < GetDataState_DEPRECATED().ControlIndices.Num());
	return GetDataState_DEPRECATED().ControlIndices[IndexNum];
}

const TArray<int32> &FModumateObjectInstance::GetControlPointIndices() const
{
	return GetDataState_DEPRECATED().ControlIndices;
}

void FModumateObjectInstance::AddControlPointIndex(int32 Index)
{
	GetDataState_DEPRECATED().ControlIndices.Add(Index);
}

void FModumateObjectInstance::SetControlPointIndices(const TArray<int32> &NewControlPointIndices)
{
	GetDataState_DEPRECATED().ControlIndices = NewControlPointIndices;
}

void FModumateObjectInstance::SetupGeometry()
{
	Implementation->SetupDynamicGeometry();
}

void FModumateObjectInstance::UpdateGeometry()
{
	const bool origDynamicStatus = Implementation->GetIsDynamic();
	Implementation->SetIsDynamic(true);
	Implementation->UpdateDynamicGeometry();
	Implementation->SetIsDynamic(origDynamicStatus);
}

void FModumateObjectInstance::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
{
	outPoints.Reset();
	outLines.Reset();

	Implementation->GetStructuralPointsAndLines(outPoints, outLines, bForSnapping, bForSelection);

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

void FModumateObjectInstance::AddDraftingLines(UHUDDrawWidget *HUDDrawWidget)
{
	Implementation->AddDraftingLines(HUDDrawWidget);
}

void FModumateObjectInstance::GetDraftingLines(const TSharedPtr<FDraftingComposite> &ParentPage, const FPlane &Plane, const FVector &AxisX, const FVector &AxisY, const FVector &Origin, const FBox2D &BoundingBox, TArray<TArray<FVector>> &OutPerimeters) const
{
	Implementation->GetDraftingLines(ParentPage, Plane, AxisX, AxisY, Origin, BoundingBox, OutPerimeters);
}

FVector FModumateObjectInstance::GetCorner(int32 index) const
{
	return Implementation->GetCorner(index);
}

int32 FModumateObjectInstance::GetNumCorners() const
{
	return Implementation->GetNumCorners();
}

const ILayeredObject* FModumateObjectInstance::GetLayeredInterface() const
{
	return Implementation->GetLayeredInterface();
}

const IMiterNode* FModumateObjectInstance::GetMiterInterface() const
{
	return Implementation->GetMiterInterface();
}

ISceneCaptureObject* FModumateObjectInstance::GetSceneCaptureInterface()
{
	return Implementation->GetSceneCaptureInterface();
}

void FModumateObjectInstance::DestroyActor()
{
	if (MeshActor.IsValid())
	{
		ClearAdjustmentHandles();

		TArray<AActor*> attachedActors;
		MeshActor->GetAttachedActors(attachedActors);
		for (auto *attachedActor : attachedActors)
		{
			attachedActor->Destroy();
		}

		MeshActor->Destroy();
		MeshActor.Reset();
	}
}

void FModumateObjectInstance::RestoreActor()
{
	DestroyActor();
	MeshActor = Implementation->RestoreActor();
	SetupMOIComponent();
}

void FModumateObjectInstance::PostCreateObject(bool bNewObject)
{
	if (bDestroyed)
	{
		ensureAlways(!bNewObject);
		bDestroyed = false;
	}

	// TODO: distinguish errors due to missing instance data vs. mismatched types or versions
	UpdateInstanceData();

	// This isn't strictly necessary, but it will save some flag cleaning if the parent already exists at the time of creation.
	if (FModumateObjectInstance* parentObj = GetParentObject())
	{
		parentObj->AddCachedChildID(ID);
	}
	MarkDirty(EObjectDirtyFlags::All);

	Implementation->PostCreateObject(bNewObject);
}

bool FModumateObjectInstance::GetInvertedState(FMOIStateData& OutState) const
{
	return Implementation->GetInvertedState(OutState);
}

bool FModumateObjectInstance::GetTransformedLocationState(const FTransform Transform, FMOIStateData& OutState) const
{
	return Implementation->GetTransformedLocationState(Transform, OutState);
}

FVector FModumateObjectInstance::GetObjectLocation() const
{
	return Implementation->GetLocation();
}

FQuat FModumateObjectInstance::GetObjectRotation() const
{
	return Implementation->GetRotation();
}

FTransform FModumateObjectInstance::GetWorldTransform() const
{
	return Implementation->GetWorldTransform();
}

bool FModumateObjectInstance::HasProperty(EBIMValueScope Scope, const FBIMNameType &Name) const
{
	return GetDataState_DEPRECATED().ObjectProperties.HasProperty(Scope, Name);
}

FModumateCommandParameter FModumateObjectInstance::GetProperty(EBIMValueScope Scope, const FBIMNameType &Name) const
{
	return GetDataState_DEPRECATED().ObjectProperties.GetProperty(Scope, Name);
}

const FBIMPropertySheet &FModumateObjectInstance::GetProperties() const
{
	return GetDataState_DEPRECATED().ObjectProperties;
}

void FModumateObjectInstance::SetProperty(EBIMValueScope Scope, const FBIMNameType &Name, const FModumateCommandParameter &Param)
{
	GetDataState_DEPRECATED().ObjectProperties.SetProperty(Scope, Name, Param);
}

void FModumateObjectInstance::SetAllProperties(const FBIMPropertySheet &NewProperties)
{
	GetDataState_DEPRECATED().ObjectProperties = NewProperties;
}

// data records are USTRUCTs used in serialization and clipboard operations
FMOIDataRecord_DEPRECATED FModumateObjectInstance::AsDataRecord_DEPRECATED() const
{
	FMOIDataRecord_DEPRECATED ret;
	ret.ID = ID;
	ret.ObjectType = GetObjectType();
	ret.AssemblyKey = CachedAssembly.UniqueKey().ToString();
	ret.ParentID = GetParentID();
	ret.ChildIDs = CachedChildIDs;
	ret.Location = GetObjectLocation();
	ret.Rotation = GetObjectRotation().Rotator();
	ret.UVAnchor = FVector::ZeroVector;
	ret.ControlIndices = CurrentState_DEPRECATED.ControlIndices;
	ret.Extents = CurrentState_DEPRECATED.Extents;
	CurrentState_DEPRECATED.ObjectProperties.ToStringMap(ret.ObjectProperties);
	ret.ObjectInverted = CurrentState_DEPRECATED.bObjectInverted;
	return ret;
}

// FModumateObjectInstanceImplBase Implementation

FVector FModumateObjectInstanceImplBase::GetLocation() const
{
	AActor* moiActor = MOI ? MOI->GetActor() : nullptr;
	return moiActor ? moiActor->GetActorLocation() : FVector::ZeroVector;
}

FQuat FModumateObjectInstanceImplBase::GetRotation() const
{
	AActor *moiActor = MOI ? MOI->GetActor() : nullptr;
	return moiActor ? moiActor->GetActorQuat() : FQuat::Identity;
}

FTransform FModumateObjectInstanceImplBase::GetWorldTransform() const
{
	return FTransform(GetRotation(), GetLocation());
}

FVector FModumateObjectInstanceImplBase::GetCorner(int32 index) const
{
	return FVector::ZeroVector;
}

int32 FModumateObjectInstanceImplBase::GetNumCorners() const
{
	return 0;
}

void FModumateObjectInstanceImplBase::GetTypedInstanceData(UScriptStruct*& OutStructDef, void*& OutStructPtr)
{
}

void FModumateObjectInstanceImplBase::UpdateVisibilityAndCollision(bool &bOutVisible, bool &bOutCollisionEnabled)
{
	AActor *moiActor = MOI ? MOI->GetActor() : nullptr;
	auto *controller = moiActor ? moiActor->GetWorld()->GetFirstPlayerController<AEditModelPlayerController_CPP>() : nullptr;
	if (controller)
	{
		bool bEnabledByViewMode = controller->EMPlayerState->IsObjectTypeEnabledByViewMode(MOI->GetObjectType());
		bOutVisible = !MOI->IsRequestedHidden() && bEnabledByViewMode;
		bOutCollisionEnabled = !MOI->IsCollisionRequestedDisabled() && bEnabledByViewMode;
		moiActor->SetActorHiddenInGame(!bOutVisible);
		moiActor->SetActorEnableCollision(bOutCollisionEnabled);
	}
}

void FModumateObjectInstanceImplBase::ShowAdjustmentHandles(AEditModelPlayerController_CPP *Controller, bool bShow)
{
	// By default, only show top-level handles, and allow them to hide or show their children appropriately.
	for (auto &ah : MOI->GetAdjustmentHandles())
	{
		if (ah.IsValid() && (ah->HandleParent == nullptr))
		{
			ah->SetEnabled(bShow);
		}
	}
}

void FModumateObjectInstanceImplBase::OnSelected(bool bNewSelected)
{
	AActor *moiActor = MOI ? MOI->GetActor() : nullptr;
	auto *controller = moiActor ? moiActor->GetWorld()->GetFirstPlayerController<AEditModelPlayerController_CPP>() : nullptr;
	if (controller && controller->EMPlayerState->SelectedObjects.Num() == 1)
	{
		MOI->ShowAdjustmentHandles(controller, bNewSelected);
	}
}

void FModumateObjectInstanceImplBase::OnAssemblyChanged()
{
	MOI->MarkDirty(EObjectDirtyFlags::Structure);
}

AActor *FModumateObjectInstanceImplBase::RestoreActor()
{
	if (UWorld *world = MOI ? MOI->GetWorld() : nullptr)
	{
		return CreateActor(world, MOI->GetObjectLocation(), MOI->GetObjectRotation());
	}

	return nullptr;
}

AActor *FModumateObjectInstanceImplBase::CreateActor(UWorld *world, const FVector &loc, const FQuat &rot)
{
	World = world;

	if (AEditModelGameMode_CPP* gameMode = World->GetAuthGameMode<AEditModelGameMode_CPP>())
	{
		DynamicMeshActor = World->SpawnActor<ADynamicMeshActor>(gameMode->DynamicMeshActorClass.Get(), FTransform(rot, loc));

		if (MOI && DynamicMeshActor.IsValid() && DynamicMeshActor->Mesh)
		{
			ECollisionChannel collisionObjType = UModumateTypeStatics::CollisionTypeFromObjectType(MOI->GetObjectType());
			DynamicMeshActor->Mesh->SetCollisionObjectType(collisionObjType);
		}
	}

	return DynamicMeshActor.Get();
}

void FModumateObjectInstanceImplBase::PostCreateObject(bool bNewObject)
{

}

void FModumateObjectInstanceImplBase::Destroy()
{

}

bool FModumateObjectInstanceImplBase::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	if (MOI == nullptr)
	{
		return false;
	}

	switch (DirtyFlag)
	{
	case EObjectDirtyFlags::Structure:
		SetupDynamicGeometry();
		break;
	case EObjectDirtyFlags::Visuals:
		MOI->UpdateVisibilityAndCollision();
		break;
	default:
		break;
	}

	return true;
}

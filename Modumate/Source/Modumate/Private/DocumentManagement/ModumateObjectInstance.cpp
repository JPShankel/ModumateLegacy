// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/ModumateObjectInstance.h"

#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/Material.h"
#include "Misc/OutputDeviceNull.h"
#include "UObject/ConstructorHelpers.h"
#include "Algo/Transform.h"
#include "Algo/Accumulate.h"

#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"
#include "DocumentManagement/ModumateDocument.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UI/HUDDrawWidget.h"
#include "UnrealClasses/LineActor.h"
#include "DocumentManagement/ModumateCommands.h"
#include "DocumentManagement/Objects/MOIFactory.h"
#include "Drafting/ModumateDraftingElements.h"
#include "Database/ModumateObjectDatabase.h"
#include "UnrealClasses/ModumateObjectComponent_CPP.h"

class AEditModelPlayerController_CPP;

namespace Modumate
{
	FModumateObjectInstance::FModumateObjectInstance(
		UWorld *world,
		FModumateDocument *doc,
		const FModumateObjectAssembly &obAsm,
		int32 id)
		: World(world)
		, Document(doc)
		, ObjectAssembly(obAsm)
		, bDestroyed(false)
		, ID(id)
	{
		ensureAlways(obAsm.ObjectType != EObjectType::OTNone);

		if (world == nullptr)
		{
			return;
		}
		FQuat rot = FQuat::Identity;
		FVector loc(ForceInitToZero);

		PreviewState.ObjectID = CurrentState.ObjectID = ID;
		PreviewState.ObjectAssemblyKey = CurrentState.ObjectAssemblyKey = ObjectAssembly.UniqueKey();
		PreviewState.ObjectType = CurrentState.ObjectType = obAsm.ObjectType;

		MakeImplementation();
		MakeActor(loc, rot);
	}

	FModumateObjectInstance::FModumateObjectInstance(
		UWorld *world,
		FModumateDocument *doc,
		const FMOIDataRecord &obRec)
		: World(world)
		, Document(doc)
		, ObjectAssembly()
		, bDestroyed(false)
	{
		if (world == nullptr)
		{
			return;
		}

		ID = obRec.ID;

		PreviewState.ObjectID = CurrentState.ObjectID = ID;

		// TODO: refactor assembly keys to use FName
		if (obRec.AssemblyKey.Len() > 0 && obRec.AssemblyKey != TEXT("None"))
		{
			const FModumateObjectAssembly *obAsm;
			if (UModumateObjectAssemblyStatics::ObjectTypeSupportsDDL2(obRec.ObjectType))
			{
				if (ensureAlways(doc->PresetManager.TryGetProjectAssemblyForPreset(obRec.ObjectType, *obRec.AssemblyKey, obAsm)))
				{
					ObjectAssembly = *obAsm;
				}
			}
			else
			{
				obAsm = doc->PresetManager.GetAssemblyByKey(UModumateTypeStatics::ToolModeFromObjectType(obRec.ObjectType), FName(*obRec.AssemblyKey));
				ensureAlways(obAsm != nullptr);
				if (obAsm != nullptr)
				{
					ObjectAssembly = *obAsm;
				}
			}
		}
		else
		{
			// Some MOI types don't have an assembly, so just set their object type directly
			ObjectAssembly.ObjectType = obRec.ObjectType;
		}

		PreviewState.ObjectAssemblyKey = CurrentState.ObjectAssemblyKey = ObjectAssembly.UniqueKey();
		PreviewState.ObjectType = obRec.ObjectType;
		CurrentState.ObjectType = obRec.ObjectType;

		MakeImplementation();
		MakeActor(obRec.Location, obRec.Rotation.Quaternion());

		CurrentState.Extents = obRec.Extents;
		CurrentState.ParentID = obRec.ParentID;
		CurrentState.ControlPoints = obRec.ControlPoints;
		CurrentState.ControlIndices = obRec.ControlIndices;
		CurrentState.bObjectInverted = obRec.ObjectInverted;
		CurrentState.ObjectProperties.FromStringMap(obRec.ObjectProperties);

		SetObjectRotation(obRec.Rotation.Quaternion());
		SetObjectLocation(obRec.Location);

		Implementation->SetIsDynamic(false);
		PreviewState = CurrentState;
	}

	void FModumateObjectInstance::MakeImplementation()
	{
		Implementation = FMOIFactory::MakeMOIImplementation(GetObjectType(), this);
	}

	void FModumateObjectInstance::MakeActor(const FVector &Loc, const FQuat &Rot)
	{
		if (ensure(Implementation && World.IsValid() && (ID != MOD_ID_NONE)))
		{
			MeshActor = Implementation->CreateActor(World.Get(), Loc, Rot);
			SetupMOIComponent();
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

	TArray<FModelDimensionString> FModumateObjectInstance::GetDimensionStrings() const
	{
		return Implementation->GetDimensionStrings();
	}

	EObjectType FModumateObjectInstance::GetObjectType() const
	{
		return ObjectAssembly.ObjectType;
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
			// If we need to do anything else during destruction, like cached its destroyed state, that would go here.
			DestroyActor();

			bDestroyed = true;
		}
	}

	void FModumateObjectInstance::SetParentObject(FModumateObjectInstance *newParentObj, bool bForceUpdate)
	{
		int32 newParentID = newParentObj ? newParentObj->ID : 0;
		if ((GetParentID() != newParentID) || bForceUpdate)
		{
			FModumateObjectInstance *oldParent = Document->GetObjectById(GetParentID());
			if (oldParent != nullptr)
			{
				oldParent->RemoveChild(this, false);
			}
			if (newParentObj != nullptr)
			{
				newParentObj->AddChild(this, false);
			}
			if (GetParentID() != newParentID)
			{
				SetParentID(newParentID);
			}

			MarkDirty(EObjectDirtyFlags::Structure);
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
		Algo::Transform(Children, ret, [this](int32 id) { return Document->GetObjectById(id); });
		return ret.FilterByPredicate([](FModumateObjectInstance *ob) {return ob != nullptr; });
	}

	TArray<const FModumateObjectInstance *> FModumateObjectInstance::GetChildObjects() const
	{
		TArray<const FModumateObjectInstance * > ret;
		Algo::TransformIf(
			Children, 
			ret, 
			[this](int32 id) { return Document->GetObjectById(id) != nullptr; }, 
			[this](int32 id) { return Document->GetObjectById(id); }
		);

		return ret;
	}

	bool FModumateObjectInstance::AddChild(FModumateObjectInstance *child, bool bUpdateChildHierarchy)
	{
		if (child)
		{
			if (child->GetParentID() == ID)
			{
				return false;
			}

			if (FModumateObjectInstance *oldParent = child->GetParentObject())
			{
				oldParent->RemoveChild(child, bUpdateChildHierarchy);
			}

			child->SetParentID(ID);

			if (Children.AddUnique(child->ID) != INDEX_NONE)
			{
				MarkDirty(EObjectDirtyFlags::Structure);
			}

			if (bUpdateChildHierarchy)
			{
				child->MarkDirty(EObjectDirtyFlags::Structure);
			}

			return true;
		}

		return false;
	}

	bool FModumateObjectInstance::RemoveChild(FModumateObjectInstance *child, bool bUpdateChildHierarchy)
	{
		if (child)
		{
			if (child->GetParentID() != ID)
			{
				return false;
			}

			child->SetParentID(MOD_ID_NONE);

			if (Children.Remove(child->ID) > 0)
			{
				MarkDirty(EObjectDirtyFlags::Structure);
			}

			if (bUpdateChildHierarchy)
			{
				child->MarkDirty(EObjectDirtyFlags::Structure);
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
					for (FSignedID directedEdgeID : graphVertex->ConnectedEdgeIDs)
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

					for (FSignedID directedEdgeID : graphFace->EdgeIDs)
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
			if (const FGraph2D *surfaceGraph = Document->FindSurfaceGraph(GetParentID()))
			{
				switch (objectType)
				{
				case EObjectType::OTSurfaceVertex:
					if (const FGraph2DVertex *vertex = surfaceGraph->FindVertex(ID))
					{
						for (FEdgeID directedEdgeID : vertex->Edges)
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

						for (FEdgeID directedEdgeID : polygon->Edges)
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
		case EObjectType::OTFloorSegment:
			//case OTRoofSegment:
		case EObjectType::OTCabinet:
			return true;
		default:
			return false;
		}
	}

	float FModumateObjectInstance::CalculateThickness() const
	{
		return ObjectAssembly.CalculateThickness().AsWorldCentimeters();
	}

	FVector FModumateObjectInstance::GetNormal() const
	{
		return Implementation->GetNormal();
	}

	void FModumateObjectInstance::OnAssemblyChanged()
	{
		Implementation->OnAssemblyChanged();
	}

	void FModumateObjectInstance::MarkDirty(EObjectDirtyFlags DirtyFlag)
	{
		if (!IsDirty(DirtyFlag))
		{
			DirtyFlags |= DirtyFlag;
			Document->RegisterDirtyObject(DirtyFlag, this, true);
			Implementation->SetIsDynamic(true);
		}
	}

	bool FModumateObjectInstance::IsDirty(EObjectDirtyFlags DirtyFlag) const
	{
		return ((DirtyFlags & DirtyFlag) == DirtyFlag);
	}

	bool FModumateObjectInstance::CleanObject(EObjectDirtyFlags DirtyFlag)
	{
		bool bSuccess = false;

		// Only one flag being cleaned at a time is supported; check that here
		int32 flagInt = (int32)DirtyFlag;
		if (!ensureAlwaysMsgf((DirtyFlag != EObjectDirtyFlags::None) && ((flagInt & (flagInt - 1)) == 0),
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

			// Don't clean objects that were destroyed after they were marked dirty.
			if (bDestroyed)
			{
				bValidObjectToClean = false;
			}

			// If we have a parent assigned, then by assume that
			// it needs to be exist and be clean before we can clean ourselves.
			// NOTE: this is expected if we try to clean children before parents, like during loading.
			if (GetParentID() != MOD_ID_NONE)
			{
				const FModumateObjectInstance *parentObj = Document->GetObjectById(GetParentID());
				if ((parentObj == nullptr) || parentObj->IsDirty(DirtyFlag))
				{
					bValidObjectToClean = false;
				}
			}

			// Now actually clean the object.
			if (bValidObjectToClean)
			{
				bSuccess = Implementation->CleanObject(DirtyFlag);
			}

			if (bSuccess)
			{
				// Now mark the children as dirty with the same flag, since this parent just cleaned itself.
				for (FModumateObjectInstance *childObj : GetChildObjects())
				{
					childObj->MarkDirty(DirtyFlag);
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

	FMOIDelta::FMOIDelta(const TArray<FMOIStateData> &States)
	{
		AddCreateDestroyStates(States);
	}

	FMOIDelta::FMOIDelta(const TArray<FModumateObjectInstance*> &Objects)
	{
		AddMutationStates(Objects);
	}

	bool FMOIDelta::ApplyTo(FModumateDocument *doc, UWorld *world) const
	{
		return doc->ApplyMOIDelta(*this, world);
	}

	TSharedPtr<FDelta> FMOIDelta::MakeInverse() const
	{
		TSharedPtr<FMOIDelta> delta = MakeShareable(new FMOIDelta());

		for (auto &sp : StatePairs)
		{
			delta->StatePairs.Add(FStatePair(sp.Value, sp.Key));
		}

		return delta;
	}

	FModumateCommand FMOIDelta::AsCommand(const FString &CommandID) const
	{
		FModumateFunctionParameterSet params;
		ToParameterSet(params);

		FModumateCommand command(CommandID);
		command.SetParameterSet(params);

		return command;
	}

	void FMOIDelta::AddCreateDestroyStates(const TArray<FMOIStateData> &States)
	{
		for (auto &state : States)
		{
			if (ensureAlways(state.StateType == EMOIDeltaType::Create || state.StateType == EMOIDeltaType::Destroy))
			{
				FMOIStateData baseState = state;
				FMOIStateData targetState = state;
				baseState.StateType = targetState.StateType == EMOIDeltaType::Create ? EMOIDeltaType::Destroy : EMOIDeltaType::Create;
				StatePairs.Add(FStatePair(baseState, targetState));
			}
		}
	}

	void FMOIDelta::AddMutationStates(const TArray<FModumateObjectInstance*> &Objects)
	{
		for (auto *ob : Objects)
		{
			if (!ob->GetIsInPreviewMode())
			{
				UE_LOG(LogTemp, Warning, TEXT("Tried to add mutation state for MOI #%d, but it wasn't in preview mode!"), ob->ID);
				continue;
			}

			FMOIStateData baseState = ob->CurrentState;
			FMOIStateData targetState = ob->PreviewState;

			if (baseState == targetState)
			{
				UE_LOG(LogTemp, Log, TEXT("Tried to add mutation state for MOI #%d, but its preview state is identical to its saved state!"), ob->ID);
				continue;
			}

			baseState.StateType = EMOIDeltaType::Mutate;
			targetState.StateType = EMOIDeltaType::Mutate;
			StatePairs.Add(FStatePair(baseState, targetState));
		}
	}

	bool FMOIStateData::ToParameterSet(const FString &Prefix, FModumateFunctionParameterSet &OutParameterSet) const
	{
		TMap<FString, FString> propertyMap;
		if (!ObjectProperties.ToStringMap(propertyMap))
		{
			return false;
		}

		TArray<FString> propertyNames, propertyValues;

		propertyMap.GenerateKeyArray(propertyNames);
		propertyMap.GenerateValueArray(propertyValues);

		OutParameterSet.SetValue(Prefix + Modumate::Parameters::kPropertyNames, propertyNames);
		OutParameterSet.SetValue(Prefix + Modumate::Parameters::kPropertyValues, propertyValues);

		OutParameterSet.SetValue(Prefix + Modumate::Parameters::kExtents, Extents);
		OutParameterSet.SetValue(Prefix + Modumate::Parameters::kControlPoints, ControlPoints);
		OutParameterSet.SetValue(Prefix + Modumate::Parameters::kIndices, ControlIndices);
		OutParameterSet.SetValue(Prefix + Modumate::Parameters::kInverted, bObjectInverted);
		OutParameterSet.SetValue(Prefix + Modumate::Parameters::kAssembly, ObjectAssemblyKey);
		OutParameterSet.SetValue(Prefix + Modumate::Parameters::kParent, ParentID);
		OutParameterSet.SetValue(Prefix + Modumate::Parameters::kLocation, Location);
		OutParameterSet.SetValue(Prefix + Modumate::Parameters::kQuaternion, Orientation);
		OutParameterSet.SetValue(Prefix + Modumate::Parameters::kType, EnumValueString(EMOIDeltaType,StateType));
		OutParameterSet.SetValue(Prefix + Modumate::Parameters::kObjectType, EnumValueString(EObjectType, ObjectType));
		OutParameterSet.SetValue(Prefix + Modumate::Parameters::kObjectID, ObjectID);

		return true;
	}

	bool FMOIStateData::FromParameterSet(const FString &Prefix, const FModumateFunctionParameterSet &ParameterSet)
	{
		TArray<FString> propertyNames, propertyValues;
		propertyNames = ParameterSet.GetValue(Prefix + Modumate::Parameters::kPropertyNames, propertyNames);
		propertyValues = ParameterSet.GetValue(Prefix + Modumate::Parameters::kPropertyValues, propertyValues);

		if (!ensureAlways(propertyNames.Num() == propertyValues.Num()))
		{
			return false;
		}

		FModumateFunctionParameterSet::FStringMap propertyMap;
		for (int32 i = 0; propertyNames.Num(); ++i)
		{
			propertyMap.Add(propertyNames[i], propertyValues[i]);
		}

		ObjectProperties.FromStringMap(propertyMap);

		Extents = ParameterSet.GetValue(Prefix + Modumate::Parameters::kExtents);
		ControlPoints = ParameterSet.GetValue(Prefix + Modumate::Parameters::kControlPoints);
		ControlIndices = ParameterSet.GetValue(Prefix + Modumate::Parameters::kIndices);
		bObjectInverted = ParameterSet.GetValue(Prefix + Modumate::Parameters::kInverted);
		ObjectAssemblyKey = ParameterSet.GetValue(Prefix + Modumate::Parameters::kAssembly);
		ParentID = ParameterSet.GetValue(Prefix + Modumate::Parameters::kParent);
		Location = ParameterSet.GetValue(Prefix + Modumate::Parameters::kLocation);
		Orientation = ParameterSet.GetValue(Prefix + Modumate::Parameters::kQuaternion);
		StateType = EnumValueByString(EMOIDeltaType,ParameterSet.GetValue(Prefix + Modumate::Parameters::kType).AsString());
		ObjectID = ParameterSet.GetValue(Prefix + Modumate::Parameters::kObjectID);
		ObjectType = EnumValueByString(EObjectType, ParameterSet.GetValue(Prefix + Modumate::Parameters::kObjectType).AsString());

		return true;
	}

	bool FMOIStateData::operator==(const FMOIStateData& Other) const
	{
		// TODO: replace with better per-property equality checks, when we no longer use FModumateFunctionParameterSet for serialization.
		FModumateFunctionParameterSet thisParamSet, otherParamSet;
		TMap<FString, FString> thisStringMap, otherStringMap;
		if (ToParameterSet(TEXT(""), thisParamSet) && Other.ToParameterSet(TEXT(""), otherParamSet) &&
			thisParamSet.ToStringMap(thisStringMap) && otherParamSet.ToStringMap(otherStringMap))
		{
			return thisStringMap.OrderIndependentCompareEqual(otherStringMap);
		}

		return false;
	}

	static const FString BaseStatePrefix = TEXT("BaseState");
	static const FString TargetStatePrefix = TEXT("TargetState");

	static inline FString MakeInstancePrefix(const FString &Prefix, int32 ID)
	{
		return Prefix + FString::Printf(TEXT("%d_"), ID);
	}

	bool FMOIDelta::ToParameterSet(FModumateFunctionParameterSet &OutParameterSet) const
	{
		TArray<int32> keys;
		Algo::Transform(StatePairs, keys, [](const FStatePair &Pair) {return Pair.Key.ObjectID; });

		OutParameterSet.SetValue(Modumate::Parameters::kObjectIDs, keys);		
		for (auto &statePair : StatePairs)
		{
			int32 key = statePair.Key.ObjectID;
			statePair.Key.ToParameterSet(MakeInstancePrefix(BaseStatePrefix,key), OutParameterSet);
			statePair.Value.ToParameterSet(MakeInstancePrefix(TargetStatePrefix,key), OutParameterSet);
		}

		return true;
	}

	bool FMOIDelta::FromParameterSet(const FModumateFunctionParameterSet &ParameterSet)
	{
		TArray<int32> objectIDs = ParameterSet.GetValue(Modumate::Parameters::kObjectIDs);
		if (!ensureAlwaysMsgf(objectIDs.Num() > 0, TEXT("Empty MOI Delta sent to FromParameterSet")))
		{
			return false;
		}

		StatePairs.Empty();

		for (auto obID : objectIDs)
		{
			FStatePair &statePair = StatePairs.AddDefaulted_GetRef();
			statePair.Key.FromParameterSet(MakeInstancePrefix(BaseStatePrefix, obID), ParameterSet);
			statePair.Value.FromParameterSet(MakeInstancePrefix(TargetStatePrefix,obID), ParameterSet);
		}

		return true;
	}

	bool FModumateObjectInstance::SetDataState(const FMOIStateData &DataState)
	{
		if (!ensureAlways(!GetIsInPreviewMode()))
		{
			return false;
		}
		CurrentState = DataState;
		PreviewState = DataState;
		MarkDirty(EObjectDirtyFlags::Structure);

		return true;
	}

	FMOIStateData &FModumateObjectInstance::GetDataState()
	{
		return GetIsInPreviewMode() ? PreviewState : CurrentState;
	}

	const FMOIStateData &FModumateObjectInstance::GetDataState() const
	{
		return GetIsInPreviewMode() ? PreviewState : CurrentState;
	}

	bool FModumateObjectInstance::BeginPreviewOperation()
	{
		if (!ensureAlways(!GetIsInPreviewMode()))
		{
			return false;
		}
		PreviewState = CurrentState;
		bPreviewOperationMode = true;
		Implementation->SetIsDynamic(true);
		return true;
	}

	bool FModumateObjectInstance::EndPreviewOperation()
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

	bool FModumateObjectInstance::GetObjectInverted() const
	{
		return GetDataState().bObjectInverted;
	}

	void FModumateObjectInstance::SetObjectInverted(bool bNewInverted)
	{
		if (GetObjectInverted() != bNewInverted)
		{
			InvertObject();
		}
	}

	const FVector &FModumateObjectInstance::GetExtents() const
	{
		return GetDataState().Extents;
	}

	void FModumateObjectInstance::SetExtents(const FVector &NewExtents)
	{
		GetDataState().Extents = NewExtents;
	}

	int32 FModumateObjectInstance::GetParentID() const 
	{ 
		return GetDataState().ParentID;
	}

	void FModumateObjectInstance::SetParentID(int32 NewID) 
	{ 
		GetDataState().ParentID = NewID;
	}

	const FModumateObjectAssembly &FModumateObjectInstance::GetAssembly() const
	{
		return ObjectAssembly;
	}

	void FModumateObjectInstance::SetAssembly(const FModumateObjectAssembly &NewAssembly)
	{
		GetDataState().ObjectAssemblyKey = NewAssembly.UniqueKey();
		ObjectAssembly = NewAssembly;
		bAssemblyLayersReversed = false;
	}

	void FModumateObjectInstance::SetAssemblyLayersReversed(bool bNewLayersReversed)
	{
		if (bNewLayersReversed != bAssemblyLayersReversed)
		{
			ObjectAssembly.ReverseLayers();
			bAssemblyLayersReversed = bNewLayersReversed;
		}
	}

	void FModumateObjectInstance::SetControlPoint(int32 Index, const FVector &Value)
	{
		if (ensureAlways(GetDataState().ControlPoints.Num() > Index))
		{
			GetDataState().ControlPoints[Index] = Value;
		}
	}

	const FVector &FModumateObjectInstance::GetControlPoint(int32 Index) const
	{
		ensureAlways(Index < GetDataState().ControlPoints.Num());
		return GetDataState().ControlPoints[Index];
	}

	void FModumateObjectInstance::SetControlPointIndex(int32 IndexNum, int32 IndexVal)
	{
		if (ensureAlways(GetDataState().ControlIndices.Num() > IndexNum))
		{
			GetDataState().ControlIndices[IndexNum] = IndexVal;
		}
	}

	int32 FModumateObjectInstance::GetControlPointIndex(int32 IndexNum) const
	{
		ensureAlways(IndexNum < GetDataState().ControlIndices.Num());
		return GetDataState().ControlIndices[IndexNum];
	}

	const TArray<FVector> &FModumateObjectInstance::GetControlPoints() const
	{
		return GetDataState().ControlPoints;
	}

	const TArray<int32> &FModumateObjectInstance::GetControlPointIndices() const
	{
		return GetDataState().ControlIndices;
	}

	void FModumateObjectInstance::AddControlPoint(const FVector &ControlPoint)
	{
		GetDataState().ControlPoints.Add(ControlPoint);
	}

	void FModumateObjectInstance::AddControlPointIndex(int32 Index)
	{
		GetDataState().ControlIndices.Add(Index);
	}

	void FModumateObjectInstance::SetControlPoints(const TArray<FVector> &NewControlPoints)
	{
		GetDataState().ControlPoints = NewControlPoints;
	}

	void FModumateObjectInstance::SetControlPointIndices(const TArray<int32> &NewControlPointIndices)
	{
		GetDataState().ControlIndices = NewControlPointIndices;
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

	bool FModumateObjectInstance::GetTriInternalNormalFromEdge(int32 cp1, int32 cp2, FVector &outNormal) const
	{
		return Implementation->GetTriInternalNormalFromEdge(cp1, cp2, outNormal);
	}

	void FModumateObjectInstance::AddDraftingLines(UHUDDrawWidget *HUDDrawWidget)
	{
		Implementation->AddDraftingLines(HUDDrawWidget);
	}

	void FModumateObjectInstance::GetDraftingLines(const TSharedPtr<FDraftingComposite> &ParentPage, const FPlane &Plane, const FVector &AxisX, const FVector &AxisY, const FVector &Origin, const FBox2D &BoundingBox, TArray<TArray<FVector>> &OutPerimeters) const
	{
		Implementation->GetDraftingLines(ParentPage, Plane, AxisX, AxisY, Origin, BoundingBox, OutPerimeters);
	}

	FVector FModumateObjectInstance::GetWallDirection() const
	{
		if (GetDataState().ControlPoints.Num() >= 2)
		{
			FVector delta = (GetDataState().ControlPoints[1] - GetDataState().ControlPoints[0]).GetSafeNormal();
			return FVector::CrossProduct(FVector(0, 0, 1), delta);
		}
		else
		{
			return Implementation->GetNormal();
		}
	}

	FVector FModumateObjectInstance::GetCorner(int32 index) const
	{
		return Implementation->GetCorner(index);
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

	void FModumateObjectInstance::PostRestoreObject()
	{
		if (ensureAlways(bDestroyed))
		{
			bDestroyed = false;
		}
	}

	void FModumateObjectInstance::InvertObject()
	{
		GetDataState().bObjectInverted = !GetDataState().bObjectInverted;
		MarkDirty(EObjectDirtyFlags::Structure);
	}

	void FModumateObjectInstance::TransverseObject()
	{
		Implementation->TransverseObject();
	}

	FVector FModumateObjectInstance::GetObjectLocation() const
	{
		return Implementation->GetLocation();
	}

	void FModumateObjectInstance::SetObjectLocation(const FVector &p)
	{
		GetDataState().Location = p;
		Implementation->SetLocation(p);
	}

	FQuat FModumateObjectInstance::GetObjectRotation() const
	{
		return Implementation->GetRotation();
	}

	void FModumateObjectInstance::SetObjectRotation(const FQuat &r)
	{
		GetDataState().Orientation = r;
		Implementation->SetRotation(r);
	}

	void FModumateObjectInstance::SetWorldTransform(const FTransform &NewTransform)
	{
		GetDataState().Location = NewTransform.GetLocation();
		GetDataState().Orientation = NewTransform.GetRotation();
		return Implementation->SetWorldTransform(NewTransform);
	}

	FTransform FModumateObjectInstance::GetWorldTransform() const
	{
		return Implementation->GetWorldTransform();
	}

	bool FModumateObjectInstance::HasProperty(BIM::EScope Scope, const BIM::FNameType &Name) const
	{
		return GetDataState().ObjectProperties.HasProperty(Scope, Name);
	}

	FModumateCommandParameter FModumateObjectInstance::GetProperty(BIM::EScope Scope, const BIM::FNameType &Name) const
	{
		return GetDataState().ObjectProperties.GetProperty(Scope, Name);
	}

	const BIM::FBIMPropertySheet &FModumateObjectInstance::GetProperties() const
	{
		return GetDataState().ObjectProperties;
	}

	void FModumateObjectInstance::SetProperty(BIM::EScope Scope, const BIM::FNameType &Name, const FModumateCommandParameter &Param)
	{
		GetDataState().ObjectProperties.SetProperty(Scope, Name, Param);
	}

	void FModumateObjectInstance::SetAllProperties(const BIM::FBIMPropertySheet &NewProperties)
	{
		GetDataState().ObjectProperties = NewProperties;
	}

	void FModumateObjectInstance::SetFromDataRecordAndRotation(const FMOIDataRecordV1 &dataRec, const FVector &origin, const FQuat &rotation)
	{
		Implementation->SetFromDataRecordAndRotation(dataRec, origin, rotation);

		// TODO: this is messy because we allow objects to customize how they respond to rotational deltas without modifying their data state,
		// but we need to update the data state in order to use deltas. So for now, allow the existing implementations to mess with their actors
		// however they want, but make sure we track the changes in the data state afterwards. We should either delete these functions,
		// or ensure they behave consistently for objects in the same coordinate space.
		GetDataState().Location = GetObjectLocation();
		GetDataState().Orientation = GetObjectRotation();
	}

	void FModumateObjectInstance::SetFromDataRecordAndDisplacement(const FMOIDataRecordV1 &dataRec, const FVector &displacement)
	{
		Implementation->SetFromDataRecordAndDisplacement(dataRec, displacement);
		GetDataState().Location = GetObjectLocation();
		GetDataState().Orientation = GetObjectRotation();
	}

	// data records are USTRUCTs used in serialization and clipboard operations
	FMOIDataRecord FModumateObjectInstance::AsDataRecord() const
	{
		FMOIDataRecord ret;
		ret.ID = ID;
		ret.ObjectType = GetObjectType();
		ret.AssemblyKey = ObjectAssembly.UniqueKey().ToString();
		ret.ParentID = GetParentID();
		ret.ChildIDs = Children;
		ret.Location = GetObjectLocation();
		ret.Rotation = GetObjectRotation().Rotator();
		ret.ControlPoints = CurrentState.ControlPoints;
		ret.ControlIndices = CurrentState.ControlIndices;
		ret.Extents = CurrentState.Extents;
		CurrentState.ObjectProperties.ToStringMap(ret.ObjectProperties);
		ret.ObjectInverted = CurrentState.bObjectInverted;
		return ret;
	}

	FModumateWallMount FModumateObjectInstance::GetWallMountForChild(const FModumateObjectInstance *child, int32 originIndex) const
	{
		if ((GetObjectType() != EObjectType::OTWallSegment) || (child == nullptr))
		{
			FModumateWallMount ret;
			ret.OriginIndex = originIndex;
			ret.RelativePosition = FVector::ZeroVector;
			ret.OrientationDelta = FQuat::Identity;
			ret.OriginalControlPoints = child->GetControlPoints();
			return ret;
		}

		return child->GetWallMountForSelf(originIndex);
	}

	TMap<FModumateObjectInstance*, FModumateWallMount> FModumateObjectInstance::GetWallMountsForChildren(int32 originIndex)
	{
		TMap<FModumateObjectInstance*, FModumateWallMount> childWallMounts;
		for (auto &childOb : GetChildObjects())
		{
			childWallMounts.Add(childOb,GetWallMountForChild(childOb, originIndex));
		}
		return childWallMounts;
	}

	FModumateWallMount FModumateObjectInstance::GetWallMountForSelf(int32 originIndex) const
	{
		return Implementation->GetWallMountForSelf(originIndex);
	}

	void FModumateObjectInstance::SetWallMountForSelf(const FModumateWallMount &wm)
	{
		Implementation->SetWallMountForSelf(wm);
	}

	// FModumateObjectInstanceImplBase Implementation

	void FModumateObjectInstanceImplBase::SetRotation(const FQuat &r)
	{
		AActor *moiActor = MOI ? MOI->GetActor() : nullptr;
		if (moiActor)
		{
			moiActor->SetActorRotation(r);
		}
	}

	FQuat FModumateObjectInstanceImplBase::GetRotation() const
	{
		AActor *moiActor = MOI ? MOI->GetActor() : nullptr;
		return moiActor ? moiActor->GetActorQuat() : FQuat::Identity;
	}

	void FModumateObjectInstanceImplBase::SetLocation(const FVector &p)
	{
		AActor *moiActor = MOI ? MOI->GetActor() : nullptr;
		if (moiActor)
		{
			moiActor->SetActorLocation(p);
		}
	}

	FVector FModumateObjectInstanceImplBase::GetLocation() const
	{
		AActor *moiActor = MOI ? MOI->GetActor() : nullptr;
		return moiActor ? moiActor->GetActorLocation() : FVector::ZeroVector;
	}

	void FModumateObjectInstanceImplBase::SetWorldTransform(const FTransform &NewTransform)
	{
		SetLocation(NewTransform.GetLocation());
		SetRotation(NewTransform.GetRotation());
	}

	FTransform FModumateObjectInstanceImplBase::GetWorldTransform() const
	{
		return FTransform(GetRotation(), GetLocation());
	}

	void FModumateObjectInstanceImplBase::SetFromDataRecordAndRotation(const FMOIDataRecordV1 &dataRec, const FVector &origin, const FQuat &rotation)
	{
		SetRotation(dataRec.Rotation.Quaternion()*rotation);
		SetLocation(origin + rotation.RotateVector(dataRec.Location - origin));
	}

	void FModumateObjectInstanceImplBase::SetFromDataRecordAndDisplacement(const FMOIDataRecordV1 &dataRec, const FVector &displacement)
	{
		SetLocation(dataRec.Location + displacement);
	}

	FVector FModumateObjectInstanceImplBase::GetCorner(int32 index) const
	{
		if (ensureAlways((MOI != nullptr) && (index >= 0)))
		{
			if (index < MOI->GetControlPoints().Num())
			{
				return MOI->GetControlPoint(index);
			}
		}
		return GetLocation();
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
		if (controller)
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
		return world->SpawnActor<AActor>(loc, rot.Rotator());
	}

	bool FModumateObjectInstanceImplBase::CleanObject(EObjectDirtyFlags DirtyFlag)
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

	FModumateWallMount FModumateObjectInstanceImplBase::GetWallMountForSelf(int32 originIndex) const
	{
		ensureMsgf(false, TEXT("Unimplemented GetWallMountForSelf for object type %s!"),
			MOI ? *EnumValueString(EObjectType, MOI->GetObjectType()) : TEXT("?"));

		return FModumateWallMount();
	}

	void FModumateObjectInstanceImplBase::SetWallMountForSelf(const FModumateWallMount &wm)
	{
		ensureMsgf(false, TEXT("Unimplemented SetWallMountForSelf for object type %s!"),
			MOI ? *EnumValueString(EObjectType, MOI->GetObjectType()) : TEXT("?"));

		return;
	}
}

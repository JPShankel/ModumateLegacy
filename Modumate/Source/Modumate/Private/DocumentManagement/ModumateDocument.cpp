// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/ModumateDocument.h"

#include "Online/ModumateAnalyticsStatics.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "UnrealClasses/LineActor.h"

#include "Internationalization/Internationalization.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"

#include "Graph/Graph2D.h"
#include "Graph/Graph3DDelta.h"

#include "ModumateCore/PlatformFunctions.h"
#include "Drafting/DraftingManager.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "ToolsAndAdjustments/Interface/EditModelToolInterface.h"
#include "DocumentManagement/ModumateDelta.h"
#include "Drafting/ModumateDraftingView.h"
#include "ModumateCore/ModumateMitering.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"

#include "Algo/Transform.h"
#include "Algo/Accumulate.h"
#include "Misc/Paths.h"
#include "JsonObjectConverter.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"

#include "Database/ModumateObjectDatabase.h"
#include "UnrealClasses/ModumateObjectComponent_CPP.h"

#include "UnrealClasses/Modumate.h"

using namespace Modumate::Mitering;

namespace Modumate
{

const FName FModumateDocument::DocumentHideRequestTag(TEXT("DocumentHide"));

FModumateDocument::FModumateDocument() :
	NextID(1)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::ModumateDocument"));

	GatherDocumentMetadata();

	//Set Default Values
	//Values will be deprecated in favor of instance-level overrides for assemblies
	DefaultWallHeight = 243.84f;
	ElevationDelta = 0;
	DefaultWindowHeight = 91.44f;
	DefaultDoorHeight = 0.f;

	PresetManager.InitAssemblyDBs();
}

FModumateDocument::~FModumateDocument()
{
}

void FModumateDocument::Undo(UWorld *world)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::Undo"));
	ensureAlways(UndoRedoMacroStack.Num() == 0);
	if (UndoBuffer.Num() > 0)
	{
		UndoRedo *ur = UndoBuffer.Last(0);
		UndoBuffer.RemoveAt(UndoBuffer.Num() - 1);
		RedoBuffer.Add(ur);

		int32 undoBufferSize = UndoBuffer.Num();
		int32 redoBufferSize = RedoBuffer.Num();

		ur->Undo();

		AEditModelPlayerState_CPP* EMPlayerState = Cast<AEditModelPlayerState_CPP>(world->GetFirstPlayerController()->PlayerState);
		EMPlayerState->RefreshActiveAssembly();

		ensureAlways(undoBufferSize == UndoBuffer.Num());
		ensureAlways(redoBufferSize == RedoBuffer.Num());
	}
}

void FModumateDocument::Redo(UWorld *world)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::Redo"));
	ensureAlways(UndoRedoMacroStack.Num() == 0);
	if (RedoBuffer.Num() > 0)
	{
		UndoRedo *ur = RedoBuffer.Last(0);
		RedoBuffer.RemoveAt(RedoBuffer.Num() - 1);
		UndoBuffer.Add(ur);

		int32 undoBufferSize = UndoBuffer.Num();
		int32 redoBufferSize = RedoBuffer.Num();

		ur->Redo();

		AEditModelPlayerState_CPP* EMPlayerState = Cast<AEditModelPlayerState_CPP>(world->GetFirstPlayerController()->PlayerState);
		EMPlayerState->RefreshActiveAssembly();

		ensureAlways(undoBufferSize == UndoBuffer.Num());
		ensureAlways(redoBufferSize == RedoBuffer.Num());
	}
}

void FModumateDocument::BeginUndoRedoMacro()
{
	UndoRedoMacroStack.Push(UndoBuffer.Num());
}

void FModumateDocument::EndUndoRedoMacro()
{
	if (UndoRedoMacroStack.Num() == 0)
	{
		return;
	}

	int32 start = UndoRedoMacroStack.Pop();

	if (start >= UndoBuffer.Num())
	{
		return;
	}

	TArray<UndoRedo*> section;
	for (int32 i = start; i < UndoBuffer.Num(); ++i)
	{
		section.Add(UndoBuffer[i]);
	}

	UndoRedo *ur = new UndoRedo();
	for (auto s : section)
	{
		ur->Deltas.Append(s->Deltas);
	}

	UndoBuffer.SetNum(start, true);

	ur->Redo = [section]()
	{
		for (auto &subdo : section)
		{
			subdo->Redo();
		}
	};

	ur->Undo = [section]()
	{
		for (int i = section.Num() - 1; i >= 0; --i)
		{
			section[i]->Undo();
		}
	};

	UndoBuffer.Add(ur);
}

void FModumateDocument::SetDefaultWallHeight(float height)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::SetDefaultWallHeight"));
	ClearRedoBuffer();
	float orig = DefaultWallHeight;
	UndoRedo *ur = new UndoRedo();
	ur->Redo = [ur, orig, height, this]()
	{
		DefaultWallHeight = height;
		UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::SetDefaultWallHeight::Redo"));
		ur->Undo = [this, orig]()
		{
			UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::SetDefaultWallHeight::Undo"));
			DefaultWallHeight = orig;
		};
	};

	UndoBuffer.Add(ur);
	ur->Redo();
}

void FModumateDocument::SetDefaultRailHeight(float height)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::SetDefaultRailHeight"));
	ClearRedoBuffer();
	float orig = DefaultRailHeight;
	UndoRedo *ur = new UndoRedo();
	ur->Redo = [ur, orig, height, this]()
	{
		DefaultRailHeight = height;
		UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::SetDefaultRailHeight::Redo"));
		ur->Undo = [this, orig]()
		{
			UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::SetDefaultRailHeight::Undo"));
			DefaultRailHeight = orig;
		};
	};

	UndoBuffer.Add(ur);
	ur->Redo();
}

void FModumateDocument::SetDefaultCabinetHeight(float height)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::SetDefaultCabinetHeight"));
	ClearRedoBuffer();
	float orig = DefaultCabinetHeight;
	UndoRedo *ur = new UndoRedo();
	ur->Redo = [ur, orig, height, this]()
	{
		DefaultCabinetHeight = height;
		UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::SetDefaultCabinetHeight::Redo"));
		ur->Undo = [this, orig]()
		{
			UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::SetDefaultCabinetHeight::Undo"));
			DefaultCabinetHeight = orig;
		};
	};

	UndoBuffer.Add(ur);
	ur->Redo();
}

void FModumateDocument::SetDefaultDoorHeightWidth(float height, float width)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::SetDefaultDoorHeightWidth"));
	ClearRedoBuffer();
	float origHeight = DefaultDoorHeight;
	float origWidth = DefaultDoorWidth;
	UndoRedo *ur = new UndoRedo();
	ur->Redo = [ur, origHeight, origWidth, height, width, this]()
	{
		DefaultDoorHeight = height;
		DefaultDoorWidth = width;
		UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::SetDefaultDoorHeightWidth::Redo"));
		ur->Undo = [this, origHeight, origWidth]()
		{
			UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::SetDefaultDoorHeightWidth::Undo"));
			DefaultDoorHeight = origHeight;
			DefaultDoorWidth = origWidth;
		};
	};

	UndoBuffer.Add(ur);
	ur->Redo();
}

void FModumateDocument::SetDefaultWindowHeightWidth(float height, float width)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::SetDefaultWindowHeightWidth"));
	ClearRedoBuffer();
	float origHeight = DefaultWindowHeight;
	float origWidth = DefaultWindowWidth;
	UndoRedo *ur = new UndoRedo();
	ur->Redo = [ur, origHeight, origWidth, height, width, this]()
	{
		DefaultWindowHeight = height;
		DefaultWindowWidth = width;
		UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::SetDefaultWindowHeightWidth::Redo"));
		ur->Undo = [this, origHeight, origWidth]()
		{
			UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::SetDefaultWindowHeightWidth::Undo"));
			DefaultWindowHeight = origHeight;
			DefaultWindowWidth = origWidth;
		};
	};

	UndoBuffer.Add(ur);
	ur->Redo();
}

void FModumateDocument::SetDefaultJustificationZ(float newValue)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::SetDefaultJustificationZ"));
	ClearRedoBuffer();
	float orig = DefaultJustificationZ;
	UndoRedo *ur = new UndoRedo();
	ur->Redo = [ur, orig, newValue, this]()
	{
		DefaultJustificationZ = newValue;
		UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::SetDefaultJustificationZ::Redo"));
		ur->Undo = [this, orig]()
		{
			UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::SetDefaultJustificationZ::Undo"));
			DefaultJustificationZ = orig;
		};
	};

	UndoBuffer.Add(ur);
	ur->Redo();
}

void FModumateDocument::SetDefaultJustificationXY(float newValue)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::SetDefaultJustificationXY"));
	ClearRedoBuffer();
	float orig = DefaultJustificationXY;
	UndoRedo *ur = new UndoRedo();
	ur->Redo = [ur, orig, newValue, this]()
	{
		DefaultJustificationXY = newValue;
		UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::SetDefaultJustificationXY::Redo"));
		ur->Undo = [this, orig]()
		{
			UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::SetDefaultJustificationXY::Undo"));
			DefaultJustificationXY = orig;
		};
	};

	UndoBuffer.Add(ur);
	ur->Redo();
}

void FModumateDocument::SetAssemblyForObjects(UWorld *world,TArray<int32> ids, const FModumateObjectAssembly &assembly)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::SetAssemblyForWalls"));
	ClearRedoBuffer();
	UndoRedo *ur = new UndoRedo();

	ur->Redo = [this, ur, ids, assembly]()
	{
		TMap<FModumateObjectInstance *, FModumateObjectAssembly> originals;

		for (auto id : ids)
		{
			FModumateObjectInstance *ob = GetObjectById(id);
			if (ob != nullptr)
			{
				originals.Add(ob, ob->GetAssembly());
				ob->SetAssembly(assembly);
				ob->OnAssemblyChanged();
			}
		}

		ur->Undo = [originals, this]()
		{
			for (auto o : originals)
			{
				o.Key->SetAssembly(o.Value);
				o.Key->OnAssemblyChanged();
			}
		};
	};
	UndoBuffer.Add(ur);
	ur->Redo();
}

void FModumateDocument::AddHideObjectsById(UWorld *world, const TArray<int32> &ids)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::AddHideObjectsById"));

	ClearRedoBuffer();

	UndoRedo *ur = new UndoRedo();

	ur->Redo = [this, ur, ids]()
	{
		UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::AddHideObjectsById::Redo"));
		for (auto id : ids)
		{
			FModumateObjectInstance *obj = GetObjectById(id);
			if (obj && !HiddenObjectsID.Contains(id))
			{
				obj->RequestHidden(FModumateDocument::DocumentHideRequestTag, true);
				obj->RequestCollisionDisabled(FModumateDocument::DocumentHideRequestTag, true);
				HiddenObjectsID.Add(id);
			}
		}

		ur->Undo = [this, ids]()
		{
			UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::AddHideObjectsById::Undo"));
			for (auto id : ids)
			{
				FModumateObjectInstance *obj = GetObjectById(id);
				if (HiddenObjectsID.Contains(id))
				{
					obj->RequestHidden(FModumateDocument::DocumentHideRequestTag, false);
					obj->RequestCollisionDisabled(FModumateDocument::DocumentHideRequestTag, false);
				}
				HiddenObjectsID.Remove(id);
			}
		};
	};

	UndoBuffer.Add(ur);

	ur->Redo();
}

void FModumateDocument::UnhideAllObjects(UWorld *world)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::UnhideAllObjects"));

	ClearRedoBuffer();

	UndoRedo *ur = new UndoRedo();
	TSet<int32> ids = HiddenObjectsID;

	ur->Redo = [this, ur, ids]()
	{
		UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::UnhideAllObjects::Redo"));
		for (auto id : ids)
		{
			if (FModumateObjectInstance *obj = GetObjectById(id))
			{
				obj->RequestHidden(FModumateDocument::DocumentHideRequestTag, false);
				obj->RequestCollisionDisabled(FModumateDocument::DocumentHideRequestTag, false);
			}
		}

		HiddenObjectsID.Empty();
		for (FModumateObjectInstance *obj : ObjectInstanceArray)
		{
			obj->UpdateVisibilityAndCollision();
		}

		ur->Undo = [this, ids]()
		{
			UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::UnhideAllObjects::Undo"));
			for (auto id : ids)
			{
				if (FModumateObjectInstance *obj = GetObjectById(id))
				{
					obj->RequestHidden(FModumateDocument::DocumentHideRequestTag, true);
					obj->RequestCollisionDisabled(FModumateDocument::DocumentHideRequestTag, true);
				}
			}

			HiddenObjectsID = ids;
			for (FModumateObjectInstance *obj : ObjectInstanceArray)
			{
				obj->UpdateVisibilityAndCollision();
			}
		};
	};

	UndoBuffer.Add(ur);

	ur->Redo();
}

bool FModumateDocument::MoveObjects(UWorld *world, const TArray<int32> &obs, const FVector &v)
{
	TArray<FModumateObjectInstance*> mois;
	Algo::Transform(obs, mois, [this](int32 id) {return GetObjectById(id); });
	return MoveObjects(world, mois, v);
}

bool FModumateDocument::MoveObjects(UWorld *world, const TArray<FModumateObjectInstance*> &obs, const FVector &moveDelta)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::MoveObjects"));

	if (obs.Num() == 0)
	{
		return false;
	}

	ClearRedoBuffer();

	UndoRedo *ur = new UndoRedo();

	TMap<int32, FMOIDataRecord> originalObjRecords;

	EGraph3DObjectType objectType;
	TArray<int32> volumeObjectIDs;

	for (auto *obj : obs)
	{
		int32 objID = obj->ID;

		if (IsObjectInVolumeGraph(objID, objectType))
		{
			volumeObjectIDs.Add(objID);
		}
		else
		{
			originalObjRecords.Add(objID, obj->AsDataRecord());
		}
	}
	
	// MoveMetaVertices has its own undo/redo, so this whole command needs its own macro
	if (volumeObjectIDs.Num() > 0)
	{
		BeginUndoRedoMacro();
		MoveMetaObjects(world, volumeObjectIDs, moveDelta);
	}

	ur->Redo = [this, ur, moveDelta, originalObjRecords, volumeObjectIDs]()
	{
		UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::MoveObjects::Redo"));
		for (auto &kvp : originalObjRecords)
		{
			FModumateObjectInstance *moi = GetObjectById(kvp.Key);
			if (moi != nullptr)
			{
				moi->SetFromDataRecordAndDisplacement(kvp.Value, moveDelta);
			}
		}

		ur->Undo = [this, originalObjRecords, volumeObjectIDs]()
		{
			UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::MoveObjects::Undo"));
			for (auto &kvp : originalObjRecords)
			{
				FModumateObjectInstance *moi = GetObjectById(kvp.Key);
				if (moi != nullptr)
				{
					moi->SetFromDataRecordAndDisplacement(kvp.Value, FVector::ZeroVector);
				}
			}
		};
	};

	UndoBuffer.Add(ur);

	ur->Redo();

	if (volumeObjectIDs.Num() > 0)
	{
		EndUndoRedoMacro();
	}

	return true;
}

void FModumateDocument::RotateObjects(UWorld *world, const TArray<int32> &obs, const FVector &v, const FQuat &q)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::RotateObjects"));
	TArray< FModumateObjectInstance*> mois;
	Algo::Transform(obs, mois, [this](int32 id) {return GetObjectById(id); });
	RotateObjects(world, mois, v, q);
}

void FModumateDocument::RotateObjects(UWorld *world, const TArray<FModumateObjectInstance*> &obs, const FVector &v, const FQuat &q)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::RotateObjects"));
	if (obs.Num() == 0)
	{
		return;
	}

	ClearRedoBuffer();

	UndoRedo *ur = new UndoRedo();

	TMap<int32, FMOIDataRecordV1> originalData;

	TArray<int32> volumeObjectIDs;
	EGraph3DObjectType objectType;

	for (auto *ob : obs)
	{
		int32 objID = ob->ID;

		if (IsObjectInVolumeGraph(objID, objectType))
		{
			volumeObjectIDs.Add(objID);
		}
		else
		{
			originalData.Add(ob->ID, ob->AsDataRecord());
		}
	}

	// TODO: wall mounted objects like portals can be moved along with the metaplanes that host their walls
	// This will be refactored to allow us to simply move metaplanes and have all the mounted objects come along automatically
	// In the meantime, child objects will be in the originalLocations map and will be moved separately so we macro the metaplane's undo/redo
	if (volumeObjectIDs.Num() > 0)
	{
		BeginUndoRedoMacro();
		RotateMetaObjectsAboutOrigin(world, volumeObjectIDs, v, q);
	}

	ur->Redo = [this, ur, v,q, originalData, volumeObjectIDs]()
	{
		UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::RotateObjects::Redo"));

		/*
		TODO: hierarchy and geometry operations are redundant here and the semantics of updating are not yet well understood
		On refactor, we want a simple way of modifying a metaplane and expecting everything to update itself cleanly
		*/
		for (auto &kvp : originalData)
		{
			FModumateObjectInstance *moi = GetObjectById(kvp.Key);
			if (ensureAlways(moi != nullptr))
			{
				moi->SetFromDataRecordAndRotation(kvp.Value, v, q);
			}
		}

		for (auto &vid : volumeObjectIDs)
		{
			FModumateObjectInstance *moi = GetObjectById(vid);
			if (ensureAlways(moi != nullptr))
			{
				moi->UpdateGeometry();
			}
		}

		ur->Undo = [this, originalData, volumeObjectIDs]()
		{
			UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::RotateObjects::Undo"));
			for (auto kvp : originalData)
			{
				FModumateObjectInstance *moi = GetObjectById(kvp.Key);
				if (ensureAlways(moi != nullptr))
				{
					moi->SetFromDataRecordAndRotation(kvp.Value, FVector::ZeroVector, FQuat::Identity);
				}
			}
			for (auto &vid : volumeObjectIDs)
			{
				FModumateObjectInstance *moi = GetObjectById(vid);
				if (ensureAlways(moi != nullptr))
				{
					moi->UpdateGeometry();
				}
			}
		};
	};

	UndoBuffer.Add(ur);
	ur->Redo();

	if (volumeObjectIDs.Num() > 0)
	{
		EndUndoRedoMacro();
	}
}

bool FModumateDocument::SetObjectTransforms(UWorld *world, const TArray<int32> &ObjectIDs, const TArray<int32> &ParentIDs, const TArray<FVector> &Positions, const TArray<FQuat> &Rotations)
{
	int32 numObjects = ObjectIDs.Num();
	if ((numObjects == 0) || (numObjects != ParentIDs.Num()) || (numObjects != Positions.Num()) || (numObjects != Rotations.Num()))
	{
		return false;
	}

	for (int32 i = 0; i < numObjects; ++i)
	{
		int32 objectID = ObjectIDs[i];
		int32 newParentID = ParentIDs[i];

		EGraph3DObjectType graphObjType;
		if (IsObjectInVolumeGraph(objectID, graphObjType))
		{
			UE_LOG(LogTemp, Warning, TEXT("Volume Graph objects are not supported in SetObjectTransform!"));
			return false;
		}

		FModumateObjectInstance *targetObject = GetObjectById(objectID);
		if (targetObject == nullptr)
		{
			return false;
		}
	}

	ClearRedoBuffer();
	UndoRedo *ur = new UndoRedo();

	ur->Redo = [this, ur, numObjects, ObjectIDs, ParentIDs, Positions, Rotations]()
	{
		TArray<FModumateObjectInstance *> targetObjects;
		TArray<int32> oldParentIDs;
		TArray<FVector> oldPositions;
		TArray<FQuat> oldRotations;

		for (int32 i = 0; i < numObjects; ++i)
		{
			FModumateObjectInstance *targetObject = GetObjectById(ObjectIDs[i]);
			targetObjects.Add(targetObject);

			oldParentIDs.Add(targetObject->GetParentID());

			FTransform transform = targetObject->GetWorldTransform();
			oldPositions.Add(transform.GetLocation());
			oldRotations.Add(transform.GetRotation());

			FModumateObjectInstance *newParentObject = GetObjectById(ParentIDs[i]);
			targetObject->SetParentObject(newParentObject);
			targetObject->SetWorldTransform(FTransform(Rotations[i], Positions[i]));
			targetObject->MarkDirty(EObjectDirtyFlags::Structure);
		}

		ur->Undo = [this, numObjects, ObjectIDs, oldParentIDs, oldPositions, oldRotations]()
		{
			for (int32 i = 0; i < numObjects; ++i)
			{
				FModumateObjectInstance *targetObject = GetObjectById(ObjectIDs[i]);

				FModumateObjectInstance *oldParentObject = GetObjectById(oldParentIDs[i]);
				targetObject->SetParentObject(oldParentObject);
				targetObject->SetWorldTransform(FTransform(oldRotations[i], oldPositions[i]));
				targetObject->MarkDirty(EObjectDirtyFlags::Structure);
			}
		};
	};

	UndoBuffer.Add(ur);
	ur->Redo();

	return true;
}

bool FModumateDocument::UpdateGeometry(UWorld *world, int32 id, const TArray<FVector> &points, const FVector& extents)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::UpdateGeometry"));
	FModumateObjectInstance *moi = GetObjectById(id);

	if (moi == nullptr)
	{
		return false;
	}

	switch (moi->GetObjectType())
	{
	case EObjectType::OTFloorSegment:
	case EObjectType::OTCabinet:
	case EObjectType::OTCountertop:
	case EObjectType::OTTrim:
	case EObjectType::OTRoofFace:
	case EObjectType::OTMetaPlane:
	case EObjectType::OTCutPlane:
	case EObjectType::OTScopeBox:
	case EObjectType::OTWallSegment:
		break;
	default:
		ensureAlwaysMsgf(false, TEXT("Invalid MOI; UpdateGeometry is not supported for type %s!"),
			*EnumValueString(EObjectType, moi->GetObjectType()));
		return false;
	}

	// TODO: use a different entry-point for updating metadata geometry,
	// but for now it makes the most sense to reuse poly point/edge handles.
	FGraph3DDelta graphDelta;
	bool bApplyDelta = false;
	if (moi->GetObjectType() == EObjectType::OTMetaPlane)
	{
		const FGraph3DFace *graphFace = VolumeGraph.FindFace(id);
		if (ensureAlways(graphFace && (graphFace->VertexIDs.Num() == points.Num())))
		{
			return MoveMetaVertices(world, graphFace->VertexIDs, points);
		}
	}

	const FModumateObjectInstance *parentObj = moi->GetParentObject();
	int32 parentPlaneID = (parentObj && (parentObj->GetObjectType() == EObjectType::OTMetaPlane)) ? parentObj->ID : MOD_ID_NONE;

	ClearRedoBuffer();
	UndoRedo *ur = new UndoRedo();

	ur->Redo = [ur, id, parentPlaneID, world, points, extents, moi, this]()
	{
		TArray<FVector> oldPoints = moi->GetControlPoints();
		FVector oldExtents = moi->GetExtents();

		moi->SetControlPoints(points);
		moi->SetExtents(extents);
		moi->MarkDirty(EObjectDirtyFlags::Structure);

		if (parentPlaneID != MOD_ID_NONE)
		{
			UpdateMitering(world, { parentPlaneID });
		}

		ur->Undo = [world, id, parentPlaneID, oldPoints, oldExtents, this]()
		{
			FModumateObjectInstance *moi = GetObjectById(id);
			if (moi != nullptr)
			{
				moi->SetControlPoints(oldPoints);
				moi->SetExtents(oldExtents);
				moi->MarkDirty(EObjectDirtyFlags::Structure);

				if (parentPlaneID != MOD_ID_NONE)
				{
					UpdateMitering(world, { parentPlaneID });
				}
			}
		};
	};

	UndoBuffer.Add(ur);
	ur->Redo();

	return true;
}

void FModumateDocument::UpdateControlIndices(int32 id, const TArray<int32> &indices)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::UpdateControlIndices"));
	FModumateObjectInstance *moi = GetObjectById(id);

	if (!ensureAlways(moi != nullptr))
	{
		return;
	}

	// first, make sure it's possible to set these control indices on this object
	for (int32 index : indices)
	{
		if (index >= moi->GetControlPoints().Num())
		{
			return;
		}
	}

	ClearRedoBuffer();
	UndoRedo *ur = new UndoRedo();

	ur->Redo = [ur, id, indices, moi, this]()
	{
		TArray<int32> oldIndices = moi->GetControlPointIndices();

		moi->SetControlPointIndices(indices);
		moi->UpdateGeometry();
		ur->Undo = [id, oldIndices, this]()
		{
			FModumateObjectInstance *ob = GetObjectById(id);
			if (ob != nullptr)
			{
				ob->SetControlPointIndices(oldIndices);
				ob->UpdateGeometry();
			}
		};
	};

	UndoBuffer.Add(ur);
	ur->Redo();
}

void FModumateDocument::UpdateControlValues(int32 id, const TArray<FVector> &controlPoints, const TArray<int32> &controlIndices)
{
	FModumateObjectInstance *moi = GetObjectById(id);

	if (!ensureAlways(moi != nullptr))
	{
		return;
	}

	ClearRedoBuffer();
	UndoRedo *ur = new UndoRedo();

	ur->Redo = [ur, id, moi, controlPoints, controlIndices, this]()
	{
		TArray<FVector> oldCPs = moi->GetControlPoints();
		TArray<int32> oldCIs = moi->GetControlPointIndices();

		moi->SetControlPoints(controlPoints);
		moi->SetControlPointIndices(controlIndices);
		moi->UpdateGeometry();

		ur->Undo = [id, oldCPs, oldCIs, this]()
		{
			FModumateObjectInstance *moi = GetObjectById(id);
			if (moi != nullptr)
			{
				moi->SetControlPoints(oldCPs);
				moi->SetControlPointIndices(oldCIs);
				moi->UpdateGeometry();
			}
		};
	};

	UndoBuffer.Add(ur);
	ur->Redo();
}

void FModumateDocument::Split(int32 id, const TArray<FVector> &pointsA, const TArray<FVector> &pointsB, const TArray<int32> &indicesA, const TArray<int32> &indicesB)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::Split"));
	FModumateObjectInstance *moi = GetObjectById(id);

	if (!moi || !moi->CanBeSplit())
	{
		UE_LOG(LogCallTrace, Error, TEXT("Cannot split invalid MOI, ID: %d!"), id);
		return;
	}

	UWorld* world = moi->GetActor()->GetWorld();
	if (world == nullptr)
	{
		return;
	}

	// Start of clone & modify undo/redo-able block
	BeginUndoRedoMacro();
	{
		// First undo/redo-able step; clone the original object so that we can give it the second half of control points
		int32 oldParentID = moi->GetParentID();
		int32 cloneID = CloneObject(world, moi);

		// Second undo/redoable step; modify the original object and its clone to have the two halves of the control points
		{
			FVector extentsA = moi->GetExtents();
			FVector extentsB = moi->GetExtents();

			ClearRedoBuffer();
			UndoRedo *ur = new UndoRedo();

			ur->Redo = [ur, id, cloneID, pointsA, pointsB, indicesA, indicesB, extentsA, extentsB, moi, this]()
			{
				TArray<FVector> oldPoints = moi->GetControlPoints();
				TArray<int32> oldIndices = moi->GetControlPointIndices();
				FVector oldExtents = moi->GetExtents();

				moi->SetControlPoints(pointsA);
				moi->SetControlPointIndices(indicesA);
				moi->SetExtents(extentsA);
				moi->SetupGeometry();

				FModumateObjectInstance *moiClone = GetObjectById(cloneID);
				if (ensureAlways(moiClone))
				{
					moiClone->SetControlPoints(pointsB);
					moiClone->SetControlPointIndices(indicesB);
					moiClone->SetExtents(extentsB);
					moiClone->SetupGeometry();
				}

				ur->Undo = [id, oldPoints, oldIndices, oldExtents, this]()
				{
					FModumateObjectInstance *ob = GetObjectById(id);
					if (ensureAlways(ob))
					{
						ob->SetControlPoints(oldPoints);
						ob->SetControlPointIndices(oldIndices);
						ob->SetExtents(oldExtents);
						ob->SetupGeometry();
					}
				};
			};

			UndoBuffer.Add(ur);
			ur->Redo();
		}

		// Third undo/redoable step; potentially group the two resulting cabinets together
		if (moi->GetObjectType() == EObjectType::OTCabinet)
		{
			MakeGroupObject(world, TArray<int32>({ id, cloneID }), true, oldParentID);
		}
	}
	EndUndoRedoMacro();
}

void FModumateDocument::AdjustMoiHoleVerts(int32 id, const FVector &location, const TArray<FVector>& holeVerts)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::AdjustMoiHoleVerts"));
	FModumateObjectInstance *holeObject = GetObjectById(id);

	if (holeObject == nullptr)
	{
		return;
	}
	TArray<FVector> oldHoleVerts = holeObject->GetControlPoints();
	FVector oldLocation = holeObject->GetActor()->GetActorLocation();
	FVector newLocation = location;

	ClearRedoBuffer();
	UndoRedo *ur = new UndoRedo();
	ur->Redo = [ur, id, holeVerts, oldHoleVerts, oldLocation, newLocation, this]()
	{
		FModumateObjectInstance *holeRedoObj = GetObjectById(id);
		holeRedoObj->SetControlPoints(holeVerts);
		holeRedoObj->UpdateGeometry();

		auto *wallParent = holeRedoObj->GetParentObject();
		if (wallParent && wallParent->GetObjectType() == EObjectType::OTWallSegment)
		{
			wallParent->SetupGeometry();
		}

		ur->Undo = [this, id, oldLocation, oldHoleVerts]()
		{
			if (FModumateObjectInstance *holeUndoObj = GetObjectById(id))
			{
				holeUndoObj->SetControlPoints(oldHoleVerts);
				holeUndoObj->UpdateGeometry();

				auto *wallParent = holeUndoObj->GetParentObject();
				if (wallParent && wallParent->GetObjectType() == EObjectType::OTWallSegment)
				{
					wallParent->SetupGeometry();
				}
			}
		};
	};

	UndoBuffer.Add(ur);
	ur->Redo();
}

bool FModumateDocument::InvertObjects(const TArray<FModumateObjectInstance*> &obs)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::InvertObjects"));
	if (obs.Num() == 0)
	{
		return false;
	}

	UndoRedo *ur = new UndoRedo();
	ClearRedoBuffer();

	ur->Redo = [this, ur, obs]()
	{
		for (auto &s : obs)
		{
			s->InvertObject();
		}
		ur->Undo = [this, obs]()
		{
			for (auto &s : obs)
			{
				s->InvertObject();
			}
		};
	};

	UndoBuffer.Add(ur);
	ur->Redo();

	return true;
}

void FModumateDocument::RestoreDeletedObjects(const TArray<int32> &ids)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::RestoreDeletedObjects"));
	if (ids.Num() == 0)
	{
		return;
	}

	UndoRedo *ur = new UndoRedo();
	ClearRedoBuffer();

	ur->Redo = [this, ur, ids]()
	{
		UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::RestoreDeletedObjects::Redo"));
		TArray<FModumateObjectInstance*> oldObs;
		for (auto id : ids)
		{
			FModumateObjectInstance *oldOb = TryGetDeletedObject(id);
			if (oldOb != nullptr)
			{
				RestoreObjectImpl(oldOb);
				oldObs.AddUnique(oldOb);
			}
		}

		ur->Undo = [this, oldObs]()
		{
			UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::RestoreDeletedObjects::Undo"));
			for (auto &ob : oldObs)
			{
				DeleteObjectImpl(ob);
			}
		};
	};
	UndoBuffer.Add(ur);
	ur->Redo();
}

bool FModumateDocument::DeleteObjectImpl(FModumateObjectInstance *ob, bool keepInDeletedList)
{
	if (ob)
	{
		// Store off the connected objects, in case they will be affected by this deletion
		TArray<FModumateObjectInstance *> connectedMOIs;
		ob->GetConnectedMOIs(connectedMOIs);

		// Remove a deleted object from its parent, so the hierarchy can be correct
		if (auto *parent = ob->GetParentObject())
		{
			parent->RemoveChild(ob);

			// But also store the old parent, in case the deleted object is restored
			ob->SetParentID(parent->ID);
		}

		ob->Destroy();
		ObjectInstanceArray.Remove(ob);
		ObjectsByID.Remove(ob->ID);

		if (keepInDeletedList)
		{
			DeletedObjects.Add(ob->ID, ob);
		}
		else
		{
			delete ob;
		}

		// Update visibility & collision enabled on neighbors, in case it was dependent on this MOI.
		for (FModumateObjectInstance *obj : connectedMOIs)
		{
			obj->UpdateVisibilityAndCollision();
		}

		return true;
	}

	return false;
}

bool FModumateDocument::RestoreObjectImpl(FModumateObjectInstance *obj)
{
	if (obj && (DeletedObjects.FindRef(obj->ID) == obj))
	{
		DeletedObjects.Remove(obj->ID);
		ObjectInstanceArray.AddUnique(obj);
		ObjectsByID.Add(obj->ID, obj);
		obj->RestoreActor();
		obj->SetParentObject(GetObjectById(obj->GetParentID()), true);
		obj->MarkDirty(EObjectDirtyFlags::Visuals);
		obj->PostRestoreObject();

		RestoreChildrenImpl(obj);

		return true;
	}

	return false;
}

// TODO: this function results in extra SetupGeometry calls.  It should be replaced by a 
// RestoreObjectsImpl that can restore the actors and recover the parent/child relationships
// and then SetupGeometry given a correct object heirarchy
bool FModumateDocument::RestoreChildrenImpl(FModumateObjectInstance *obj)
{
	if (obj)
	{
		for (int32 childID : obj->GetChildren())
		{
			auto childObj = GetObjectById(childID);
			if (childObj)
			{
				childObj->RestoreActor();
				childObj->SetupGeometry();
				childObj->SetParentObject(obj, true);

				// TODO: this code hits an ensureAlways when the childObj is already not destroyed,
				// possibly because PostRestoreObject is called in RestoreObjectImpl
				//obj->PostRestoreObject();

				RestoreChildrenImpl(childObj);
			}
		}

		return true;
	}

	return false;
}

FModumateObjectInstance* FModumateDocument::CreateOrRestoreObjFromObjectType(
	UWorld *World,
	EObjectType OT,
	int32 ID,
	int32 ParentID,
	const FVector &Extents,
	const TArray<FVector> *CPS,
	const TArray<int32> *CPI,
	bool bInverted)
{
	FModumateObjectAssembly obAsm;
	obAsm.ObjectType = OT;
	return CreateOrRestoreObjFromAssembly(World, obAsm, ID, ParentID, Extents, CPS, CPI, bInverted);
}

FModumateObjectInstance* FModumateDocument::CreateOrRestoreObjFromAssembly(
	UWorld *World,
	const FModumateObjectAssembly &Assembly,
	int32 ID,
	int32 ParentID,
	const FVector &Extents,
	const TArray<FVector> *CPS,
	const TArray<int32> *CPI,
	bool bInverted)
{
	// Check to make sure NextID represents the next highest ID we can allocate to a new object.
	if (ID >= NextID)
	{
		NextID = ID + 1;
	}

	FModumateObjectInstance* obj = TryGetDeletedObject(ID);
	if (obj && RestoreObjectImpl(obj))
	{
		return obj;
	}
	else
	{
		if (!ensureAlwaysMsgf(!ObjectsByID.Contains(ID),
			TEXT("Tried to create a new object with the same ID (%d) as an existing one!"), ID))
		{
			return nullptr;
		}

		obj = new FModumateObjectInstance(World, this, Assembly, ID);
		ObjectInstanceArray.AddUnique(obj);
		ObjectsByID.Add(ID, obj);

		if (CPS)
		{
			obj->SetControlPoints(*CPS);
		}
		if (CPI)
		{
			obj->SetControlPointIndices(*CPI);
		}
		obj->SetObjectInverted(bInverted);
		obj->SetExtents(Extents);
		obj->SetupGeometry();
		obj->SetParentObject(GetObjectById(ParentID));
		obj->UpdateVisibilityAndCollision();

		UModumateAnalyticsStatics::RecordObjectCreation(World, Assembly.ObjectType);

		return obj;
	}
}

bool FModumateDocument::ApplyMOIDelta(const FMOIDelta &Delta, UWorld *World)
{
	for (auto &kvp : Delta.StatePairs)
	{
		const FMOIStateData &baseState = kvp.Key;
		const FMOIStateData &targetState = kvp.Value;

		switch (kvp.Value.StateType)
		{
			case EMOIDeltaType::Create:
			{
				/*
				TODO: consolidate all object creation code here
				*/
				EToolMode toolMode = UModumateTypeStatics::ToolModeFromObjectType(targetState.ObjectType);

				// No tool mode implies a line segment (used by multiple tools with no assembly)
				// TODO: generalize assemblyless/tool modeless non-graph MOIs
				const FModumateObjectAssembly *assembly = toolMode != EToolMode::VE_NONE ? PresetManager.GetAssemblyByKey(toolMode, targetState.ObjectAssemblyKey) : nullptr;

				// If we got an assembly, build the object with it, otherwise by type
				FModumateObjectInstance *newInstance = (assembly != nullptr) ?
					CreateOrRestoreObjFromAssembly(World, *assembly, targetState.ObjectID, targetState.ParentID, targetState.Extents, &targetState.ControlPoints, &targetState.ControlIndices, targetState.bObjectInverted) :
					CreateOrRestoreObjFromObjectType(World, targetState.ObjectType, targetState.ObjectID, targetState.ParentID, FVector::ZeroVector, &targetState.ControlPoints, &targetState.ControlIndices, targetState.bObjectInverted);

				if (newInstance != nullptr)
				{
					if (NextID <= newInstance->ID)
					{
						NextID = newInstance->ID + 1;
					}
					newInstance->SetObjectLocation(targetState.Location);
					newInstance->SetObjectRotation(targetState.Orientation);

					// TODO: this is relatively safe, but shouldn't be necessary; either
					// - properties should be in the construct explicitly,
					// - the FModumateObjectInstance constructor should accept FMOIStateData,
					// - or the creation flow should allow setting the entire state, as Mutate does, except after creation.
					newInstance->SetAllProperties(targetState.ObjectProperties);
				}
			}
			break;

			case EMOIDeltaType::Destroy:
			{
				FModumateObjectInstance *obj = GetObjectById(targetState.ObjectID);
				if (ensureAlways(obj != nullptr))
				{
					DeleteObjectImpl(obj);
				}
			}
			break;

			case EMOIDeltaType::Mutate:
			{
				FModumateObjectInstance *MOI = GetObjectById(targetState.ObjectID);

				if (!ensureAlways(MOI != nullptr))
				{
					return false;
				}

				switch (MOI->GetObjectType())
				{
				case EObjectType::OTMetaPlane:
					ensureAlwaysMsgf(false, TEXT("Illegal MOI type sent to ApplyMOIDelta"));
					return false;
				};

				EToolMode toolMode = UModumateTypeStatics::ToolModeFromObjectType(targetState.ObjectType);
				if (toolMode != EToolMode::VE_NONE)
				{
					FName assemblyKey = MOI->GetAssembly().DatabaseKey;
					if (targetState.ObjectAssemblyKey != assemblyKey)
					{
						const FModumateObjectAssembly *obAsm = PresetManager.GetAssemblyByKey(toolMode, targetState.ObjectAssemblyKey);
						if (ensureAlwaysMsgf(obAsm != nullptr, TEXT("Could not find assembly by key %s"), *targetState.ObjectAssemblyKey.ToString()))
						{
							MOI->SetAssembly(*obAsm);
						}
					}
				}

				MOI->SetDataState(targetState);
			}
			break;

			default:
				ensureAlways(false);
			break;
		};
	}

	return true;
}

void FModumateDocument::ApplyGraph2DDelta(const FGraph2DDelta &Delta, UWorld *World)
{
	FGraph2D *targetSurfaceGraph = nullptr;

	switch (Delta.DeltaType)
	{
	case EGraph2DDeltaType::Add:
		if (!ensure(!SurfaceGraphs.Contains(Delta.ID)))
		{
			return;
		}
		targetSurfaceGraph = &SurfaceGraphs.Add(Delta.ID, FGraph2D(Delta.ID));
		break;
	case EGraph2DDeltaType::Edit:
		if (!ensure(SurfaceGraphs.Contains(Delta.ID)))
		{
			return;
		}
		targetSurfaceGraph = SurfaceGraphs.Find(Delta.ID);
		break;
	case EGraph2DDeltaType::Remove:
		ensure(SurfaceGraphs.Contains(Delta.ID));
		SurfaceGraphs.Remove(Delta.ID);
		return;
	}

	// TODO: potentially finalize delta by updating connected objects
	targetSurfaceGraph->ApplyDelta(Delta);
}

void FModumateDocument::ApplyGraph3DDelta(const FGraph3DDelta &Delta, UWorld *World)
{
	TArray<FVector> controlPoints;

	// TODO: the graph may need an understanding of "net" deleted objects
	// objects that are deleted, but their metadata (like hosted obj) is not
	// passed on to another object
	// if it is passed on to another object, the MarkDirty here is redundant
	TSet<int32> dirtyGroupIDs;
	for (auto& kvp : Delta.VertexDeletions)
	{
		auto vertex = VolumeGraph.FindVertex(kvp.Key);
		if (vertex == nullptr)
		{
			continue;
		}
		dirtyGroupIDs.Append(vertex->GroupIDs);
	}
	for (auto& kvp : Delta.EdgeDeletions)
	{
		auto edge = VolumeGraph.FindEdge(kvp.Key);
		if (edge == nullptr)
		{
			continue;
		}
		dirtyGroupIDs.Append(edge->GroupIDs);
	}
	for (auto& kvp : Delta.FaceDeletions)
	{
		auto face = VolumeGraph.FindFace(kvp.Key);
		if (face == nullptr)
		{
			continue;
		}
		dirtyGroupIDs.Append(face->GroupIDs);
	}

	for (int32 groupID : dirtyGroupIDs)
	{
		auto groupObj = GetObjectById(groupID);
		if (groupObj != nullptr)
		{
			groupObj->MarkDirty(EObjectDirtyFlags::Structure);
		}
	}

	VolumeGraph.ApplyDelta(Delta);

	for (auto &kvp : Delta.VertexAdditions)
	{
		const FVector &vertexPos = kvp.Value;
		controlPoints.SetNum(1);
		controlPoints[0] = vertexPos;
		CreateOrRestoreObjFromObjectType(World, EObjectType::OTMetaVertex,
			kvp.Key, MOD_ID_NONE, FVector::ZeroVector, &controlPoints, nullptr);
	}

	for (auto &kvp : Delta.VertexDeletions)
	{
		FModumateObjectInstance *deletedVertexObj = GetObjectById(kvp.Key);
		if (ensureAlways(deletedVertexObj))
		{
			DeleteObjectImpl(deletedVertexObj);
		}
	}

	for (auto &kvp : Delta.EdgeAdditions)
	{
		const TArray<int32> &edgeVertexIDs = kvp.Value.Vertices;
		ensureAlways(kvp.Value.Vertices.Num() == 2);
		controlPoints.SetNum(2);
		FVector edgePosition(ForceInitToZero);
		const FGraph3DVertex *startVertex = VolumeGraph.FindVertex(edgeVertexIDs[0]);
		const FGraph3DVertex *endVertex = VolumeGraph.FindVertex(edgeVertexIDs[1]);
		if (startVertex && endVertex)
		{
			controlPoints[0] = startVertex->Position;
			controlPoints[1] = endVertex->Position;
			edgePosition = 0.5f * (startVertex->Position + endVertex->Position);
		}

		CreateOrRestoreObjFromObjectType(World, EObjectType::OTMetaEdge,
			kvp.Key, MOD_ID_NONE, FVector::ZeroVector, &controlPoints, nullptr);
	}

	for (auto &kvp : Delta.EdgeDeletions)
	{
		FModumateObjectInstance *deletedEdgeObj = GetObjectById(kvp.Key);
		if (ensureAlways(deletedEdgeObj))
		{
			DeleteObjectImpl(deletedEdgeObj);
		}
	}

	for (auto &kvp : Delta.FaceAdditions)
	{
		controlPoints.Reset();
		for (int32 faceVertexID : kvp.Value.Vertices)
		{
			const FGraph3DVertex *faceVertex = VolumeGraph.FindVertex(faceVertexID);
			if (ensureAlways(faceVertex))
			{
				controlPoints.Add(faceVertex->Position);
			}
		}

		CreateOrRestoreObjFromObjectType(World, EObjectType::OTMetaPlane,
			kvp.Key, MOD_ID_NONE, FVector::ZeroVector, &controlPoints, nullptr);
	}

	// when modifying objects in the document, first add objects, then modify parent objects, then delete objects
	for (auto &kvp : Delta.ParentIDUpdates)
	{
		auto obj = GetObjectById(kvp.Key);
		if (obj == nullptr)
		{
			const FGraph3DHostedObjectDelta& objUpdate = kvp.Value;
			auto newParentObj = GetObjectById(objUpdate.NextParentID);
			// When the previous object exists, use its information to clone a new object
			auto hostedObj = GetObjectById(objUpdate.PreviousHostedObjID);
			if (hostedObj)
			{
				auto newObj = CreateOrRestoreObjFromAssembly(World, hostedObj->GetAssembly(), kvp.Key, objUpdate.NextParentID, hostedObj->GetExtents(), nullptr, &hostedObj->GetControlPointIndices(), hostedObj->GetObjectInverted());
			}
			// Otherwise, attempt to restore the previous object (during undo)
			else
			{
				auto *oldObj = TryGetDeletedObject(kvp.Key);
				if (ensureAlways(oldObj != nullptr))
				{
					RestoreObjectImpl(oldObj);
				}
			}
		}
	}

	// there is a separate loop afterwards because re-parenting objects may rely on objects that are being added
	TSet<int32> objsMarkedForDelete;
	for (auto &kvp : Delta.ParentIDUpdates)
	{
		auto obj = GetObjectById(kvp.Key);
		if (obj != nullptr)
		{
			FGraph3DHostedObjectDelta objUpdate = kvp.Value;
			auto newParentObj = GetObjectById(objUpdate.NextParentID);

			if (newParentObj != nullptr)
			{
				// Save off the world transform of the portal, since we want it to be the same regardless
				// of its changed relative transforms between its old and new parents.
				FTransform worldTransform = obj->GetWorldTransform();
				newParentObj->AddChild(obj);
				obj->SetWorldTransform(worldTransform);
			}
			else
			{
				objsMarkedForDelete.Add(kvp.Key);
			}
		}
	}

	for (auto objID : objsMarkedForDelete)
	{
		DeleteObjectImpl(GetObjectById(objID));
	}

	for (auto &kvp : Delta.FaceDeletions)
	{
		FModumateObjectInstance *deletedFaceObj = GetObjectById(kvp.Key);
		if (ensureAlways(deletedFaceObj))
		{
			DeleteObjectImpl(deletedFaceObj);
		}
	}
}

bool FModumateDocument::ApplyDeltas(const TArray<TSharedPtr<FDelta>> &Deltas, UWorld *World)
{
	ClearRedoBuffer();

	BeginUndoRedoMacro();
	UndoRedo *ur = new UndoRedo();
	ur->Deltas = Deltas;

	ur->Redo = [this, ur, World]()
	{
		for (auto& delta : ur->Deltas)
		{
			delta->ApplyTo(this, World);
		}

		PostApplyDelta(World);

		ur->Undo = [this, ur, World]()
		{
			auto inverseDeltas = ur->Deltas;
			Algo::Reverse(inverseDeltas);

			for (auto& delta : inverseDeltas)
			{
				TSharedPtr<FDelta> inverseDelta = delta->MakeInverse();
				if (inverseDelta != nullptr)
				{
					inverseDelta->ApplyTo(this, World);
				}
			}

			PostApplyDelta(World);
		};
	};

	UndoBuffer.Add(ur);
	ur->Redo();

	UpdateRoomAnalysis(World);
	EndUndoRedoMacro();

	return true;
}

bool FModumateDocument::UpdateGraphObjects(UWorld *World)
{
	// TODO: unclear whether this is correct or the best place -
	// set the faces containing or contained by dirty faces dirty as well	
	for (auto& kvp : VolumeGraph.GetFaces())
	{
		auto& face = kvp.Value;
		if (!face.bDirty)
		{
			continue;
		}

		if (auto containingFace = VolumeGraph.FindFace(face.ContainingFaceID))
		{
			containingFace->Dirty(false);
		}
		for (int32 containedFaceID : face.ContainedFaceIDs)
		{
			if (auto containedFace = VolumeGraph.FindFace(containedFaceID))
			{
				containedFace->Dirty(false);
			}
		}
	}

	TSet<int32> dirtyGroupIDs;
	TArray<int32> cleanedVertices, cleanedEdges, cleanedFaces;
	if (VolumeGraph.CleanGraph(cleanedVertices, cleanedEdges, cleanedFaces))
	{
		for (int32 vertexID : cleanedVertices)
		{
			FGraph3DVertex *graphVertex = VolumeGraph.FindVertex(vertexID);
			FModumateObjectInstance *vertexObj = GetObjectById(vertexID);
			if (graphVertex && vertexObj && (vertexObj->GetObjectType() == EObjectType::OTMetaVertex))
			{
				vertexObj->SetObjectLocation(graphVertex->Position);
			}
			dirtyGroupIDs.Append(graphVertex->GroupIDs);
		}

		for (int32 edgeID : cleanedEdges)
		{
			FGraph3DEdge *graphEdge = VolumeGraph.FindEdge(edgeID);
			FModumateObjectInstance *edgeObj = GetObjectById(edgeID);
			if (!(graphEdge && edgeObj && (edgeObj->GetObjectType() == EObjectType::OTMetaEdge)))
			{
				continue;
			}

			FGraph3DVertex *startVertex = VolumeGraph.FindVertex(graphEdge->StartVertexID);
			FGraph3DVertex *endVertex = VolumeGraph.FindVertex(graphEdge->EndVertexID);
			if (!(startVertex && endVertex))
			{
				continue;
			}

			edgeObj->SetControlPoints({ startVertex->Position, endVertex->Position });
			edgeObj->MarkDirty(EObjectDirtyFlags::Structure);
			dirtyGroupIDs.Append(graphEdge->GroupIDs);
		}

		for (int32 faceID : cleanedFaces)
		{
			FGraph3DFace *graphFace = VolumeGraph.FindFace(faceID);
			FModumateObjectInstance *faceObj = GetObjectById(faceID);
			if (!(graphFace && faceObj && (faceObj->GetObjectType() == EObjectType::OTMetaPlane)))
			{
				continue;
			}

			// TODO: using perimeter instead of cached positions would help adjustment handles
			// show up on only exterior edges
			faceObj->SetControlPoints(graphFace->CachedPositions);
			
			faceObj->MarkDirty(EObjectDirtyFlags::Structure);
			dirtyGroupIDs.Append(graphFace->GroupIDs);
		}
	}

	// dirty group objects that are related to dirtied graph objects
	for (int32 groupID : dirtyGroupIDs)
	{
		FModumateObjectInstance *groupObj = GetObjectById(groupID);
		if (groupObj != nullptr)
		{
			groupObj->MarkDirty(EObjectDirtyFlags::Structure);
		}
	}

	return true;
}

bool FModumateDocument::FinalizeGraphDeltas(TArray <FGraph3DDelta> &Deltas, TArray<int32> &OutAddedFaceIDs, TArray<int32> &OutAddedVertexIDs, TArray<int32> &OutAddedEdgeIDs)
{
	FGraph3D moiTempGraph;
	FGraph3D::CloneFromGraph(moiTempGraph, VolumeGraph);

	for (auto& delta : Deltas)
	{
		if (!FinalizeGraphDelta(moiTempGraph, delta))
		{
			return false;
		}

		for (auto &kvp : delta.FaceAdditions)
		{
			if (kvp.Value.ParentObjIDs.Num() == 1 && GetObjectById(kvp.Value.ParentObjIDs[0]) == nullptr)
			{
				OutAddedFaceIDs.Add(kvp.Key);
			}
		}
		for (auto &kvp : delta.VertexAdditions)
		{
			OutAddedVertexIDs.Add(kvp.Key);
		}
		for (auto &kvp : delta.EdgeAdditions)
		{
			OutAddedEdgeIDs.Add(kvp.Key);
		}

		for (auto &kvp : delta.FaceDeletions)
		{
			OutAddedFaceIDs.Remove(kvp.Key);
		}
		for (auto &kvp : delta.VertexDeletions)
		{
			OutAddedVertexIDs.Remove(kvp.Key);
		}
		for (auto &kvp : delta.EdgeDeletions)
		{
			OutAddedEdgeIDs.Remove(kvp.Key);
		}
	}

	for (int32 faceID : OutAddedFaceIDs)
	{
		if (moiTempGraph.FindOverlappingFace(faceID) != MOD_ID_NONE)
		{
			return false;
		}
	}

	return true;
}

bool FModumateDocument::PostApplyDelta(UWorld *World)
{
	UpdateGraphObjects(World);
	FGraph3D::CloneFromGraph(TempVolumeGraph, VolumeGraph);

	// Now that objects may have been deleted, validate the player state so that none of them are incorrectly referenced.
	AEditModelPlayerState_CPP *playerState = Cast<AEditModelPlayerState_CPP>(World->GetFirstPlayerController()->PlayerState);
	playerState->ValidateSelectionsAndView();

	return true;
}

void FModumateDocument::DeleteObjects(const TArray<int32> &obIds, bool bAllowRoomAnalysis, bool bDeleteConnected)
{
	TArray<FModumateObjectInstance*> mois;
	Algo::Transform(obIds,mois,[this](int32 id) { return GetObjectById(id);});

	DeleteObjects(
		mois.FilterByPredicate([](FModumateObjectInstance *ob) { return ob != nullptr; }),
		bAllowRoomAnalysis, bDeleteConnected
	);
}

void FModumateDocument::DeleteObjects(const TArray<FModumateObjectInstance*> &obs, bool bAllowRoomAnalysis, bool bDeleteConnected)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::DeleteObjects"));
	if (obs.Num() == 0)
	{
		return;
	}

	UWorld *world = obs[0]->GetWorld();
	if (!ensureAlways(world != nullptr))
	{
		return;
	}

	TArray<int32> vertex3DDeletions, edge3DDeletions, face3DDeletions, graphGroupDeletions;
	TSet<FTypedGraphObjID> graphGroupMembers;
	bool bDeletingObjectsInGraph = false;
	bool bDeletingNavigableObjects = false;
	for (int32 i = obs.Num() - 1; i >= 0; --i)
	{
		FModumateObjectInstance *ob = obs[i];

		EGraph3DObjectType graphObjType;
		if (IsObjectInVolumeGraph(ob->ID, graphObjType))
		{
			bDeletingObjectsInGraph = true;
			switch (graphObjType)
			{
			case EGraph3DObjectType::Vertex:
				vertex3DDeletions.Add(ob->ID);
				break;
			case EGraph3DObjectType::Edge:
				edge3DDeletions.Add(ob->ID);
				break;
			case EGraph3DObjectType::Face:
				face3DDeletions.Add(ob->ID);
				break;
			default:
				break;
			}
		}

		if (ob->GetObjectType() == EObjectType::OTFloorSegment)
		{
			bDeletingNavigableObjects = true;
		}

		if (VolumeGraph.GetGroup(ob->ID, graphGroupMembers))
		{
			graphGroupDeletions.Add(ob->ID);
			bDeletingObjectsInGraph = true;
		}
	}

	// Do room analysis if we're affecting any room-related objects, such as meta graph objects or navigable objects
	bool bDoRoomAnalysis = ((bDeletingObjectsInGraph || bDeletingNavigableObjects) && bAllowRoomAnalysis);

	// TODO: we should strive to immediately delete these graph objects, to make this operation closer match other
	// meta object functions, and then perform the rest of the object deletions. This will make the deltas simpler to use.
	FGraph3DDelta graphDelta;
	bool bApplyDelta = false;
	if (bDeletingObjectsInGraph)
	{
		bApplyDelta = TempVolumeGraph.GetDeltaForDeleteObjects(vertex3DDeletions, edge3DDeletions, face3DDeletions, graphGroupDeletions, graphDelta, bDeleteConnected);
	}

	// If we're allowing room analysis, then start a macro since it may have side effects
	if (bDoRoomAnalysis)
	{
		BeginUndoRedoMacro();
	}

	// GetDeltaForDeleteObjects will find additional objects to delete that are made invalid by the original deletes
	// the descendants of these objects need to be deleted as well below
	TArray<FModumateObjectInstance*> obsAndGraphObs = obs;
	for (auto& kvp : graphDelta.FaceDeletions)
	{
		auto ob = GetObjectById(kvp.Key);
		if (ob != nullptr)
		{
			obsAndGraphObs.AddUnique(ob);
		}
	}
	for (auto& kvp : graphDelta.EdgeDeletions)
	{
		auto ob = GetObjectById(kvp.Key);
		if (ob != nullptr)
		{
			obsAndGraphObs.AddUnique(ob);
		}
	}
	for (auto& kvp : graphDelta.VertexDeletions)
	{
		auto ob = GetObjectById(kvp.Key);
		if (ob != nullptr)
		{
			obsAndGraphObs.AddUnique(ob);
		}
	}

	// TODO: do we depend on order or can we add ob in main loop?
	TArray<FModumateObjectInstance *> allobs;
	for (auto &ob : obsAndGraphObs)
	{
		allobs.Append(ob->GetAllDescendents());
	}
	allobs.Append(obsAndGraphObs);

	TArray<int32> deletedObjParentPlaneIDs;
	for (int32 i = allobs.Num() - 1; i >= 0; --i)
	{
		FModumateObjectInstance *ob = allobs[i];

		EGraph3DObjectType graphObjType;
		if (IsObjectInVolumeGraph(ob->ID, graphObjType))
		{
			allobs.RemoveAt(i);
		}

		const FModumateObjectInstance *parent = ob->GetParentObject();
		if (parent && (parent->GetObjectType() == EObjectType::OTMetaPlane))
		{
			deletedObjParentPlaneIDs.AddUnique(parent->ID);
		}

		UModumateAnalyticsStatics::RecordObjectDeletion(world, ob->GetObjectType());
	}

	UndoRedo *ur = new UndoRedo();
	ClearRedoBuffer();

	ur->Redo = [this, ur, allobs, world, graphDelta, bApplyDelta, deletedObjParentPlaneIDs]()
	{
		TArray<FModumateObjectInstance*> deletedObjs;

		UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::DeleteObjects::Redo"));

		if (bApplyDelta)
		{
			// TODO: fix
			ApplyGraph3DDelta(graphDelta, world);
			PostApplyDelta(world);
		}

		bool doorsDirty = false;
		bool windowsDirty = false;
		// TODO: Figure out a better way to broadcast BP widget changes to reduce complexity of this function
		bool bCutPlanesDirty = false;
		bool bScopeBoxesDirty = false;
		for (auto *obj : allobs)
		{
			DeleteObjectImpl(obj);

			if (obj->GetObjectType() == EObjectType::OTDoor)
			{
				doorsDirty = true;
			}
			if (obj->GetObjectType() == EObjectType::OTWindow)
			{
				windowsDirty = true;
			}
			if (obj->GetObjectType() == EObjectType::OTCutPlane)
			{
				bCutPlanesDirty = true;
			}
			if (obj->GetObjectType() == EObjectType::OTScopeBox)
			{
				bScopeBoxesDirty = true;
			}

			deletedObjs.Add(obj);
		}

		if (windowsDirty)
		{
			ResequencePortalAssemblies_DEPRECATED(world, EObjectType::OTWindow);
		}

		if (doorsDirty)
		{
			ResequencePortalAssemblies_DEPRECATED(world, EObjectType::OTDoor);
		}

		if (bCutPlanesDirty || bScopeBoxesDirty)
		{
			AEditModelPlayerState_CPP* emPlayerState = Cast<AEditModelPlayerState_CPP>(world->GetFirstPlayerController()->PlayerState);
			if (bCutPlanesDirty && emPlayerState)
			{
				emPlayerState->OnUpdateCutPlanes.Broadcast();
			}
			if (bScopeBoxesDirty && emPlayerState)
			{
				emPlayerState->OnUpdateScopeBoxes.Broadcast();
			}
		}

		UpdateMitering(world, deletedObjParentPlaneIDs);

		ur->Undo = [this, deletedObjs, world, windowsDirty, doorsDirty, bApplyDelta, graphDelta, deletedObjParentPlaneIDs, bCutPlanesDirty, bScopeBoxesDirty]()
		{
			UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::DeleteObjects::Undo"));

			if (bApplyDelta)
			{
				auto inverseDelta = graphDelta.MakeInverse();
				inverseDelta->ApplyTo(this, world);
				PostApplyDelta(world);
			}

			// Restore objects in reverse order, to emulate the order in which the objects are created
			// so that parents know about their kids correctly.
			for (int32 i = deletedObjs.Num() - 1; i >= 0; --i)
			{
				auto &s = deletedObjs[i];
				RestoreObjectImpl(s);
			}

			if (windowsDirty)
			{
				ResequencePortalAssemblies_DEPRECATED(world, EObjectType::OTWindow);
			}

			if (doorsDirty)
			{
				ResequencePortalAssemblies_DEPRECATED(world, EObjectType::OTDoor);
			}

			if (bCutPlanesDirty || bScopeBoxesDirty)
			{
				AEditModelPlayerState_CPP* emPlayerState = Cast<AEditModelPlayerState_CPP>(world->GetFirstPlayerController()->PlayerState);
				if (bCutPlanesDirty && emPlayerState)
				{
					emPlayerState->OnUpdateCutPlanes.Broadcast();
				}
				if (bScopeBoxesDirty && emPlayerState)
				{
					emPlayerState->OnUpdateScopeBoxes.Broadcast();
				}
			}

			UpdateMitering(world, deletedObjParentPlaneIDs);
		};
	};

	UndoBuffer.Add(ur);
	ur->Redo();

	if (bDoRoomAnalysis)
	{
		UpdateRoomAnalysis(world);
		EndUndoRedoMacro();
	}
}

FModumateObjectInstance *FModumateDocument::TryGetDeletedObject(int32 id)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::TryGetDeletedObject"));
	return DeletedObjects.FindRef(id);
}

int32 FModumateDocument::MakeGroupObject(UWorld *world, const TArray<int32> &ids, bool combineWithExistingGroups, int32 parentID)
{
	UndoRedo *ur = new UndoRedo();
	ClearRedoBuffer();

	int id = NextID++;

	ur->Redo = [id,this,world,ur,ids,parentID]()
	{
		UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::MakeGroupObject::Redo"));

		TArray<FModumateObjectInstance*> obs;
		Algo::Transform(ids,obs,[this](int32 id){return GetObjectById(id);}); 

		TMap<FModumateObjectInstance*, FModumateObjectInstance*> oldParents;
		for (auto ob : obs)
		{
			oldParents.Add(ob, ob->GetParentObject());
		}

		auto *groupObj = CreateOrRestoreObjFromObjectType(world, EObjectType::OTGroup, id, parentID);

		for (auto ob : obs)
		{
			ob->SetParentObject(groupObj);
		}

		ur->Undo = [groupObj, oldParents, this]()
		{
			UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::MakeGroupObject::Undo"));

			for (auto oldP : oldParents)
			{
				oldP.Key->SetParentObject(oldP.Value);
			}

			DeleteObjectImpl(groupObj);
		};
	};
	UndoBuffer.Add(ur);
	ur->Redo();
	return id;
}

void FModumateDocument::UnmakeGroupObjects(UWorld *world, const TArray<int32> &groupIds)
{
	UndoRedo *ur = new UndoRedo();
	ClearRedoBuffer();

	AEditModelGameMode_CPP *gameMode = world->GetAuthGameMode<AEditModelGameMode_CPP>();

	ur->Redo = [this, world, ur, groupIds]()
	{
		UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::UnmakeGroupObjects::Redo"));

		TArray<FModumateObjectInstance*> obs;
		Algo::Transform(groupIds,obs,[this](int32 id){return GetObjectById(id);});

		TMap<FModumateObjectInstance*, TArray<FModumateObjectInstance*>> oldChildren;

		for (auto ob : obs)
		{
			TArray<FModumateObjectInstance*> children = ob->GetChildObjects();
			oldChildren.Add(ob, children);
			for (auto child : children)
			{
				child->SetParentObject(nullptr);
			}
			DeleteObjectImpl(ob);
		}

		ur->Undo = [oldChildren, this]()
		{
			UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::UnmakeGroupObjects::Undo"));

			for (auto oldC : oldChildren)
			{
				RestoreObjectImpl(oldC.Key);
				for (auto child : oldC.Value)
				{
					child->SetParentObject(oldC.Key);
				}
			}

		};
	};
	UndoBuffer.Add(ur);
	ur->Redo();
}

// TODO: we should merge this with MoveMetaObjects and make it a general transformation operation
bool FModumateDocument::RotateMetaObjectsAboutOrigin(UWorld *World, const TArray<int32> &ObjectIDs, const FVector &origin, const FQuat &rotation)
{
	TMap<int32, FVector> idsAndPositions;
	for (int32 objID : ObjectIDs)
	{
		EGraph3DObjectType type;
		if (!IsObjectInVolumeGraph(objID, type))
		{
			UE_LOG(LogTemp, Error, TEXT("Object not in Volume Graph"));
			continue;
		}

		// TODO: objects besides face
		if (type == EGraph3DObjectType::Face)
		{
			FGraph3DFace *face = VolumeGraph.FindFace(objID);

			for (auto vertexID : face->VertexIDs)
			{
				auto vertex = VolumeGraph.FindVertex(vertexID);
				FVector delta = rotation.RotateVector(vertex->Position - origin);
				if (vertex && !idsAndPositions.Contains(vertexID))
				{
					idsAndPositions.Add(vertexID,origin + delta);
				}
			}
		}
		else if (type == EGraph3DObjectType::Edge)
		{
			FGraph3DEdge *edge = VolumeGraph.FindEdge(objID);

			auto vertex = VolumeGraph.FindVertex(edge->StartVertexID);

			FVector delta = rotation.RotateVector(vertex->Position - origin);

			if (vertex && !idsAndPositions.Contains(edge->StartVertexID))
			{
				idsAndPositions.Add(edge->StartVertexID,origin + delta);
			}

			vertex = VolumeGraph.FindVertex(edge->EndVertexID);
			delta = rotation.RotateVector(vertex->Position - origin);
			if (vertex && !idsAndPositions.Contains(edge->EndVertexID))
			{
				idsAndPositions.Add(edge->EndVertexID, origin + delta);
			}
		}
		else if (type == EGraph3DObjectType::Vertex)
		{
			FGraph3DVertex *vertex = VolumeGraph.FindVertex(objID);
			FVector delta = rotation.RotateVector(vertex->Position - origin);

			if (vertex && !idsAndPositions.Contains(objID))
			{
				idsAndPositions.Add(objID, origin + delta);
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Object is not a meta object."));
		}
	}

	TArray<int32> ids;
	TArray<FVector> positions;
	for (auto& kvp : idsAndPositions)
	{
		ids.Add(kvp.Key);
		positions.Add(kvp.Value);
	}

	return MoveMetaVertices(World, ids, positions);
}


bool FModumateDocument::MoveMetaObjects(UWorld *World, const TArray<int32> &ObjectIDs, const FVector &Displacement)
{
	TMap<int32, FVector> idsAndPositions;
	for (int32 objID : ObjectIDs)
	{
		EGraph3DObjectType type;
		if (!IsObjectInVolumeGraph(objID, type))
		{
			UE_LOG(LogTemp, Error, TEXT("Object not in Volume Graph"));
			continue;
		}

		// TODO: objects besides face
		if (type == EGraph3DObjectType::Face)
		{
			FGraph3DFace *face = VolumeGraph.FindFace(objID);

			for (auto vertexID : face->VertexIDs)
			{
				auto vertex = VolumeGraph.FindVertex(vertexID);
				if (vertex && !idsAndPositions.Contains(vertexID))
				{
					idsAndPositions.Add(vertexID, vertex->Position + Displacement);
				}
			}
		}
		else if (type == EGraph3DObjectType::Edge)
		{
			FGraph3DEdge *edge = VolumeGraph.FindEdge(objID);

			auto vertex = VolumeGraph.FindVertex(edge->StartVertexID);
			if (vertex && !idsAndPositions.Contains(edge->StartVertexID))
			{
				idsAndPositions.Add(edge->StartVertexID, vertex->Position + Displacement);
			}
			
			vertex = VolumeGraph.FindVertex(edge->EndVertexID);
			if (vertex && !idsAndPositions.Contains(edge->EndVertexID))
			{
				idsAndPositions.Add(edge->EndVertexID, vertex->Position + Displacement);
			}
		}
		else if (type == EGraph3DObjectType::Vertex)
		{
			FGraph3DVertex *vertex = VolumeGraph.FindVertex(objID);

			if (vertex && !idsAndPositions.Contains(objID))
			{
				idsAndPositions.Add(objID, vertex->Position + Displacement);
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Object is not a meta object."));
		}
	}

	TArray<int32> ids;
	TArray<FVector> positions;
	for (auto& kvp : idsAndPositions)
	{
		ids.Add(kvp.Key);
		positions.Add(kvp.Value);
	}

	return MoveMetaVertices(World, ids, positions);
}

bool FModumateDocument::MoveMetaVertices(UWorld *World, int32 ObjectID, const FVector &Displacement)
{
	EGraph3DObjectType type;
	if (!IsObjectInVolumeGraph(ObjectID, type))
	{
		UE_LOG(LogTemp, Error, TEXT("Object not in Volume Graph"));
		return false;
	}

	// TODO: objects besides face
	if (type != EGraph3DObjectType::Face)
	{
		UE_LOG(LogTemp, Warning, TEXT("Only face objects are moveable with the move tool"));
		return false;
	}

	FGraph3DFace *face = VolumeGraph.FindFace(ObjectID);

	TArray<FVector> VertexPositions;

	for (auto vertexID : face->VertexIDs)
	{
		auto vertex = VolumeGraph.FindVertex(vertexID);
		VertexPositions.Add(vertex->Position + Displacement);
	}

	return MoveMetaVertices(World, face->VertexIDs, VertexPositions);
}

bool FModumateDocument::MoveMetaVertices(UWorld *World, const TArray<int32> &VertexIDs, const TArray<FVector> &VertexPositions)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::MoveMetaVertices"));

	TArray<FGraph3DDelta> deltas;

	if (!TempVolumeGraph.GetDeltaForVertexMovements(VertexIDs, VertexPositions, deltas, NextID))
	{
		FGraph3D::CloneFromGraph(TempVolumeGraph, VolumeGraph);
		return false;
	}

	TArray<int32> faceIDs, vertexIDs, edgeIDs;
	if (!FinalizeGraphDeltas(deltas, faceIDs, vertexIDs, edgeIDs))
	{
		FGraph3D::CloneFromGraph(TempVolumeGraph, VolumeGraph);
		return false;
	}

	TArray<TSharedPtr<FDelta>> deltaptrs;
	for (auto& delta : deltas)
	{
		deltaptrs.Add(MakeShareable<FDelta>(new FGraph3DDelta(delta)));
	}

	return ApplyDeltas(deltaptrs, World);
}

bool FModumateDocument::JoinMetaObjects(UWorld *World, const TArray<int32> &ObjectIDs)
{
	if (ObjectIDs.Num() < 2)
	{
		return false;
	}

	EGraph3DObjectType objectType;
	if (!IsObjectInVolumeGraph(ObjectIDs[0], objectType))
	{
		return false;
	}

	TArray<FGraph3DDelta> graphDeltas;
	if (!TempVolumeGraph.GetDeltasForObjectJoin(graphDeltas, ObjectIDs, NextID, objectType))
	{
		FGraph3D::CloneFromGraph(TempVolumeGraph, VolumeGraph);
		return false;
	}

	TArray<int32> faceIDs, vertexIDs, edgeIDs;
	if (!FinalizeGraphDeltas(graphDeltas, faceIDs, vertexIDs, edgeIDs))
	{
		FGraph3D::CloneFromGraph(TempVolumeGraph, VolumeGraph);
		return false;
	}

	TArray<TSharedPtr<FDelta>> deltaptrs;
	for (auto& delta : graphDeltas)
	{
		deltaptrs.Add(MakeShareable<FDelta>(new FGraph3DDelta(delta)));
	}
	return ApplyDeltas(deltaptrs, World);
}

int32 FModumateDocument::MakeRoom(UWorld *World, const TArray<FSignedID> &FaceIDs)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::MakeRoom"));

	UndoRedo *ur = new UndoRedo();
	ClearRedoBuffer();

	int id = NextID++;

	ur->Redo = [this, ur, id, World, FaceIDs]()
	{
		UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::MakeRoom::Redo"));

		FModumateObjectInstance *newRoomObj = CreateOrRestoreObjFromObjectType(World, EObjectType::OTRoom,
			id, MOD_ID_NONE, FVector::ZeroVector, nullptr, &FaceIDs);

		UModumateRoomStatics::SetRoomConfigFromKey(newRoomObj, UModumateRoomStatics::DefaultRoomConfigKey);

		newRoomObj->UpdateVisibilityAndCollision();

		ur->Undo = [newRoomObj,this]()
		{
			DeleteObjectImpl(newRoomObj);
		};
	};

	UndoBuffer.Add(ur);
	ur->Redo();

	return id;
}

int32 FModumateDocument::MakePointsObject(
	UWorld *world,
	const TArray<int32> &idsToDelete,
	const TArray<FVector> &points,
	const TArray<int32> &controlIndices,
	EObjectType objectType,
	bool inverted,
	const FModumateObjectAssembly &assembly,
	int32 parentID,
	bool bUpdateSiblingGeometry)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::MakePointsObject"));
	UndoRedo *ur = new UndoRedo();
	ClearRedoBuffer();

	int32 id = NextID++;

	auto updateSiblingGeometry = [this, id, parentID]()
	{
		FModumateObjectInstance *parent = GetObjectById(parentID);
		if (parent)
		{
			auto siblings = parent->GetChildObjects();
			for (FModumateObjectInstance *sibling : siblings)
			{
				if (sibling && (sibling->ID != id))
				{
					sibling->UpdateGeometry();
				}
			}
		}
	};

	ur->Redo = [this, points, controlIndices, id, objectType, assembly, inverted, ur,
		idsToDelete, world, parentID, bUpdateSiblingGeometry, updateSiblingGeometry]()
	{
		UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::MakePointsObject::Redo"));

		FModumateObjectInstance *newOb = CreateOrRestoreObjFromAssembly(
			world, assembly, id, parentID, FVector::ZeroVector, &points, &controlIndices, inverted);

		TArray<FModumateObjectInstance*> objectsToDelete;
		Algo::Transform(idsToDelete,objectsToDelete,[this](int32 id){return GetObjectById(id);});

		for (auto *objToDelete : objectsToDelete)
		{
			if (objToDelete != nullptr)
			{
				DeleteObjectImpl(objToDelete);
			}
		}

		if (bUpdateSiblingGeometry)
		{
			updateSiblingGeometry();
		}

		ur->Undo = [this, id, parentID, world, idsToDelete, newOb, bUpdateSiblingGeometry, updateSiblingGeometry]()
		{
			UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::MakePointsObject::Undo"));
			DeleteObjectImpl(newOb);

			for (auto idToDelete : idsToDelete)
			{
				FModumateObjectInstance *objToDelete = TryGetDeletedObject(idToDelete);
				if (objToDelete != nullptr)
				{
					RestoreObjectImpl(objToDelete);
				}
			}

			if (bUpdateSiblingGeometry)
			{
				updateSiblingGeometry();
			}
		};
	};

	UndoBuffer.Add(ur);
	ur->Redo();

	return id;
}

bool FModumateDocument::MakeMetaObject(UWorld *world, const TArray<FVector> &points, const TArray<int32> &IDs, EObjectType objectType, int32 parentID, TArray<int32> &OutObjIDs)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::MakeMetaObject"));
	OutObjIDs.Reset();

	EGraph3DObjectType graphObjectType = UModumateTypeStatics::Graph3DObjectTypeFromObjectType(objectType);
	if (!ensureAlways(graphObjectType != EGraph3DObjectType::None))
	{
		return false;
	}

	bool bValidDelta = false;
	int32 numPoints = points.Num();
	int32 numIDs = IDs.Num();
	FGraph3DDelta graphDelta;
	TArray<FGraph3DDelta> deltas;
	int32 id = MOD_ID_NONE;

	switch (graphObjectType)
	{
	case EGraph3DObjectType::Vertex:
	{
		bValidDelta = (numPoints == 1) && TempVolumeGraph.GetDeltaForVertexAddition(points[0], graphDelta, NextID, id);
		deltas = { graphDelta };
	}
		break;
	case EGraph3DObjectType::Edge:
	{
		TArray<int32> OutEdgeIDs;
		if (numPoints == 2)
		{
			bValidDelta = TempVolumeGraph.GetDeltaForEdgeAdditionWithSplit(points[0], points[1], deltas, NextID, OutEdgeIDs, true);
		}
		else
		{
			bValidDelta = false;
		}
	}
		break;
	case EGraph3DObjectType::Face:
	{
		if (numPoints >= 3)
		{
			bValidDelta = TempVolumeGraph.GetDeltaForFaceAddition(points, deltas, NextID, id);
		}
	}
		break;
	default:
		break;
	}

	if (!bValidDelta)
	{
		// delta will be false if the object exists, out object ids should contain the existing id
		if (id != MOD_ID_NONE)
		{
			OutObjIDs.Add(id);
		}
		FGraph3D::CloneFromGraph(TempVolumeGraph, VolumeGraph);
		return false;
	}

	TArray<int32> faceIDs, vertexIDs, edgeIDs;
	if (!ensureAlways(FinalizeGraphDeltas(deltas, faceIDs, vertexIDs, edgeIDs)))
	{
		FGraph3D::CloneFromGraph(TempVolumeGraph, VolumeGraph);
		return false;
	}

	TArray<TSharedPtr<FDelta>> deltaptrs;
	for (auto& delta : deltas)
	{
		deltaptrs.Add(MakeShareable<FDelta>(new FGraph3DDelta(delta)));
	}
	bool bSuccess = ApplyDeltas(deltaptrs, world);

	OutObjIDs.Append(faceIDs);
	OutObjIDs.Append(vertexIDs);
	OutObjIDs.Append(edgeIDs);

	return bSuccess;
}

bool FModumateDocument::MakeCutPlaneObject(UWorld *world, const TArray<FVector> &points, TArray<int32> &OutObjIDs)
{
	UndoRedo *ur = new UndoRedo();
	ClearRedoBuffer();
	int32 id = NextID++;

	ur->Redo = [this, ur, id, world, points]()
	{
		auto newObj = CreateOrRestoreObjFromObjectType(world, EObjectType::OTCutPlane, id, 0, FVector::ZeroVector, &points);
		AEditModelPlayerState_CPP* emPlayerState = Cast<AEditModelPlayerState_CPP>(world->GetFirstPlayerController()->PlayerState);
		if (emPlayerState)
		{
			emPlayerState->OnUpdateCutPlanes.Broadcast();
		}
		ur->Undo = [this, world, newObj, emPlayerState]()
		{
			DeleteObjectImpl(newObj);
			if (emPlayerState)
			{
				emPlayerState->OnUpdateCutPlanes.Broadcast();
			}
		};
	};

	UndoBuffer.Add(ur);
	ur->Redo();

	OutObjIDs.Add(id);

	return true;
}

bool FModumateDocument::MakeScopeBoxObject(UWorld *world, const TArray<FVector> &points, TArray<int32> &OutObjIDs, const float Height)
{
	UndoRedo *ur = new UndoRedo();
	ClearRedoBuffer();
	int32 id = NextID++;

	FVector extents = FVector(0.0f, Height, 0.0f);

	ur->Redo = [this, ur, id, world, extents, points]()
	{
		auto newObj = CreateOrRestoreObjFromObjectType(world, EObjectType::OTScopeBox, id, 0, extents, &points);
		AEditModelPlayerState_CPP* emPlayerState = Cast<AEditModelPlayerState_CPP>(world->GetFirstPlayerController()->PlayerState);
		if (emPlayerState)
		{
			emPlayerState->OnUpdateScopeBoxes.Broadcast();
		}
		ur->Undo = [this, world, newObj, emPlayerState]()
		{
			if (emPlayerState)
			{
				emPlayerState->OnUpdateScopeBoxes.Broadcast();
			}
			DeleteObjectImpl(newObj);
		};
	};

	UndoBuffer.Add(ur);
	ur->Redo();

	OutObjIDs.Add(id);

	return true;
}

bool FModumateDocument::FinalizeGraphDelta(FGraph3D &TempGraph, FGraph3DDelta &Delta)
{
	TMap<int32, TArray<int32>> parentIDToChildrenIDs;

	for (auto &kvp : Delta.FaceAdditions)
	{
		for (int32 parentID : kvp.Value.ParentObjIDs)
		{
			if (parentIDToChildrenIDs.Contains(parentID))
			{
				parentIDToChildrenIDs[parentID].Add(kvp.Key);
			}
			else
			{
				parentIDToChildrenIDs.Add(parentID, { kvp.Key });
			}
		}
	}

	for (auto &kvp : Delta.EdgeAdditions)
	{
		for (int32 parentID : kvp.Value.ParentObjIDs)
		{
			parentID = FMath::Abs(parentID);
			if (parentIDToChildrenIDs.Contains(parentID))
			{
				parentIDToChildrenIDs[parentID].Add(FMath::Abs(kvp.Key));
			}
			else
			{
				parentIDToChildrenIDs.Add(parentID, { FMath::Abs(kvp.Key) });
			}
		}
	}

	// both the temp graph and the original graph are used here -
	// it is necessary to compare the new faces to the old faces in order to figure out where the 
	// child object are supposed to go (the original graph is used to populate parentFace below)
	TempGraph.ApplyDelta(Delta);

	TMap<int32, int32> childFaceIDToHostedID;
	TSet<int32> idsWithObjects;
	TSet<int32> rejectedObjects;

	for (auto &kvp : parentIDToChildrenIDs)
	{
		int32 parentID = kvp.Key;
		auto parentObj = GetObjectById(parentID);

		// obtained from the original graph because this face is removed by now
		auto parentFace = VolumeGraph.FindFace(parentID);

		if (parentObj && parentObj->GetChildren().Num() > 0)
		{
			for (int32 childIdx = 0; childIdx < parentObj->GetChildren().Num(); childIdx++)
			{
				auto childObj = GetObjectById(parentObj->GetChildren()[childIdx]);
				// wall/floor objects need to be cloned to each child object, and will be deleted during face deletions
				for (int32 childFaceID : kvp.Value)
				{
					if (!idsWithObjects.Contains(childFaceID))
					{
						int32 newObjID = NextID++;
						Delta.ParentIDUpdates.Add(newObjID, FGraph3DHostedObjectDelta(parentObj->GetChildren()[childIdx], MOD_ID_NONE, childFaceID));
						childFaceIDToHostedID.Add(childFaceID, newObjID);
						idsWithObjects.Add(childFaceID);
					}
					else
					{
						rejectedObjects.Add(parentObj->GetChildren()[childIdx]);
					}
				}

				// loop through children to find the new planes that they apply to
				// TODO: implementation is object specific and should be moved there
				// portals should be on 0 or 1 of the new planes, and finishes should be on all applicable planes
				for (int32 childObjId : childObj->GetChildren())
				{
					auto childHostedObj = GetObjectById(childObjId);
					if (childHostedObj && (childHostedObj->GetObjectType() == EObjectType::OTWindow || childHostedObj->GetObjectType() == EObjectType::OTDoor))
					{
						auto childFaceIDs = parentIDToChildrenIDs[parentID];

						TArray<FVector> corners;
						for (int32 corneridx = 0; corneridx < 4; corneridx++)
						{
							corners.Add(childHostedObj->GetCorner(corneridx));
						}
						TArray<FVector2D> portalCornersOnFace;
						for (FVector& corner : corners)
						{
							portalCornersOnFace.Add(parentFace->ProjectPosition2D(corner));
						}
						FBox2D portalBB = FBox2D(portalCornersOnFace);

						for (int32 childFaceID : childFaceIDs)
						{
							auto childFace = TempGraph.FindFace(childFaceID);
							// TODO: AABBs will not work for concave faces, and will even have problems for faces that aren't rectangles
							TArray<FVector2D> childCornersOnFace;
							for (FVector& childPos : childFace->CachedPositions)
							{
								childCornersOnFace.Add(parentFace->ProjectPosition2D(childPos));
							}
							// FBox2D functions are strict, so this adds tolerance
							FBox2D faceBB = FBox2D(childCornersOnFace).ExpandBy(VolumeGraph.Epsilon);

							if (faceBB.IsInside(portalBB))
							{
								Delta.ParentIDUpdates.Add(childObjId, FGraph3DHostedObjectDelta(MOD_ID_NONE, childObj->ID, childFaceIDToHostedID[childFaceID]));

								break;
							}
							// reject all cases where a portal would be split
							else 
							{
								// FBox2D functions are strict, so this adds tolerance
								faceBB = faceBB.ExpandBy(-VolumeGraph.Epsilon * 2.0f);
								if (faceBB.Intersect(portalBB))
								{
									return false;
								}
							}
						}
					}
					else if (childHostedObj && (childHostedObj->GetObjectType() == EObjectType::OTFinish))
					{
						auto childFaceIDs = parentIDToChildrenIDs[parentID];
						for (int32 childFaceID : childFaceIDs)
						{
							if (!rejectedObjects.Contains(childObj->ID))
							{
								int32 newObjID = NextID++;
								Delta.ParentIDUpdates.Add(newObjID, FGraph3DHostedObjectDelta(childObjId, MOD_ID_NONE, childFaceIDToHostedID[childFaceID]));
							}
						}
						Delta.ParentIDUpdates.Add(childObjId, FGraph3DHostedObjectDelta(MOD_ID_NONE, childObj->ID, MOD_ID_NONE));
					}
				}
			}
		}
	}

	TArray<int32> deletedObjIDs;
	for (auto &kvp : Delta.FaceDeletions)
	{
		deletedObjIDs.Add(kvp.Key);
	}
	for (auto &kvp : Delta.EdgeDeletions)
	{
		deletedObjIDs.Add(kvp.Key);
	}

	for (int32 objID : deletedObjIDs)
	{
		auto obj = GetObjectById(objID);
		auto face = TempGraph.FindFace(objID);

		if (obj && obj->GetChildren().Num() > 0)
		{
			for (int32 childIdx = 0; childIdx < obj->GetChildren().Num(); childIdx++)
			{
				Delta.ParentIDUpdates.Add(obj->GetChildren()[childIdx], FGraph3DHostedObjectDelta(MOD_ID_NONE, objID, MOD_ID_NONE));
			}
		}
	}

	return true;
}

void FModumateDocument::ClearRedoBuffer()
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::ClearRedoBuffer"));
	for (auto redo : RedoBuffer)
	{
		delete redo;
	}
	RedoBuffer.Empty();
}

void FModumateDocument::ClearUndoBuffer()
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::ClearUndoBuffer"));
	for (auto undo : UndoBuffer)
	{
		delete undo;
	}
	UndoBuffer.Empty();
	UndoRedoMacroStack.Empty();
}

FBoxSphereBounds FModumateDocument::CalculateProjectBounds() const
{
	TArray<FVector> allMOIPoints;
	TArray<FStructurePoint> curMOIPoints;
	TArray<FStructureLine> curMOILines;

	for (const FModumateObjectInstance *moi : ObjectInstanceArray)
	{
		// TODO: add this functionality to MOIs - IsMetaObject(), meta-objects do not contribute 
		// to project bounds
		if (moi->GetObjectType() == EObjectType::OTCutPlane || moi->GetObjectType() == EObjectType::OTScopeBox)
		{
			continue;
		}

		curMOIPoints.Reset();
		curMOILines.Reset();
		moi->GetStructuralPointsAndLines(curMOIPoints, curMOILines);

		for (const FStructurePoint &point : curMOIPoints)
		{
			allMOIPoints.Add(point.Point);
		}
	}

	FBoxSphereBounds projectBounds(allMOIPoints.GetData(), allMOIPoints.Num());
	return projectBounds;
}

int32 FModumateDocument::MakePortalAt(
	UWorld *world,
	EObjectType portalType,
	int32 parentID,
	const FVector2D &relativePos,
	const FQuat &relativeRot,
	bool inverted,
	const FModumateObjectAssembly &pal)
{
	AEditModelGameMode_CPP *gameState = world->GetAuthGameMode<AEditModelGameMode_CPP>();
	FModumateObjectInstance *portalParent = GetObjectById(parentID);
	if (!ensureAlways((gameState != nullptr) && (portalParent != nullptr)))
	{
		return 0;
	}

	// check whether the new portal would need to modify, split, or delete any existing trim objects
	FVector worldPos(ForceInitToZero);
	FQuat worldRot(ForceInit);
	if (!UModumateObjectStatics::GetWorldTransformOnPlaneHostedObj(portalParent,
		relativePos, relativeRot, worldPos, worldRot))
	{
		return 0;
	}

	FTransform holeTransform(worldRot, worldPos);
	TArray<FModumateObjectInstance *> trimsToDelete;
	TMap<FModumateObjectInstance *, TPair<FVector2D, FVector2D>> trimsToSplit;
	TMap<FModumateObjectInstance *, FVector2D> trimsToModify;
	bool bPortalModifiesTrim = UModumateObjectStatics::FindPortalTrimOverlaps(portalParent, pal, holeTransform,
		trimsToDelete, trimsToSplit, trimsToModify);

	ClearRedoBuffer();

	// Step 1 (optional): delete / split / modify trims affected by the portal
	if (bPortalModifiesTrim)
	{
		BeginUndoRedoMacro();

		if (trimsToDelete.Num() > 0)
		{
			DeleteObjects(trimsToDelete, false, false);
		}

		for (auto &kvp : trimsToSplit)
		{
			FModumateObjectInstance *trimObj = kvp.Key;
			FVector2D newStartTrimExtent = kvp.Value.Key;
			FVector2D newEndTrimExtent = kvp.Value.Value;

			// cut the starting trim segment short before the portal
			TArray<FVector> newStartControlPoints({ FVector(newStartTrimExtent.X, 0.0f, 0.0f), FVector(newStartTrimExtent.Y, 0.0f, 0.0f) });
			UpdateGeometry(world, trimObj->ID, newStartControlPoints, trimObj->GetExtents());

			// and create a new trim segment after the portal
			TArray<int32> emptyIDsToDelete;
			TArray<FVector> newEndControlPoints({ FVector(newEndTrimExtent.X, 0.0f, 0.0f), FVector(newEndTrimExtent.Y, 0.0f, 0.0f) });
			MakePointsObject(world, emptyIDsToDelete, newEndControlPoints, trimObj->GetControlPointIndices(), EObjectType::OTTrim, trimObj->GetObjectInverted(), trimObj->GetAssembly(), trimObj->GetParentID());
		}

		for (auto &kvp : trimsToModify)
		{
			FModumateObjectInstance *trimObj = kvp.Key;
			FVector2D newTrimExtent = kvp.Value;

			TArray<FVector> newControlPoints({ FVector(newTrimExtent.X, 0.0f, 0.0f), FVector(newTrimExtent.Y, 0.0f, 0.0f) });
			UpdateGeometry(world, trimObj->ID, newControlPoints, trimObj->GetExtents());
		}
	}

	// Step 2: actually create the portal object
	UndoRedo *ur = new UndoRedo();

	int32 id = NextID++;

	ur->Redo = [ur,inverted,world,portalType,pal,id,relativePos,relativeRot,portalParent,this]()
	{
		FModumateObjectInstance *newOb = CreateOrRestoreObjFromAssembly(world, pal, id);

		// TODO: this is needed because only for redo operations because the parent ID gets saved,
		// need to revisit this in a way that doesn't cause redundant geometry updates.
		newOb->SetParentObject(nullptr);

		newOb->SetObjectLocation(FVector(relativePos, 0.0f));
		newOb->SetObjectRotation(relativeRot);
		newOb->SetParentObject(portalParent);
		newOb->SetAssembly(pal);
		newOb->SetupGeometry();
		ResequencePortalAssemblies_DEPRECATED(world, portalType);

		if (inverted)
		{
			float w = newOb->GetControlPoint(2).Y - newOb->GetControlPoint(1).Y;
			for (int32 i=0;i<newOb->GetControlPoints().Num();++i)
			{
				FVector cp = newOb->GetControlPoint(i);
				cp.Y -= w;
				newOb->SetControlPoint(i, cp);
			}
			newOb->InvertObject();
		}

		ur->Undo = [this, id, world, portalType]()
		{
			FModumateObjectInstance *ob = GetObjectById(id);
			DeleteObjectImpl(ob);
			ResequencePortalAssemblies_DEPRECATED(world, portalType);
		};
	};

	UndoBuffer.Add(ur);
	ur->Redo();

	if (bPortalModifiesTrim)
	{
		EndUndoRedoMacro();
	}

	return id;
}

void FModumateDocument::TransverseObjects(const TArray<FModumateObjectInstance*> &obs)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::TransversePortalObjects"));
	if (obs.Num() == 0)
	{
		return;
	}

	UndoRedo *ur = new UndoRedo();
	ClearRedoBuffer();

	ur->Redo = [this, ur, obs]()
	{
		for (auto &s : obs)
		{
			s->TransverseObject();
		}
		ur->Undo = [obs]()
		{
			for (auto &s : obs)
			{
				s->TransverseObject();
			}
		};
	};

	UndoBuffer.Add(ur);
	ur->Redo();
}

int32 FModumateDocument::CreateFFE(
	UWorld *world,
	int32 parentID,
	const FVector &loc,
	const FQuat &rot,
	const FModumateObjectAssembly &obAsm,
	int32 parentFaceIdx)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::CreateObjectAt"));
	UndoRedo *ur = new UndoRedo();

	FVector locVal(loc);
	FQuat rotVal(rot);

	ClearRedoBuffer();

	int32 id = NextID++;

	// We may have to modify the assembly, preserve the old one for undo
	FModumateObjectAssembly useAsm=obAsm;
	FModumateObjectAssembly oldAsm=obAsm;

	FName newFFECode;

	/*
	If the incoming assembly has no code name, this is its first placement in the document and needs a code name
	*/
	if (useAsm.GetProperty(BIM::Parameters::Code).AsString().IsEmpty())
	{
		newFFECode = PresetManager.GetAvailableKey(TEXT("FFE"));
		useAsm.SetProperty(BIM::Parameters::Code, newFFECode);
		for (auto &l : useAsm.Layers)
		{
			l.CodeName = useAsm.GetProperty(BIM::Parameters::Code);
		}
	}

	ur->Redo = [this, newFFECode, ur, id, parentID, world, locVal, rotVal, useAsm, oldAsm, parentFaceIdx]()
	{
		UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::CreateObjectAt::Redo"));

		TArray<int32> controlIndices = { parentFaceIdx };
		FModumateObjectInstance *newOb = CreateOrRestoreObjFromAssembly(world, useAsm, id, parentID,
			FVector::ZeroVector, nullptr, &controlIndices);
		newOb->SetObjectLocation(locVal);
		newOb->SetObjectRotation(rotVal);
		newOb->SetupGeometry();

		// If we generated a code name, this is the first time this object was used
		if (!newFFECode.IsNone())
		{
			DataCollection<FModumateObjectAssembly>  *db = PresetManager.AssemblyDBs_DEPRECATED.Find(EToolMode::VE_PLACEOBJECT);
			if (ensureAlways(db != nullptr))
			{
				db->RemoveData(oldAsm);
				db->AddData(useAsm);
				OnAssemblyUpdated(world, EToolMode::VE_PLACEOBJECT, useAsm);
			}
		}

		ur->Undo = [this, oldAsm, world, useAsm, newFFECode, id]()
		{
			DataCollection<FModumateObjectAssembly>  *db = PresetManager.AssemblyDBs_DEPRECATED.Find(EToolMode::VE_PLACEOBJECT);

			// If we generated a new name, restore the old assembly without the codename
			if (ensureAlways(db != nullptr) && !newFFECode.IsNone())
			{
				db->RemoveData(useAsm);
				db->AddData(oldAsm);
				OnAssemblyUpdated(world, EToolMode::VE_PLACEOBJECT, oldAsm);
			}

			UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::CreateObjectAt::Redo"));
			FModumateObjectInstance *ob = GetObjectById(id);
			DeleteObjectImpl(ob);
		};
	};
	UndoBuffer.Add(ur);
	ur->Redo();
	return id;
}

bool FModumateDocument::CleanObjects()
{
	int32 totalObjectCleans = 0;
	TArray<FModumateObjectInstance*> curDirtyList;
	for (EObjectDirtyFlags flagToClean : UModumateTypeStatics::OrderedDirtyFlags)
	{
		// Arbitrarily cut off object cleaning iteration, in case there's bad logic that
		// creates circular dependencies that will never resolve in a single frame.
		int32 iterationSafeguard = 99;
		bool bModifiedAnyObjects = false;
		int32 objectCleans = 0;

		do
		{
			// Save off the current list of dirty objects, since it may change while cleaning them.
			bModifiedAnyObjects = false;
			TArray<FModumateObjectInstance*> &dirtyObjList = DirtyObjectMap.FindOrAdd(flagToClean);
			curDirtyList = dirtyObjList;

			for (FModumateObjectInstance *objToClean : curDirtyList)
			{
				bModifiedAnyObjects = objToClean->CleanObject(flagToClean) || bModifiedAnyObjects;
				if (bModifiedAnyObjects)
				{
					++objectCleans;
				}
			}

			if (!ensureAlwaysMsgf(--iterationSafeguard > 0, TEXT("Infinite loop detected while cleaning objects, aborting!")))
			{
				break;
			}
		} while (bModifiedAnyObjects);

		if (objectCleans > ObjectInstanceArray.Num())
		{
			UE_LOG(LogTemp, Warning, TEXT("Cleaned object(s) more than once!"));
		}

		totalObjectCleans += objectCleans;
	}

	return (totalObjectCleans > 0);
}

void FModumateDocument::RegisterDirtyObject(EObjectDirtyFlags DirtyType, FModumateObjectInstance *DirtyObj, bool bDirty)
{
	// Make sure only one dirty flag is used at a time
	int32 flagInt = (int32)DirtyType;
	if (!ensureAlwaysMsgf((DirtyObj != nullptr) && (DirtyType != EObjectDirtyFlags::None) && ((flagInt & (flagInt - 1)) == 0),
		TEXT("Must mark a non-null object with exactly one dirty flag type at a time!")))
	{
		return;
	}

	TArray<FModumateObjectInstance*> &dirtyObjList = DirtyObjectMap.FindOrAdd(DirtyType);

	if (bDirty)
	{
		// We can use TArray with Add rather than AddUnique or TSet because objects already check against their own
		// dirty flag before registering themselves with the Document.
		dirtyObjList.Add(DirtyObj);
	}
	else
	{
		dirtyObjList.Remove(DirtyObj);
	}
}

void FModumateDocument::AddCommandToHistory(const FString &cmd)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::AddCommand"));
	CommandHistory.Add(cmd);
}

TArray<FString> FModumateDocument::GetCommandHistory() const
{
	return CommandHistory;
}

void FModumateDocument::MakeNew(UWorld *world)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::MakeNew"));

	int32 numObjects = ObjectInstanceArray.Num();
	for (int32 i = numObjects - 1; i >= 0; --i)
	{
		FModumateObjectInstance *obj = ObjectInstanceArray[i];

		ObjectsByID.Remove(obj->ID);
		ObjectInstanceArray.RemoveAt(i, 1, false);

		delete obj;
	}

	for (auto& kvp : DeletedObjects)
	{
		if (kvp.Value)
		{
			delete kvp.Value;
		}
	}
	DeletedObjects.Reset();

	for (auto &kvp : DirtyObjectMap)
	{
		kvp.Value.Reset();
	}

	AEditModelGameMode_CPP *gameMode = world->GetAuthGameMode<AEditModelGameMode_CPP>();
	gameMode->ObjectDatabase->InitPresetManagerForNewDocument(PresetManager);
	
	// Clear drafting render directories
	UModumateGameInstance *modGameInst = world ? world->GetGameInstance<UModumateGameInstance>() : nullptr;
	UDraftingManager *draftMan = modGameInst ? modGameInst->DraftingManager : nullptr;
	if (draftMan != nullptr)
	{
		draftMan->Reset();
	}

	NextID = 1;

	CommandHistory.Reset();

	ClearRedoBuffer();
	ClearUndoBuffer();

	UndoRedoMacroStack.Reset();
	VolumeGraph.Reset();
	TempVolumeGraph.Reset();
	SurfaceGraphs.Reset();

	GatherDocumentMetadata();

	// Set default assemblies for tool modes
	// TODO: move to login/connect to project when we move docs to the cloud, but for now there's only one player state

	AEditModelPlayerState_CPP* pPlayerState = Cast<AEditModelPlayerState_CPP>(world->GetFirstPlayerController()->PlayerState);
	for (auto &kvp : PresetManager.AssemblyDBs_DEPRECATED)
	{
		if (kvp.Value.DataMap.Num() > 0)
		{
			auto asmIter = kvp.Value.DataMap.CreateConstIterator();
			pPlayerState->SetAssemblyForToolMode(kvp.Key, (*asmIter).Value.DatabaseKey);
		}
		else
		{
			static const FName errorItemKey(TEXT("None"));
			pPlayerState->SetAssemblyForToolMode(kvp.Key, errorItemKey);
		}
	}

	static const FString eventCategory(TEXT("FileIO"));
	static const FString eventNameNew(TEXT("NewDocument"));
	UModumateAnalyticsStatics::RecordEventSimple(world, eventCategory, eventNameNew);

	SetCurrentProjectPath();
}


void FModumateDocument::GatherDocumentMetadata()
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::GatherDocumentMetadata"));
	FPartyProfile pp;

	// Read architect metadata from the registry
	pp.Role = TEXT("Architect (Author):");
	FString regSubKey = L"SOFTWARE\\Modumate\\OrganizationInfo\\";
	FString regValue = L"CompanyName";
	pp.Name = Modumate::PlatformFunctions::GetStringValueFromHKCU(regSubKey, regValue);
	pp.Representative = TEXT("Andrew Stevenson");

	regValue = L"CompanyEmail";
	pp.Email = Modumate::PlatformFunctions::GetStringValueFromHKCU(regSubKey, regValue);

	regValue = L"CompanyPhone";
	pp.Phone = Modumate::PlatformFunctions::GetStringValueFromHKCU(regSubKey, regValue);

	regValue = L"CompanyLogoPath";
	FString relativeLogoPath = Modumate::PlatformFunctions::GetStringValueFromHKCU(regSubKey, regValue);
	pp.LogoPath = FPaths::ProjectContentDir() + relativeLogoPath;

	LeadArchitect = pp;

	// Clear the secondary parties array
	SecondaryParties.Empty();

	pp.Role = TEXT("Structural Engineer");
	pp.Name = TEXT("ZFA Atructural Engineers");
	pp.Representative = TEXT("Frank Lloyd Wright");
	pp.Email = TEXT("franklloydwright@gmail.com");
	pp.Phone = TEXT("(702) 445-2314");
	SecondaryParties.Add(pp);

	pp.Role = TEXT("Mechanical Engineer");
	pp.Name = TEXT("Austal HVAC Design and Fabrication");
	pp.Representative = TEXT("Norman Foster");
	pp.Email = TEXT("normanfosetr@gmail.com");
	pp.Phone = TEXT("(408) 145-4315");
	SecondaryParties.Add(pp);

	pp.Role = TEXT("Interior Designer");
	pp.Name = TEXT("Frank Gehry Interiors");
	pp.Representative = TEXT("Frank Gehry");
	pp.Email = TEXT("frankgehry@gmail.com");
	pp.Phone = TEXT("(415) 220-4312");
	SecondaryParties.Add(pp);

	pp.Role = TEXT("Client:");
	pp.Name = TEXT("Andrew and Susan Stevenson");
	pp.Representative = TEXT("Johnny McClient");
	pp.Email = TEXT("johnnymcclient@gmail.com");
	pp.Phone = TEXT("(650) 350-3675");
	Client = pp;

	FDraftRevision dr;

	// Clear the revisions (versions) array
	Revisions.Empty();

	dr.Name = TEXT("First Revision. Subject to review and modifications, especially in the early stages of the project.");
	dr.DateTime = FDateTime::Now();
	dr.Number = 1;
	Revisions.Add(dr);

	dr.Name = TEXT("Moved All Of Frank's Lines.");
	dr.DateTime = FDateTime::Now();
	dr.Number = 2;
	Revisions.Add(dr);

	dr.Name = TEXT("Put Frank's Lines Back so that they can be accounted for when executing the project.");
	dr.DateTime = FDateTime::Now();
	dr.Number = 3;
	Revisions.Add(dr);

	regValue = L"LicenseStampPath";
	FString relativeStampPath = Modumate::PlatformFunctions::GetStringValueFromHKCU(regSubKey, regValue);
	StampPath = FPaths::ProjectContentDir() + relativeStampPath;

	ProjectInfo.name = TEXT("Stevenson Residence");
	ProjectInfo.address1 = TEXT("200 Holly Ave.");
	ProjectInfo.address2 = TEXT("San Francisco, CA 94116");
	ProjectInfo.lotNumber = TEXT("15-200-05");
	ProjectInfo.ID = TEXT("16097.001");
	ProjectInfo.description = TEXT("A 4-bedroom, 3-bathroom new transitional home with a pool and freestanding poolhouse/garage. The living room has a billiard table with a 6-chair bar and a wine cabinet.");
}

const FModumateObjectInstance *FModumateDocument::ObjectFromActor(const AActor *actor) const
{
	return const_cast<FModumateDocument*>(this)->ObjectFromActor(const_cast<AActor*>(actor));
}

FModumateObjectInstance *FModumateDocument::ObjectFromSingleActor(AActor *actor)
{
	auto *moiComponent = actor ? actor->FindComponentByClass<UModumateObjectComponent_CPP>() : nullptr;
	if (moiComponent && (moiComponent->ObjectID != MOD_ID_NONE))
	{
		return GetObjectById(moiComponent->ObjectID);
	}

	return nullptr;
}

FModumateObjectInstance *FModumateDocument::ObjectFromActor(AActor *actor)
{
	FModumateObjectInstance *moi = ObjectFromSingleActor(actor);

	while ((moi == nullptr) && (actor != nullptr))
	{
		actor = actor->GetAttachParentActor();
		moi = ObjectFromSingleActor(actor);
	}

	return moi;
}

TArray<FModumateObjectInstance*> FModumateDocument::GetObjectsOfType(EObjectType type)
{
	return ObjectInstanceArray.FilterByPredicate([type](FModumateObjectInstance *moi)
	{
		return moi->GetObjectType() == type;
	});
}

void FModumateDocument::GetObjectIdsByAssembly(const FName &assemblyKey, TArray<int32> &outIds) const
{
	for (const auto &moi : ObjectInstanceArray)
	{
		if (moi->GetAssembly().DatabaseKey == assemblyKey)
		{
			outIds.Add(moi->ID);
		}
	}
}

TArray<const FModumateObjectInstance*> FModumateDocument::GetObjectsOfType(EObjectType type) const
{
//	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::GetObjectsOfType"));

	TArray< const FModumateObjectInstance *> outObjectsOfType;
	Algo::Transform(
			ObjectInstanceArray.FilterByPredicate([type](FModumateObjectInstance *moi)
			{
				return moi->GetObjectType() == type;
			}), 
		outObjectsOfType,
			[](FModumateObjectInstance *moi)
			{
				return moi;
			});
	return outObjectsOfType;
}

bool FModumateDocument::IsDirty() const
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::IsDirty"));
	return UndoBuffer.Num() > 0;
}


bool FModumateDocument::ExportPDF(UWorld *world, const TCHAR *filepath, const FVector &origin, const FVector &normal)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::ExportPDF"));

	CurrentDraftingView = MakeShareable(new FModumateDraftingView(world, this, FModumateDraftingView::kPDF));
	CurrentDraftingView->CurrentFilePath = FString(filepath);

	return true;
}

bool Modumate::FModumateDocument::ExportDWG(UWorld * world, const TCHAR * filepath)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::ExportDWG"));
	CurrentDraftingView = MakeShareable(new FModumateDraftingView(world, this, FModumateDraftingView::kDWG));
	CurrentDraftingView->CurrentFilePath = FString(filepath);

	return true;
}

bool FModumateDocument::Save(UWorld *world, const FString &path)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::Save"));

	AEditModelGameMode_CPP *gameMode = Cast<AEditModelGameMode_CPP>(world->GetAuthGameMode());


	TSharedPtr<FJsonObject> FileJson = MakeShareable(new FJsonObject());
	TSharedPtr<FJsonObject> HeaderJson = MakeShareable(new FJsonObject());

	// Header is its own
	FModumateDocumentHeader header;
	header.Version = Modumate::DocVersion;
	header.Thumbnail = CurrentEncodedThumbnail;
	FileJson->SetObjectField(DocHeaderField, FJsonObjectConverter::UStructToJsonObject<FModumateDocumentHeader>(header));

	FMOIDocumentRecord docRec;

	EToolMode modes[] = {
		EToolMode::VE_PLACEOBJECT,
		EToolMode::VE_WALL,
		EToolMode::VE_FLOOR,
		EToolMode::VE_DOOR,
		EToolMode::VE_WINDOW,
		EToolMode::VE_STAIR,
		EToolMode::VE_RAIL,
		EToolMode::VE_CABINET,
		EToolMode::VE_FINISH,
		EToolMode::VE_COUNTERTOP,
		EToolMode::VE_STRUCTURELINE,
		EToolMode::VE_TRIM,
		EToolMode::VE_ROOF_FACE
	};

	AEditModelPlayerController_CPP* emPlayerController = Cast<AEditModelPlayerController_CPP>(world->GetFirstPlayerController());
	AEditModelPlayerState_CPP* emPlayerState = emPlayerController->EMPlayerState;

	for (auto &mode : modes)
	{
		TScriptInterface<IEditModelToolInterface> tool = emPlayerController->ModeToTool.FindRef(mode);
		if (ensureAlways(tool))
		{
			docRec.CurrentToolAssemblyMap.Add(mode, tool->GetAssemblyKey());
		}
	}

	docRec.CommandHistory = CommandHistory;

	// DDL 2.0
	PresetManager.ToDocumentRecord(docRec);

	// Capture object instances into doc struct
	Algo::Transform(ObjectInstanceArray,docRec.ObjectInstances,
		[](const FModumateObjectInstance *ob)
			{
				return ob->AsDataRecord();
			}
		);

	VolumeGraph.Save(&docRec.VolumeGraph);

	// Save all of the surface graphs as records
	for (const auto &kvp : SurfaceGraphs)
	{
		const FGraph2D &surfaceGraph = kvp.Value;
		FGraph2DRecord &surfaceGraphRecord = docRec.SurfaceGraphs.Add(kvp.Key);
		surfaceGraph.ToDataRecord(surfaceGraphRecord, false, false);
	}

	docRec.CameraViews = SavedCameraViews;

	TSharedPtr<FJsonObject> docOb = FJsonObjectConverter::UStructToJsonObject<FMOIDocumentRecord>(docRec);
	FileJson->SetObjectField(DocObjectInstanceField, docOb);

	FString ProjectJsonString;
	TSharedRef<FPrettyJsonStringWriter> JsonStringWriter = FPrettyJsonStringWriterFactory::Create(&ProjectJsonString);

	bool bWriteJsonSuccess = FJsonSerializer::Serialize(FileJson.ToSharedRef(), JsonStringWriter);
	if (!bWriteJsonSuccess)
	{
		return false;
	}

	bool fileSaveSuccess = FFileHelper::SaveStringToFile(ProjectJsonString, *path);

	if (fileSaveSuccess)
	{
		SetCurrentProjectPath(path);

		if (auto *gameInstance = world->GetGameInstance<UModumateGameInstance>())
		{
			gameInstance->UserSettings.RecordRecentProject(path, true);
		}
	}

	return fileSaveSuccess;
}

FModumateObjectInstance *FModumateDocument::GetObjectById(int32 id)
{
	return ObjectsByID.FindRef(id);
}

const FModumateObjectInstance *FModumateDocument::GetObjectById(int32 id) const
{
	return ObjectsByID.FindRef(id);
}

bool FModumateDocument::Load(UWorld *world, const FString &path, bool setAsCurrentProject)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::Load"));

	//Get player state and tells it to empty selected object
	AEditModelPlayerController_CPP* EMPlayerController = Cast<AEditModelPlayerController_CPP>(world->GetFirstPlayerController());
	AEditModelPlayerState_CPP* EMPlayerState = EMPlayerController->EMPlayerState;
	EMPlayerState->OnNewModel();

	MakeNew(world);

	AEditModelGameMode_CPP *gameMode = world->GetAuthGameMode<AEditModelGameMode_CPP>();

	Modumate::ModumateObjectDatabase *objectDB = gameMode->ObjectDatabase;

	FModumateDocumentHeader docHeader;
	FMOIDocumentRecord docRec;

	if (FModumateSerializationStatics::TryReadModumateDocumentRecord(path, docHeader, docRec))
	{
		DataCollection<FModumateObjectAssembly> *ffeDB = PresetManager.AssemblyDBs_DEPRECATED.Find(EToolMode::VE_PLACEOBJECT);

		CommandHistory = docRec.CommandHistory;

		// Note: will check version against header and simply init from db if presets are out of date
		PresetManager.FromDocumentRecord(world, docHeader, docRec);

		// Load the connectivity graphs now, which contain associations between object IDs,
		// so that any objects whose geometry setup needs to know about connectivity can find it.
		VolumeGraph.Load(&docRec.VolumeGraph);
		FGraph3D::CloneFromGraph(TempVolumeGraph, VolumeGraph);

		// Load all of the surface graphs now
		for (const auto &kvp : docRec.SurfaceGraphs)
		{
			const FGraph2DRecord &surfaceGraphRecord = kvp.Value;
			FGraph2D surfaceGraph(kvp.Key);
			if (surfaceGraph.FromDataRecord(surfaceGraphRecord))
			{
				SurfaceGraphs.Add(kvp.Key, MoveTemp(surfaceGraph));
			}
		}

		SavedCameraViews = docRec.CameraViews;

		NextID = 1;
		for (auto &objectRecord : docRec.ObjectInstances)
		{
			// Before loading an object that requires a graph association, make sure it was successfully loaded from the graph.
			EGraph3DObjectType graphObjectType = UModumateTypeStatics::Graph3DObjectTypeFromObjectType(objectRecord.ObjectType);
			if (graphObjectType != EGraph3DObjectType::None)
			{
				if (!VolumeGraph.ContainsObject(FTypedGraphObjID(objectRecord.ID, graphObjectType)))
				{
					UE_LOG(LogTemp, Warning, TEXT("MOI #%d was skipped because its corresponding %s was missing from the graph!"),
						objectRecord.ID, *EnumValueString(EGraph3DObjectType, graphObjectType));
					continue;
				}
			}

			FModumateObjectInstance *obj = new FModumateObjectInstance(world, this, objectRecord);
			ObjectInstanceArray.AddUnique(obj);
			ObjectsByID.Add(obj->ID, obj);
			NextID = FMath::Max(NextID, obj->ID + 1);

			for (EObjectDirtyFlags dirtyFlag : UModumateTypeStatics::OrderedDirtyFlags)
			{
				obj->MarkDirty(dirtyFlag);
			}
		}

		for (auto obj : ObjectInstanceArray)
		{
			FModumateObjectInstance *parent = GetObjectById(obj->GetParentID());
			if (parent != 0)
			{
				obj->SetParentID(0);
				parent->AddChild(obj);
			}
		}

		// Now that all objects have been created and parented correctly, we can clean all of them.
		// This should take care of anything that depends on relationships between objects, like mitering.
		CleanObjects();

		// Check for objects that have errors, as a result of having been loaded and cleaned.
		// TODO: prompt the user to fix, or delete, these objects
		for (auto *obj : ObjectInstanceArray)
		{
			if (EMPlayerState->DoesObjectHaveAnyError(obj->ID))
			{
				FString objectTypeString = EnumValueString(EObjectType, obj->GetObjectType());
				UE_LOG(LogTemp, Warning, TEXT("MOI %d (%s) has an error!"), obj->ID, *objectTypeString);
			}
		}

		for (auto &cta : docRec.CurrentToolAssemblyMap)
		{
			TScriptInterface<IEditModelToolInterface> tool = EMPlayerController->ModeToTool.FindRef(cta.Key);
			if (ensureAlways(tool))
			{
				const FModumateObjectAssembly *obAsm = PresetManager.GetAssemblyByKey(cta.Key, cta.Value);
				if (obAsm != nullptr)
				{
					tool->SetAssemblyKey(obAsm->DatabaseKey);
				}
			}
		}

		// Just in case the loaded document's rooms don't match up with our current room analysis logic, do that now.
		UpdateRoomAnalysis(world);

		ResequencePortalAssemblies_DEPRECATED(world, EObjectType::OTWindow);
		ResequencePortalAssemblies_DEPRECATED(world, EObjectType::OTDoor);

		ClearUndoBuffer();

		if (setAsCurrentProject)
		{
			SetCurrentProjectPath(path);

			if (auto *gameInstance = world->GetGameInstance<UModumateGameInstance>())
			{
				gameInstance->UserSettings.RecordRecentProject(path, true);

				static const FString eventCategory(TEXT("FileIO"));
				static const FString eventName(TEXT("LoadDocument"));
				UModumateAnalyticsStatics::RecordEventSimple(world, eventCategory, eventName);
			}
		}
		return true;
	}
	return false;
}

int32 FModumateDocument::CreateObjectFromRecord(UWorld *world, const FMOIDataRecord &obRec)
{
	ClearRedoBuffer();
	int32 id = NextID++;

	UndoRedo *ur = new UndoRedo();
	ur->Redo = [this, ur, id, world, obRec]()
	{
		const FModumateObjectAssembly *obAsm = nullptr;
		if (obRec.AssemblyKey != "None")
		{
			obAsm = PresetManager.GetAssemblyByKey(UModumateTypeStatics::ToolModeFromObjectType(obRec.ObjectType), FName(*obRec.AssemblyKey));
			ensureAlways(obAsm != nullptr);
		}
		FModumateObjectInstance *obj = obAsm != nullptr ?
			CreateOrRestoreObjFromAssembly(world, *obAsm, id, obRec.ParentID, obRec.Extents, &obRec.ControlPoints, &obRec.ControlIndices) :
			CreateOrRestoreObjFromObjectType(world, obRec.ObjectType, id, obRec.ParentID, obRec.Extents, &obRec.ControlPoints, &obRec.ControlIndices);

		obj->SetupGeometry();

		ur->Undo = [id, this, obj]()
		{
			FModumateObjectInstance *ob = GetObjectById(id);
			DeleteObjectImpl(ob);
		};
	};
	UndoBuffer.Add(ur);
	ur->Redo();
	return id;
}

TArray<int32> FModumateDocument::CloneObjects(UWorld *world, const TArray<int32> &objs, const FTransform& offsetTransform)
{
	// make a list of MOI objects from the IDs, send them to the MOI version of this function return array of IDs.

	TArray<FModumateObjectInstance*> obs;
	Algo::Transform(objs,obs,[this](auto id) {return GetObjectById(id); });

	obs = CloneObjects(
		world,
		obs,
		offsetTransform)
		.FilterByPredicate([](FModumateObjectInstance *ob) {return ob != nullptr; });

	TArray<int32> ids;

	Algo::Transform(obs,ids,[this](auto *ob){return ob->ID;});

	return ids;
}

int32 FModumateDocument::CloneObject(UWorld *world, const FModumateObjectInstance *original)
{
	UndoRedo *ur = new UndoRedo();
	ClearRedoBuffer();
	int32 id = NextID++;

	ur->Redo = [id, ur, world,this,original]()
	{
		FModumateObjectInstance* obj = TryGetDeletedObject(id);
		if (obj != nullptr)
		{
			RestoreObjectImpl(obj);
		}
		else
		{
			obj = new FModumateObjectInstance(world, this, original->AsDataRecord());
			obj->SetupGeometry();
			obj->ID = id;
			obj->SetParentObject(GetObjectById(original->GetParentID()));
			ObjectInstanceArray.AddUnique(obj);
			ObjectsByID.Add(obj->ID, obj);
		}

		ur->Undo = [obj, id, this]()
		{
			FModumateObjectInstance *ob = GetObjectById(id);
			DeleteObjectImpl(ob);
		};
	};

	UndoBuffer.Add(ur);

	ur->Redo();
	return id;
}

TArray<FModumateObjectInstance *> FModumateDocument::CloneObjects(UWorld *world, const TArray<FModumateObjectInstance *> &objs, const FTransform& offsetTransform)
{
	TMap<FModumateObjectInstance*, FModumateObjectInstance*> oldToNew;

	BeginUndoRedoMacro();

	// Step 1 of clone macro: deserialize the objects, each of which will have its own undo/redo step
	TArray<FModumateObjectInstance*> newObjs;
	Algo::Transform(
		objs,
		newObjs,
		[this,world,&oldToNew,offsetTransform](FModumateObjectInstance *obj)
		{
			FModumateObjectInstance *newOb = GetObjectById(CloneObject(world, obj));
			for (int32 i=0;i<newOb->GetControlPoints().Num();++i)
			{
				FVector p = offsetTransform.TransformPosition(newOb->GetControlPoint(i));
				newOb->SetControlPoint(i, p);
			}
			oldToNew.Add(obj, newOb);
			return newOb;
		}
	);

	// Set up the correct parenting for the newly-cloned objects
	for (auto newObj : newObjs)
	{
		FModumateObjectInstance *oldParent = newObj->GetParentObject();
		FModumateObjectInstance *newParent = oldToNew.FindRef(oldParent);

		if (oldParent && newParent)
		{
			newObj->SetParentObject(newParent);
		}

		newObj->MarkDirty(EObjectDirtyFlags::Structure);
	}

	EndUndoRedoMacro();

	return newObjs;
}

void FModumateDocument::SetCurrentProjectPath(const FString& currentProjectPath)
{
	CurrentProjectPath = currentProjectPath;
	CurrentProjectName = currentProjectPath;

	if (!currentProjectPath.IsEmpty())
	{
		FString projectDir, projectExt;
		FPaths::Split(CurrentProjectPath, projectDir, CurrentProjectName, projectExt);
	}

	UModumateFunctionLibrary::SetWindowTitle(CurrentProjectName);
}

void FModumateDocument::UpdateMitering(UWorld *world, const TArray<int32> &dirtyObjIDs)
{
	TSet<int32> dirtyPlaneIDs;

	// Gather all of the dirty planes that need to be updated with new miter geometry
	for (int32 dirtyObjID : dirtyObjIDs)
	{
		FModumateObjectInstance *dirtyObj = GetObjectById(dirtyObjID);
		if (dirtyObj)
		{
			switch (dirtyObj->GetObjectType())
			{
			case EObjectType::OTMetaEdge:
			{
				const FGraph3DEdge *graphEdge = VolumeGraph.FindEdge(dirtyObjID);
				for (const FEdgeFaceConnection &edgeFaceConnection : graphEdge->ConnectedFaces)
				{
					dirtyPlaneIDs.Add(FMath::Abs(edgeFaceConnection.FaceID));
				}
				break;
			}
			case EObjectType::OTMetaPlane:
			{
				dirtyPlaneIDs.Add(dirtyObjID);

				const FGraph3DFace *graphFace = VolumeGraph.FindFace(dirtyObjID);
				graphFace->GetAdjacentFaceIDs(dirtyPlaneIDs);
				break;
			}
			case EObjectType::OTMetaVertex:
			{
				const FGraph3DVertex *graphVertex = VolumeGraph.FindVertex(dirtyObjID);
				graphVertex->GetConnectedFaces(dirtyPlaneIDs);
				break;
			}
			default:
			{
				FModumateObjectInstance *parent = dirtyObj->GetParentObject();
				if (parent && (parent->GetObjectType() == EObjectType::OTMetaPlane))
				{
					dirtyPlaneIDs.Add(parent->ID);
				}
				break;
			}
			}
		}
	}

	// Miter-aware geometry updates only require UpdateGeometry, which in turn will analyze neighboring objects in the graph.
	for (int32 dirtyPlaneID : dirtyPlaneIDs)
	{
		FModumateObjectInstance *dirtyPlane = GetObjectById(dirtyPlaneID);
		FGraph3DFace *dirtyFace = VolumeGraph.FindFace(dirtyPlaneID);
		if (dirtyPlane && dirtyFace)
		{
			const TArray<int32> &childIDs = dirtyPlane->GetChildIDs();
			if (childIDs.Num() == 1)
			{
				if (FModumateObjectInstance *dirtyPlaneHostedObj = GetObjectById(childIDs[0]))
				{
					dirtyPlaneHostedObj->MarkDirty(EObjectDirtyFlags::Structure);
				}
			}
		}
	}
}

bool FModumateDocument::CanRoomContainFace(FSignedID FaceID)
{
	const FGraph3DFace *graphFace = VolumeGraph.FindFace(FaceID);
	const FModumateObjectInstance *planeObj = GetObjectById(FMath::Abs(FaceID));

	if ((graphFace == nullptr) || (planeObj == nullptr))
	{
		return false;
	}

	// Only allow traversing to planes facing upwards (even slightly)
	FVector signedFaceNormal = FMath::Sign(FaceID) * FVector(graphFace->CachedPlane);
	float normalUpDot = (signedFaceNormal | FVector::UpVector);
	if (FMath::IsNearlyZero(normalUpDot) || (normalUpDot < 0.0f))
	{
		return false;
	}

	// Only allow traversing to planes that have a Floor child
	const TArray<int32> &planeChildIDs = planeObj->GetChildIDs();
	for (int32 planeChildID : planeChildIDs)
	{
		const FModumateObjectInstance *planeChildObj = GetObjectById(planeChildID);
		if (planeChildObj && (planeChildObj->GetObjectType() == EObjectType::OTFloorSegment))
		{
			return true;
		}
	}

	return false;

}

void FModumateDocument::UpdateRoomAnalysis(UWorld *world)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::UpdateRoomAnalysis"));

	bool bStartedMacro = false;
	bool bAnyChange = false;
	TMap<int32, int32> oldRoomIDsToNewRoomIndices;
	TMap<int32, TArray<int32>> newRoomsFaceIDs;
	TSet<int32> oldRoomsToDeleteIDs, newRoomsToCreateIndices;

	UModumateRoomStatics::CalculateRoomChanges(this, bAnyChange,
		oldRoomIDsToNewRoomIndices, newRoomsFaceIDs, oldRoomsToDeleteIDs, newRoomsToCreateIndices);

	if (bAnyChange)
	{
		// We actually have to make a change to rooms - start the undo/redo operations
		BeginUndoRedoMacro();
		ClearRedoBuffer();
		bStartedMacro = true;

		// Step 1 - modify old rooms
		UndoRedo *urModifyOldRooms = new UndoRedo();

		urModifyOldRooms->Redo = [this, world, urModifyOldRooms, oldRoomIDsToNewRoomIndices, newRoomsFaceIDs]()
		{
			TMap<int32, TArray<int32>> oldRoomsFaceIDs;

			// Use our room mapping to make the changes from old rooms to new rooms
			for (auto &oldToNew : oldRoomIDsToNewRoomIndices)
			{
				int32 oldRoomObjID = oldToNew.Key;
				int32 newRoomIndex = oldToNew.Value;

				FModumateObjectInstance *oldRoomObj = GetObjectById(oldRoomObjID);
				const TArray<int32> *newRoomFaceIDs = newRoomsFaceIDs.Find(newRoomIndex);
				if (oldRoomObj && newRoomFaceIDs)
				{
					// Keep track of the room's previous face IDs
					oldRoomsFaceIDs.Add(oldRoomObjID, oldRoomObj->GetControlPointIndices());

					oldRoomObj->SetControlPointIndices(*newRoomFaceIDs);
					oldRoomObj->MarkDirty(EObjectDirtyFlags::Structure);
				}
			}

			urModifyOldRooms->Undo = [this, world, urModifyOldRooms, oldRoomIDsToNewRoomIndices, oldRoomsFaceIDs]()
			{
				// Use our room mapping to restore the old rooms
				for (auto &oldRoomKVP : oldRoomsFaceIDs)
				{
					FModumateObjectInstance *oldRoomObj = GetObjectById(oldRoomKVP.Key);
					if (oldRoomObj)
					{
						oldRoomObj->SetControlPointIndices(oldRoomKVP.Value);
						oldRoomObj->MarkDirty(EObjectDirtyFlags::Structure);
					}
				}
			};
		};

		UndoBuffer.Add(urModifyOldRooms);
		urModifyOldRooms->Redo();

		// Step 2 - create new rooms that don't have a mapping from an old room
		for (int32 newRoomIndex : newRoomsToCreateIndices)
		{
			TArray<int32> *newRoomFaceIDs = newRoomsFaceIDs.Find(newRoomIndex);
			if (newRoomFaceIDs)
			{
				MakeRoom(world, *newRoomFaceIDs);
			}
		}

		// Step 3 - delete old rooms that don't have a mapping to a new room
		TArray<int32> oldRoomsToDeleteIDsArray = oldRoomsToDeleteIDs.Array();
		DeleteObjects(oldRoomsToDeleteIDsArray, false, false);
	}

	// Step 1/4 - re-sequence rooms to have the correct room numbers
	TMap<int32, FString> oldRoomNumbers, newRoomNumbers;
	UModumateRoomStatics::CalculateRoomNumbers(this, oldRoomNumbers, newRoomNumbers);
	if (newRoomNumbers.Num() > 0)
	{
		// Start the macro if we haven't already, since there might not have been any room content changes,
		// but there may have been room number changes. Room numbering changes can occur independently,
		// so these undo/redoable actions are separated.
		if (!bStartedMacro)
		{
			BeginUndoRedoMacro();
			ClearRedoBuffer();
			bStartedMacro = true;
		}

		UndoRedo *urSequenceRooms = new UndoRedo();

		urSequenceRooms->Redo = [this, urSequenceRooms, oldRoomNumbers, newRoomNumbers]()
		{
			for (auto &kvp : newRoomNumbers)
			{
				FModumateObjectInstance *roomObj = GetObjectById(kvp.Key);
				const FString &newRoomNumber = kvp.Value;
				roomObj->SetProperty(BIM::EScope::Room, BIM::Parameters::Number, newRoomNumber);
			}

			urSequenceRooms->Undo = [this, oldRoomNumbers]()
			{
				for (auto &kvp : oldRoomNumbers)
				{
					FModumateObjectInstance *roomObj = GetObjectById(kvp.Key);
					const FString &oldRoomNumber = kvp.Value;
					roomObj->SetProperty(BIM::EScope::Room, BIM::Parameters::Number, oldRoomNumber);
				}
			};
		};

		UndoBuffer.Add(urSequenceRooms);
		urSequenceRooms->Redo();
	}

	if (bStartedMacro)
	{
		EndUndoRedoMacro();
	}
}

const FGraph2D *FModumateDocument::FindSurfaceGraph(int32 SurfaceGraphID) const
{
	return SurfaceGraphs.Find(SurfaceGraphID);
}

FGraph2D *FModumateDocument::FindSurfaceGraph(int32 SurfaceGraphID)
{
	return SurfaceGraphs.Find(SurfaceGraphID);
}

bool FModumateDocument::IsObjectInVolumeGraph(int32 ObjID, EGraph3DObjectType &OutObjType) const
{
	bool bIsInGraph = false;

	const FModumateObjectInstance *moi = GetObjectById(ObjID);
	if (moi == nullptr)
	{
		return bIsInGraph;
	}

	OutObjType = UModumateTypeStatics::Graph3DObjectTypeFromObjectType(moi->GetObjectType());
	bool bIsVolumeGraphType = (OutObjType != EGraph3DObjectType::None);
	bIsInGraph = VolumeGraph.ContainsObject(FTypedGraphObjID(ObjID, OutObjType));
	ensureAlways(bIsVolumeGraphType == bIsInGraph);

	return bIsInGraph;
}

void FModumateDocument::DisplayDebugInfo(UWorld* world)
{
	auto displayMsg = [](const FString &msg)
	{
		GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, msg, false);
	};


	auto displayObjectCount = [this,displayMsg](EObjectType ot, const TCHAR *name)
	{
		TArray<FModumateObjectInstance*> obs = GetObjectsOfType(ot);
		if (obs.Num() > 0)
		{
			displayMsg(FString::Printf(TEXT("%s - %d"), name, obs.Num()));
		}
	};

	AEditModelPlayerState_CPP* emPlayerState = Cast<AEditModelPlayerState_CPP>(world->GetFirstPlayerController()->PlayerState);

	displayMsg(TEXT("OBJECT COUNTS"));
	displayObjectCount(EObjectType::OTWallSegment, TEXT("OTWallSegment"));
	displayObjectCount(EObjectType::OTRailSegment, TEXT("OTRailSegment"));
	displayObjectCount(EObjectType::OTFloorSegment, TEXT("OTFloorSegment"));
	displayObjectCount(EObjectType::OTRoofFace, TEXT("OTRoof"));
	displayObjectCount(EObjectType::OTDoor, TEXT("OTDoor"));
	displayObjectCount(EObjectType::OTWindow, TEXT("OTWindow"));
	displayObjectCount(EObjectType::OTFurniture, TEXT("OTFurniture"));
	displayObjectCount(EObjectType::OTCabinet, TEXT("OTCabinet"));
	displayObjectCount(EObjectType::OTStaircase, TEXT("OTStaircase"));
	displayObjectCount(EObjectType::OTFinish, TEXT("OTFinish"));
	displayObjectCount(EObjectType::OTGroup, TEXT("OTGroup"));
	displayObjectCount(EObjectType::OTRoom, TEXT("OTRoom"));

	TSet<FString> asms;

	Algo::Transform(ObjectInstanceArray, asms,
		[](const FModumateObjectInstance *ob)
		{
			return ob->GetAssembly().UniqueKey().ToString();
		}
	);

	for (auto &a : asms)
	{
		int32 instanceCount = Algo::Accumulate(
			ObjectInstanceArray, 0,
			[a](int32 total, const FModumateObjectInstance *ob)
			{
				return ob->GetAssembly().UniqueKey().ToString() == a ? total + 1 : total;
			}
		);

		displayMsg(FString::Printf(TEXT("Assembly %s: %d"), *a, instanceCount));
	}

	displayMsg(FString::Printf(TEXT("Undo Buffer: %d"), UndoBuffer.Num()));
	displayMsg(FString::Printf(TEXT("Redo Buffer: %d"), RedoBuffer.Num()));
	displayMsg(FString::Printf(TEXT("Deleted Obs: %d"), DeletedObjects.Num()));
	displayMsg(FString::Printf(TEXT("Active Obs: %d"), ObjectInstanceArray.Num()));

	FString selected = FString::Printf(TEXT("Selected Obs: %d"), emPlayerState->SelectedObjects.Num());
	if (emPlayerState->SelectedObjects.Num() > 0)
	{
		selected += TEXT(" | ");
		for (auto &sel : emPlayerState->SelectedObjects)
		{
			selected += FString::Printf(TEXT("SEL: %d #CHILD: %d PARENTID: %d"), sel->ID, sel->GetChildren().Num(),sel->GetParentID());
		}
	}
	displayMsg(selected);
}

void FModumateDocument::DrawDebugVolumeGraph(UWorld* world)
{
	const float drawVerticalOffset = 5.0f;
	const float pointThickness = 8.0f;
	const float lineThickness = 2.0f;
	const float arrowSize = 10.0f;
	const float faceEdgeOffset = 15.0f;
	const FVector textOffset = 20.0f * FVector::UpVector;

	for (const auto &kvp : VolumeGraph.GetVertices())
	{
		const FGraph3DVertex &graphVertex = kvp.Value;
		FVector vertexDrawPos = graphVertex.Position;

		world->LineBatcher->DrawPoint(vertexDrawPos, FLinearColor::Red, pointThickness, 0);
		FString vertexString = FString::Printf(TEXT("Vertex #%d: [%s]%s"), graphVertex.ID,
			*FString::JoinBy(graphVertex.ConnectedEdgeIDs, TEXT(", "), [](const FSignedID &edgeID) { return FString::Printf(TEXT("%d"), edgeID); }),
			(graphVertex.GroupIDs.Num() == 0) ? TEXT("") : *FString::Printf(TEXT(" {%s}"), *FString::JoinBy(graphVertex.GroupIDs, TEXT(", "), [](const int32 &GroupID) { return FString::Printf(TEXT("%d"), GroupID); }))
		);

		DrawDebugString(world, vertexDrawPos + textOffset, vertexString, nullptr, FColor::White, 0.0f, true);
	}

	for (const auto &kvp : VolumeGraph.GetEdges())
	{
		const FGraph3DEdge &graphEdge = kvp.Value;
		const FGraph3DVertex *startGraphVertex = VolumeGraph.FindVertex(graphEdge.StartVertexID);
		const FGraph3DVertex *endGraphVertex = VolumeGraph.FindVertex(graphEdge.EndVertexID);
		if (startGraphVertex && endGraphVertex)
		{
			FVector startDrawPos = startGraphVertex->Position;
			FVector endDrawPos = endGraphVertex->Position;

			DrawDebugDirectionalArrow(world, startDrawPos, endDrawPos, arrowSize, FColor::Blue, false, -1.f, 0xFF, lineThickness);
			FString edgeString = FString::Printf(TEXT("Edge #%d: [%d, %d]%s"), graphEdge.ID, graphEdge.StartVertexID, graphEdge.EndVertexID,
				(graphEdge.GroupIDs.Num() == 0) ? TEXT("") : *FString::Printf(TEXT(" {%s}"), *FString::JoinBy(graphEdge.GroupIDs, TEXT(", "), [](const int32 &GroupID) { return FString::Printf(TEXT("%d"), GroupID); })));
			DrawDebugString(world, 0.5f * (startDrawPos + endDrawPos) + textOffset, edgeString, nullptr, FColor::White, 0.0f, true);
		}
	}

	for (const auto &kvp : VolumeGraph.GetFaces())
	{
		const FGraph3DFace &face = kvp.Value;

		int32 numEdges = face.EdgeIDs.Num();
		for (int32 edgeIdx = 0; edgeIdx < numEdges; ++edgeIdx)
		{
			FSignedID edgeID = face.EdgeIDs[edgeIdx];
			const FVector &edgeNormal = face.CachedEdgeNormals[edgeIdx];
			const FVector &prevEdgeNormal = face.CachedEdgeNormals[(edgeIdx + numEdges - 1) % numEdges];
			const FVector &nextEdgeNormal = face.CachedEdgeNormals[(edgeIdx + 1) % numEdges];

			const FGraph3DEdge *graphEdge = VolumeGraph.FindEdge(edgeID);
			const FGraph3DVertex *startGraphVertex = VolumeGraph.FindVertex(graphEdge->StartVertexID);
			const FGraph3DVertex *endGraphVertex = VolumeGraph.FindVertex(graphEdge->EndVertexID);

			bool bEdgeForward = (edgeID > 0);
			FVector startDrawPos = bEdgeForward ? startGraphVertex->Position : endGraphVertex->Position;
			FVector endDrawPos = bEdgeForward ? endGraphVertex->Position : startGraphVertex->Position;
			startDrawPos += faceEdgeOffset * (edgeNormal + prevEdgeNormal).GetSafeNormal();
			endDrawPos += faceEdgeOffset * (edgeNormal + nextEdgeNormal).GetSafeNormal();

			FVector edgeNStartPos = (startDrawPos + endDrawPos) / 2.0f;
			FVector edgeNEndPos = edgeNStartPos + edgeNormal * faceEdgeOffset;

			DrawDebugDirectionalArrow(world, startDrawPos, endDrawPos, arrowSize, FColor::Green, false, -1.f, 0, lineThickness);
			DrawDebugDirectionalArrow(world, edgeNStartPos, edgeNEndPos, arrowSize, FColor::Blue, false, -1.f, 0, lineThickness);
		}

		FString faceString = FString::Printf(TEXT("Face #%d: [%s]%s"), face.ID,
			*FString::JoinBy(face.EdgeIDs, TEXT(", "), [](const FSignedID &edgeID) { return FString::Printf(TEXT("%d"), edgeID); }),
			(face.GroupIDs.Num() == 0) ? TEXT("") : *FString::Printf(TEXT(" {%s}"), *FString::JoinBy(face.GroupIDs, TEXT(", "), [](const int32 &GroupID) { return FString::Printf(TEXT("%d"), GroupID); }))
		);
		DrawDebugString(world, face.CachedCenter, faceString, nullptr, FColor::White, 0.0f, true);

		FVector planeNormal = face.CachedPlane;
		DrawDebugDirectionalArrow(world, face.CachedCenter, face.CachedCenter + 2.0f * faceEdgeOffset * planeNormal, arrowSize, FColor::Green, false, -1.f, 0xFF, lineThickness);
	}

	for (const auto &kvp : VolumeGraph.GetPolyhedra())
	{
		const FGraph3DPolyhedron &polyhedron = kvp.Value;
		FString polyhedronFacesString = FString::JoinBy(polyhedron.FaceIDs, TEXT(", "), [](const FSignedID &faceID) { return FString::Printf(TEXT("%d"), faceID); });
		FString polyhedronTypeString = !polyhedron.bClosed ? FString(TEXT("open")) :
			FString::Printf(TEXT("closed, %s, %s"),
				polyhedron.bInterior ? TEXT("interior") : TEXT("exterior"),
				polyhedron.bConvex ? TEXT("convex") : TEXT("concave")
			);
		FString polyhedronString = FString::Printf(TEXT("Polyhedron: #%d: [%s], %s"),
			polyhedron.ID, *polyhedronFacesString, *polyhedronTypeString
		);
		GEngine->AddOnScreenDebugMessage(polyhedron.ID, 0.0f, FColor::White, polyhedronString, false);
	}
}


/*
TODO: to be deprecated in favor of clients querying the PresetManager for assemblies by object type instead of tool mode with DDL 2.0
*/
TArray<FModumateObjectAssembly> FModumateDocument::GetAssembliesForToolMode_DEPRECATED(UWorld *world, EToolMode mode) const
{
	EObjectType objectType = UModumateTypeStatics::ObjectTypeFromToolMode(mode);
	TArray<FModumateObjectAssembly> ret;

	if (UModumateObjectAssemblyStatics::ObjectTypeSupportsDDL2(objectType))
	{
		PresetManager.GetProjectAssembliesForObjectType(objectType, ret);
		return ret;
	}
	else
	{
		const DataCollection<FModumateObjectAssembly> *db = PresetManager.AssemblyDBs_DEPRECATED.Find(mode);
		if (db != nullptr)
		{
			db->DataMap.GenerateValueArray(ret);
		}
		return ret;
	}
}

bool FModumateDocument::RemoveAssembly(UWorld *world, EToolMode toolMode, const FName &assemblyKey, const FName &replacementKey)
{
	DataCollection<FModumateObjectAssembly> *db = PresetManager.AssemblyDBs_DEPRECATED.Find(toolMode);
	const FModumateObjectAssembly *pOriginal = PresetManager.GetAssemblyByKey(toolMode, assemblyKey);

	if (ensureAlways(db != nullptr && pOriginal != nullptr))
	{
		FModumateObjectAssembly originalAssembly = *pOriginal;

		TArray<int32> obIds;
		GetObjectIdsByAssembly(assemblyKey, obIds);

		UndoRedo *ur = new UndoRedo();

		if (replacementKey.IsNone())
		{
			ensureAlways(obIds.Num() == 0);
			ur->Redo = [ur, db, originalAssembly]()
			{
				db->RemoveData(originalAssembly);

				ur->Undo = [originalAssembly, db]()
				{
					db->AddData(originalAssembly);
				};
			};
			ur->Redo();
			UndoBuffer.Add(ur);
			return true;
		}
		else
		{
			const FModumateObjectAssembly *pReplacement = PresetManager.GetAssemblyByKey(toolMode,replacementKey);
			TArray<FModumateObjectInstance*> affectedObs;
			Algo::Transform(obIds, affectedObs, [this](int32 id) {return GetObjectById(id); });
			if (ensureAlways(pReplacement != nullptr))
			{
				FModumateObjectAssembly replacementAssembly = *pReplacement;
				ur->Redo = [this, world, ur, affectedObs, db, replacementAssembly, originalAssembly]()
				{
					for (auto &ob : affectedObs)
					{
						ob->SetAssembly(replacementAssembly);
						ob->OnAssemblyChanged();
					}
					db->RemoveData(originalAssembly);

					ur->Undo = [this, world, affectedObs, originalAssembly, db]()
					{
						db->AddData(originalAssembly);
						for (auto &ob : affectedObs)
						{
							ob->SetAssembly(originalAssembly);
							ob->OnAssemblyChanged();
						}
					};
				};
				ur->Redo();
				UndoBuffer.Add(ur);
				return true;
			}
		}
	}
	return false;
}

void FModumateDocument::OnAssemblyUpdated(
	UWorld *world,
	EToolMode mode,
	const FModumateObjectAssembly &assembly)
{
	TArray<FModumateObjectInstance*> mois = GetObjectsOfType(assembly.ObjectType);
	for (auto &moi : mois)
	{
		if (moi->GetAssembly().DatabaseKey == assembly.DatabaseKey)
		{
			moi->SetAssembly(assembly);
			moi->OnAssemblyChanged();
		}
	}

	AEditModelPlayerState_CPP* playerState = Cast<AEditModelPlayerState_CPP>(world->GetFirstPlayerController()->PlayerState);
	playerState->RefreshActiveAssembly();
}

/*
Assembly Mutators
*/
void FModumateDocument::ImportPresetsIfMissing_DEPRECATED(UWorld *world, const TArray<FName> &presets)
{
}

FModumateObjectAssembly FModumateDocument::OverwriteAssembly_DEPRECATED(
	UWorld *world,
	EToolMode mode,
	const FModumateObjectAssembly &assembly)
{
	FModumateObjectAssembly newAsm = assembly;

	if (newAsm.DatabaseKey == TEXT("temp"))
	{
		AEditModelPlayerState_CPP* playerState = Cast<AEditModelPlayerState_CPP>(world->GetFirstPlayerController()->PlayerState);
		playerState->RefreshActiveAssembly();

		ensureAlways(false);
		playerState->SetTemporaryAssembly(mode, newAsm);
		return newAsm;
	}

	ClearRedoBuffer();
	UndoRedo *ur = new UndoRedo();
	FModumateObjectAssembly oldAsm;

	DataCollection<FModumateObjectAssembly>  *db = PresetManager.AssemblyDBs_DEPRECATED.Find(mode);
	ensureAlways(db != nullptr);

	const FModumateObjectAssembly *pOldAsm = db->GetData(newAsm.DatabaseKey);
	ensureAlways(pOldAsm != nullptr);
	oldAsm = (pOldAsm != nullptr) ? *pOldAsm : newAsm;

	ur->Redo = [this, ur, db, mode, world, newAsm, oldAsm]()
	{
		db->RemoveData(oldAsm);
		db->AddData(newAsm);

		OnAssemblyUpdated(world, mode, newAsm);

		ur->Undo = [this, mode, ur, db, world, newAsm, oldAsm]()
		{
			db->RemoveData(newAsm);
			db->AddData(oldAsm);
			OnAssemblyUpdated(world, mode, oldAsm);
		};
	};

	UndoBuffer.Add(ur);
	ur->Redo();
	return newAsm;
}

FModumateObjectAssembly FModumateDocument::CreateNewAssembly_DEPRECATED(
	UWorld *world,
	EToolMode mode,
	const FModumateObjectAssembly &assembly)
{
	ClearRedoBuffer();
	UndoRedo *ur = new UndoRedo();

	DataCollection<FModumateObjectAssembly> *db = PresetManager.AssemblyDBs_DEPRECATED.Find(mode);
	ensureAlways(db != nullptr);

	FModumateObjectAssembly newAsm = assembly;

	if (newAsm.DatabaseKey.IsNone())
	{
		// All DDL 2.0 assemblies should come in with their own database key supplied by the preset manager
		ensureAlways(!UModumateObjectAssemblyStatics::ObjectTypeSupportsDDL2(assembly.ObjectType));
		newAsm.DatabaseKey = PresetManager.GetAvailableKey(*EnumValueString(EObjectType, assembly.ObjectType));
	}

	ur->Redo = [this, ur, db, mode, world, newAsm]()
	{
		db->AddData(newAsm);

		ur->Undo = [this, mode, ur, db, world, newAsm]()
		{
			db->RemoveData(newAsm);
		};
	};

	UndoBuffer.Add(ur);
	ur->Redo();
	return newAsm;
}

FModumateObjectAssembly FModumateDocument::CreateOrOverwriteAssembly_DEPRECATED(
	UWorld *world,
	EToolMode mode,
	const FModumateObjectAssembly &assembly)
{
	DataCollection<FModumateObjectAssembly>  *db = PresetManager.AssemblyDBs_DEPRECATED.Find(mode);
	ensureAlways(db != nullptr);
	if (db->GetData(assembly.DatabaseKey) != nullptr)
	{
		return OverwriteAssembly_DEPRECATED(world, mode, assembly);
	}
	return CreateNewAssembly_DEPRECATED(world, mode, assembly);
}

}
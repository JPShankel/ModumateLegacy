// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include <functional>
#include "UnrealClasses/DynamicMeshActor.h"
#include "DocumentManagement/ModumateSerialization.h"
#include "Database/ModumateObjectDatabase.h"
#include "BIMKernel/BIMLegacyPortals.h"
#include "Graph/Graph2D.h"
#include "DocumentManagement/ModumatePresetManager.h"
#include "Graph/Graph3D.h"
#include "DocumentManagement/ModumateObjectInstance.h"
#include "Runtime/Engine/Classes/Debug/ReporterGraph.h"
#include "DocumentManagement/ModumateCameraView.h"

class AEditModelPlayerState_CPP;

/**
 *
 */


namespace Modumate
{
	class FModumateObjectInstance;
	class FModumateDraftingView;
	class FDelta;
}

class MODUMATE_API FModumateDocument
{
private:

	struct UndoRedo
	{
		TArray<TSharedPtr<Modumate::FDelta>> Deltas;
	};

	TArray<UndoRedo*> UndoBuffer, RedoBuffer;
	int32 NextID;

	TArray<Modumate::FModumateObjectInstance*> ObjectInstanceArray;
	TMap<int32, Modumate::FModumateObjectInstance*> ObjectsByID;
	TMap<int32, Modumate::FModumateObjectInstance*> DeletedObjects;

	void GatherDocumentMetadata();
	float DefaultWallHeight;
	float DefaultRailHeight = 100.f;
	float DefaultCabinetHeight = 87.63f;
	float DefaultDoorHeight = 203.2f;
	float DefaultDoorWidth = 91.44f;
	float DefaultWindowHeight = 100.f;
	float DefaultWindowWidth = 50.f;
	float DefaultJustificationZ = 0.5f;
	float DefaultJustificationXY = 0.f;

	TArray<int32> UndoRedoMacroStack;
	TArray<FString> CommandHistory;

	// The volume connectivity information, for the purpose of keeping track of connectivity of all planar objects;
	// the source of truth for mitering, room detection, volume calculation, slicing floorplans/sections/elevations, heat/acoustics, etc.
	Modumate::FGraph3D VolumeGraph;

	// Copy of the volume graph to work with multi-stage deltas
	Modumate::FGraph3D TempVolumeGraph;

	// The surface graphs used by the current document, mapped by owning object ID
	TMap<int32, Modumate::FGraph2D> SurfaceGraphs;

	TMap<EObjectDirtyFlags, TArray<Modumate::FModumateObjectInstance*>> DirtyObjectMap;

public:

	FModumateDocument();
	~FModumateDocument();

	// Default Value Library
	float ElevationDelta;

	// Data
	Modumate::FModumateObjectInstance *GetObjectById(int32 id);
	const Modumate::FModumateObjectInstance *GetObjectById(int32 id) const;
	TSet<int32> HiddenObjectsID;
	TArray<FModumateCameraView> SavedCameraViews;

	// TODO: resolve how best to get IDs for created objects into MOI deltas
	// For now, just ask what the next ID will be
	int32 GetNextAvailableID() const { return NextID; }

	// Document modifiers
	int32 CreateFFE(UWorld *world, int32 parentID, const FVector &loc, const FQuat &rot, const FModumateObjectAssembly &obAsm, int32 parentFaceIdx = INDEX_NONE);

	const TArray<Modumate::FModumateObjectInstance*>& GetObjectInstances() const { return ObjectInstanceArray; }
	TArray<Modumate::FModumateObjectInstance*>& GetObjectInstances() { return ObjectInstanceArray; }

	bool CleanObjects(TArray<TSharedPtr<Modumate::FDelta>>* OutSideEffectDeltas = nullptr);
	void RegisterDirtyObject(EObjectDirtyFlags DirtyType, Modumate::FModumateObjectInstance *DirtyObj, bool bDirty);

	void AddCommandToHistory(const FString &cmd);
	TArray<FString> GetCommandHistory() const;

	void BeginUndoRedoMacro();
	void EndUndoRedoMacro();
	bool InUndoRedoMacro() const;

	void SetAssemblyForObjects(UWorld *world, TArray<int32> ids, const FModumateObjectAssembly &assembly);

	void SetDefaultWallHeight(float height);
	float GetDefaultWallHeight() const { return DefaultWallHeight; } // return DWH from private;
	void SetDefaultRailHeight(float height);
	float GetDefaultRailHeight() const { return DefaultRailHeight; } // return DRH from private;
	void SetDefaultCabinetHeight(float height);
	float GetDefaultCabinetHeight() const { return DefaultCabinetHeight; } // return DCH from private;
	void SetDefaultDoorHeightWidth(float height, float width);
	float GetDefaultDoorHeight() const { return DefaultDoorHeight; }
	float GetDefaultDoorWidth() const { return DefaultDoorWidth; }
	void SetDefaultWindowHeightWidth(float height, float width);
	float GetDefaultWindowHeight() const { return DefaultWindowHeight; }
	float GetDefaultWindowWidth() const { return DefaultWindowWidth; }

	void SetDefaultJustificationZ(float newValue);
	float GetDefaultJustificationZ() const { return DefaultJustificationZ; } // return DJZ from private;
	void SetDefaultJustificationXY(float newValue);
	float GetDefaultJustificationXY() const { return DefaultJustificationXY; } // return DJXY from private;

	void AdjustMoiHoleVerts(int32 id, const FVector &location, const TArray<FVector> &holeVerts);

	int32 MakePortalAt(UWorld *world, EObjectType portalType, int32 parentID, const FVector2D &relativePos, const FQuat &relativeRot, bool inverted, const FModumateObjectAssembly &pal);
	void TransverseObjects(const TArray<Modumate::FModumateObjectInstance*> &obs);

	int32 MakeRoom(UWorld *World, const TArray<Modumate::FSignedID> &FaceIDs);
	int32 MakePointsObject(UWorld *world, const TArray<int32> &idsToDelete, const TArray<FVector> &points, const TArray<int32> &controlIndices,
		EObjectType objectType, bool inverted, const FModumateObjectAssembly &assembly, int32 parentID, bool bUpdateSiblingGeometry = false);

	bool MakeMetaObject(UWorld *world, const TArray<FVector> &points, const TArray<int32> &IDs, EObjectType objectType, int32 parentID, TArray<int32> &OutObjIDs);

	bool MakeScopeBoxObject(UWorld *world, const TArray<FVector> &points, TArray<int32> &OutObjIDs, const float Height);

	// Allocates IDs for new objects, finds new parent IDs for objects, and marks objects for deletion after another graph operation
	bool FinalizeGraph2DDelta(const Modumate::FGraph2DDelta &Delta, TMap<int32, Modumate::FGraph2DHostedObjectDelta> &OutParentIDUpdates);
	bool FinalizeGraphDelta(Modumate::FGraph3D &TempGraph, Modumate::FGraph3DDelta &Delta);

	bool UpdateGeometry(UWorld *world, int32 id, const TArray<FVector> &points, const FVector& extents);
	void UpdateControlIndices(int32 id, const TArray<int32> &indices);
	void UpdateControlValues(int32 id, const TArray<FVector> &controlPoints, const TArray<int32> &controlIndices);
	void Split(int32 id, const TArray<FVector> &pointsA, const TArray<FVector> &pointsB, const TArray<int32> &indicesA, const TArray<int32> &indicesB);

	bool GetVertexMovementDeltas(const TArray<int32>& VertexIDs, const TArray<FVector>& VertexPositions, TArray<TSharedPtr<Modumate::FDelta>>& OutDeltas);
	bool MoveMetaVertices(UWorld* World, const TArray<int32>& VertexIDs, const TArray<FVector>& VertexPositions);

	bool JoinMetaObjects(UWorld *World, const TArray<int32> &ObjectIDs);

	int32 MakeGroupObject(UWorld *world, const TArray<int32> &ids, bool combineWithExistingGroups, int32 parentID);
	void UnmakeGroupObjects(UWorld *world, const TArray<int32> &groupIds);

	Modumate::FModumateObjectInstance *ObjectFromActor(AActor *actor);
	const Modumate::FModumateObjectInstance *ObjectFromActor(const AActor *actor) const;

	void UpdateMitering(UWorld *world, const TArray<int32> &dirtyObjIDs);

	bool CanRoomContainFace(Modumate::FSignedID FaceID);
	void UpdateRoomAnalysis(UWorld *world);

	const Modumate::FGraph3D &GetVolumeGraph() const { return VolumeGraph; }

	// TODO: we should remove the TempVolumeGraph entirely if we can rely on inverting deltas,
	// or give graph delta creators their own copy of a graph to modify if we still need to use it for delta creation.
	const Modumate::FGraph3D &GetTempVolumeGraph() const { return TempVolumeGraph; }
	Modumate::FGraph3D &GetTempVolumeGraph() { return TempVolumeGraph; }

	const Modumate::FGraph2D *FindSurfaceGraph(int32 SurfaceGraphID) const;
	Modumate::FGraph2D *FindSurfaceGraph(int32 SurfaceGraphID);

	int32 CalculatePolyhedra() { return VolumeGraph.CalculatePolyhedra(); }
	bool IsObjectInVolumeGraph(int32 ObjID, Modumate::EGraph3DObjectType &OutObjType) const;

	TArray<const Modumate::FModumateObjectInstance*> GetObjectsOfType(EObjectType type) const;
	TArray<Modumate::FModumateObjectInstance*> GetObjectsOfType(EObjectType type);

	void GetObjectIdsByAssembly(const FName &assemblyKey, TArray<int32> &outIDs) const;

	static const FName DocumentHideRequestTag;
	void AddHideObjectsById(UWorld *world, const TArray<int32> &ids);
	void UnhideAllObjects(UWorld *world);

	// Deletion and restoration functions used internally by undo/redo-aware functions
private:
	bool DeleteObjectImpl(Modumate::FModumateObjectInstance *ob, bool keepInDeletedList = true);
	bool RestoreObjectImpl(Modumate::FModumateObjectInstance *ob);
	bool RestoreChildrenImpl(Modumate::FModumateObjectInstance *obj);

	Modumate::FModumateObjectInstance* CreateOrRestoreObjFromAssembly(UWorld *World, const FModumateObjectAssembly &Assembly,
		int32 ID, int32 ParentID = 0, const FVector &Extents = FVector::ZeroVector,
		const TArray<FVector> *CPS = nullptr, const TArray<int32> *CPI = nullptr, bool bInverted = false);

	Modumate::FModumateObjectInstance* CreateOrRestoreObjFromObjectType(UWorld *World, EObjectType OT,
		int32 ID, int32 ParentID = 0, const FVector &Extents = FVector::ZeroVector,
		const TArray<FVector> *CPS = nullptr, const TArray<int32> *CPI = nullptr, bool bInverted = false);

public:
	bool ApplyMOIDelta(const Modumate::FMOIDelta &Delta, UWorld *World);
	void ApplyGraph2DDelta(const Modumate::FGraph2DDelta &Delta, UWorld *World);
	void ApplyGraph3DDelta(const Modumate::FGraph3DDelta &Delta, UWorld *World);
	bool ApplyDeltas(const TArray<TSharedPtr<Modumate::FDelta>> &Deltas, UWorld *World);
	bool UpdateVolumeGraphObjects(UWorld *World);

private:
	bool FinalizeGraphDeltas(TArray <Modumate::FGraph3DDelta> &Deltas, TArray<int32> &OutAddedFaceIDs, TArray<int32> &OutAddedVertexIDs, TArray<int32> &OutAddedEdgeIDs);
	bool PostApplyDeltas(UWorld *World);

	// Helper function for ObjectFromActor
	Modumate::FModumateObjectInstance *ObjectFromSingleActor(AActor *actor);

	void PerformUndoRedo(UWorld* World, TArray<UndoRedo*>& FromBuffer, TArray<UndoRedo*>& ToBuffer);

public:

	Modumate::FModumateObjectInstance *TryGetDeletedObject(int32 id);

	void DeleteObjects(const TArray<Modumate::FModumateObjectInstance*> &obs, bool bAllowRoomAnalysis = true, bool bDeleteConnected = true);
	bool InvertObjects(const TArray<Modumate::FModumateObjectInstance*> &obs);
	void RestoreDeletedObjects(const TArray<int32> &ids);
	void DeleteObjects(const TArray<int32> &obIds, bool bAllowRoomAnalysis = true, bool bDeleteConnected = true);

	void MakeNew(UWorld *world);
	bool Save(UWorld *world, const FString &path);
	bool Load(UWorld *world, const FString &path, bool setAsCurrentProject);
	void SetCurrentProjectPath(const FString& currentProjectPath = FString());

	int32 CreateObjectFromRecord(UWorld *world, const FMOIDataRecord &obRec);

	TArray<int32> CloneObjects(UWorld *world, const TArray<int32> &obs, const FTransform& offsetTransform = FTransform::Identity);
	TArray<Modumate::FModumateObjectInstance *> CloneObjects(UWorld *world, const TArray<Modumate::FModumateObjectInstance *> &obs, const FTransform& offsetTransform = FTransform::Identity);
	int32 CloneObject(UWorld *world, const Modumate::FModumateObjectInstance *original);

	bool ExportPDF(UWorld *world, const TCHAR *filepath, const FVector &origin, const FVector &normal);
	bool ExportDWG(UWorld *world, const TCHAR *filepath);

	void Undo(UWorld *World);
	void Redo(UWorld *World);

	void ClearRedoBuffer();
	void ClearUndoBuffer();

	FBoxSphereBounds CalculateProjectBounds() const;

	bool IsDirty() const;
	void DisplayDebugInfo(UWorld* world);
	void DrawDebugVolumeGraph(UWorld* world);

	FString CurrentProjectPath;
	FString CurrentProjectName;

	// Document metadata
	FString DocumentPath;

	FString CurrentEncodedThumbnail;

	struct FPartyProfile
	{
		FString Role, Name, Representative, Email, Phone, LogoPath;
	};

	struct FDraftRevision
	{
		FString Name;
		FDateTime DateTime;
		int Number;
	};

	struct FProjectInfo
	{
		FString name;
		FString address1;
		FString address2;
		FString lotNumber;
		FString ID;
		FString description;
	};

	FString StampPath;
	FPartyProfile LeadArchitect, Client;
	FProjectInfo ProjectInfo;
	TArray<FPartyProfile> SecondaryParties;
	TArray<FDraftRevision> Revisions;

	/*
	Assemblies
	TODO: Assembly management to all happen through the preset manager and be associated with object types instead of tool modes
	Functions will remain here, refactored for object types, to take advantage of undo/redo
	*/
	FPresetManager PresetManager;

	// to be deprecated in favor of a single "update assembly" function that will call through to the preset manager
	// assemblies to be accessed by object type in the preset manager, not by the tool type here
	TArray<FModumateObjectAssembly> GetAssembliesForToolMode_DEPRECATED(UWorld *world, EToolMode mode) const;
	FModumateObjectAssembly CreateNewAssembly_DEPRECATED(UWorld *world, EToolMode mode, const FModumateObjectAssembly &assembly);
	FModumateObjectAssembly OverwriteAssembly_DEPRECATED(UWorld *world, EToolMode mode, const FModumateObjectAssembly &assembly);
	FModumateObjectAssembly CreateOrOverwriteAssembly_DEPRECATED(UWorld *world, EToolMode mode, const FModumateObjectAssembly &assembly);

	// DDL 1.0 presets imported from the "marketplace," to be replaced by built in DDL 2.0 presets
	void ImportPresetsIfMissing_DEPRECATED(UWorld *world, const TArray<FName> &presets);
		
	// Not deprecated, but will be refactored for object type and preset key
	void OnAssemblyUpdated(UWorld *world, EToolMode mode, const FModumateObjectAssembly &assembly);
	bool RemoveAssembly(UWorld *world, EToolMode toolMode, const FName &assemblyKey, const FName &replacementKey);

private:
	// TODO: All sequencing/coding schemes etc to be handled by a single key pool in the preset manager
	// Vestigial implementation kept in place to mark where this currently expect to be done when it's working
	void ResequencePortalAssemblies_DEPRECATED(UWorld *world, EObjectType portalType) {};

private:
	TSharedPtr<Modumate::FModumateDraftingView> CurrentDraftingView = nullptr;
};

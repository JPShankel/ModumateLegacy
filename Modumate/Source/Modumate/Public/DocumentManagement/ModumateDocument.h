// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "Database/ModumateObjectDatabase.h"
#include "DocumentManagement/DocumentDelta.h"
#include "DocumentManagement/ModumateCameraView.h"
#include "DocumentManagement/ModumatePresetManager.h"
#include "DocumentManagement/ModumateSerialization.h"
#include "Graph/Graph2D.h"
#include "Graph/Graph2DDelta.h"
#include "Graph/Graph3D.h"
#include "Graph/Graph3DDelta.h"
#include "Objects/ModumateObjectInstance.h"


namespace Modumate
{
	class FModumateDraftingView;
}

class FModumateObjectInstance;

class MODUMATE_API FModumateDocument
{
private:

	struct UndoRedo
	{
		TArray<FDeltaPtr> Deltas;
	};

	TArray<TSharedPtr<UndoRedo>> UndoBuffer, RedoBuffer;
	TArray<FDeltaPtr> PreviewDeltas;

	int32 NextID;

	TArray<FModumateObjectInstance*> ObjectInstanceArray;
	TMap<int32, FModumateObjectInstance*> ObjectsByID;
	TMap<int32, FModumateObjectInstance*> DeletedObjects;

	void GatherDocumentMetadata();
	float DefaultWallHeight;
	float DefaultRailHeight = 106.68f;
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
	TMap<int32, TSharedPtr<Modumate::FGraph2D>> SurfaceGraphs;

	// Link between 2D graph objects and the surface graph they are a part of
	TMap<int32, int32> SurfaceGraphsBy2DGraphObj;

	TMap<EObjectDirtyFlags, TArray<FModumateObjectInstance*>> DirtyObjectMap;

public:

	FModumateDocument();
	~FModumateDocument();

	// Default Value Library
	float ElevationDelta;

	// Data
	FModumateObjectInstance *GetObjectById(int32 id);
	const FModumateObjectInstance *GetObjectById(int32 id) const;
	TSet<int32> HiddenObjectsID;
	TArray<FModumateCameraView> SavedCameraViews;

	// TODO: resolve how best to get IDs for created objects into MOI deltas
	// For now, just ask what the next ID will be
	int32 GetNextAvailableID() const { return NextID; }

	const TArray<FModumateObjectInstance*>& GetObjectInstances() const { return ObjectInstanceArray; }
	TArray<FModumateObjectInstance*>& GetObjectInstances() { return ObjectInstanceArray; }

	bool CleanObjects(TArray<FDeltaPtr>* OutSideEffectDeltas = nullptr);
	void RegisterDirtyObject(EObjectDirtyFlags DirtyType, FModumateObjectInstance *DirtyObj, bool bDirty);

	void AddCommandToHistory(const FString &cmd);
	TArray<FString> GetCommandHistory() const;

	void BeginUndoRedoMacro();
	void EndUndoRedoMacro();
	bool InUndoRedoMacro() const;

	void SetAssemblyForObjects(UWorld *world, TArray<int32> ids, const FBIMAssemblySpec &assembly);

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

	int32 MakeRoom(UWorld *World, const TArray<FGraphSignedID> &FaceIDs);
	bool MakeMetaObject(UWorld *world, const TArray<FVector> &points, const TArray<int32> &IDs, EObjectType objectType, int32 parentID, TArray<int32> &OutObjIDs);

	bool MakeScopeBoxObject(UWorld *world, const TArray<FVector> &points, TArray<int32> &OutObjIDs, const float Height);

	// Allocates IDs for new objects, finds new parent IDs for objects, and marks objects for deletion after another graph operation
	bool FinalizeGraph2DDelta(const FGraph2DDelta &Delta, TMap<int32, FGraph2DHostedObjectDelta> &OutParentIDUpdates);
	bool FinalizeGraphDelta(Modumate::FGraph3D &TempGraph, FGraph3DDelta &Delta);

	bool GetVertexMovementDeltas(const TArray<int32>& VertexIDs, const TArray<FVector>& VertexPositions, TArray<FDeltaPtr>& OutDeltas);
	bool MoveMetaVertices(UWorld* World, const TArray<int32>& VertexIDs, const TArray<FVector>& VertexPositions);

	bool JoinMetaObjects(UWorld *World, const TArray<int32> &ObjectIDs);

	int32 MakeGroupObject(UWorld *world, const TArray<int32> &ids, bool combineWithExistingGroups, int32 parentID);
	void UnmakeGroupObjects(UWorld *world, const TArray<int32> &groupIds);

	FModumateObjectInstance *ObjectFromActor(AActor *actor);
	const FModumateObjectInstance *ObjectFromActor(const AActor *actor) const;

	void UpdateMitering(UWorld *world, const TArray<int32> &dirtyObjIDs);

	bool CanRoomContainFace(FGraphSignedID FaceID);
	void UpdateRoomAnalysis(UWorld *world);

	const Modumate::FGraph3D &GetVolumeGraph() const { return VolumeGraph; }

	// TODO: we should remove the TempVolumeGraph entirely if we can rely on inverting deltas,
	// or give graph delta creators their own copy of a graph to modify if we still need to use it for delta creation.
	const Modumate::FGraph3D &GetTempVolumeGraph() const { return TempVolumeGraph; }
	Modumate::FGraph3D &GetTempVolumeGraph() { return TempVolumeGraph; }

	const TSharedPtr<Modumate::FGraph2D> FindSurfaceGraph(int32 SurfaceGraphID) const;
	TSharedPtr<Modumate::FGraph2D> FindSurfaceGraph(int32 SurfaceGraphID);

	const TSharedPtr<Modumate::FGraph2D> FindSurfaceGraphByObjID(int32 ObjectID) const;
	TSharedPtr<Modumate::FGraph2D> FindSurfaceGraphByObjID(int32 ObjectID);

	int32 CalculatePolyhedra() { return VolumeGraph.CalculatePolyhedra(); }
	bool IsObjectInVolumeGraph(int32 ObjID, Modumate::EGraph3DObjectType &OutObjType) const;

	TArray<const FModumateObjectInstance*> GetObjectsOfType(EObjectType type) const;
	TArray<FModumateObjectInstance*> GetObjectsOfType(EObjectType type);
	using FObjectTypeSet = TSet<EObjectType>;
	TArray<const FModumateObjectInstance*> GetObjectsOfType(const FObjectTypeSet& types) const;
	TArray<FModumateObjectInstance*> GetObjectsOfType(const FObjectTypeSet& types);

	void GetObjectIdsByAssembly(const FBIMKey& AssemblyKey, TArray<int32>& OutIDs) const;

	static const FName DocumentHideRequestTag;
	void AddHideObjectsById(UWorld *world, const TArray<int32> &ids);
	void UnhideAllObjects(UWorld *world);
	void UnhideObjectsById(UWorld *world, const TArray<int32> &ids);

	// Deletion and restoration functions used internally by undo/redo-aware functions
private:
	bool DeleteObjectImpl(FModumateObjectInstance *ob, bool keepInDeletedList = true);
	bool RestoreObjectImpl(FModumateObjectInstance *ob);
	bool RestoreChildrenImpl(FModumateObjectInstance *obj);

	FModumateObjectInstance* CreateOrRestoreObj(UWorld* World, const FMOIStateData& StateData);

	// Preview Operations
public:
	bool ApplyPreviewDeltas(const TArray<FDeltaPtr> &Deltas, UWorld *World);
	void ClearPreviewDeltas(UWorld *World);

	bool GetPreviewVertexMovementDeltas(const TArray<int32>& VertexIDs, const TArray<FVector>& VertexPositions, TArray<FDeltaPtr>& OutDeltas);

public:
	bool ApplyMOIDelta(const FMOIDelta& Delta, UWorld* World);
	void ApplyGraph2DDelta(const FGraph2DDelta &Delta, UWorld *World);
	void ApplyGraph3DDelta(const FGraph3DDelta &Delta, UWorld *World);
	bool ApplyDeltas(const TArray<FDeltaPtr> &Deltas, UWorld *World);

	void UpdateVolumeGraphObjects(UWorld *World);

private:
	bool FinalizeGraphDeltas(TArray <FGraph3DDelta> &Deltas, TArray<int32> &OutAddedFaceIDs, TArray<int32> &OutAddedVertexIDs, TArray<int32> &OutAddedEdgeIDs);
	bool PostApplyDeltas(UWorld *World);

	// Helper function for ObjectFromActor
	FModumateObjectInstance *ObjectFromSingleActor(AActor *actor);

	void PerformUndoRedo(UWorld* World, TArray<TSharedPtr<UndoRedo>>& FromBuffer, TArray<TSharedPtr<UndoRedo>>& ToBuffer);

public:

	FModumateObjectInstance *TryGetDeletedObject(int32 id);

	void DeleteObjects(const TArray<FModumateObjectInstance*> &obs, bool bAllowRoomAnalysis = true, bool bDeleteConnected = true);
	void RestoreDeletedObjects(const TArray<int32> &ids);
	void DeleteObjects(const TArray<int32> &obIds, bool bAllowRoomAnalysis = true, bool bDeleteConnected = true);

	void MakeNew(UWorld *world);
	bool Save(UWorld *world, const FString &path);
	bool Load(UWorld *world, const FString &path, bool setAsCurrentProject);
	void SetCurrentProjectPath(const FString& currentProjectPath = FString());

	TArray<int32> CloneObjects(UWorld *world, const TArray<int32> &obs, const FTransform& offsetTransform = FTransform::Identity);
	TArray<FModumateObjectInstance *> CloneObjects(UWorld *world, const TArray<FModumateObjectInstance *> &obs, const FTransform& offsetTransform = FTransform::Identity);
	int32 CloneObject(UWorld *world, const FModumateObjectInstance *original);

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
	void DrawDebugSurfaceGraphs(UWorld* world);

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

	FPresetManager PresetManager;

private:
	// TODO: All sequencing/coding schemes etc to be handled by a single key pool in the preset manager
	// Vestigial implementation kept in place to mark where this currently expect to be done when it's working
	void ResequencePortalAssemblies_DEPRECATED(UWorld *world, EObjectType portalType) {};

private:
	TSharedPtr<Modumate::FModumateDraftingView> CurrentDraftingView = nullptr;
};

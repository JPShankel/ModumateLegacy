// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "Database/ModumateObjectDatabase.h"
#include "DocumentManagement/DocumentDelta.h"
#include "DocumentManagement/ModumateCameraView.h"
#include "DocumentManagement/ModumateSerialization.h"
#include "Graph/Graph2D.h"
#include "Graph/Graph2DDelta.h"
#include "Graph/Graph3D.h"
#include "Graph/Graph3DDelta.h"
#include "Objects/ModumateObjectInstance.h"

#include "ModumateDocument.generated.h"

namespace Modumate
{
	class FModumateDraftingView;
}

class AModumateObjectInstance;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnAppliedMOIDeltas, EObjectType, ObjectType, int32, Count, EMOIDeltaType, DeltaType);

UCLASS()
class MODUMATE_API UModumateDocument : public UObject
{
	GENERATED_BODY()

private:

	struct UndoRedo
	{
		TArray<FDeltaPtr> Deltas;
	};

	TArray<TSharedPtr<UndoRedo>> UndoBuffer, RedoBuffer;
	TArray<FDeltaPtr> PreviewDeltas;

	int32 NextID;
	int32 PrePreviewNextID;
	int32 ReservingObjectID;
	bool bApplyingPreviewDeltas, bFastClearingPreviewDeltas, bSlowClearingPreviewDeltas;

	UPROPERTY()
	TArray<AModumateObjectInstance*> ObjectInstanceArray;

	UPROPERTY()
	TMap<int32, AModumateObjectInstance*> ObjectsByID;

	UPROPERTY()
	TMap<int32, AModumateObjectInstance*> DeletedObjects;

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

	// The volume connectivity information, for the purpose of keeping track of connectivity of all planar objects;
	// the source of truth for mitering, room detection, volume calculation, slicing floorplans/sections/elevations, heat/acoustics, etc.
	Modumate::FGraph3D VolumeGraph;

	// Copy of the volume graph to work with multi-stage deltas
	Modumate::FGraph3D TempVolumeGraph;

	// The surface graphs used by the current document, mapped by owning object ID
	TMap<int32, TSharedPtr<Modumate::FGraph2D>> SurfaceGraphs;

	TMap<EObjectDirtyFlags, TArray<AModumateObjectInstance*>> DirtyObjectMap;

	bool bTrackingDeltaObjects = false;
	TMap<EObjectType, TSet<int32>> DeltaCreatedObjects, DeltaDestroyedObjects;

public:

	UModumateDocument(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// Default Value Library
	float ElevationDelta;

	// Data
	AModumateObjectInstance *GetObjectById(int32 id);
	const AModumateObjectInstance *GetObjectById(int32 id) const;
	TSet<int32> HiddenObjectsID;
	TArray<FModumateCameraView> SavedCameraViews;

	// TODO: resolve how best to get IDs for created objects into MOI deltas
	// For now, just ask what the next ID will be
	int32 GetNextAvailableID() const;
	int32 ReserveNextIDs(int32 reservingObjID);
	void SetNextID(int32 ID, int32 reservingObjID);

	const TArray<AModumateObjectInstance*>& GetObjectInstances() const { return ObjectInstanceArray; }
	TArray<AModumateObjectInstance*>& GetObjectInstances() { return ObjectInstanceArray; }

	bool CleanObjects(TArray<FDeltaPtr>* OutSideEffectDeltas = nullptr);
	void RegisterDirtyObject(EObjectDirtyFlags DirtyType, AModumateObjectInstance *DirtyObj, bool bDirty);

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
	bool MakeMetaObject(UWorld *world, const TArray<FVector> &points, TArray<int32>& OutObjectIDs, TArray<FDeltaPtr>& OutDeltaPtrs, bool bSplitAndUpdateFaces = true);

	bool PasteMetaObjects(const FGraph3DRecord* InRecord, TArray<FDeltaPtr>& OutDeltaPtrs, TMap<int32, TArray<int32>>& OutCopiedToPastedIDs, const FVector &Offset, bool bIsPreview);

	bool MakeScopeBoxObject(UWorld *world, const TArray<FVector> &points, TArray<int32> &OutObjIDs, const float Height);

	// Allocates IDs for new objects, finds new parent IDs for objects, and marks objects for deletion after another graph operation
	bool FinalizeGraph2DDelta(const FGraph2DDelta &Delta, TMap<int32, FGraph2DHostedObjectDelta> &OutParentIDUpdates);
	bool FinalizeGraphDelta(Modumate::FGraph3D &TempGraph, const FGraph3DDelta &Delta, TArray<FDeltaPtr> &OutSideEffectDeltas);

	bool GetVertexMovementDeltas(const TArray<int32>& VertexIDs, const TArray<FVector>& VertexPositions, TArray<FDeltaPtr>& OutDeltas);
	bool MoveMetaVertices(UWorld* World, const TArray<int32>& VertexIDs, const TArray<FVector>& VertexPositions);

	bool JoinMetaObjects(UWorld *World, const TArray<int32> &ObjectIDs);

	int32 MakeGroupObject(UWorld *world, const TArray<int32> &ids, bool combineWithExistingGroups, int32 parentID);
	void UnmakeGroupObjects(UWorld *world, const TArray<int32> &groupIds);

	AModumateObjectInstance *ObjectFromActor(AActor *actor);
	const AModumateObjectInstance *ObjectFromActor(const AActor *actor) const;

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

	TArray<const AModumateObjectInstance*> GetObjectsOfType(EObjectType type) const;
	TArray<AModumateObjectInstance*> GetObjectsOfType(EObjectType type);
	using FObjectTypeSet = TSet<EObjectType>;
	TArray<const AModumateObjectInstance*> GetObjectsOfType(const FObjectTypeSet& types) const;
	TArray<AModumateObjectInstance*> GetObjectsOfType(const FObjectTypeSet& types);

	void GetObjectIdsByAssembly(const FGuid& AssemblyKey, TArray<int32>& OutIDs) const;

	static const FName DocumentHideRequestTag;
	void AddHideObjectsById(UWorld *world, const TArray<int32> &ids);
	void UnhideAllObjects(UWorld *world);
	void UnhideObjectsById(UWorld *world, const TArray<int32> &ids);

	UPROPERTY()
	FOnAppliedMOIDeltas OnAppliedMOIDeltas;

	// Deletion and restoration functions used internally by undo/redo-aware functions
private:
	bool DeleteObjectImpl(AModumateObjectInstance *ob);
	bool RestoreObjectImpl(AModumateObjectInstance *ob);

	AModumateObjectInstance* CreateOrRestoreObj(UWorld* World, const FMOIStateData& StateData);

	// Preview Operations
public:
	bool StartPreviewing();
	bool ApplyPreviewDeltas(const TArray<FDeltaPtr> &Deltas, UWorld *World);
	bool IsPreviewingDeltas() const;
	void ClearPreviewDeltas(UWorld *World, bool bFastClear = false);
	void CalculateSideEffectDeltas(TArray<FDeltaPtr>& Deltas, UWorld* World);

	bool GetPreviewVertexMovementDeltas(const TArray<int32>& VertexIDs, const TArray<FVector>& VertexPositions, TArray<FDeltaPtr>& OutDeltas);

public:
	bool ApplyMOIDelta(const FMOIDelta& Delta, UWorld* World);
	void ApplyGraph2DDelta(const FGraph2DDelta &Delta, UWorld *World);
	void ApplyGraph3DDelta(const FGraph3DDelta &Delta, UWorld *World);
	bool ApplyDeltas(const TArray<FDeltaPtr> &Deltas, UWorld *World);
	bool ApplyPresetDelta(const FBIMPresetDelta& PresetDelta, UWorld* World);

	void UpdateVolumeGraphObjects(UWorld *World);

private:
	bool FinalizeGraphDeltas(const TArray<FGraph3DDelta> &InDeltas, TArray<FDeltaPtr> &OutDeltas);
	bool PostApplyDeltas(UWorld *World);
	void StartTrackingDeltaObjects();
	void EndTrackingDeltaObjects();

	// Helper function for ObjectFromActor
	AModumateObjectInstance *ObjectFromSingleActor(AActor *actor);

	void PerformUndoRedo(UWorld* World, TArray<TSharedPtr<UndoRedo>>& FromBuffer, TArray<TSharedPtr<UndoRedo>>& ToBuffer);

public:

	AModumateObjectInstance *TryGetDeletedObject(int32 id);

	void DeleteObjects(const TArray<AModumateObjectInstance*> &initialObjectsToDelete, bool bAllowRoomAnalysis = true, bool bDeleteConnected = true);
	void GetDeleteObjectsDeltas(TArray<FDeltaPtr> &OutDeltas, const TArray<AModumateObjectInstance*> &initialObjectsToDelete, bool bAllowRoomAnalysis = true, bool bDeleteConnected = true);
	void RestoreDeletedObjects(const TArray<int32> &ids);
	void DeleteObjects(const TArray<int32> &obIds, bool bAllowRoomAnalysis = true, bool bDeleteConnected = true);

	void MakeNew(UWorld *World);
	bool SerializeRecords(UWorld* World, FModumateDocumentHeader& OutHeader, FMOIDocumentRecord& OutDocumentRecord);
	bool Save(UWorld *world, const FString &path, bool bSetAsCurrentProject);
	bool Load(UWorld *world, const FString &path, bool bSetAsCurrentProject, bool bRecordAsRecentProject);
	bool LoadDeltas(UWorld *world, const FString &path, bool bSetAsCurrentProject, bool bRecordAsRecentProject); // Debug - Loads all deltas into the redo buffer for replay purposes
	void SetCurrentProjectPath(const FString& currentProjectPath = FString());

	TArray<int32> CloneObjects(UWorld *world, const TArray<int32> &obs, const FTransform& offsetTransform = FTransform::Identity);
	TArray<AModumateObjectInstance *> CloneObjects(UWorld *world, const TArray<AModumateObjectInstance *> &obs, const FTransform& offsetTransform = FTransform::Identity);
	int32 CloneObject(UWorld *world, const AModumateObjectInstance *original);

	bool ExportPDF(UWorld *world, const TCHAR *filepath, const FVector &origin, const FVector &normal);
	bool ExportDWG(UWorld *world, const TCHAR *filepath);

	void Undo(UWorld *World);
	void Redo(UWorld *World);

	void ClearRedoBuffer();
	void ClearUndoBuffer();

	FBoxSphereBounds CalculateProjectBounds() const;

	bool IsDirty() const { return bIsDirty; }

	void DisplayDebugInfo(UWorld* world);
	void DrawDebugVolumeGraph(UWorld* world);
	void DrawDebugSurfaceGraphs(UWorld* world);

	FString CurrentProjectPath;
	FString CurrentProjectName;

	// Document metadata
	FString DocumentPath;

	FString CurrentEncodedThumbnail;

	// Generates a GUID...non-const because all generated guids are stored to avoid (infinitessimal) chance of duplication
	bool MakeNewGUIDForPreset(FBIMPresetInstance& Preset);

	const FBIMPresetCollection& GetPresetCollection() const;
	FBIMPresetCollection& GetPresetCollection();

private:
	TSharedPtr<Modumate::FModumateDraftingView> CurrentDraftingView = nullptr;
	FBIMPresetCollection BIMPresetCollection;
	bool bIsDirty = true;
};

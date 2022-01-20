// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "Database/ModumateObjectDatabase.h"
#include "DocumentManagement/DocumentDelta.h"
#include "DocumentManagement/DocumentSettings.h"
#include "DocumentManagement/ModumateSerialization.h"
#include "DrawingDesigner/DrawingDesignerDocument.h"
#include "Graph/Graph2D.h"
#include "Graph/Graph2DDelta.h"
#include "Graph/Graph3D.h"
#include "Graph/Graph3DDelta.h"
#include "Objects/ModumateObjectInstance.h"

#include "ModumateDocument.generated.h"

class FModumateDraftingView;
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

	// The initial hash of the document; normally 0, unless VerifiedDeltasRecords cannot be loaded or used.
	uint32 InitialDocHash = 0;

	// The FDeltasRecords that have been locally on the multiplayer client, but haven't yet been verified and sent back by the server.
	TArray<FDeltasRecord> UnverifiedDeltasRecords;

	// The FDeltasRecords that have either been applied locally offline, or been verified and sent by the server to all clients.
	TArray<FDeltasRecord> VerifiedDeltasRecords;

	UPROPERTY()
	TArray<AModumateObjectInstance*> ObjectInstanceArray;

	UPROPERTY()
	TMap<int32, AModumateObjectInstance*> ObjectsByID;

	UPROPERTY()
	TMap<int32, AModumateObjectInstance*> DeletedObjects;

	TMap <EObjectType, TSet<int32>> ObjectsByType;

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
	TMap<int32, TSharedPtr<FGraph3D>> VolumeGraphs;
	int32 RootVolumeGraph = MOD_ID_NONE;
	// TODO: move active graph to EMPlayerState or elsewhere since it isn't really part of the document.
	int32 ActiveVolumeGraph = MOD_ID_NONE;
	// Map the element (face, edge, etc) ID to the ID of the graph that contains it.
	TMap<int32, int32> GraphElementsToGraph3DMap;

	// Copy of the volume graph to work with multi-stage deltas
	FGraph3D TempVolumeGraph;

	// The surface graphs used by the current document, mapped by owning object ID
	TMap<int32, TSharedPtr<FGraph2D>> SurfaceGraphs;

	TMap<EObjectDirtyFlags, TArray<AModumateObjectInstance*>> DirtyObjectMap;

	bool bTrackingDeltaObjects = false;
	FAffectedObjMap DeltaAffectedObjects;
	TSet<int32> DeltaDirtiedObjects;
	TSet<FGuid> DeltaAffectedPresets;

public:

	UModumateDocument();

	UPROPERTY()
	FDrawingDesignerDocument DrawingDesignerDocument;

	void SetLocalUserInfo(const FString& InLocalUserID, int32 InLocalUserIdx);

	// Default Value Library
	float ElevationDelta;

	// Data
	AModumateObjectInstance *GetObjectById(int32 id);
	const AModumateObjectInstance *GetObjectById(int32 id) const;
	TSet<int32> HiddenObjectsID;
	TMap<uint32, FGuid> TypicalEdgeDetails;

	// TODO: resolve how best to get IDs for created objects into MOI deltas
	// For now, just ask what the next ID will be
	int32 GetNextAvailableID() const;
	int32 ReserveNextIDs(int32 reservingObjID);
	void SetNextID(int32 ID, int32 reservingObjID);

	// TODO: this functionality could potentially be rolled into the Delta constructor
	void InitDeltaMetadata(FDeltasRecord& DeltasRecord);

	const TArray<AModumateObjectInstance*>& GetObjectInstances() const { return ObjectInstanceArray; }
	TArray<AModumateObjectInstance*>& GetObjectInstances() { return ObjectInstanceArray; }

	static const int32 CleanIterationSafeguard;
	bool CleanObjects(TArray<FDeltaPtr>* OutSideEffectDeltas = nullptr, bool bDeleteUncleanableObjects = false, bool bInitialLoad = false);
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
	bool FinalizeGraph2DDeltas(const TArray<FGraph2DDelta>& InDeltas, int32& InNextID, TArray<FDeltaPtr>& OutDeltas) const;
	bool FinalizeGraph2DDelta(const FGraph2DDelta& Delta, int32& InNextID, TArray<FDeltaPtr>& OutSideEffectDeltas) const;
	bool FinalizeGraphDelta(FGraph3D &TempGraph, const FGraph3DDelta &Delta, TArray<FDeltaPtr> &OutSideEffectDeltas);

	bool GetVertexMovementDeltas(const TArray<int32>& VertexIDs, const TArray<FVector>& VertexPositions, TArray<FDeltaPtr>& OutDeltas);
	bool MoveMetaVertices(UWorld* World, const TArray<int32>& VertexIDs, const TArray<FVector>& VertexPositions);

	bool JoinMetaObjects(UWorld *World, const TArray<int32> &ObjectIDs);

	bool GetGraph2DDeletionDeltas(int32 Graph2DID, int32& InNextID, TArray<FDeltaPtr>& OutDeltas) const;

	int32 MakeGroupObject(UWorld *world, const TArray<int32> &ids, bool combineWithExistingGroups, int32 parentID);
	void UnmakeGroupObjects(UWorld *world, const TArray<int32> &groupIds);

	AModumateObjectInstance *ObjectFromActor(AActor *actor);
	const AModumateObjectInstance *ObjectFromActor(const AActor *actor) const;

	void UpdateMitering(UWorld *world, const TArray<int32> &dirtyObjIDs);

	bool CanRoomContainFace(FGraphSignedID FaceID);
	void UpdateRoomAnalysis(UWorld *world);

	FGraph3D* GetVolumeGraph(int32 GraphId = MOD_ID_NONE);
	const FGraph3D* GetVolumeGraph(int32 GraphId = MOD_ID_NONE) const;
	// Find the volume graph that contains the graph3D element
	FGraph3D* FindVolumeGraph(int32 ElementID);
	const FGraph3D* FindVolumeGraph(int32 ElementID) const;
	int32 GetActiveVolumeGraphID() const { return ActiveVolumeGraph; }
	void SetActiveVolumeGraphID(int32 NewID);
	int32 GetRootVolumeGraphID() const { return RootVolumeGraph;  }

	int32 FindGraph3DByObjID(int32 MetaObjectID) const;

	// TODO: we should remove the TempVolumeGraph entirely if we can rely on inverting deltas,
	// or give graph delta creators their own copy of a graph to modify if we still need to use it for delta creation.
	const FGraph3D &GetTempVolumeGraph() const { return TempVolumeGraph; }
	FGraph3D &GetTempVolumeGraph() { return TempVolumeGraph; }

	const TSharedPtr<FGraph2D> FindSurfaceGraph(int32 SurfaceGraphID) const;
	TSharedPtr<FGraph2D> FindSurfaceGraph(int32 SurfaceGraphID);

	const TSharedPtr<FGraph2D> FindSurfaceGraphByObjID(int32 ObjectID) const;
	TSharedPtr<FGraph2D> FindSurfaceGraphByObjID(int32 ObjectID);

	int32 CalculatePolyhedra() { return GetVolumeGraph()->CalculatePolyhedra(); }
	bool IsObjectInVolumeGraph(int32 ObjID, EGraph3DObjectType &OutObjType) const;

	// TODO: refactor for output parameters: GetObjectsOfType(EObjectType, TArray<const AModumateObjectInstance
	TArray<const AModumateObjectInstance*> GetObjectsOfType(EObjectType Type) const;
	TArray<AModumateObjectInstance*> GetObjectsOfType(EObjectType Type);
	using FObjectTypeSet = TSet<EObjectType>;
	TArray<const AModumateObjectInstance*> GetObjectsOfType(const FObjectTypeSet& Types) const;
	TArray<AModumateObjectInstance*> GetObjectsOfType(const FObjectTypeSet& Types);

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
	void CalculateSideEffectDeltas(TArray<FDeltaPtr>& Deltas, UWorld* World, bool bIsPreview);

	bool GetPreviewVertexMovementDeltas(const TArray<int32>& VertexIDs, const TArray<FVector>& VertexPositions, TArray<FDeltaPtr>& OutDeltas);

public:
	bool ReconcileRemoteDeltas(const FDeltasRecord& DeltasRecord, UWorld* World, FDeltasRecord& OutReconciledRecord, uint32& OutVerifiedDocHash);
	bool RollBackUnverifiedDeltas(uint32 VerifiedDocHash, UWorld* World);
	bool ApplyRemoteDeltas(const FDeltasRecord& DeltasRecord, UWorld* World, bool bApplyOwnDeltas);
	bool ApplyRemoteUndo(UWorld* World, const FString& UndoingUserID, const TArray<uint32>& UndoRecordHashes);
	bool ApplyMOIDelta(const FMOIDelta& Delta, UWorld* World);
	void ApplyGraph2DDelta(const FGraph2DDelta &Delta, UWorld *World);
	void ApplyGraph3DDelta(const FGraph3DDelta &Delta, UWorld *World);
	bool ApplyDeltas(const TArray<FDeltaPtr> &Deltas, UWorld *World, bool bRedoingDeltas = false);
	bool ApplyPresetDelta(const FBIMPresetDelta& PresetDelta, UWorld* World);
	bool ApplySettingsDelta(const FDocumentSettingDelta& SettingsDelta, UWorld* World);

	void PurgeDeltas();

	void UpdateVolumeGraphObjects(UWorld *World);

	FBox GetAffectedBounds(const FAffectedObjMap& AffectedObjects, const TSet<int32>& DirtiedObjects) const;

private:
	bool FinalizeGraphDeltas(const TArray<FGraph3DDelta> &InDeltas, TArray<FDeltaPtr> &OutDeltas, int32 GraphID = MOD_ID_NONE);
	bool PostApplyDeltas(UWorld *World, bool bCleanObjects, bool bMarkDocumentDirty);
	bool DeepCloneForFinalize(FGraph3D& TempGraph, const AModumateObjectInstance* ChildObj, int32 ChildFaceID, TArray<FDeltaPtr>& OutDerivedDeltas);
	void StartTrackingDeltaObjects();
	void EndTrackingDeltaObjects();

	// Helper function for ObjectFromActor
	AModumateObjectInstance *ObjectFromSingleActor(AActor *actor);

	void ApplyInvertedDeltas(UWorld* World, const TArray<FDeltaPtr>& Deltas);
	void PerformUndoRedo(UWorld* World, TArray<TSharedPtr<UndoRedo>>& FromBuffer, TArray<TSharedPtr<UndoRedo>>& ToBuffer);

public:

	AModumateObjectInstance *TryGetDeletedObject(int32 id);

	void DeleteObjects(const TArray<AModumateObjectInstance*> &initialObjectsToDelete, bool bAllowRoomAnalysis = true, bool bDeleteConnected = true);
	bool GetDeleteObjectsDeltas(TArray<FDeltaPtr> &OutDeltas, const TArray<AModumateObjectInstance*> &initialObjectsToDelete, bool bAllowRoomAnalysis = true, bool bDeleteConnected = true);
	void RestoreDeletedObjects(const TArray<int32> &ids);
	void DeleteObjects(const TArray<int32> &obIds, bool bAllowRoomAnalysis = true, bool bDeleteConnected = true);

	void MakeNew(UWorld* World, bool bClearName = true);
	bool SerializeRecords(UWorld* World, FModumateDocumentHeader& OutHeader, FMOIDocumentRecord& OutDocumentRecord);
	static bool SaveRecords(const FString& FilePath, const FModumateDocumentHeader& InHeader, const FMOIDocumentRecord& InDocumentRecord);
	bool SaveFile(UWorld* World, const FString& FilePath, bool bUserFile, bool bAsync = false, const TFunction<void (bool)>& OnSaveFunction = nullptr);
	bool SaveAsBinary(UWorld* World, TArray<uint8>& OutBuffer);
	bool LoadRecord(UWorld* World, const FModumateDocumentHeader& InHeader, const FMOIDocumentRecord& InDocumentRecord, bool bClearName = true);
	bool LoadFile(UWorld* World, const FString& Path, bool bSetAsCurrentProject, bool bRecordAsRecentProject);
	bool LoadDeltas(UWorld* World, const FString& Path, bool bSetAsCurrentProject, bool bRecordAsRecentProject); // Debug - Loads all deltas into the redo buffer for replay purposes
	void SetCurrentProjectName(const FString& NewProjectName = FString(), bool bAsPath = true);

	// Expose the serialized structs that were most recently used to either load or save the document
	const FModumateDocumentHeader& GetLastSerializedHeader() const { return CachedHeader; }
	const FMOIDocumentRecord& GetLastSerializedRecord() const { return CachedRecord; }

	TArray<int32> CloneObjects(UWorld *world, const TArray<int32> &obs, const FTransform& offsetTransform = FTransform::Identity);
	TArray<AModumateObjectInstance *> CloneObjects(UWorld *world, const TArray<AModumateObjectInstance *> &obs, const FTransform& offsetTransform = FTransform::Identity);
	int32 CloneObject(UWorld *world, const AModumateObjectInstance *original);

	bool ExportDWG(UWorld* World, const TCHAR* Filepath);

	void Undo(UWorld *World);
	void Redo(UWorld *World);
	bool GetUndoRecordsFromClient(UWorld* World, const FString& UserID, TArray<uint32>& OutUndoRecordHashes) const;

	void ClearRedoBuffer();
	void ClearUndoBuffer();

	uint32 GetLatestVerifiedDocHash() const;
	const TArray<FDeltasRecord>& GetVerifiedDeltasRecords() const { return VerifiedDeltasRecords; }
	int32 FindDeltasRecordIdxByHash(uint32 RecordHash) const;
	uint32 GetInitialDocHash() const { return InitialDocHash; }

	FBoxSphereBounds CalculateProjectBounds() const;

	bool IsDirty(bool bUserFile = true) const;
	void SetDirtyFlags(bool bNewDirty);

	void DisplayDebugInfo(UWorld* world);
	void DrawDebugVolumeGraph(UWorld* world);
	void DrawDebugSurfaceGraphs(UWorld* world);
	void DisplayMultiplayerDebugInfo(UWorld* world);
	void DisplayDesignOptionDebugInfo(UWorld* World);

	FString CurrentProjectPath;
	FString CurrentProjectName;

	// Document metadata
	FString DocumentPath;

	FString CurrentEncodedThumbnail;

	// Generates a GUID...non-const because all generated guids are stored to avoid (infinitessimal) chance of duplication
	bool MakeNewGUIDForPreset(FBIMPresetInstance& Preset);
	bool DuplicatePreset(UWorld* World, const FGuid& OriginalPreset, FBIMPresetInstance& OutPreset);

	const FBIMPresetCollection& GetPresetCollection() const;
	FBIMPresetCollection& GetPresetCollection();

	bool PresetIsInUse(const FGuid& InPreset) const;
	void DeletePreset(UWorld* World, const FGuid& DeleteGUID, const FGuid& ReplacementGUID = FGuid());

	const FDocumentSettings& GetCurrentSettings() const { return CurrentSettings; }

	// Utilities and constants for allocating pools of object IDs for different users
	// TODO: upgrade all object IDs to 64-bit integers, to allow space for negation (in the graph), user index bits, and enough IDs for large projects.
	static const int32 UserIdxBits;
	static const int32 MaxUserIdx;
	static const int32 LocalObjIDBits;
	static const int32 ObjIDUserMask;
	static const int32 LocalObjIDMask;

	static int32 MPObjIDFromLocalObjID(int32 LocalObjID, int32 UserIdx);
	static void SplitMPObjID(int32 MPObjID, int32& OutLocalObjID, int32& OutUserIdx);

private:
	TSharedPtr<FModumateDraftingView> CurrentDraftingView = nullptr;
	FBIMPresetCollection BIMPresetCollection;
	FDocumentSettings CurrentSettings;
	FString CachedLocalUserID;
	int32 CachedLocalUserIdx = 0;

	// TODO: refactor the different types of dirtiness into flags if we add more with multiplayer, but until then we know there are only two types of dirtiness.
	bool bUserFileDirty = true;
	bool bAutoSaveDirty = true;

	FModumateDocumentHeader CachedHeader;
	FMOIDocumentRecord CachedRecord;

	void UpdateWindowTitle();
	void RecordSavedProject(UWorld* World, const FString& FilePath, bool bUserFile);

public: 

	UFUNCTION()
	void DrawingSendResponse(const FString& FunctionName, const FString& Argument) const;

	UFUNCTION()
	void drawing_request_document();

	UFUNCTION()
	void web_push_document_update();

	UFUNCTION()
	void drawing_apply_delta(const FString& InDelta);

	UFUNCTION()
	void drawing_request_view_list();

	UFUNCTION()
	void drawing_get_drawing_image(const FString& InRequest);

	UFUNCTION()
	void drawing_get_clicked(const FString& InRequest);

	UFUNCTION()
	void set_moi_property(const int32 ID, const FString& Property, const FString& Value);

	UFUNCTION()
	void create_moi(const FString& MOIType);

	UFUNCTION()
	void delete_moi(const FString& IDStr);

	UFUNCTION()
	void set_design_option_visible(int32 ID,bool bVisible);

	UFUNCTION()
	void set_moi_display_name(int32 ID,const FString& Name);

	void UpdateWebMOIs(const EObjectType ObjectType);
};

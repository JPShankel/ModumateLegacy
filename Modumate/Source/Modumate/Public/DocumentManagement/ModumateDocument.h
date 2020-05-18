// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include <functional>
#include "DynamicMeshActor.h"
#include "ModumateSerialization.h"
#include "ModumateObjectDatabase.h"
#include "ModumateCrafting.h"
#include "ModumateGraph.h"
#include "ModumatePresetManager.h"
#include "Graph3D.h"
#include "ModumateObjectInstance.h"
#include "Runtime/Engine/Classes/Debug/ReporterGraph.h"
#include "ModumateCameraView.h"

class AEditModelPlayerState_CPP;

/**
 *
 */


namespace Modumate
{
	class FModumateObjectInstance;
	class FModumateDraftingView;
	class FDelta;

	class MODUMATE_API FModumateDocument
	{
	private:

		struct UndoRedo
		{
			// TODO: if Deltas are universally used and consistently applied, Undo and Redo functions
			// should not be needed anymore
			std::function<void()> Undo, Redo;

			TArray<TSharedPtr<FDelta>> Deltas;
		};

		TArray<UndoRedo*> UndoBuffer, RedoBuffer;
		int32 NextID;

		TArray<FModumateObjectInstance*> ObjectInstanceArray;
		TMap<int32, FModumateObjectInstance*> ObjectsByID;
		TMap<int32, FModumateObjectInstance*> DeletedObjects;

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
		FGraph3D VolumeGraph;

		// Copy of the volume graph to work with multi-stage deltas
		FGraph3D TempVolumeGraph;

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

		// Document modifiers
		int32 CreateFFE(UWorld *world, int32 parentID, const FVector &loc, const FQuat &rot, const FModumateObjectAssembly &obAsm, int32 parentFaceIdx = INDEX_NONE);

		const TArray<FModumateObjectInstance*>& GetObjectInstances() const { return ObjectInstanceArray; }
		TArray<FModumateObjectInstance*>& GetObjectInstances() { return ObjectInstanceArray; }

		bool CleanObjects();
		void RegisterDirtyObject(EObjectDirtyFlags DirtyType, FModumateObjectInstance *DirtyObj, bool bDirty);

		void AddCommandToHistory(const FString &cmd);
		TArray<FString> GetCommandHistory() const;

		void BeginUndoRedoMacro();
		void EndUndoRedoMacro();

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

		int32 MakeFinish(UWorld *world, int32 targetObjectID, int32 faceIndex, const FModumateObjectAssembly &newFinishAssembly);

		void AdjustMoiHoleVerts(int32 id, const FVector &location, const TArray<FVector> &holeVerts);

		int32 MakePortalAt(UWorld *world, EObjectType portalType, int32 parentID, const FVector2D &relativePos, const FQuat &relativeRot, bool inverted, const FModumateObjectAssembly &pal);
		void TransverseObjects(const TArray<FModumateObjectInstance*> &obs);

		int32 CreateLineSegmentObject(UWorld *world, const FVector &p1, const FVector &p2, int32 parentID);

		int32 MakeRoom(UWorld *World, const TArray<FSignedID> &FaceIDs);
		int32 MakePointsObject(UWorld *world, const TArray<int32> &idsToDelete, const TArray<FVector> &points, const TArray<int32> &controlIndices,
			EObjectType objectType, bool inverted, const FModumateObjectAssembly &assembly, int32 parentID, bool bUpdateSiblingGeometry = false);

		bool MakeMetaObject(UWorld *world, const TArray<FVector> &points, const TArray<int32> &IDs, EObjectType objectType, int32 parentID, TArray<int32> &OutObjIDs);

		bool MakeCutPlaneObject(UWorld *world, const TArray<FVector> &points, TArray<int32> &OutObjIDs);
		bool MakeScopeBoxObject(UWorld *world, const TArray<FVector> &points, TArray<int32> &OutObjIDs, const float Height);

		// Allocates IDs for new objects, finds new parent IDs for objects, and marks objects for deletion after another graph operation
		bool FinalizeGraphDelta(FGraph3D &TempGraph, FGraph3DDelta &Delta);

		bool UpdateGeometry(UWorld *world, int32 id, const TArray<FVector> &points, const FVector& extents);
		void UpdateControlIndices(int32 id, const TArray<int32> &indices);
		void UpdateControlValues(int32 id, const TArray<FVector> &controlPoints, const TArray<int32> &controlIndices);
		void Split(int32 id, const TArray<FVector> &pointsA, const TArray<FVector> &pointsB, const TArray<int32> &indicesA, const TArray<int32> &indicesB);
		void UpdateLineSegment(int32 id, const FVector &p1, const FVector &p2);

		bool RotateMetaObjectsAboutOrigin(UWorld *World, const TArray<int32> &ObjectIDs, const FVector &origin, const FQuat &rotation);

		bool MoveMetaObjects(UWorld *World, const TArray<int32> &ObjectIDs, const FVector &Displacement);
		bool MoveMetaVertices(UWorld *World, int32 ObjectID, const FVector &Displacement);
		bool MoveMetaVertices(UWorld *World, const TArray<int32> &VertexIDs, const TArray<FVector> &VertexPositions);

		bool JoinMetaObjects(UWorld *World, const TArray<int32> &ObjectIDs);

		int32 MakeRailSection(UWorld *world, const TArray<int32> &ids, const TArray<FVector> &points, int32 parentID);
		int32 MakeCabinetFrame(UWorld *world, const TArray<FVector> &points, float height, const FModumateObjectAssembly &cal, int32 parentID);

		int32 MakeGroupObject(UWorld *world, const TArray<int32> &ids, bool combineWithExistingGroups, int32 parentID);
		void UnmakeGroupObjects(UWorld *world, const TArray<int32> &groupIds);

		FModumateObjectInstance *ObjectFromActor(AActor *actor);
		const FModumateObjectInstance *ObjectFromActor(const AActor *actor) const;

		void UpdateMitering(UWorld *world, const TArray<int32> &dirtyObjIDs);

		bool CanRoomContainFace(FSignedID FaceID);
		void UpdateRoomAnalysis(UWorld *world);

		const FGraph3D &GetVolumeGraph() const { return VolumeGraph; }

		// TODO: we should remove the TempVolumeGraph entirely if we can rely on inverting deltas,
		// or give graph delta creators their own copy of a graph to modify if we still need to use it for delta creation.
		const FGraph3D &GetTempVolumeGraph() const { return TempVolumeGraph; }
		FGraph3D &GetTempVolumeGraph() { return TempVolumeGraph; }

		int32 CalculatePolyhedra() { return VolumeGraph.CalculatePolyhedra(); }
		bool IsObjectInVolumeGraph(int32 ObjID, EGraph3DObjectType &OutObjType) const;

		TArray<const FModumateObjectInstance*> GetObjectsOfType(EObjectType type) const;
		TArray<FModumateObjectInstance*> GetObjectsOfType(EObjectType type);

		void GetObjectIdsByAssembly(const FName &assemblyKey, TArray<int32> &outIDs) const;

		static const FName DocumentHideRequestTag;
		void AddHideObjectsById(UWorld *world, const TArray<int32> &ids);
		void UnhideAllObjects(UWorld *world);

		bool MoveObjects(UWorld *world, const TArray<int32> &obs, const FVector &v);
		bool MoveObjects(UWorld *world, const TArray<FModumateObjectInstance*> &obs, const FVector &v);
		void RotateObjects(UWorld *world, const TArray<FModumateObjectInstance*> &obs, const FVector &v, const FQuat &q);
		void RotateObjects(UWorld *world, const TArray<int32> &obs, const FVector &v, const FQuat &q);
		bool SetObjectTransforms(UWorld *world, const TArray<int32> &ObjectIDs, const TArray<int32> &ParentIDs, const TArray<FVector> &Positions, const TArray<FQuat> &Rotations);

		// Deletion and restoration functions used internally by undo/redo-aware functions
	private:
		bool DeleteObjectImpl(FModumateObjectInstance *ob, bool keepInDeletedList = true);
		bool RestoreObjectImpl(FModumateObjectInstance *ob);
		bool RestoreChildrenImpl(FModumateObjectInstance *obj);

		FModumateObjectInstance* CreateOrRestoreObjFromAssembly(UWorld *World, const FModumateObjectAssembly &Assembly,
			int32 ID, int32 ParentID = 0, const FVector &Extents = FVector::ZeroVector,
			const TArray<FVector> *CPS = nullptr, const TArray<int32> *CPI = nullptr);

		FModumateObjectInstance* CreateOrRestoreObjFromObjectType(UWorld *World, EObjectType OT,
			int32 ID, int32 ParentID = 0, const FVector &Extents = FVector::ZeroVector,
			const TArray<FVector> *CPS = nullptr, const TArray<int32> *CPI = nullptr);

	public:
		bool ApplyMOIDelta(const FMOIDelta &Delta, UWorld *World);
		void ApplyGraph3DDelta(const FGraph3DDelta &Delta, UWorld *World);
		bool ApplyDeltas(const TArray<TSharedPtr<FDelta>> &Deltas, UWorld *World);
		bool UpdateGraphObjects(UWorld *World);

	private:
		bool FinalizeGraphDeltas(TArray <FGraph3DDelta> &Deltas, TArray<int32> &OutAddedFaceIDs, TArray<int32> &OutAddedVertexIDs, TArray<int32> &OutAddedEdgeIDs);
		bool PostApplyDelta(UWorld *World);

		// Helper function for ObjectFromActor
		FModumateObjectInstance *ObjectFromSingleActor(AActor *actor);

	public:

		FModumateObjectInstance *TryGetDeletedObject(int32 id);

		void DeleteObjects(const TArray<FModumateObjectInstance*> &obs, bool bAllowRoomAnalysis = true, bool bDeleteConnected = true);
		bool InvertObjects(const TArray<FModumateObjectInstance*> &obs);
		void DecomposeObject(UWorld *world, int32 id);
		void RestoreDeletedObjects(const TArray<int32> &ids);
		void DeleteObjects(const TArray<int32> &obIds, bool bAllowRoomAnalysis = true, bool bDeleteConnected = true);

		void MakeNew(UWorld *world);
		bool Save(UWorld *world, const FString &path);
		bool Load(UWorld *world, const FString &path, bool setAsCurrentProject);
		void SetCurrentProjectPath(const FString& currentProjectPath = FString());

		int32 CreateObjectFromRecord(UWorld *world, const FMOIDataRecord &obRec);

		TArray<int32> CloneObjects(UWorld *world, const TArray<int32> &obs, const FTransform& offsetTransform = FTransform::Identity);
		TArray<FModumateObjectInstance *> CloneObjects(UWorld *world, const TArray<FModumateObjectInstance *> &obs, const FTransform& offsetTransform = FTransform::Identity);
		int32 CloneObject(UWorld *world, const FModumateObjectInstance *original);

		bool ExportPDF(UWorld *world, const TCHAR *filepath, const FVector &origin, const FVector &normal);

		void Undo(UWorld *world);
		void Redo(UWorld *world);

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
		TSharedPtr<FModumateDraftingView> CurrentDraftingView = nullptr;
	};
}

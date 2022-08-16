// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MOIState.h"
#include "ToolsAndAdjustments/Common/SelectedObjectToolMixin.h"

class UModumateDocument;

struct FMOIDocumentRecordV5;
using FMOIDocumentRecord = FMOIDocumentRecordV5;

struct FDocumentDelta;
using FDeltaPtr = TSharedPtr<FDocumentDelta>;
struct FMOIDeltaState;
enum class EMOIDeltaType : uint8;

struct FGraph3DDelta;
class FGraph3D;
class AEditModelPlayerController;
class AModumateObjectInstance;

// Helper functions for delta preview operations
class FModumateObjectDeltaStatics
{
public:
	static void GetTransformableIDs(const TArray<int32>& InObjectIDs, UModumateDocument *doc, TSet<int32>& OutTransformableIDs);
	// Get deltas for movement of an object
	static bool MoveTransformableIDs(const TMap<int32, FTransform>& ObjectMovements, UModumateDocument *doc, UWorld *World, bool bIsPreview, const TArray<FDeltaPtr>* AdditionalDeltas = nullptr);

	static void SaveSelection(const TArray<int32>& InObjectIDs, UModumateDocument* doc, FMOIDocumentRecord* OutRecord);

	static bool PasteObjects(const FMOIDocumentRecord* InRecord, const FVector& InOrigin, UModumateDocument* Doc, const AEditModelPlayerController* Controller,
		bool bIsPreview, const TArray<FDeltaPtr>* AdditionalDeltas = nullptr);
	static bool PasteObjectsWithinSurfaceGraph(const FMOIDocumentRecord* InRecord, const FVector& InOrigin, TArray<FDeltaPtr> &OutDeltas, UModumateDocument* doc,
		int32 &nextID, const AEditModelPlayerController* Controller, bool bIsPreview);

	static bool MakeSwapEdgeDetailDeltas(UModumateDocument* Doc, const TArray<uint32>& EdgeIDs, FGuid NewDetailPresetID, TArray<FDeltaPtr>& OutDeltas);

	// Create graph deltas that will create deleted edges & vertices in a new graph with matching IDs.
	static void ConvertGraphDeleteToMove(const TArray<FGraph3DDelta>& GraphDeltas, const FGraph3D* OldGraph, int32& NextID, TArray<FGraph3DDelta>& OutDeltas);
	// Copy oldGraph into active graph and move hosted objects with the copy.
	static void MergeGraphToCurrentGraph(UModumateDocument* Doc, const FGraph3D* OldGraph, int32& NextID, TArray<FDeltaPtr>& OutDeltas, TSet<int32>& OutItemsForSelection);

	// Duplicate a set of groups.
	static void DuplicateGroups(const UModumateDocument* Doc, const TSet<int32>& GroupIDs, int32& NextID, TArray<TPair<FSelectedObjectToolMixin::CopyDeltaType, FDeltaPtr>>& OutDeltas);

	// Transform all positions for groups.
	static void GetDeltasForGroupTransforms(UModumateDocument* Doc, const TMap<int32,FTransform>& OriginalGroupVertexTranslations, const FTransform transform,
		TArray<FDeltaPtr>& OutDeltas);

	// Create deltas for wholesale deletion of graph and its contents.
	static void GetDeltasForGraphDelete(const UModumateDocument* Doc, int32 GraphID, TArray<FDeltaPtr>& OutDeltas);

	static void GetDeltasForFaceSpanMerge(const UModumateDocument* Doc, const TArray<int32>& SpanIDs, TArray<FDeltaPtr>& OutDeltas);
	static void GetDeltasForEdgeSpanMerge(const UModumateDocument* Doc, const TArray<int32>& SpanIDs, TArray<FDeltaPtr>& OutDeltas);
	static void GetDeltasForSpanSplit(UModumateDocument* Doc, const TArray<int32>& SpanIDs, TArray<FDeltaPtr>& OutDeltas, TSet<int32>* OutNewMOIs = nullptr);
	static void GetDeltasForFaceSpanAddRemove(const UModumateDocument* Doc, int32 SpanID, const TArray<int32>& AddFaces, const TArray<int32>& RemoveFaces, TArray<FDeltaPtr>& OutDeltas);
	static void GetDeltasForEdgeSpanAddRemove(const UModumateDocument* Doc, int32 SpanID, const TArray<int32>& AddFaces, const TArray<int32>& RemoveFaces, TArray<FDeltaPtr>& OutDeltas);
	// Create delta to map the graph IDs for span Moi according to CopiedToPastedObjIDs.
	static void GetDeltaForSpanMapping(const AModumateObjectInstance* Moi, const TMap<int32, TArray<int32>>& CopiedToPastedObjIDs, TArray<FDeltaPtr>& OutDeltas);
	
	// InTargetEdgeID are MetaEdges
	// InAssemblyGUID is used for hosted obj creation
	// MOIStateData is state data from tool, that can be used to modify preview or commit delta
	static bool GetEdgeSpanCreationDeltas(const TArray<int32>& InTargetEdgeIDs, int32& InNextID, const FGuid& InAssemblyGUID, FMOIStateData& MOIStateData, TArray<FDeltaPtr>& OutDeltaPtrs, int32& OutNewSpanID, int32& OutNewObjID);
	static bool GetFaceSpanCreationDeltas(const TArray<int32>& InTargetFaceIDs, int32& InNextID, const FGuid& InAssemblyGUID, FMOIStateData& MOIStateData, TArray<FDeltaPtr>& OutDeltaPtrs, int32& OutNewSpanID, int32& OutNewObjID);

	// InAssemblyGUID is used for replacing
	// MOIStateData is state data from tool, that can be used to modify preview or commit delta
	static bool GetEdgeSpanEditAssemblyDeltas(const UModumateDocument* Doc, const int32& InTargetSpanID, int32& InNextID, const FGuid& InAssemblyGUID, FMOIStateData& MOIStateData, TArray<FDeltaPtr>& OutDeltaPtrs, int32& OutNewObjID);
	static bool GetFaceSpanEditAssemblyDeltas(const UModumateDocument* Doc, const int32& InTargetSpanID, int32& InNextID, const FGuid& InAssemblyGUID, FMOIStateData& MOIStateData, TArray<FDeltaPtr>& OutDeltaPtrs, int32& OutNewObjID);

	// Preview delta depends on which creation tool mode
	// InTargetEdgeIDs must be existing MetaEdges
	static bool GetObjectCreationDeltasForEdgeSpans(const UModumateDocument* Doc, EToolCreateObjectMode CreationMode, const TArray<int32>& InTargetEdgeIDs, int32& InNextID, 
		int32 InTargetSpanIndex, const FGuid& InAssemblyGUID, FMOIStateData& MOIStateData, TArray<FDeltaPtr>& OutDeltaPtrs, TArray<int32>& OutNewObjectIDs);
	static bool GetObjectCreationDeltasForFaceSpans(const UModumateDocument* Doc, EToolCreateObjectMode CreationMode, const TArray<int32>& InTargetFaceIDs, int32& InNextID,
		int32 InTargetSpanIndex, const FGuid& InAssemblyGUID, FMOIStateData& MOIStateData, TArray<FDeltaPtr>& OutDeltaPtrs, TArray<int32>& OutNewObjectIDs);
};

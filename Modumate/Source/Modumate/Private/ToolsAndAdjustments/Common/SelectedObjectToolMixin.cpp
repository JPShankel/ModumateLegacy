#include "ToolsAndAdjustments/Common/SelectedObjectToolMixin.h"

#include "DocumentManagement/ModumateDocument.h"
#include "ModumateCore/ModumateObjectDeltaStatics.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"

using namespace Modumate;

const FName FSelectedObjectToolMixin::StateRequestTag(TEXT("SelectedObjectTool"));

FSelectedObjectToolMixin::FSelectedObjectToolMixin(AEditModelPlayerController_CPP *InController)
	: ControllerPtr(InController)
{

}

void FSelectedObjectToolMixin::AcquireSelectedObjects()
{
	if (!ControllerPtr.IsValid())
	{
		return;
	}

	ReleaseSelectedObjects();

	AEditModelPlayerState_CPP *playerState = ControllerPtr->EMPlayerState;

	for (auto obj : playerState->SelectedObjects)
	{
		OriginalSelectedObjects.Add(obj->ID);
	}

	TSet<int32> vertexIDs;
	FModumateObjectDeltaStatics::GetVertexIDs(OriginalSelectedObjects.Array(), ControllerPtr->GetDocument(), vertexIDs);
	auto doc = ControllerPtr->GetDocument();

	for (int32 id : vertexIDs)
	{
		auto obj = doc->GetObjectById(id);
		if (obj == nullptr)
		{
			continue;
		}

		OriginalCornerTransforms.Add(id, obj->GetWorldTransform());
	}

	// TODO: move this kind of logic into (and rename) GetVertexIDs
	// We didn't find any meta objects, so just gather the whole selection list
	if (OriginalCornerTransforms.Num() == 0)
	{
		for (auto &ob : playerState->SelectedObjects)
		{
			auto objectAndDescendents = ob->GetAllDescendents();
			objectAndDescendents.Add(ob);
			for (auto &kid : objectAndDescendents)
			{
				OriginalCornerTransforms.Add(kid->ID, kid->GetWorldTransform());
			}
		}
	}

	for (auto &kvp : OriginalCornerTransforms)
	{
		auto moi = doc->GetObjectById(kvp.Key);
		if (moi == nullptr)
		{
			continue;
		}

		moi->RequestCollisionDisabled(StateRequestTag, true);
		moi->ShowAdjustmentHandles(nullptr, false);
		moi->BeginPreviewOperation();
	}
}

void FSelectedObjectToolMixin::ReleaseSelectedObjects()
{
	auto doc = ControllerPtr->GetDocument();
	if (doc == nullptr)
	{
		return;
	}

	doc->ClearPreviewDeltas(ControllerPtr->GetWorld());

	for (auto &kvp : OriginalCornerTransforms)
	{
		auto moi = doc->GetObjectById(kvp.Key);
		if (moi == nullptr)
		{
			continue;
		}

		moi->RequestCollisionDisabled(StateRequestTag, false);
		moi->EndPreviewOperation();
	}

	OriginalCornerTransforms.Empty();
	OriginalSelectedObjects.Empty();
}

void FSelectedObjectToolMixin::ReleaseObjectsAndApplyDeltas()
{
	FModumateDocument* doc = ControllerPtr->GetDocument();
	UWorld* world = ControllerPtr->GetWorld();
	const FGraph3D& volumeGraph = doc->GetVolumeGraph();

	// For all the acquired targets, collect the previewed changes in a way that can be applied as deltas to the document
	TArray<FModumateObjectInstance*> targetPhysicalMOIs;
	TMap<int32, TMap<int32, FVector2D>> combinedVertex2DMovements;
	TMap<int32, FVector> vertex3DMovements;
	for (auto &kvp : OriginalCornerTransforms)
	{
		FModumateObjectInstance* targetMOI = doc->GetObjectById(kvp.Key);
		int32 targetID = targetMOI->ID;
		int32 targetParentID = targetMOI->GetParentID();
		const TArray<FVector>& targetCPs = targetMOI->GetControlPoints();
		int32 numCPs = targetCPs.Num();
		EObjectType objectType = targetMOI->GetObjectType();
		EGraphObjectType graph2DObjType = UModumateTypeStatics::Graph2DObjectTypeFromObjectType(objectType);
		EGraph3DObjectType graph3DObjType = UModumateTypeStatics::Graph3DObjectTypeFromObjectType(objectType);


		if (graph2DObjType != EGraphObjectType::None)
		{
			// TODO: all of parenting information is gathered here for projecting 3D control points into 2D surface graph positions,
			// which should be unnecessary if surface graph MOIs expose their 2D information directly.
			const FModumateObjectInstance* surfaceObj = doc->GetObjectById(targetParentID);
			const FGraph2D* surfaceGraph = doc->FindSurfaceGraph(targetParentID);
			const FModumateObjectInstance* surfaceParent = surfaceObj ? surfaceObj->GetParentObject() : nullptr;
			if (!ensure(surfaceObj && surfaceGraph && surfaceParent))
			{
				continue;
			}

			int32 surfaceGraphFaceIndex = UModumateObjectStatics::GetParentFaceIndex(surfaceObj);

			TArray<FVector> facePoints;
			FVector faceNormal, faceAxisX, faceAxisY;
			if (!ensure(UModumateObjectStatics::GetGeometryFromFaceIndex(surfaceParent, surfaceGraphFaceIndex, facePoints, faceNormal, faceAxisX, faceAxisY)))
			{
				continue;
			}
			FVector faceOrigin = facePoints[0];

			TMap<int32, FVector2D>& vertex2DMovements = combinedVertex2DMovements.FindOrAdd(targetParentID);
			if (auto graphObject = surfaceGraph->FindObject(targetID))
			{
				TArray<int32> vertexIDs;
				graphObject->GetVertexIDs(vertexIDs);
				int32 numCorners = targetMOI->GetNumCorners();
				if (!ensureAlways(vertexIDs.Num() == numCorners))
				{
					return;
				}
				for (int32 idx = 0; idx < numCorners; idx++)
				{
					vertex2DMovements.Add(vertexIDs[idx], UModumateGeometryStatics::ProjectPoint2D(targetMOI->GetCorner(idx), faceAxisX, faceAxisY, faceOrigin));
				}
			}
		}
		else if (auto graphObject = volumeGraph.FindObject(targetID))
		{
			TArray<int32> vertexIDs;
			graphObject->GetVertexIDs(vertexIDs);
			int32 numCorners = targetMOI->GetNumCorners();
			if (!ensureAlways(vertexIDs.Num() == numCorners))
			{
				return;
			}
			for (int32 idx = 0; idx < numCorners; idx++)
			{
				vertex3DMovements.Add(vertexIDs[idx], targetMOI->GetCorner(idx));
			}
		}
		else
		{
			targetPhysicalMOIs.Add(targetMOI);
		}
	}

	TArray<TSharedPtr<FDelta>> deltas;

	doc->ClearPreviewDeltas(world);
	// First, get deltas for applying volume graph changes as vertex movements
	// TODO: this might be better structured as just a delta collection step, but FinalizeGraphDeltas etc. use the temporary graph,
	// so as long as this function is appropriate in directly applying the deltas, we'll just perform the vertex movement right away.
	if (vertex3DMovements.Num() > 0)
	{
		TArray<int32> vertexMoveIDs;
		TArray<FVector> vertexMovePositions;
		for (auto& kvp : vertex3DMovements)
		{
			vertexMoveIDs.Add(kvp.Key);
			vertexMovePositions.Add(kvp.Value);
		}
		doc->GetVertexMovementDeltas(vertexMoveIDs, vertexMovePositions, deltas);
	}

	// Next, get deltas for surface graph changes
	if (combinedVertex2DMovements.Num() > 0)
	{
		int32 nextID = doc->GetNextAvailableID();
		TArray<FGraph2DDelta> surfaceGraphDeltas;
		for (auto& kvp : combinedVertex2DMovements)
		{
			surfaceGraphDeltas.Reset();
			FGraph2D* surfaceGraph = doc->FindSurfaceGraph(kvp.Key);
			if (!ensure(surfaceGraph) || (kvp.Value.Num() == 0) ||
				!surfaceGraph->MoveVertices(surfaceGraphDeltas, nextID, kvp.Value))
			{
				continue;
			}

			for (auto& delta : surfaceGraphDeltas)
			{
				deltas.Add(MakeShareable(new FGraph2DDelta{ delta }));
			}
		}
	}

	// Next, get deltas for physical MOI movements as regular state data changes with an FMOIDelta
	if (targetPhysicalMOIs.Num() > 0)
	{
		deltas.Add(MakeShareable(new FMOIDelta(targetPhysicalMOIs)));
	}

	// Release the acquired objects
	for (auto &kvp : OriginalCornerTransforms)
	{
		auto moi = doc->GetObjectById(kvp.Key);
		if (moi == nullptr)
		{
			continue;
		}

		moi->RequestCollisionDisabled(StateRequestTag, false);
		moi->EndPreviewOperation();
	}

	// And finally, apply the deltas now that the objects are no longer being previewed and the desired changes have been captured as deltas.
	doc->ApplyDeltas(deltas, world);

	OriginalCornerTransforms.Empty();
	OriginalSelectedObjects.Empty();
}

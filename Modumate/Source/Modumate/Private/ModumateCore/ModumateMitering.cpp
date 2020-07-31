#include "ModumateCore/ModumateMitering.h"

#include "DocumentManagement/ModumateDocument.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "DocumentManagement/ModumateMiterNodeInterface.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "DocumentManagement/ModumateObjectInstance.h"
#include "DrawDebugHelpers.h"

namespace Modumate
{
	namespace Mitering
	{
		const FModumateObjectInstance *FMiterHelpers::GetChildLayeredObj(const FModumateObjectInstance *PlaneObj)
		{
			const FModumateDocument *doc = PlaneObj ? PlaneObj->GetDocument() : nullptr;
			if (!ensure(doc != nullptr))
			{
				return nullptr;
			}

			const TArray<int32> &childIDs = PlaneObj->GetChildIDs();
			if (childIDs.Num() != 1)
			{
				return nullptr;
			}

			// The layered interface is intended as a condition and requirement for mitering,
			// even if we aren't fully taking advantage of its cached layer information yet.
			const FModumateObjectInstance *childObj = doc->GetObjectById(childIDs[0]);
			if (childObj && childObj->GetLayeredInterface())
			{
				return childObj;
			}

			return nullptr;
		}

		bool FMiterHelpers::UpdateMiteredLayerGeoms(const FModumateObjectInstance *PlaneHostedObj, const FGraph3DFace *PlaneFace,
			const TArray<FPolyHole3D> *Holes, TArray<FLayerGeomDef> &OutLayerGeometries)
		{
			const FModumateObjectInstance *parentPlane = PlaneHostedObj ? PlaneHostedObj->GetParentObject() : nullptr;
			if (!ensureAlways(PlaneHostedObj && parentPlane && PlaneFace))
			{
				return false;
			}

			FVector curStartDelta, curEndDelta;
			UModumateObjectStatics::GetExtrusionDeltas(PlaneHostedObj, curStartDelta, curEndDelta);

			// TODO: we wish we could read from the graph, but the tools modify control points rather than the graph contents (for good reason)
			const TArray<FVector> &planePoints = parentPlane->GetControlPoints();// PlaneFace->CachedPositions;
			int32 numPoints = planePoints.Num();

			// If we can't use the plane face's edge normals, then we won't be able to determine any mitering.
			if (!ensure((PlaneFace->CachedEdgeNormals.Num() == numPoints) && (PlaneFace->EdgeIDs.Num() == numPoints)))
			{
				return false;
			}

			const auto &layers = PlaneHostedObj->GetAssembly().CachedAssembly.Layers;
			int32 numLayers = layers.Num();
			OutLayerGeometries.SetNum(numLayers);

			const FModumateDocument *doc = PlaneHostedObj->GetDocument();

			// Collate the edge extensions by layer, so that we can build each layer's geometry independently.
			// Sourcing them from the miter node interface allows separate miter logic to determine
			// how layers can be extended independently from each other.
			TArray<TArray<FVector2D>> edgeExtensionsByLayer;
			edgeExtensionsByLayer.SetNum(numLayers);
			for (int32 layerIdx = 0; layerIdx < numLayers; ++layerIdx)
			{
				edgeExtensionsByLayer[layerIdx].SetNumZeroed(numPoints);
			}

			for (int32 edgeIdx = 0; edgeIdx < numPoints; ++edgeIdx)
			{
				FGraphSignedID edgeID = PlaneFace->EdgeIDs[edgeIdx];
				const FModumateObjectInstance* edgeObj = doc->GetObjectById(FMath::Abs(edgeID));
				const IMiterNode *miterNode = edgeObj ? edgeObj->GetMiterInterface() : nullptr;
				if (!ensure(miterNode))
				{
					return false;
				}

				const FMiterData &miterData = miterNode->GetMiterData();
				const FMiterParticipantData *participantData = miterData.ParticipantsByID.Find(PlaneHostedObj->ID);
				if (!ensure(participantData))
				{
					return false;
				}

				for (int32 layerIdx = 0; layerIdx < numLayers; ++layerIdx)
				{
					edgeExtensionsByLayer[layerIdx][edgeIdx] = participantData->LayerExtensions[layerIdx];
				}
			}

			float curThickness = 0.0f;
			float totalThickness = PlaneHostedObj->CalculateThickness();
			FVector planeNormal = parentPlane->GetNormal();
			FVector planeCenter = parentPlane->GetObjectLocation();
			FVector layerAxisX = parentPlane->GetObjectRotation().GetAxisX();

			// For each layer, generate points that interpolate between the extension values of the two outermost sides of the
			// object, that take both adjacent edge's retractions into account.
			TArray<FVector> layerPointsA, layerPointsB;
			for (int32 layerIdx = 0; layerIdx < numLayers; ++layerIdx)
			{
				const FModumateObjectAssemblyLayer &layer = layers[layerIdx];
				float layerStartPCT = curThickness / totalThickness;
				float layerThickness = layer.Thickness.AsWorldCentimeters();
				curThickness += layerThickness;
				float layerEndPCT = curThickness / totalThickness;

				FVector layerStartDelta = FMath::Lerp(curStartDelta, curEndDelta, layerStartPCT);
				FVector layerEndDelta = FMath::Lerp(curStartDelta, curEndDelta, layerEndPCT);

				TArray<FVector2D> &edgeExtensions = edgeExtensionsByLayer[layerIdx];

				// For each point, compute its new position based on the extensions of its adjacent edges,
				// adjusted for how far through the layered assembly it is, and relative to the plane's center.
				layerPointsA = planePoints;
				layerPointsB = planePoints;
				for (int32 pointIdx = 0; pointIdx < numPoints; ++pointIdx)
				{
					int32 prevPointIdx = (pointIdx - 1 + numPoints) % numPoints;
					int32 nextPointIdx = (pointIdx + 1) % numPoints;

					int32 prevEdgeIdx = prevPointIdx;
					int32 nextEdgeIdx = pointIdx;

					const FVector2D &prevEdgeExtensions = edgeExtensions[prevEdgeIdx];
					const FVector2D &nextEdgeExtensions = edgeExtensions[nextEdgeIdx];

					const FVector &prevPoint = planePoints[prevPointIdx];
					const FVector &nextPoint = planePoints[nextPointIdx];

					const FVector &prevEdgeNormal = PlaneFace->CachedEdgeNormals[prevEdgeIdx];
					const FVector &nextEdgeNormal = PlaneFace->CachedEdgeNormals[nextEdgeIdx];

					UModumateGeometryStatics::GetExtendedCorner(layerPointsA[pointIdx], prevPoint, nextPoint,
						prevEdgeNormal, nextEdgeNormal, prevEdgeExtensions.X, nextEdgeExtensions.X);
					layerPointsA[pointIdx] += layerStartDelta - planeCenter;

					UModumateGeometryStatics::GetExtendedCorner(layerPointsB[pointIdx], prevPoint, nextPoint,
						prevEdgeNormal, nextEdgeNormal, prevEdgeExtensions.Y, nextEdgeExtensions.Y);
					layerPointsB[pointIdx] += layerEndDelta - planeCenter;
				}

				FLayerGeomDef &layerGeomDef = OutLayerGeometries[layerIdx];
				layerGeomDef.Init(layerPointsA, layerPointsB, planeNormal, layerAxisX, Holes);
			}

			return true;
		}
	}
}

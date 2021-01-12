#include "ModumateCore/ModumateMitering.h"

#include "DocumentManagement/ModumateDocument.h"
#include "ModumateCore/LayerGeomDef.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "Objects/MiterNode.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "Objects/ModumateObjectInstance.h"
#include "DrawDebugHelpers.h"

namespace Modumate
{
	namespace Mitering
	{
		const AModumateObjectInstance *FMiterHelpers::GetChildLayeredObj(const AModumateObjectInstance *PlaneObj)
		{
			const UModumateDocument* doc = PlaneObj ? PlaneObj->GetDocument() : nullptr;
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
			const AModumateObjectInstance *childObj = doc->GetObjectById(childIDs[0]);
			if (childObj && childObj->GetLayeredInterface())
			{
				return childObj;
			}

			return nullptr;
		}

		bool FMiterHelpers::UpdateMiteredLayerGeoms(const AModumateObjectInstance *PlaneHostedObj, const FGraph3DFace *PlaneFace,
			const TArray<FPolyHole3D> *Holes, TArray<FLayerGeomDef> &OutLayerGeometries)
		{
			const AModumateObjectInstance *parentPlane = PlaneHostedObj ? PlaneHostedObj->GetParentObject() : nullptr;
			if (!ensureAlways(PlaneHostedObj && parentPlane && PlaneFace))
			{
				return false;
			}

			FVector curStartDelta, curEndDelta;
			UModumateObjectStatics::GetExtrusionDeltas(PlaneHostedObj, curStartDelta, curEndDelta);

			const TArray<FVector> &planePoints = PlaneFace->CachedPositions;
			int32 numPoints = planePoints.Num();

			// If we can't use the plane face's edge normals, then we won't be able to determine any mitering.
			if (!ensure((PlaneFace->CachedEdgeNormals.Num() == numPoints) && (PlaneFace->EdgeIDs.Num() == numPoints)))
			{
				return false;
			}

			const auto& layers = PlaneHostedObj->GetAssembly().Layers;
			int32 numLayers = layers.Num();
			OutLayerGeometries.SetNum(numLayers);

			const UModumateDocument* doc = PlaneHostedObj->GetDocument();

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
				const AModumateObjectInstance* edgeObj = doc->GetObjectById(FMath::Abs(edgeID));
				const IMiterNode *miterNode = edgeObj ? edgeObj->GetMiterInterface() : nullptr;
				if (!ensure(miterNode))
				{
					return false;
				}

				const FMiterData& miterData = miterNode->GetMiterData();
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
			FVector planeCenter = parentPlane->GetLocation();
			FVector layerAxisX = parentPlane->GetRotation().GetAxisX();

			// For each layer, generate points that interpolate between the extension values of the two outermost sides of the
			// object, that take both adjacent edge's retractions into account.
			bool bAllLayersValid = true;
			TArray<FVector> layerPointsA, layerPointsB;
			for (int32 layerIdx = 0; layerIdx < numLayers; ++layerIdx)
			{
				const FBIMLayerSpec& layer = layers[layerIdx];
				float layerStartPCT = curThickness / totalThickness;
				float layerThickness = layer.ThicknessCentimeters;
				curThickness += layerThickness;
				float layerEndPCT = curThickness / totalThickness;

				FVector layerStartDelta = FMath::Lerp(curStartDelta, curEndDelta, layerStartPCT);
				FVector layerEndDelta = FMath::Lerp(curStartDelta, curEndDelta, layerEndPCT);

				const TArray<FVector2D>& edgeExtensions = edgeExtensionsByLayer[layerIdx];

				// For each point in the plane, calculate up to 3 distinct points, based on the extensions of its own edge, the previous edge, and the next edge.
				// 0-length extensions in all edges will result in 3 identical points, but for consistency with the worst-case number of points, they are all output.
				// Extending an edge will by default add points that are the intersections of the adjacent extended edges,
				// but only in the case of parallel adjacent edges, it will create two additional edges, from the unextended points to the new extended points.
				// These "stitch" faces are necessary to capture details like a wall tunneling through a floor.
				// However, if retracting an edge will cause it to intersect with the previous or next edge, then these "stitch" face edges would create self-intersecting polygons,
				// so the current fallback is to merge the 3 degenerate points at the intersection.
				// TODO: remove redundant calculations of intersection points, and simplify point calculation in cases of 0-length extensions.
				layerPointsA.Reset();
				layerPointsB.Reset();
				for (int32 pointIdx = 0; pointIdx < numPoints; ++pointIdx)
				{
					// Calculate maximum possible useful attributes of the current, previous, and next edges that are consistent for both sides of a layer.
					int32 prevPointIdx = (pointIdx - 1 + numPoints) % numPoints;
					int32 nextPointIdx = (pointIdx + 1) % numPoints;
					int32 nextNextPointIdx = (pointIdx + 2) % numPoints;

					int32 prevEdgeIdx = prevPointIdx;
					int32 curEdgeIdx = pointIdx;
					int32 nextEdgeIdx = nextPointIdx;

					const FVector& prevPoint = planePoints[prevPointIdx];
					const FVector& curPoint = planePoints[pointIdx];
					const FVector& nextPoint = planePoints[nextPointIdx];
					const FVector& nextNextPoint = planePoints[nextNextPointIdx];

					const FVector2D& prevEdgeExtensions = edgeExtensions[prevEdgeIdx];
					const FVector2D& curEdgeExtensions = edgeExtensions[curEdgeIdx];
					const FVector2D& nextEdgeExtensions = edgeExtensions[nextEdgeIdx];

					const FVector& prevEdgeNormal = PlaneFace->CachedEdgeNormals[prevEdgeIdx];
					const FVector& curEdgeNormal = PlaneFace->CachedEdgeNormals[curEdgeIdx];
					const FVector& nextEdgeNormal = PlaneFace->CachedEdgeNormals[nextEdgeIdx];

					const FVector prevEdgeDir = (curPoint - prevPoint).GetSafeNormal();
					const FVector curEdgeDir = (nextPoint - curPoint).GetSafeNormal();
					const FVector nextEdgeDir = (nextNextPoint - nextPoint).GetSafeNormal();

					bool bSplitCurVertex = FVector::Parallel(prevEdgeDir, curEdgeDir);
					bool bSplitNextVertex = FVector::Parallel(curEdgeDir, nextEdgeDir);

					// For each side of a layer, compute stitch face points independently, based on intersecting with adjacent extended edges.
					TArray<FVector>* sideLayerPoints[2] = { &layerPointsA, &layerPointsB };
					const FVector sideLayerDeltas[2] = { layerStartDelta, layerEndDelta };
					for (int32 sideIdx = 0; sideIdx < 2; ++sideIdx)
					{
						TArray<FVector>* layerPoints = sideLayerPoints[sideIdx];
						const FVector& layerDelta = sideLayerDeltas[sideIdx];
						float prevEdgeExtension = prevEdgeExtensions[sideIdx];
						float curEdgeExtension = curEdgeExtensions[sideIdx];
						float nextEdgeExtension = nextEdgeExtensions[sideIdx];

						const FVector prevSidePoint = prevPoint + layerDelta - planeCenter;
						const FVector curSidePoint = curPoint + layerDelta - planeCenter;
						const FVector nextSidePoint = nextPoint + layerDelta - planeCenter;
						const FVector nextNextSidePoint = nextNextPoint + layerDelta - planeCenter;

						const FVector prevExtPoint = prevSidePoint - prevEdgeExtension * prevEdgeNormal;
						const FVector curExtPoint = curSidePoint - curEdgeExtension * curEdgeNormal;
						const FVector nextExtPoint = nextSidePoint - curEdgeExtension * curEdgeNormal;
						const FVector nextNextExtPoint = nextNextSidePoint - nextEdgeExtension * nextEdgeNormal;

						FVector2D intersectionDists;
						FVector intersectionPoint;

						// Early-exit if there are no extensions to compute; the projected planar points will be the final layer points.
						if (FMath::IsNearlyZero(prevEdgeExtension) && FMath::IsNearlyZero(curEdgeExtension))
						{
							layerPoints->Add(curSidePoint);
							layerPoints->Add(curSidePoint);
						}
						// If we're splitting the current vertex, then make sure that the extension/retraction won't collide with adjacent edges
						// in a way that creates a self-intersecting polygon.
						else if (bSplitCurVertex)
						{
							if (UModumateGeometryStatics::RayIntersection3D(
								curExtPoint, curEdgeDir,
								prevExtPoint, prevEdgeDir,
								intersectionPoint, intersectionDists.X, intersectionDists.Y))
							{
								layerPoints->Add(intersectionPoint);
								layerPoints->Add(intersectionPoint);
							}
							else
							{
								layerPoints->Add(curSidePoint);
								layerPoints->Add(curExtPoint);
							}
						}
						// If we're not splitting the current vertex, then only use the adjacent extended edges to compute the resulting layer point, if possible.
						// TODO: this result can be different then the above ray intersection, but only in problematic edge cases that might be able to be handled more simply.
						else
						{
							if (UModumateGeometryStatics::RayIntersection3D(
								prevExtPoint, prevEdgeDir,
								nextExtPoint, -curEdgeDir,
								intersectionPoint, intersectionDists.X, intersectionDists.Y))
							{
								layerPoints->Add(intersectionPoint);
								layerPoints->Add(intersectionPoint);
							}
							else
							{
								layerPoints->Add(curSidePoint);
								layerPoints->Add(curSidePoint);
							}
						}

						// Early-exit if there are no extensions to compute; the projected planar points will be the final layer points.
						if (FMath::IsNearlyZero(curEdgeExtension) && FMath::IsNearlyZero(nextEdgeExtension))
						{
							layerPoints->Add(nextSidePoint);
						}
						// If we're splitting the next vertex, then make sure that the extension/retraction won't collide with adjacent edges
						// in a way that creates a self-intersecting polygon.
						else if (bSplitNextVertex)
						{
							if (UModumateGeometryStatics::RayIntersection3D(
								nextExtPoint, -curEdgeDir,
								nextNextExtPoint, -nextEdgeDir,
								intersectionPoint, intersectionDists.X, intersectionDists.Y))
							{
								layerPoints->Add(intersectionPoint);
							}
							else
							{
								layerPoints->Add(nextExtPoint);
							}
						}
						// If we're not splitting the next vertex, then only use the adjacent extended edges to compute the resulting layer point, if possible.
						// TODO: this result can be different then the above ray intersection, but only in problematic edge cases that might be able to be handled more simply.
						else
						{
							if (UModumateGeometryStatics::RayIntersection3D(
								curExtPoint, curEdgeDir,
								nextNextExtPoint, -nextEdgeDir,
								intersectionPoint, intersectionDists.X, intersectionDists.Y))
							{
								layerPoints->Add(intersectionPoint);
							}
							else
							{
								layerPoints->Add(nextSidePoint);
							}
						}
					}
				}

				FLayerGeomDef& layerGeomDef = OutLayerGeometries[layerIdx];
				layerGeomDef.Init(layerPointsA, layerPointsB, planeNormal, layerAxisX, Holes, true);
				bAllLayersValid &= layerGeomDef.bValid;
			}

			return bAllLayersValid;
		}
	}
}

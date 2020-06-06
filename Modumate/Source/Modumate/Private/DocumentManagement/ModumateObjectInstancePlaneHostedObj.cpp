// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/ModumateObjectInstancePlaneHostedObj.h"

#include "UnrealClasses/AdjustmentHandleActor_CPP.h"
#include "ToolsAndAdjustments/Handles/EditModelPortalAdjustmentHandles.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "ToolsAndAdjustments/Common/EditModelPolyAdjustmentHandles.h"
#include "Graph/Graph3D.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Drafting/ModumateDraftingElements.h"
#include "ModumateCore/ModumateMitering.h"
#include "DocumentManagement/ModumateObjectInstance.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "Algo/Transform.h"


class AEditModelPlayerController_CPP;

namespace Modumate
{
	using namespace Mitering;

	FMOIPlaneHostedObjImpl::FMOIPlaneHostedObjImpl(FModumateObjectInstance *InMOI)
		: FDynamicModumateObjectInstanceImpl(InMOI)
	{
		CachedLayerDims.UpdateLayersFromAssembly(MOI->GetAssembly());
		CachedLayerDims.UpdateFinishFromObject(MOI);
	}

	FMOIPlaneHostedObjImpl::~FMOIPlaneHostedObjImpl()
	{
	}

	FQuat FMOIPlaneHostedObjImpl::GetRotation() const
	{
		const FModumateObjectInstance *parent = MOI->GetParentObject();
		if (ensure(parent && (parent->GetObjectType() == EObjectType::OTMetaPlane)))
		{
			return parent->GetObjectRotation();
		}
		else
		{
			return FQuat::Identity;
		}
	}

	FVector FMOIPlaneHostedObjImpl::GetLocation() const
	{
		const FModumateObjectInstance *parent = MOI->GetParentObject();
		if (ensure(parent && (parent->GetObjectType() == EObjectType::OTMetaPlane)))
		{
			float thickness = MOI->CalculateThickness();
			FVector planeNormal = parent->GetNormal();
			FVector planeLocation = parent->GetObjectLocation();
			float offsetPct = MOI->GetExtents().X;
			return planeLocation + ((0.5f - offsetPct) * thickness * planeNormal);
		}
		else
		{
			return FVector::ZeroVector;
		}
	}

	FVector FMOIPlaneHostedObjImpl::GetCorner(int32 CornerIndex) const
	{
		// Handle the meta plane host case which just returns the meta plane with this MOI's offset
		const FModumateObjectInstance *parent = MOI->GetParentObject();
		if (ensure(parent && (parent->GetObjectType() == EObjectType::OTMetaPlane)))
		{
			int32 numPlanePoints = parent->GetControlPoints().Num();
			bool bOnStartingSide = (CornerIndex < numPlanePoints);
			int32 numLayers = LayerGeometries.Num();
			int32 pointIndex = CornerIndex % numPlanePoints;

			if (ensure((numLayers == MOI->GetAssembly().Layers.Num()) && numLayers > 0))
			{
				const FVector &layerGeomPoint = bOnStartingSide ?
					LayerGeometries[0].PointsA[pointIndex] :
					LayerGeometries[numLayers - 1].PointsB[pointIndex];
				return parent->GetObjectLocation() + layerGeomPoint;
			}
		}

		return GetLocation();
	}

	FVector FMOIPlaneHostedObjImpl::GetNormal() const
	{
		const FModumateObjectInstance *planeParent = MOI->GetParentObject();
		if (ensureAlways(planeParent && (planeParent->GetObjectType() == EObjectType::OTMetaPlane)))
		{
			return planeParent->GetNormal();
		}
		else
		{
			return FVector::ZeroVector;
		}
	}

	bool FMOIPlaneHostedObjImpl::CleanObject(EObjectDirtyFlags DirtyFlag)
	{
		switch (DirtyFlag)
		{
		case EObjectDirtyFlags::Structure:
		{
			// TODO: as long as the assembly is not stored inside of the data state, and its layers can be reversed,
			// then this is the centralized opportunity to match up the reversal of layers with whatever the intended inversion state is,
			// based on preview/current state changing, assembly changing, object creation, etc.
			MOI->SetAssemblyLayersReversed(MOI->GetObjectInverted());

			CachedLayerDims.UpdateLayersFromAssembly(MOI->GetAssembly());
			CachedLayerDims.UpdateFinishFromObject(MOI);

			// When structure (assembly, offset, or plane structure) changes, mark neighboring
			// edges as miter-dirty, so they can re-evaluate mitering with our new structure.
			UpdateConnectedEdges();

			for (FModumateObjectInstance *connectedEdge : CachedConnectedEdges)
			{
				connectedEdge->MarkDirty(EObjectDirtyFlags::Mitering);
			}
		}
		break;
		case EObjectDirtyFlags::Mitering:
		{
			// Make sure all connected edges have resolved mitering,
			// so we can generate our own mitered mesh correctly.
			for (FModumateObjectInstance *connectedEdge : CachedConnectedEdges)
			{
				if (connectedEdge->IsDirty(EObjectDirtyFlags::Mitering))
				{
					return false;
				}
			}

			SetupDynamicGeometry();
		}
		break;
		case EObjectDirtyFlags::Visuals:
		{
			MOI->UpdateVisibilityAndCollision();
			UpdateMeshWithLayers(false, false);
		}
		break;
		}

		return true;
	}

	void FMOIPlaneHostedObjImpl::SetupDynamicGeometry()
	{
		GotGeometry = true;
		DynamicMeshActor->HoleActors.Reset();

		bGeometryDirty = true;
		InternalUpdateGeometry();
	}

	void FMOIPlaneHostedObjImpl::UpdateDynamicGeometry()
	{
		if (!GotGeometry) return;

		// If our parent metaplane is being adjusted, then just update our own mesh to match rather than derive points from the doc graph
		const FModumateObjectInstance *parent = MOI->GetParentObject();
		if (ensure(parent != nullptr) && parent->GetIsInPreviewMode())
		{
			UpdateMeshWithLayers(false, true);
		}
		else
		{
			InternalUpdateGeometry();
		}
	}

	void FMOIPlaneHostedObjImpl::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
	{
		int32 numCP = MOI->GetControlPoints().Num();
		const FModumateObjectInstance *parent = MOI->GetParentObject();

		if (ensure(parent && (parent->GetObjectType() == EObjectType::OTMetaPlane) && (numCP == 0)))
		{
			numCP = parent->GetControlPoints().Num();

			for (int32 i = 0; i < numCP; ++i)
			{
				int32 edgeIdxA = i;
				int32 edgeIdxB = (i + 1) % numCP;

				FVector cornerMinA = GetCorner(edgeIdxA);
				FVector cornerMinB = GetCorner(edgeIdxB);
				FVector edgeDir = (cornerMinB - cornerMinA).GetSafeNormal();

				FVector cornerMaxA = GetCorner(edgeIdxA + numCP);
				FVector cornerMaxB = GetCorner(edgeIdxB + numCP);

				outPoints.Add(FStructurePoint(cornerMinA, edgeDir, edgeIdxA));

				outLines.Add(FStructureLine(cornerMinA, cornerMinB, edgeIdxA, edgeIdxB));
				outLines.Add(FStructureLine(cornerMaxA, cornerMaxB, edgeIdxA + numCP, edgeIdxB + numCP));
				outLines.Add(FStructureLine(cornerMinA, cornerMaxA, edgeIdxA, edgeIdxA + numCP));
			}
		}
	}

	void FMOIPlaneHostedObjImpl::InternalUpdateGeometry()
	{
		// TODO: make sure children are portals

		FModumateObjectInstance *parent = MOI->GetParentObject();
		int32 numCP = MOI->GetControlPoints().Num();
		if (!(parent && (parent->GetObjectType() == EObjectType::OTMetaPlane) && (numCP == 0)))
		{
			return;
		}

		TArray<FModumateObjectInstance*> children = MOI->GetChildObjects();
		bool didChange = (children.Num() != DynamicMeshActor->HoleActors.Num()) || bGeometryDirty;

		if (!didChange)
		{
			for (auto &child : children)
			{
				if (!DynamicMeshActor->HoleActors.Contains(child->GetActor()))
				{
					didChange = true;
					break;
				}
			}
		}

		if (!didChange)
		{
			TArray<const AActor*> childActors;
			Algo::Transform(children,childActors,[](const FModumateObjectInstance *ob){return ob->GetActor();});

			for (auto &hole : DynamicMeshActor->HoleActors)
			{
				if (!childActors.Contains(hole))
				{
					didChange = true;
					break;
				}
			}
		}

		if (didChange || (DynamicMeshActor->PreviewHoleActors.Num() > 0))
		{
			DynamicMeshActor->HoleActors.Reset();
			for (auto *child : children)
			{
				if ((child->GetObjectType() == EObjectType::OTDoor) || (child->GetObjectType() == EObjectType::OTWindow))
				{
					DynamicMeshActor->HoleActors.Add(child->GetActor());
				}
			}
			DynamicMeshActor->HoleActors.Append(DynamicMeshActor->PreviewHoleActors);

			UpdateMeshWithLayers(false, true);

			for (auto *child : children)
			{
				if (child->GetObjectType() == EObjectType::OTFinish)
				{
					if (auto *childMeshActor = Cast<ADynamicMeshActor>(child->GetActor()))
					{
						childMeshActor->HoleActors = DynamicMeshActor->HoleActors;
					}
				}

				child->MarkDirty(EObjectDirtyFlags::Structure);
			}

			CachedLayerDims.UpdateFinishFromObject(MOI);
		}
		else
		{
			UpdateMeshWithLayers(false, true);
		}

		bGeometryDirty = false;
	}

	void FMOIPlaneHostedObjImpl::SetupAdjustmentHandles(AEditModelPlayerController_CPP *controller)
	{
		FModumateObjectInstance *parent = MOI->GetParentObject();
		if (!ensureAlways(parent && (parent->GetObjectType() == EObjectType::OTMetaPlane)))
		{
			return;
		}

		if (AdjustmentHandles.Num() > 0)
		{
			return;
		}

		auto makeActor = [this, controller](FEditModelAdjustmentHandleBase *impl, UStaticMesh *mesh, const FVector &s, const int32& side, const TArray<int32>& CP, float offsetDist)
		{
			if (!ensureAlways(World.IsValid() && DynamicMeshActor.IsValid()))
			{
				return impl;
			}

			AAdjustmentHandleActor_CPP *actor = World->SpawnActor<AAdjustmentHandleActor_CPP>(AAdjustmentHandleActor_CPP::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
			actor->SetActorMesh(mesh);
			actor->SetHandleScaleScreenSize(s);
			if (CP.Num() > 0)
			{
				actor->SetPolyHandleSide(CP, impl->MOI, offsetDist);
			}
			else if (side >= 0)
			{
				actor->SetWallHandleSide(side, impl->MOI, offsetDist);
			}

			actor->bIsPointAdjuster = false;

			impl->Handle = actor;
			impl->Controller = controller;
			actor->Implementation = impl;
			actor->AttachToActor(DynamicMeshActor.Get(), FAttachmentTransformRules::KeepRelativeTransform);
			AdjustmentHandles.Add(actor);
			return impl;
		};

		UStaticMesh *faceAdjusterMesh = controller->EMPlayerState->GetEditModelGameMode()->FaceAdjusterMesh;

		int32 numParentCPs = parent->GetControlPoints().Num();
		for (int32 i = 0; i < numParentCPs; ++i)
		{
			makeActor(new FAdjustPolyPointHandle(parent, i, (i + 1) % numParentCPs), faceAdjusterMesh, FVector(0.0015f, 0.0015f, 0.0015f), -1, TArray<int32>{i, (i + 1) % numParentCPs}, 16.0f);
		}

		// Make justification handles
		auto makeJusticationHandleActor = [this, controller](FJustificationHandle *impl, UStaticMesh *mesh, const FVector &size, float layerPercentage)
		{
			if (!ensureAlways(World.IsValid() && DynamicMeshActor.IsValid()))
			{
				return impl;
			}

			AAdjustmentHandleActor_CPP *actor = World->SpawnActor<AAdjustmentHandleActor_CPP>(AAdjustmentHandleActor_CPP::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
			actor->SetActorMesh(mesh);
			actor->SetHandleScaleScreenSize(size);
			actor->HandleType = EHandleType::Justification;
			actor->ParentMOI = MOI;
			actor->Controller = controller;

			impl->Handle = actor;
			actor->Implementation = impl;
			impl->Controller = controller;
			impl->LayerPercentage = layerPercentage;
			actor->AttachToActor(DynamicMeshActor.Get(), FAttachmentTransformRules::KeepRelativeTransform);
			AdjustmentHandles.Add(actor);

			return impl;
		};

		AEditModelGameMode_CPP *gameMode = controller->EMPlayerState->GetEditModelGameMode();
		static const FVector justificationHandleChildSize(0.0008f);
		static const FVector justificationHandleBaseSize(0.003f);

		// Make the two justification handles for setting the plane hosted object on either side of the plane
		FJustificationHandle *frontJustificationHandle = makeJusticationHandleActor(new FJustificationHandle(MOI), gameMode->JustificationHandleChildMesh, justificationHandleChildSize, 0.f);
		FJustificationHandle *backJustificationHandle = makeJusticationHandleActor(new FJustificationHandle(MOI), gameMode->JustificationHandleChildMesh, justificationHandleChildSize, 1.f);
		TArray<AAdjustmentHandleActor_CPP*> newHandleChildren;
		newHandleChildren.Add(frontJustificationHandle->Handle.Get());
		newHandleChildren.Add(backJustificationHandle->Handle.Get());

		// Make the invert handle, as a part of the justification handle
		UStaticMesh *invertHandleMesh = gameMode->InvertHandleMesh;
		auto *invertHandle = makeActor(new FAdjustInvertHandle(MOI), invertHandleMesh, FVector(0.003f, 0.003f, 0.003f), 4, TArray<int32>(), 0.0f);
		if (ensureAlways(invertHandle))
		{
			newHandleChildren.Add(invertHandle->Handle.Get());
		}

		FJustificationHandle* justificationBase = makeJusticationHandleActor(new FJustificationHandle(MOI), gameMode->JustificationHandleMesh, justificationHandleBaseSize, 0.5f);
		justificationBase->Handle->HandleChildren = newHandleChildren;
		for (auto& child : newHandleChildren)
		{
			child->HandleParent = justificationBase->Handle.Get();
		}
	}

	void FMOIPlaneHostedObjImpl::ClearAdjustmentHandles(AEditModelPlayerController_CPP *controller)
	{
		if (FModumateObjectInstance *parentObj = MOI ? MOI->GetParentObject() : nullptr)
		{
			parentObj->ClearAdjustmentHandles(controller);
		}

		FDynamicModumateObjectInstanceImpl::ClearAdjustmentHandles(controller);
	}

	void FMOIPlaneHostedObjImpl::ShowAdjustmentHandles(AEditModelPlayerController_CPP *controller, bool show)
	{
		if (FModumateObjectInstance *parentObj = MOI ? MOI->GetParentObject() : nullptr)
		{
			parentObj->ShowAdjustmentHandles(controller, show);
		}

		FDynamicModumateObjectInstanceImpl::ShowAdjustmentHandles(controller, show);
	}

	void FMOIPlaneHostedObjImpl::OnSelected(bool bNewSelected)
	{
		FDynamicModumateObjectInstanceImpl::OnSelected(bNewSelected);

		if (const FModumateObjectInstance *parent = MOI ? MOI->GetParentObject() : nullptr)
		{
			TArray<FModumateObjectInstance *> connectedMOIs;
			parent->GetConnectedMOIs(connectedMOIs);
			for (FModumateObjectInstance *connectedMOI : connectedMOIs)
			{
				connectedMOI->UpdateVisibilityAndCollision();
			}
		}
	}

	void FMOIPlaneHostedObjImpl::GetPlaneIntersections(TArray<FVector> &OutIntersections, const TArray<FVector> &InPoints, const FPlane &InPlane) const
	{
		const FModumateObjectInstance *parent = MOI->GetParentObject();
		const FVector parentLocation = parent->GetObjectLocation();

		int32 numPoints = InPoints.Num();
		for (int32 pointIdx = 0; pointIdx < numPoints; pointIdx++)
		{
			FVector layerPoint1 = InPoints[pointIdx] + parentLocation;
			FVector layerPoint2 = InPoints[(pointIdx + 1) % numPoints] + parentLocation;

			FVector point;
			if (FMath::SegmentPlaneIntersection(layerPoint1, layerPoint2, InPlane, point))
			{
				OutIntersections.Add(point);
			}
		}
	}

	bool FMOIPlaneHostedObjImpl::GetRangesForHolesOnPlane(TArray<TPair<float, float>> &OutRanges, TPair<FVector, FVector> &Intersection, const FLayerGeomDef &Layer, const float CurrentThickness, const FPlane &Plane, const FVector &AxisX, const FVector &AxisY, const FVector &Origin) const
	{
		FVector intersectionStart = Intersection.Key;
		FVector intersectionEnd = Intersection.Value;

		// TODO: only works when the plane goes through the object once (2 intersections)
		FVector2D start = UModumateGeometryStatics::ProjectPoint2D(Origin, AxisX, AxisY, intersectionStart);
		FVector2D end = UModumateGeometryStatics::ProjectPoint2D(Origin, AxisX, AxisY, intersectionEnd);
		float length = (end - start).Size();

		TArray<TPair<float, float>> holeRanges;
		TArray<TPair<float, float>> mergedRanges;
		int32 idx = 0;

		// intersect holes with the plane, and figure out the ranges along the original line where there is a valid hole
		for (auto& hole : Layer.Holes3D)
		{
			if (!Layer.HolesValid[idx])
			{
				continue;
			}
			TArray<FVector> projectedHolePoints;
			for (auto& point : hole.Points)
			{
				// TODO: cache this value
				projectedHolePoints.Add(point + GetNormal() * CurrentThickness);
			}

			// TODO: once there is potential for concave holes, generalize the sorter in GetDraftingLines and sort here as well
			TArray<FVector> holeIntersections;
			GetPlaneIntersections(holeIntersections, projectedHolePoints, Plane);

			if (holeIntersections.Num() != 2)
			{
				continue;
			}
			FVector2D holeStart = UModumateGeometryStatics::ProjectPoint2D(Origin, AxisX, AxisY, holeIntersections[0]);
			FVector2D holeEnd = UModumateGeometryStatics::ProjectPoint2D(Origin, AxisX, AxisY, holeIntersections[1]);

			float hs = (holeStart - start).Size() / length;
			float he = (holeEnd - start).Size() / length;
			if (hs > he)
			{
				Swap(hs, he);
			}
			holeRanges.Add(TPair<float, float>(hs, he));
			idx++;
		}

		holeRanges.Sort();

		//TODO: merging the ranges may be unnecessary because it seems like if holes overlap, one of them will be invalid
		for (auto& range : holeRanges)
		{
			if (mergedRanges.Num() == 0)
			{
				mergedRanges.Push(range);
				continue;
			}
			auto& currentRange = mergedRanges.Top();
			// if the sorted ranges overlap, combine them
			if (range.Value > currentRange.Value && range.Key < currentRange.Value)
			{
				mergedRanges.Top() = TPair<float, float>(currentRange.Key, range.Value);
			}
			else if (range.Key > currentRange.Value)
			{
				mergedRanges.Push(range);
			}
		}

		// lines for drafting are drawn in the areas where there aren't holes, 
		// so OutRanges inverts the merged ranges (which is where there are holes)
		if (mergedRanges.Num() == 0)
		{
			OutRanges.Add(TPair<float, float>(0, 1));
		}
		else
		{
			OutRanges.Add(TPair<float, float>(0, mergedRanges[0].Key));
			for (int32 rangeIdx = 0; rangeIdx < mergedRanges.Num() - 1; rangeIdx++)
			{
				OutRanges.Add(TPair<float, float>(mergedRanges[rangeIdx].Value, mergedRanges[rangeIdx + 1].Key));
			}
			OutRanges.Add(TPair<float, float>(mergedRanges[mergedRanges.Num()-1].Value, 1));
		}

		return true;
	}

	void FMOIPlaneHostedObjImpl::GetDraftingLines(const TSharedPtr<FDraftingComposite> &ParentPage, const FPlane &Plane, const FVector &AxisX, const FVector &AxisY, const FVector &Origin, const FBox2D &BoundingBox, TArray<TArray<FVector>> &OutPerimeters) const
	{
		OutPerimeters.Reset();

		Units::FThickness innerThickness = Units::FThickness::Points(0.125f);
		Units::FThickness structureThickness = Units::FThickness::Points(0.375f);
		Units::FThickness outerThickness = Units::FThickness::Points(0.25f);

		FMColor innerColor = FMColor::Gray64;
		FMColor outerColor = FMColor::Gray32;
		FMColor structureColor = FMColor::Black;

		const FModumateObjectInstance *parent = MOI->GetParentObject();
		const FVector parentLocation = parent->GetObjectLocation();

		// this sorter is used to sort the intersections between a layer's points and a plane.
		// assuming the points in a layer are planar, all intersections will fall on the same line.
		// since the intersections are on a line, we can sort the vectors simply and know the intersections 
		// are ordered such that adjacent pairs of intersections represent a line segment going through the layer polygon
		auto sorter = [this](const FVector &rhs, const FVector &lhs) {
			if (!FMath::IsNearlyEqual(rhs.X, lhs.X, KINDA_SMALL_NUMBER))
			{
				return rhs.X > lhs.X;
			}
			else if (!FMath::IsNearlyEqual(rhs.Y, lhs.Y, KINDA_SMALL_NUMBER))
			{
				return rhs.Y > lhs.Y;
			}
			else if (!FMath::IsNearlyEqual(rhs.Z, lhs.Z, KINDA_SMALL_NUMBER))
			{
				return rhs.Z > lhs.Z;
			}
			return true;
		};

		float currentThickness = 0.0f;
		int32 numLines = LayerGeometries.Num() + 1;
		for (int32 layerIdx = 0; layerIdx < numLines; layerIdx++)
		{
			bool usePointsA = layerIdx < LayerGeometries.Num();
			auto& layer = usePointsA ? LayerGeometries[layerIdx] : LayerGeometries[layerIdx-1];

			auto dwgLayerType = FModumateLayerType::kSeparatorCutStructuralLayer;
			if (layerIdx == 0)
			{
				dwgLayerType = FModumateLayerType::kSeparatorCutOuterSurface;
			}
			else if (layerIdx == LayerGeometries.Num())
			{
				dwgLayerType = FModumateLayerType::kSeparatorCutMinorLayer;
			}

			TArray<FVector> intersections;
			GetPlaneIntersections(intersections, usePointsA ? layer.PointsA : layer.PointsB, Plane);

			intersections.Sort(sorter);
			// we can make mask perimeters when there are an even amount of intersection between a simple polygon and a plane
			bool bMakeMaskPerimeter = (intersections.Num() % 2 == 0);

			Units::FThickness lineThickness;
			FMColor lineColor = innerColor;
			if (FMath::IsNearlyEqual(currentThickness, CachedLayerDims.StructureWidthStart) ||
				FMath::IsNearlyEqual(currentThickness, CachedLayerDims.StructureWidthEnd, KINDA_SMALL_NUMBER))
			{
				lineThickness = structureThickness;
				lineColor = structureColor;
			}
			// TODO: currently, finishes are not rendered in drawings.  Given finish lines are implemented,
			// the wall needs to check whether there is a finish on the outside before choosing to use the
			// outer layer weight as opposed to the inner layer weight.
			// Finish lines are not considered structural, they are all inner except for the outermost layer
			else if (layerIdx == 0 || layerIdx == LayerGeometries.Num())
			{
				lineThickness = outerThickness;
				lineColor = outerColor;
			}
			else
			{
				lineThickness = innerThickness;
				lineColor = innerColor;
			}

			for (int32 idx = 0; idx < intersections.Num() - 1; idx += 2)
			{
				// find vertices for the stencil masks, to hide geometry behind the object from the edge detection custom shader
				if (bMakeMaskPerimeter)
				{
					if (layerIdx == 0)
					{
						OutPerimeters.Add(TArray<FVector>());
						OutPerimeters[idx / 2].Add(intersections[idx]);
						OutPerimeters[idx / 2].Add(intersections[idx + 1]);
					}
					else if (layerIdx == numLines - 1)
					{
						// TODO: remove this constraint, potentially need to figure out where the ray would intersect with the plane
						// sometimes only some of the layers of a wall intersect the plane, resulting in a discrepancy in the amount 
						// of intersection points.  Currently, the mask geometry expects 4 points
						if (intersections.Num() / 2 == OutPerimeters.Num())
						{
							OutPerimeters[idx / 2].Add(intersections[idx + 1]);
							OutPerimeters[idx / 2].Add(intersections[idx]);
						}
					}
				}

				TArray<TPair<float, float>> lineRanges;
				FVector intersectionStart = intersections[idx];
				FVector intersectionEnd = intersections[idx+1];
				TPair<FVector, FVector> currentIntersection = TPair<FVector, FVector>(intersectionStart, intersectionEnd);
				GetRangesForHolesOnPlane(lineRanges, currentIntersection, layer, currentThickness, Plane, -AxisX, -AxisY, Origin);

				// TODO: unclear why the axes need to be flipped here, could be because of the different implementation of finding intersections
				FVector2D start = UModumateGeometryStatics::ProjectPoint2D(Origin, -AxisX, -AxisY, intersectionStart);
				FVector2D end = UModumateGeometryStatics::ProjectPoint2D(Origin, -AxisX, -AxisY, intersectionEnd);
				FVector2D delta = end - start;

				for (auto& range : lineRanges)
				{
					FVector2D clippedStart, clippedEnd;
					FVector2D rangeStart = start + delta * range.Key;
					FVector2D rangeEnd = start + delta * range.Value;

					if (UModumateFunctionLibrary::ClipLine2DToRectangle(rangeStart, rangeEnd, BoundingBox, clippedStart, clippedEnd))
					{
						TSharedPtr<FDraftingLine> line = MakeShareable(new FDraftingLine(
							Units::FCoordinates2D::WorldCentimeters(clippedStart), 
							Units::FCoordinates2D::WorldCentimeters(clippedEnd), 
							lineThickness, lineColor));
						ParentPage->Children.Add(line);
						line->SetLayerTypeRecursive(dwgLayerType);
					}
				}
			}

			currentThickness += layer.Thickness;
		}
	}

	// Plane-hosted objects have their geometry updated by their parents during pending move/rotate
	void FMOIPlaneHostedObjImpl::SetFromDataRecordAndRotation(const FMOIDataRecord &dataRec, const FVector &origin, const FQuat &rotation)
	{}

	void FMOIPlaneHostedObjImpl::SetFromDataRecordAndDisplacement(const FMOIDataRecord &dataRec, const FVector &displacement)
	{}

	void FMOIPlaneHostedObjImpl::UpdateMeshWithLayers(bool bRecreateMesh, bool bRecalculateEdgeExtensions)
	{
		const FModumateDocument *doc = MOI ? MOI->GetDocument() : nullptr;
		if (!ensureAlwaysMsgf(doc, TEXT("Tried to update invalid plane-hosted object!")))
		{
			return;
		}

		int32 parentID = MOI->GetParentID();
		const FGraph3DFace *planeFace = doc->GetVolumeGraph().FindFace(parentID);
		const FModumateObjectInstance *parentPlane = doc->GetObjectById(parentID);
		if (!ensureMsgf(parentPlane, TEXT("Plane-hosted object (ID %d) is missing parent object (ID %d)!"), MOI->ID, parentID) ||
			!ensureMsgf(planeFace, TEXT("Plane-hosted object (ID %d) is missing parent graph face (ID %d)!"), MOI->ID, parentID))
		{
			return;
		}

		DynamicMeshActor->SetActorLocation(parentPlane->GetObjectLocation());
		DynamicMeshActor->SetActorRotation(FQuat::Identity);
		DynamicMeshActor->UpdateHolesFromActors();
		const TArray<FPolyHole3D> &holes = DynamicMeshActor->GetHoles3D();

		if (!FMiterHelpers::UpdateMiteredLayerGeoms(MOI, planeFace, &holes, LayerGeometries))
		{
			return;
		}

		DynamicMeshActor->Assembly = MOI->GetAssembly();
		DynamicMeshActor->LayerGeometries = LayerGeometries;

		bool bEnableCollision = !MOI->GetIsInPreviewMode() && !parentPlane->GetIsInPreviewMode();
		DynamicMeshActor->UpdatePlaneHostedMesh(bRecreateMesh, bEnableCollision, bEnableCollision);
	}

	void FMOIPlaneHostedObjImpl::UpdateConnectedEdges()
	{
		CachedParentConnectedMOIs.Reset();

		const FModumateObjectInstance *planeParent = MOI ? MOI->GetParentObject() : nullptr;
		if (planeParent)
		{
			planeParent->GetConnectedMOIs(CachedParentConnectedMOIs);
		}

		CachedConnectedEdges.Reset();
		for (FModumateObjectInstance *planeConnectedMOI : CachedParentConnectedMOIs)
		{
			if (planeConnectedMOI && (planeConnectedMOI->GetObjectType() == EObjectType::OTMetaEdge))
			{
				CachedConnectedEdges.Add(planeConnectedMOI);
			}
		}
	}
}

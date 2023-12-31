// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "Objects/PlaneHostedObj.h"

#include "Algo/Transform.h"
#include "Algo/Accumulate.h"

#include "DocumentManagement/ModumateDocument.h"
#include "Objects/ModumateObjectInstance.h"
#include "Drafting/ModumateDraftingElements.h"
#include "Graph/Graph3D.h"
#include "Graph/Graph3DFace.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateMitering.h"
#include "Objects/ModumateObjectStatics.h"
#include "Objects/MOIDelta.h"
#include "Objects/MetaPlaneSpan.h"
#include "UI/Properties/InstPropWidgetFlip.h"
#include "UI/Properties/InstPropWidgetOffset.h"
#include "UI/ToolTray/ToolTrayBlockProperties.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "Quantities/QuantitiesManager.h"
#include "DrawingDesigner/DrawingDesignerLine.h"

class AEditModelPlayerController;

FMOIPlaneHostedObjData::FMOIPlaneHostedObjData()
{
}

FMOIPlaneHostedObjData::FMOIPlaneHostedObjData(int32 InVersion)
	: Version(InVersion)
{
}

AMOIPlaneHostedObj::AMOIPlaneHostedObj()
	: AModumateObjectInstance()
{
	FWebMOIProperty prop;
	
	prop.name = TEXT("Offset");
	prop.type = EWebMOIPropertyType::offset;
	prop.displayName = TEXT("Offset");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.name, prop);

	prop.name = TEXT("FlipSigns");
	prop.type = EWebMOIPropertyType::flip3D;
	prop.displayName = TEXT("Flip");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.name, prop);	
}

FQuat AMOIPlaneHostedObj::GetRotation() const
{
	const AModumateObjectInstance *parent = GetParentObject();
	if (ensure(parent && IsValidParentObjectType(parent->GetObjectType())))
	{
		return parent->GetRotation();
	}
	else
	{
		return FQuat::Identity;
	}
}

FVector AMOIPlaneHostedObj::GetLocation() const
{
	const AModumateObjectInstance *parent = GetParentObject();
	if (ensure(parent && IsValidParentObjectType(parent->GetObjectType())))
	{
		float thickness, startOffset;
		FVector normal;
		UModumateObjectStatics::GetPlaneHostedValues(this, thickness, startOffset, normal);

		FVector planeLocation = parent->GetLocation();
		return planeLocation + (startOffset + (0.5f * thickness)) * normal;
	}
	else
	{
		return FVector::ZeroVector;
	}
}

FVector AMOIPlaneHostedObj::GetCorner(int32 CornerIndex) const
{
	// Handle the meta plane host case which just returns the meta plane with this MOI's offset
	const AModumateObjectInstance *parent = GetParentObject();
	if (ensure(parent && IsValidParentObjectType(parent->GetObjectType())))
	{
		int32 numPlanePoints = parent->GetNumCorners();
		bool bOnStartingSide = (CornerIndex < numPlanePoints);
		int32 numLayers = LayerGeometries.Num();
		int32 pointIndex = CornerIndex % numPlanePoints;

		if (ensure((numLayers == GetAssembly().Layers.Num()) && numLayers > 0))
		{
			auto& layerPoints = bOnStartingSide ? LayerGeometries[0].OriginalPointsA : LayerGeometries[numLayers - 1].OriginalPointsB;

			if (ensure((3 * numPlanePoints) == layerPoints.Num()))
			{
				return parent->GetLocation() + layerPoints[3 * pointIndex];
			}
		}
	}

	return GetLocation();
}

FVector AMOIPlaneHostedObj::GetNormal() const
{
	const AModumateObjectInstance *planeParent = GetParentObject();
	if (ensureAlways(planeParent && IsValidParentObjectType(planeParent->GetObjectType())))
	{
		return planeParent->GetNormal();
	}
	else
	{
		return FVector::ZeroVector;
	}
}

void AMOIPlaneHostedObj::PreDestroy()
{
	MarkEdgesMiterDirty();
	Super::PreDestroy();
}

bool AMOIPlaneHostedObj::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	switch (DirtyFlag)
	{
	case EObjectDirtyFlags::Structure:
	{
		// TODO: as long as the assembly is not stored inside of the data state, and its layers can be reversed,
		// then this is the centralized opportunity to match up the reversal of layers with whatever the intended inversion state is,
		// based on preview/current state changing, assembly changing, object creation, etc.
		SetAssemblyLayersReversed(InstanceData.FlipSigns.Y < 0);

		CachedLayerDims.UpdateLayersFromAssembly(Document,GetAssembly());
		CachedLayerDims.UpdateFinishFromObject(this);

		// When structure (assembly, offset, or plane structure) changes, mark neighboring
		// edges as miter-dirty, so they can re-evaluate mitering with our new structure.
		MarkDirty(EObjectDirtyFlags::Mitering);
		MarkEdgesMiterDirty();
		Document->DirtyAllCutPlanes();
	}
	break;
	case EObjectDirtyFlags::Mitering:
	{
		// Make sure all connected edges have resolved mitering,
		// so we can generate our own mitered mesh correctly.
		for (AModumateObjectInstance *connectedEdge : CachedConnectedEdges)
		{
			if (connectedEdge->IsDirty(EObjectDirtyFlags::Mitering))
			{
				return false;
			}
		}

		UpdateMeshWithLayers(false, true);
		UpdateAlignmentTargets();

		// Mark SurfaceGraph children as structurally dirty, since their bounds may have changed as a result of re-mitering.
		for (int32 childID : CachedChildIDs)
		{
			AModumateObjectInstance* childObj = Document->GetObjectById(childID);
			if (childObj && (childObj->GetObjectType() == EObjectType::OTSurfaceGraph))
			{
				childObj->MarkDirty(EObjectDirtyFlags::Structure);
			}
		}
	}
	break;
	case EObjectDirtyFlags::Visuals:
	{
		return TryUpdateVisuals();
	}
	default:
		break;
	}

	return true;
}

void AMOIPlaneHostedObj::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
{
	const AModumateObjectInstance *parent = GetParentObject();

	if (ensure(parent && IsValidParentObjectType(parent->GetObjectType())))
	{
		int32 numPlanePoints = parent->GetNumCorners();

		for (int32 i = 0; i < numPlanePoints; ++i)
		{
			int32 edgeIdxA = i;
			int32 edgeIdxB = (i + 1) % numPlanePoints;

			FVector cornerMinA = GetCorner(edgeIdxA);
			FVector cornerMinB = GetCorner(edgeIdxB);
			FVector edgeDir = (cornerMinB - cornerMinA).GetSafeNormal();

			FVector cornerMaxA = GetCorner(edgeIdxA + numPlanePoints);
			FVector cornerMaxB = GetCorner(edgeIdxB + numPlanePoints);

			outPoints.Add(FStructurePoint(cornerMinA, edgeDir, edgeIdxA));
			outLines.Add(FStructureLine(cornerMinA, cornerMinB, edgeIdxA, edgeIdxB));
			outLines.Add(FStructureLine(cornerMaxA, cornerMaxB, edgeIdxA + numPlanePoints, edgeIdxB + numPlanePoints));
			outLines.Add(FStructureLine(cornerMinA, cornerMaxA, edgeIdxA, edgeIdxA + numPlanePoints));
		}
	}
}

void AMOIPlaneHostedObj::ToggleAndUpdateCapGeometry(bool bEnableCap)
{
	if (DynamicMeshActor.IsValid())
	{
		bEnableCap ? DynamicMeshActor->SetupCapGeometry() : DynamicMeshActor->ClearCapGeometry();
	}
}

bool AMOIPlaneHostedObj::OnSelected(bool bIsSelected)
{
	if (!AModumateObjectInstance::OnSelected(bIsSelected))
	{
		return false;
	}

	if (const AModumateObjectInstance *parent = GetParentObject())
	{
		TArray<AModumateObjectInstance *> connectedMOIs;
		parent->GetConnectedMOIs(connectedMOIs);
		for (AModumateObjectInstance *connectedMOI : connectedMOIs)
		{
			connectedMOI->MarkDirty(EObjectDirtyFlags::Visuals);
		}
	}

	return true;
}

bool AMOIPlaneHostedObj::GetInvertedState(FMOIStateData& OutState) const
{
	return GetFlippedState(EAxis::Y, OutState);
}

bool AMOIPlaneHostedObj::GetFlippedState(EAxis::Type FlipAxis, FMOIStateData& OutState) const
{
	OutState = GetStateData();

	FMOIPlaneHostedObjData modifiedPlaneHostedObjData = InstanceData;

	float curFlipSign = modifiedPlaneHostedObjData.FlipSigns.GetComponentForAxis(FlipAxis);
	modifiedPlaneHostedObjData.FlipSigns.SetComponentForAxis(FlipAxis, -curFlipSign);

	return OutState.CustomData.SaveStructData(modifiedPlaneHostedObjData);
}

bool AMOIPlaneHostedObj::GetOffsetState(const FVector& AdjustmentDirection, FMOIStateData& OutState) const
{
	float projectedAdjustment = AdjustmentDirection | GetNormal();
	if (FMath::IsNearlyZero(projectedAdjustment, THRESH_NORMALS_ARE_ORTHOGONAL))
	{
		projectedAdjustment = 0.0f;
	}

	float projectedAdjustmentSign = FMath::Sign(projectedAdjustment);
	EDimensionOffsetType nextOffsetType = InstanceData.Offset.GetNextType(projectedAdjustmentSign, InstanceData.FlipSigns.Y);

	FMOIPlaneHostedObjData modifiedPlaneHostedObjData = InstanceData;
	modifiedPlaneHostedObjData.Offset.type = nextOffsetType;
	OutState = GetStateData();

	return OutState.CustomData.SaveStructData(modifiedPlaneHostedObjData);
}

void AMOIPlaneHostedObj::RegisterInstanceDataUI(UToolTrayBlockProperties* PropertiesUI)
{
	static const FString flipPropertyName(TEXT("Flip"));
	if (auto flipField = PropertiesUI->RequestPropertyField<UInstPropWidgetFlip>(this, flipPropertyName))
	{
		flipField->RegisterValue(this, EAxisList::XYZ);
		flipField->ValueChangedEvent.AddDynamic(this, &AMOIPlaneHostedObj::OnInstPropUIChangedFlip);
	}

	static const FString offsetPropertyName(TEXT("Offset"));
	if (auto offsetField = PropertiesUI->RequestPropertyField<UInstPropWidgetOffset>(this, offsetPropertyName))
	{
		offsetField->RegisterValue(this, InstanceData.Offset);
		offsetField->ValueChangedEvent.AddDynamic(this, &AMOIPlaneHostedObj::OnInstPropUIChangedOffset);
	}
}

void AMOIPlaneHostedObj::GetDraftingLines(const TSharedPtr<FDraftingComposite> &ParentPage, const FPlane &Plane, const FVector &AxisX, const FVector &AxisY, const FVector &Origin, const FBox2D &BoundingBox, TArray<TArray<FVector>> &OutPerimeters) const
{
	OutPerimeters.Reset();

	ModumateUnitParams::FThickness innerThickness = ModumateUnitParams::FThickness::Points(0.375f);
	ModumateUnitParams::FThickness outerThickness = ModumateUnitParams::FThickness::Points(0.75f);

	FMColor innerColor = FMColor::Gray64;
	FMColor outerColor = FMColor::Gray32;
	FMColor structureColor = FMColor::Black;
	bool bUseMiterPriority = false;

	bool bGetFarLines = ParentPage->lineClipping.IsValid();
	if (!bGetFarLines)
	{
		const AModumateObjectInstance *parent = GetParentObject();
		FVector parentLocation = parent->GetLocation();

		FModumateLayerType layerTypeOuterSurface;
		FModumateLayerType layerTypeMinorSurface;
		FModumateLayerType layerTypeEndCaps;

		switch (GetObjectType())
		{
		case EObjectType::OTCountertop:
			layerTypeOuterSurface = FModumateLayerType::kCountertopCut;
			layerTypeMinorSurface = layerTypeOuterSurface;
			layerTypeEndCaps = layerTypeOuterSurface;
			break;

		case EObjectType::OTSystemPanel:
			layerTypeOuterSurface = FModumateLayerType::kSystemPanelCut;
			layerTypeMinorSurface = layerTypeOuterSurface;
			layerTypeEndCaps = layerTypeOuterSurface;
			break;

		default:
			layerTypeOuterSurface = FModumateLayerType::kSeparatorCutOuterSurface;
			layerTypeMinorSurface = FModumateLayerType::kSeparatorCutMinorLayer;
			layerTypeEndCaps = FModumateLayerType::kSeparatorCutEndCaps;
			bUseMiterPriority = true;
			break;
		}

		auto isStructuralLayer = [this](int32 LayerIdx)
		{
			return 
				LayerIdx < GetAssembly().Layers.Num()
				&& LayerIdx > 0
				&& GetAssembly().Layers[LayerIdx].LayerPriority.PriorityGroup == EBIMPresetLayerPriorityGroup::Structure;
		};

		TArray<FVector2D> previousLinePoints;
		int32 numLayers = LayerGeometries.Num();
		for (int32 layerIdx = 0; layerIdx < numLayers; layerIdx++)
		{
			auto& layer = LayerGeometries[layerIdx];
			bool usePointsA = true;
			do
			{
				if (usePointsA && layerIdx != 0 && LayerGeometries[layerIdx - 1].UniquePointsB == layer.UniquePointsA)
				{
					usePointsA = false;
					continue;
				}

				FModumateLayerType dwgLayerType;
				ModumateUnitParams::FThickness lineThickness;
				FMColor lineColor(FMColor::Black);

				if ((usePointsA && (layerIdx == 0)) || (!usePointsA && (layerIdx == numLayers - 1)))
				{
					dwgLayerType = layerTypeOuterSurface;
					lineThickness = outerThickness;
					lineColor = outerColor;
				}
				else
				{
					dwgLayerType = layerTypeMinorSurface;
					lineThickness = innerThickness;
					lineColor = innerColor;

					if (bUseMiterPriority)
					{
						if (isStructuralLayer(layerIdx)
							|| isStructuralLayer(layerIdx + (usePointsA ? -1  : +1)) )
						{
							dwgLayerType = FModumateLayerType::kSeparatorCutStructuralLayer;
							lineColor = structureColor;
						}
					} 
				}

				TArray<FVector> intersections;
				UModumateGeometryStatics::GetPlaneIntersections(intersections, usePointsA ? layer.UniquePointsA : layer.UniquePointsB, Plane, parentLocation);

				intersections.Sort(UModumateGeometryStatics::Points3dSorter);

				int32 linePoint = 0;

				for (int32 idx = 0; idx < intersections.Num() - 1; idx += 2)
				{
					TArray<TPair<float, float>> lineRanges;
					FVector intersectionStart = intersections[idx];
					FVector intersectionEnd = intersections[idx + 1];
					TPair<FVector, FVector> currentIntersection = TPair<FVector, FVector>(intersectionStart, intersectionEnd);
					// Hole coords are on the parent meta-plane so project.
					FVector samplePoint = usePointsA ? layer.OriginalPointsA[0] : layer.OriginalPointsB[0];
					FVector hole3DDisplacement = (samplePoint | layer.Normal) * layer.Normal;
					layer.GetRangesForHolesOnPlane(lineRanges, currentIntersection,
						parentLocation + hole3DDisplacement, Plane, AxisX, AxisY, Origin);

					FVector2D start = UModumateGeometryStatics::ProjectPoint2D(intersectionStart, AxisX, AxisY, Origin);
					FVector2D end = UModumateGeometryStatics::ProjectPoint2D(intersectionEnd, AxisX, AxisY, Origin);
					FVector2D delta = end - start;

					ParentPage->inPlaneLines.Emplace(FVector(start, 0), FVector(end, 0));

					for (auto& range : lineRanges)
					{
						FVector2D clippedStart, clippedEnd;
						FVector2D rangeStart = start + delta * range.Key;
						FVector2D rangeEnd = start + delta * range.Value;

						if (UModumateFunctionLibrary::ClipLine2DToRectangle(rangeStart, rangeEnd, BoundingBox, clippedStart, clippedEnd))
						{
							TSharedPtr<FDraftingLine> line = MakeShared<FDraftingLine>(
								FModumateUnitCoord2D::WorldCentimeters(clippedStart),
								FModumateUnitCoord2D::WorldCentimeters(clippedEnd),
								lineThickness, lineColor);
							ParentPage->Children.Add(line);
							line->SetLayerTypeRecursive(dwgLayerType);
						}

						// End caps or exposed sides:
						if (previousLinePoints.Num() > linePoint)
						{
							if (UModumateFunctionLibrary::ClipLine2DToRectangle(previousLinePoints[linePoint], rangeStart, BoundingBox, clippedStart, clippedEnd))
							{
								TSharedPtr<FDraftingLine> line = MakeShared<FDraftingLine>(
									FModumateUnitCoord2D::WorldCentimeters(clippedStart),
									FModumateUnitCoord2D::WorldCentimeters(clippedEnd),
									outerThickness, outerColor);
								ParentPage->Children.Add(line);
								line->SetLayerTypeRecursive(layerTypeEndCaps);
							}
							if (UModumateFunctionLibrary::ClipLine2DToRectangle(previousLinePoints[linePoint + 1], rangeEnd, BoundingBox, clippedStart, clippedEnd))
							{
								TSharedPtr<FDraftingLine> line = MakeShared<FDraftingLine>(
									FModumateUnitCoord2D::WorldCentimeters(clippedStart),
									FModumateUnitCoord2D::WorldCentimeters(clippedEnd),
									outerThickness, outerColor);
								ParentPage->Children.Add(line);
								line->SetLayerTypeRecursive(layerTypeEndCaps);
							}
						}
						previousLinePoints.SetNum(FMath::Max(linePoint + 2, previousLinePoints.Num()));
						previousLinePoints[linePoint] = rangeStart;
						previousLinePoints[linePoint + 1] = rangeEnd;
						linePoint += 2;
					}

				}

				usePointsA = !usePointsA;
			} while (!usePointsA);
		}
	}
	else  // Get far lines.
	{
		GetBeyondDraftingLines(ParentPage, Plane, BoundingBox);
	}
}

void AMOIPlaneHostedObj::PostLoadInstanceData()
{
	bool bFixedInstanceData = false;

	if (InstanceData.Version < InstanceData.CurrentVersion)
	{
		if (InstanceData.Version < 1)
		{
			if (InstanceData.bLayersInverted_DEPRECATED)
			{
				InstanceData.FlipSigns.Y = -1.0f;
			}
		}
		if (InstanceData.Version < 2)
		{
			float flippedJustification = (-InstanceData.FlipSigns.Y * (InstanceData.Justification_DEPRECATED - 0.5f)) + 0.5f;
			if (FMath::IsNearlyEqual(flippedJustification, 0.0f))
			{
				InstanceData.Offset.type = EDimensionOffsetType::Negative;
			}
			else if (FMath::IsNearlyEqual(flippedJustification, 1.0f))
			{
				InstanceData.Offset.type = EDimensionOffsetType::Positive;
			}
			else 
			{
				InstanceData.Offset.type = EDimensionOffsetType::Centered;
			}
		}

		InstanceData.Version = InstanceData.CurrentVersion;
		bFixedInstanceData = true;
	}

	// Check for invalid flip signs regardless of version numbers, due to non-serialization-version bugs
	for (int32 axisIdx = 0; axisIdx < 3; ++axisIdx)
	{
		float& flipSign = InstanceData.FlipSigns[axisIdx];
		if (FMath::Abs(flipSign) != 1.0f)
		{
			flipSign = 1.0f;
			bFixedInstanceData = true;
		}
	}

	if (bFixedInstanceData)
	{
		StateData.CustomData.SaveStructData(InstanceData);
	}
}

void AMOIPlaneHostedObj::UpdateMeshWithLayers(bool bRecreateMesh, bool bRecalculateEdgeExtensions)
{
	const UModumateDocument* doc = GetDocument();
	if (!ensureAlwaysMsgf(doc, TEXT("Tried to update invalid plane-hosted object!")))
	{
		return;
	}

	int32 parentID = GetParentID();
	const AMOIMetaPlaneSpan* parentSpan = Cast<AMOIMetaPlaneSpan>(doc->GetObjectById(parentID));

	if (!ensureMsgf(parentSpan, TEXT("Plane-hosted object (ID %d) is missing parent object (ID %d)!"), ID, parentID))
	{
		return;
	}

	const FGraph3DFace* parentFace = parentSpan->GetPerimeterFace();

	DynamicMeshActor->SetActorLocation(parentSpan->GetLocation());
	DynamicMeshActor->SetActorRotation(FQuat::Identity);

	CachedHoles.Reset();

	for (auto& hole : parentFace->CachedHoles)
	{
		TempHoleRelativePoints.Reset();
		Algo::Transform(hole.Points, TempHoleRelativePoints, [parentFace](const FVector &worldPoint) { return worldPoint - parentFace->CachedCenter; });
		CachedHoles.Add(FPolyHole3D(TempHoleRelativePoints));
	}

	if (!FMiterHelpers::UpdateMiteredLayerGeoms(this, parentFace, &CachedHoles, LayerGeometries, CachedExtendedSurfaceFaces))
	{
		return;
	}

	DynamicMeshActor->Assembly = GetAssembly();
	DynamicMeshActor->LayerGeometries = LayerGeometries;

	// TODO: now that preview deltas have replaced object-specific preview mode, this change might be too overreaching and affect too many objects.
	// We should reevaluate the tradeoffs between all updating objects consistently not updating their collision during preview deltas vs
	// keeping track of which objects are affected by preview deltas and only preserving old collision for those.
	// In addition, this logic shouldn't even be necessary if cursor raycast results could be correctly read from physics geometry
	// modified during the same frame before generating preview deltas in tools,
	// but relying on PhysX collision cooking and deferred actor/mesh flag updating may prevent that.
	bool bEnableCollision = true;
	bool bUpdateCollision = !doc->IsPreviewingDeltas();
	FVector2D uvFlip(InstanceData.FlipSigns.X, InstanceData.FlipSigns.Z);
	DynamicMeshActor->UpdatePlaneHostedMesh(bRecreateMesh, bUpdateCollision, bEnableCollision, FVector::ZeroVector, uvFlip);
}

void AMOIPlaneHostedObj::UpdateConnectedEdges()
{
	// no plane span means we're being destroyed
	const AMOIMetaPlaneSpan* planeSpan = Cast<AMOIMetaPlaneSpan>(GetParentObject());
	if (planeSpan == nullptr)
	{
		return;
	}

	CachedParentConnectedMOIs.Reset();
	CachedConnectedEdges.Reset();

	for (int32 id : planeSpan->InstanceData.GraphMembers)
	{
		const AModumateObjectInstance* planeParent = Document->GetObjectById(id);
		if (planeParent)
		{
			planeParent->GetConnectedMOIs(CachedParentConnectedMOIs);
		}

		for (AModumateObjectInstance* planeConnectedMOI : CachedParentConnectedMOIs)
		{
			if (planeConnectedMOI && (planeConnectedMOI->GetObjectType() == EObjectType::OTMetaEdge))
			{
				CachedConnectedEdges.Add(planeConnectedMOI);
			}
		}
	}

	UpdateAlignmentTargets();
}

void AMOIPlaneHostedObj::MarkEdgesMiterDirty()
{
	UpdateConnectedEdges();
	for (AModumateObjectInstance *connectedEdge : CachedConnectedEdges)
	{
		connectedEdge->MarkDirty(EObjectDirtyFlags::Mitering);
	}
}

void AMOIPlaneHostedObj::GetBeyondDraftingLines(const TSharedPtr<FDraftingComposite>& ParentPage, const FPlane& Plane,
	const FBox2D& BoundingBox) const
{
	static const ModumateUnitParams::FThickness outerThickness = ModumateUnitParams::FThickness::Points(0.25f);

	const AModumateObjectInstance *parent = GetParentObject();
	if (!ensure(parent))
	{
		return;
	}
	FVector parentLocation = parent->GetLocation();
	FModumateLayerType layerType;

	switch (GetObjectType())
	{
	case EObjectType::OTCountertop:
		layerType = FModumateLayerType::kCountertopBeyond;
		break;

	case EObjectType::OTSystemPanel:
		layerType = FModumateLayerType::kSystemPanelBeyond;
		break;

	default:
		layerType = FModumateLayerType::kSeparatorBeyondSurfaceEdges;
		break;
	}

	TArray<TPair<FEdge, FModumateLayerType>> backgroundLines;

	const int numLayers = LayerGeometries.Num();

	if (numLayers > 0)
	{
		const float totalThickness = Algo::Accumulate(LayerGeometries, 0.0f,
			[](float s, const auto& layer) { return s + layer.Thickness; });
		const FVector planeOffset = totalThickness * LayerGeometries[0].Normal;

		auto addPerimeterLines = [&backgroundLines, parentLocation, layerType](const TArray<FVector>& points)
		{
			int numPoints = points.Num();
			for (int i = 0; i < numPoints; ++i)
			{
				FEdge line(points[i] + parentLocation,
					points[(i + 1) % numPoints] + parentLocation);
				backgroundLines.Emplace(line, layerType);

			}

		};

		// Perimeter lines:
		for (int32 layer = 0; layer < numLayers; ++layer)
		{
			if (layer == 0
				|| LayerGeometries[layer].OriginalPointsA != LayerGeometries[layer - 1].OriginalPointsB)
			{
				addPerimeterLines(LayerGeometries[layer].OriginalPointsA);
			}
			addPerimeterLines(LayerGeometries[layer].OriginalPointsB);
		}

		auto addOpeningLines = [&backgroundLines](const FLayerGeomDef& layer, FVector parentLocation, bool bSideA)
		{
			const auto& holes = layer.Holes3D;
			for (const auto& hole: holes)
			{
				int32 n = hole.Points.Num();
				for (int32 i = 0; i < n; ++i)
				{
					FVector v1 = layer.ProjectToPlane(hole.Points[i], bSideA);
					FVector v2 = layer.ProjectToPlane(hole.Points[(i + 1) % n], bSideA);
					FEdge line(v1 + parentLocation, v2 + parentLocation);
					backgroundLines.Emplace(line, FModumateLayerType::kSeparatorBeyondSurfaceEdges);
				}
			}

		};

		// Opening perimeter lines (front & back):
		addOpeningLines(LayerGeometries[0], parentLocation, true);
		addOpeningLines(LayerGeometries[numLayers - 1], parentLocation, false);

		// Opening corner lines:
		for (const auto& hole : LayerGeometries[0].Holes3D)
		{
			for (const auto& p : hole.Points)
			{
				FVector holePoint = LayerGeometries[0].ProjectToPlane(p, true) + parentLocation;
				backgroundLines.Emplace(FEdge(holePoint, holePoint + planeOffset),
					FModumateLayerType::kSeparatorBeyondSurfaceEdges);
			}
		}

		// Perimeter corner lines.
		int32 numPoints = LayerGeometries[0].OriginalPointsA.Num();
		for (int32 corner = 0; corner < numPoints; ++corner)
		{
			FVector previousPoint = LayerGeometries[0].OriginalPointsA[corner] + parentLocation;
			for (const auto& layer: LayerGeometries)
			{
				FVector pA = layer.OriginalPointsA[corner] + parentLocation;
				FVector pB = layer.OriginalPointsB[corner] + parentLocation;
				if (previousPoint != pA)
				{
					FEdge line(previousPoint, pA);
					backgroundLines.Emplace(line, layerType);
				}
				FEdge line(pA, pB);
				backgroundLines.Emplace(line, layerType);
				previousPoint = pB;
			}
		}

		FVector2D boxClipped0;
		FVector2D boxClipped1;
		for (const auto& line : backgroundLines)
		{
			FVector v0 { line.Key.Vertex[0] };
			FVector v1 { line.Key.Vertex[1] };

			bool bV1Infront = Plane.PlaneDot(v0) >= 0.0f;
			bool bV2Infront = Plane.PlaneDot(v1) >= 0.0f;
			if (bV1Infront || bV2Infront)
			{
				if (!bV1Infront || !bV2Infront)
				{   // Intersect with Plane:
					FVector intersect { FMath::LinePlaneIntersection(v0, v1, Plane) };
					(bV1Infront ? v1 : v0) = intersect;
				}

				TArray<FEdge> clippedLineSections = ParentPage->lineClipping->ClipWorldLineToView({ v0, v1 });
				for (const auto& lineSection: clippedLineSections)
				{
					FVector2D vert0(lineSection.Vertex[0]);
					FVector2D vert1(lineSection.Vertex[1]);

					if (UModumateFunctionLibrary::ClipLine2DToRectangle(vert0, vert1, BoundingBox, boxClipped0, boxClipped1))
					{
						TSharedPtr<FDraftingLine> draftingLine = MakeShared<FDraftingLine>(
							FModumateUnitCoord2D::WorldCentimeters(boxClipped0),
							FModumateUnitCoord2D::WorldCentimeters(boxClipped1),
							outerThickness, FMColor::Black);
						ParentPage->Children.Add(draftingLine);
						draftingLine->SetLayerTypeRecursive(line.Value);
					}
				}
			}
		}
	}
}

void AMOIPlaneHostedObj::OnInstPropUIChangedFlip(int32 FlippedAxisInt)
{
	EAxis::Type flippedAxis = static_cast<EAxis::Type>(FlippedAxisInt);
	if (Document)
	{
		auto deltaPtr = MakeShared<FMOIDelta>();
		auto& newStateData = deltaPtr->AddMutationState(this);
		auto newInstanceData = InstanceData;
		newInstanceData.FlipSigns.SetComponentForAxis(flippedAxis, -newInstanceData.FlipSigns.GetComponentForAxis(flippedAxis));
		newStateData.CustomData.SaveStructData(newInstanceData);

		Document->ApplyDeltas({ deltaPtr }, GetWorld());
	}
}

void AMOIPlaneHostedObj::OnInstPropUIChangedOffset(const FDimensionOffset& NewValue)
{
	if (Document && (InstanceData.Offset != NewValue))
	{
		auto deltaPtr = MakeShared<FMOIDelta>();
		auto& newStateData = deltaPtr->AddMutationState(this);
		auto newInstanceData = InstanceData;
		newInstanceData.Offset = NewValue;
		newStateData.CustomData.SaveStructData(newInstanceData);

		Document->ApplyDeltas({ deltaPtr }, GetWorld());
	}
}

bool AMOIPlaneHostedObj::ProcessQuantities(FQuantitiesCollection& QuantitiesVisitor) const
{
	QuantitiesVisitor.Add(CachedQuantities);
	return true;
}

void AMOIPlaneHostedObj::GetDrawingDesignerItems(const FVector& viewDirection, TArray<FDrawingDesignerLine>& OutDrawingLines, float MinLength /*= 0.0f*/) const
{
	FVector parentLocation = GetParentObject()->GetLocation();

	const int32 numInitialLines = OutDrawingLines.Num();

	const auto& layers = LayerGeometries;
	TArray<FVector> previousLayer;
	previousLayer.SetNum(layers[0].OriginalPointsA.Num());
	for (int32 layerIdx = 0; layerIdx <= layers.Num(); ++layerIdx)
	{
		const TArray<FVector>& points = layerIdx == layers.Num() ? layers[layerIdx - 1].OriginalPointsB :
			layers[layerIdx].OriginalPointsA;

		FVector previousCorner1(BIG_NUMBER);
		FVector previousCorner2(BIG_NUMBER);

		const int32 numPoints = points.Num();
		for (int32 p = 0; p < numPoints; ++p)
		{
			FVector p1(points[p] + parentLocation);
			FVector p2(points[(p + 1) % numPoints] + parentLocation);
			FDrawingDesignerLine line;
			line.P1 = p1;
			line.P2 = p2;
			line.Thickness = layerIdx == 0 || layerIdx == layers.Num() ? 1.0f : 0.5f;

			const float lineLength = line.Length();
			if (lineLength >= MinLength && lineLength > 0.0f)
			{
				OutDrawingLines.Add(line);
			}

			if (layerIdx != 0)
			{
				FDrawingDesignerLine cornerLine;
				cornerLine.P1 = p1;
				cornerLine.P2 = previousLayer[p];

				if (cornerLine.Length() >= MinLength && (cornerLine.P1 != previousCorner1 || cornerLine.P2 != previousCorner2))
				{
					OutDrawingLines.Add(cornerLine);
					previousCorner1 = cornerLine.P1; previousCorner2 = cornerLine.P2;
				}
			}
			previousLayer[p] = p1;
		}
	}

	for (int32 l = numInitialLines; l < OutDrawingLines.Num(); ++l)
	{
		auto& line = OutDrawingLines[l];
		line.Thickness = 0.053f;
	}
}

bool AMOIPlaneHostedObj::GetBoundingLines(TArray<FDrawingDesignerLine>& outBounding)  const
{
	//Only return points for layerIdx == 0 and layerIdx == layers.Num()-1
	auto origin = LayerGeometries[0].OriginalPointsA;
	FVector parentLocation = GetParentObject()->GetLocation();

	const auto& layers = LayerGeometries;
	TArray<FVector> topPoints;
	TArray<FVector>  bottomPoints;
	
	auto acculumateLines = [&](const TArray<FVector>& layerPoints, TArray<FVector>&collector) {
		const TArray<FVector>& points = layerPoints;
		const int32 numPoints = points.Num();
		for (int32 p = 0; p < numPoints; ++p)
		{
			FVector p1(points[p] + parentLocation);
			FVector p2(points[(p + 1) % numPoints] + parentLocation);
			//Sometimes p1 can == p2, but we need them all to draw cross lines
			collector.Add(p1);
			outBounding.Add(FDrawingDesignerLine(p1, p2));	
		}
	};


	auto& firstLayer = layers[0].OriginalPointsA;
	auto& lastLayer = layers[layers.Num() - 1].OriginalPointsB;

	/* FIRST LAYER */
	acculumateLines(firstLayer, topPoints);

	/* LAST LAYER OTHER SIDE */
	acculumateLines(lastLayer, bottomPoints);

	/* Joining lines */
	//TODO: This can be sped up to a single loop incl with the last 2 if needed -JN
	for (int i = 0; i < topPoints.Num(); i++) {
		if(ensure(bottomPoints.Num() > i))
		{
			FVector p1 = topPoints[i];
			FVector p2 = bottomPoints[i];
			outBounding.Add(FDrawingDesignerLine(p1, p2));
		}
	}

	return true;
}

void AMOIPlaneHostedObj::UpdateQuantities()
{
	const FBIMAssemblySpec& assembly = GetAssembly();
	const int32 numLayers = assembly.Layers.Num();
	auto assemblyGuid = assembly.UniqueKey();
	int32 parentID = GetParentID();
	const FGraph3DFace* hostingFace = nullptr;

	const AMOIMetaPlaneSpan* span = Cast<AMOIMetaPlaneSpan>(Document->GetObjectById(parentID));

	if (ensure(span))
	{
		hostingFace = span->GetPerimeterFace();
	}

	if (!ensure(hostingFace))
	{
		return;
	}

	CachedQuantities.Empty();
	float assemblyArea = hostingFace->CalculateArea();
	float assemblyLength = StateData.ObjectType == EObjectType::OTWallSegment ? FQuantitiesCollection::LengthOfWallFace(*hostingFace)
		: 0.0f;

	CachedQuantities.AddQuantity(assemblyGuid, 0.0f, assemblyLength, assemblyArea);
	CachedQuantities.AddLayersQuantity(LayerGeometries, assembly.Layers, assemblyGuid);
	GetWorld()->GetGameInstance<UModumateGameInstance>()->GetQuantitiesManager()->SetDirtyBit();
}

bool AMOIPlaneHostedObj::IsValidParentObjectType(EObjectType ParentObjectType) const
{
	return ParentObjectType == EObjectType::OTMetaPlane || ParentObjectType == EObjectType::OTMetaPlaneSpan;
}

void AMOIPlaneHostedObj::UpdateAlignmentTargets()
{
	const AMOIMetaPlaneSpan* parentSpan = Cast<const AMOIMetaPlaneSpan>(GetParentObject());

	if (parentSpan != nullptr && parentSpan->GetPerimeterFace()!=nullptr)
	{
		TSet<int32> targets;
		UModumateObjectStatics::GetMOIAlignmentTargets(Document, parentSpan, targets);
		CachedAlignmentTargets = targets.Array();
	}
	else
	{
		return;
	}

	// If we don't have any alignment data, we're done
	if (StateData.Alignment.subjectPZP.zoneId.IsEmpty())
	{
		return;
	}

	FPlane myPZP;
	UModumateObjectStatics::GetPlaneHostedZonePlane(this, StateData.Alignment.subjectPZP.zoneId, StateData.Alignment.subjectPZP.origin, StateData.Alignment.subjectPZP.displacement, myPZP);

	const AMOIPlaneHostedObj* target = Cast<AMOIPlaneHostedObj>(Document->GetObjectById(StateData.Alignment.targetPZP.moiId));
	float delta=0.0f;
	if (target != nullptr)
	{
		FPlane theirPZP;
		UModumateObjectStatics::GetPlaneHostedZonePlane(target, StateData.Alignment.targetPZP.zoneId, StateData.Alignment.targetPZP.origin, StateData.Alignment.targetPZP.displacement, theirPZP);
		delta = FVector::PointPlaneDist(theirPZP.GetOrigin(), myPZP.GetOrigin(), myPZP.GetNormal());
	}
	else
	{
		delta = FVector::PointPlaneDist(myPZP.GetOrigin(), parentSpan->GetPerimeterFace()->CachedPlane.GetOrigin(), myPZP.GetNormal());
	}

	InstanceData.Offset.type = EDimensionOffsetType::Custom;
	InstanceData.Offset.customValue = delta;
	StateData.CustomData.SaveStructData(InstanceData);
}

bool AMOIPlaneHostedObj::ToWebMOI(FWebMOI& OutMOI) const
{
	if (Super::ToWebMOI(OutMOI))
	{
		for (auto& layer : GetAssembly().Layers)
		{
			FWebMOIZone& zone = OutMOI.zones.AddDefaulted_GetRef();
			zone.id = layer.PresetZoneID;
			zone.displayName = layer.ZoneDisplayName;
		}

		for (auto& alignmentTarget : CachedAlignmentTargets)
		{
			const AModumateObjectInstance* MOI = Document->GetObjectById(alignmentTarget);
			if (ensure(MOI != nullptr))
			{
				FWebMOIAlignmentTarget& newTarget = OutMOI.alignmentTargets.AddDefaulted_GetRef();
				newTarget.moiId = MOI->ID;
				
				FBIMPresetCollection& presetCollection = Document->GetPresetCollection();
				const FBIMPresetInstance* preset = presetCollection.PresetFromGUID(MOI->GetStateData().AssemblyGUID);
				if (ensure(preset != nullptr))
				{
					newTarget.presetId = MOI->GetStateData().AssemblyGUID;
					newTarget.displayName = preset->DisplayName.ToString();
				}
				
				for (auto& layer : MOI->GetAssembly().Layers)
				{
					FWebMOIZone& newZone = newTarget.zones.AddDefaulted_GetRef();
					newZone.id = layer.PresetZoneID;
					newZone.displayName = layer.ZoneDisplayName;
				}
			}
		}

		return true;
	}
	return false;
}


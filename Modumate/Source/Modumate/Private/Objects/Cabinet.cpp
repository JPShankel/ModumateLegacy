// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "Objects/Cabinet.h"

#include "Algo/Accumulate.h"
#include "DocumentManagement/ModumateCommands.h"
#include "Drafting/ModumateClippingTriangles.h"
#include "Drafting/ModumateDraftingElements.h"
#include "DrawDebugHelpers.h"
#include "ModumateCore/LayerGeomDef.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "ToolsAndAdjustments/Handles/AdjustPolyPointHandle.h"
#include "ToolsAndAdjustments/Handles/CabinetExtrusionHandle.h"
#include "ToolsAndAdjustments/Handles/CabinetFrontFaceHandle.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UI/AdjustmentHandleAssetData.h"
#include "UI/EditModelPlayerHUD.h"

using namespace Modumate::Units;

FMOICabinetImpl::FMOICabinetImpl(FModumateObjectInstance *moi)
	: FModumateObjectInstanceImplBase(moi)
	, AdjustmentHandlesVisible(false)
	, CachedExtrusionDelta(ForceInitToZero)
	, bCurrentFaceValid(false)
	, bBaseIsRectangular(false)
{
}

FMOICabinetImpl::~FMOICabinetImpl()
{
}

FVector FMOICabinetImpl::GetCorner(int32 index) const
{
	int32 numBasePoints = CachedBasePoints.Num();
	if ((numBasePoints < 3) || (index < 0) || (index >= (2 * numBasePoints)))
	{
		return GetLocation();
	}

	bool bIsBasePoint = index < numBasePoints;
	const FVector& basePoint = CachedBasePoints[index % numBasePoints];

	return basePoint + (bIsBasePoint ? FVector::ZeroVector : CachedExtrusionDelta);
}

int32 FMOICabinetImpl::GetNumCorners() const
{
	return CachedBasePoints.Num();
}

void FMOICabinetImpl::GetTypedInstanceData(UScriptStruct*& OutStructDef, void*& OutStructPtr)
{
	OutStructDef = InstanceData.StaticStruct();
	OutStructPtr = &InstanceData;
}

FVector FMOICabinetImpl::GetNormal() const
{
	return CachedBaseOrigin.GetRotation().GetAxisZ();
}

bool FMOICabinetImpl::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	if (MOI == nullptr)
	{
		return false;
	}

	switch (DirtyFlag)
	{
	case EObjectDirtyFlags::Structure:
	{
		if (!UpdateCachedGeometryData())
		{
			return false;
		}

		SetupDynamicGeometry();
	}
		break;
	case EObjectDirtyFlags::Visuals:
		MOI->UpdateVisibilityAndCollision();
		break;
	default:
		break;
	}

	return true;
}

void FMOICabinetImpl::UpdateVisibilityAndCollision(bool &bOutVisible, bool &bOutCollisionEnabled)
{
	FModumateObjectInstanceImplBase::UpdateVisibilityAndCollision(bOutVisible, bOutCollisionEnabled);

	if (FrontFacePortalActor.IsValid())
	{
		bool bHasFrontFace = (InstanceData.FrontFaceIndex != INDEX_NONE) && bCurrentFaceValid;
		FrontFacePortalActor->SetActorHiddenInGame(!bHasFrontFace || !bOutVisible);
		FrontFacePortalActor->SetActorEnableCollision(bHasFrontFace && bOutCollisionEnabled);
	}
}

void FMOICabinetImpl::SetupDynamicGeometry()
{
	if (CachedBasePoints.Num() == 0)
	{
		return;
	}

	bool bUpdateCollision = !MOI->IsInPreviewMode();
	bool bEnableCollision = !MOI->IsInPreviewMode();

	if (!FrontFacePortalActor.IsValid())
	{
		FrontFacePortalActor = MOI->GetWorld()->SpawnActor<ACompoundMeshActor>(ACompoundMeshActor::StaticClass());
		FrontFacePortalActor->AttachToActor(DynamicMeshActor.Get(), FAttachmentTransformRules::SnapToTargetIncludingScale);
	}

	UpdateCabinetActors(MOI->GetAssembly(), CachedBasePoints, CachedExtrusionDelta, InstanceData.FrontFaceIndex,
		InstanceData.bFrontFaceLateralInverted, bUpdateCollision, bEnableCollision,
		DynamicMeshActor.Get(), FrontFacePortalActor.Get(), bCurrentFaceValid);

	// refresh handle visibility, don't destroy & recreate handles
	AEditModelPlayerController_CPP *controller = DynamicMeshActor->GetWorld()->GetFirstPlayerController<AEditModelPlayerController_CPP>();

	// TODO: revisit the handle paradigm for cabinets
	MOI->ShowAdjustmentHandles(controller, AdjustmentHandlesVisible);
}

void FMOICabinetImpl::GetStructuralPointsAndLines(TArray<FStructurePoint>& OutPoints, TArray<FStructureLine>& OutLines, bool bForSnapping, bool bForSelection) const
{
	int32 numBasePoints = CachedBasePoints.Num();
	for (int32 baseIdxStart = 0; baseIdxStart < numBasePoints; ++baseIdxStart)
	{
		int32 baseIdxEnd = (baseIdxStart + 1) % numBasePoints;
		const FVector& baseEdgeStart = CachedBasePoints[baseIdxStart];
		const FVector& baseEdgeEnd = CachedBasePoints[baseIdxEnd];
		FVector baseEdgeDir = (baseEdgeEnd - baseEdgeStart).GetSafeNormal();
		FVector topEdgeStart = baseEdgeStart + CachedExtrusionDelta;
		FVector topEdgeEnd = baseEdgeEnd + CachedExtrusionDelta;

		OutPoints.Add(FStructurePoint(baseEdgeStart, baseEdgeDir, baseIdxStart));
		OutPoints.Add(FStructurePoint(topEdgeStart, baseEdgeDir, baseIdxStart));

		OutLines.Add(FStructureLine(baseEdgeStart, baseEdgeEnd, baseIdxStart, baseIdxEnd));
		OutLines.Add(FStructureLine(baseEdgeStart, topEdgeStart, baseIdxStart, baseIdxStart + numBasePoints));
		OutLines.Add(FStructureLine(topEdgeStart, topEdgeEnd, baseIdxStart + numBasePoints, baseIdxEnd + numBasePoints));
	}
}

bool FMOICabinetImpl::GetInvertedState(FMOIStateData& OutState) const
{
	OutState = MOI->GetStateData();

	FMOICabinetData modifiedCabinetData = InstanceData;
	modifiedCabinetData.bFrontFaceLateralInverted = !modifiedCabinetData.bFrontFaceLateralInverted;

	return OutState.CustomData.SaveStructData(modifiedCabinetData);
}

void FMOICabinetImpl::RectangleCheck(const TArray<FVector>& Points, bool& bOutIsRectangular, FVector& OutMostVerticalDir)
{
	bOutIsRectangular = false;
	OutMostVerticalDir = FVector::ZeroVector;

	// Quadrilateral check
	int32 numPoints = Points.Num();
	if (numPoints != 4)
	{
		return;
	}

	for (int32 pointIdx = 0; pointIdx < 4; ++pointIdx)
	{
		const FVector& edgeStartPoint = Points[pointIdx];
		const FVector& edgeEndPoint = Points[(pointIdx + 1) % numPoints];
		const FVector& edgePrevPoint = Points[(pointIdx - 1 + numPoints) % numPoints];
		FVector edgeCurDir = (edgeEndPoint - edgeStartPoint).GetSafeNormal();
		FVector edgePrevDir = (edgeStartPoint - edgePrevPoint).GetSafeNormal();

		// Angle check
		if (!FVector::Orthogonal(edgeCurDir, edgePrevDir))
		{
			return;
		}

		// Find the most vertical edge for the up-axis of the rectangle
		if (OutMostVerticalDir.IsNearlyZero() || (FMath::Abs(edgeCurDir.Z) > FMath::Abs(OutMostVerticalDir.Z)))
		{
			OutMostVerticalDir = edgeCurDir;
		}
	}

	bOutIsRectangular = true;
}

bool FMOICabinetImpl::GetFaceGeometry(const TArray<FVector>& BasePoints, const FVector& ExtrusionDelta, int32 FaceIndex,
	TArray<FVector>& OutFacePoints, FTransform& OutFaceTransform)
{
	OutFacePoints.Reset();
	OutFaceTransform.SetIdentity();

	int32 numBasePoints = BasePoints.Num();
	if ((FaceIndex == INDEX_NONE) || (FaceIndex > numBasePoints))
	{
		return false;
	}

	FVector faceNormal(ForceInitToZero), faceUp(ForceInitToZero);
	FVector extrusionDir = ExtrusionDelta.GetSafeNormal();

	// If the face is on one of the rectangular sides of the prism
	if (FaceIndex < numBasePoints)
	{
		const FVector& baseEdgeStartPoint = BasePoints[FaceIndex];
		const FVector& baseEdgeEndPoint = BasePoints[(FaceIndex + 1) % numBasePoints];
		FVector baseEdgeDir = (baseEdgeEndPoint - baseEdgeStartPoint).GetSafeNormal();

		OutFacePoints.Add(baseEdgeStartPoint);
		OutFacePoints.Add(baseEdgeEndPoint);
		OutFacePoints.Add(baseEdgeEndPoint + ExtrusionDelta);
		OutFacePoints.Add(baseEdgeStartPoint + ExtrusionDelta);

		FPlane facePlane;
		if (!UModumateGeometryStatics::GetPlaneFromPoints(OutFacePoints, facePlane))
		{
			return false;
		}

		faceNormal = extrusionDir ^ baseEdgeDir;
		if ((faceNormal | facePlane) < 0.0f)
		{
			faceNormal *= -1.0f;
		}

		faceUp = (FMath::Abs(baseEdgeDir.Z) > FMath::Abs(extrusionDir.Z)) ? baseEdgeDir : extrusionDir;
	}
	// If the face is on the extruded face of the prism
	else
	{
		faceNormal = extrusionDir;

		bool bIsRectangular;
		RectangleCheck(BasePoints, bIsRectangular, faceUp);
		if (!bIsRectangular)
		{
			return false;
		}

		for (const FVector& basePoint : BasePoints)
		{
			// Use the extruded point for the face point
			OutFacePoints.Add(basePoint + ExtrusionDelta);
		}
	}

	if (!faceUp.IsNormalized())
	{
		return false;
	}
	else if (faceUp.Z < 0.0f)
	{
		faceUp *= -1.0f;
	}

	OutFaceTransform.SetRotation(FRotationMatrix::MakeFromYZ(faceNormal, faceUp).ToQuat());

	FVector faceCenter = 0.25f * Algo::Accumulate(OutFacePoints, FVector::ZeroVector);
	FVector faceAxisX = OutFaceTransform.GetUnitAxis(EAxis::X);

	FBox2D faceRelativeBox(ForceInitToZero);
	for (const FVector& facePoint : OutFacePoints)
	{
		faceRelativeBox += UModumateGeometryStatics::ProjectPoint2D(facePoint, faceAxisX, faceUp, faceCenter);
	}

	FVector2D faceSize2D = faceRelativeBox.GetSize();
	OutFaceTransform.SetScale3D(FVector(faceSize2D.X, 1.0f, faceSize2D.Y));

	FVector faceFlatSize = (faceSize2D.X * faceAxisX) + (faceSize2D.Y * faceUp);
	FVector faceOrigin = faceCenter - 0.5f * faceFlatSize;
	OutFaceTransform.SetLocation(faceOrigin);

	return true;
}

bool FMOICabinetImpl::UpdateCabinetActors(const FBIMAssemblySpec& Assembly, const TArray<FVector>& InBasePoints, const FVector& InExtrusionDelta,
	int32 FrontFaceIndex, bool bFaceLateralInverted, bool bUpdateCollision, bool bEnableCollision,
	ADynamicMeshActor* CabinetBoxActor, ACompoundMeshActor* CabinetFaceActor, bool& bOutFaceValid)
{
	bOutFaceValid = false;
	int32 numBasePoints = InBasePoints.Num();
	auto& extrusions = Assembly.Extrusions;
	if (!ensure((extrusions.Num() == 1) && (numBasePoints >= 3) && !InExtrusionDelta.IsNearlyZero() && (FrontFaceIndex <= numBasePoints) && CabinetBoxActor && CabinetFaceActor))
	{
		return false;
	}

	// Get enough geometric information to set up the portal assembly and toe kick, if specified
	TArray<FVector> basePoints = InBasePoints;
	FVector extrusionDelta = InExtrusionDelta;
	float extrusionDist = extrusionDelta.Size();
	FVector extrusionDir = extrusionDelta / extrusionDist;
	FPlane basePlane(basePoints[0], extrusionDir);
	UWorld* world = CabinetBoxActor->GetWorld();

	FVector2D toeKickDimensions(Assembly.ToeKickDepth.AsWorldCentimeters(), Assembly.ToeKickHeight.AsWorldCentimeters());
	float faceInsetDist = 0.0f;

	TArray<FVector> facePoints;
	FTransform faceTransform;
	bOutFaceValid = (FrontFaceIndex != INDEX_NONE) && FMOICabinetImpl::GetFaceGeometry(basePoints, extrusionDelta, FrontFaceIndex, facePoints, faceTransform);

	FVector faceOrigin = faceTransform.GetLocation();
	FQuat faceRot = faceTransform.GetRotation();
	FVector faceNormal = faceRot.GetAxisY();
	FVector faceUp = faceRot.GetAxisZ();
	FVector faceScale3D = faceTransform.GetScale3D();
	FVector portalOrigin(ForceInitToZero);

	if (bOutFaceValid)
	{
		// If the front face is on one of the extruded sides, then potentially re-orient the base points and front index if the face's bottom isn't along the base
		// (i.e. a vertical-wall-mounted cabinet, flush with the floor, whose front face is assigned to one of the vertical sides with a toe kick)
		if (FrontFaceIndex < numBasePoints)
		{
			if (!FVector::Coincident(extrusionDir, faceUp))
			{
				// Find the point in the original base points that corresponds to the face origin, the bottom of the face
				FVector projectedFaceOrigin = FVector::PointPlaneProject(faceOrigin, basePlane);
				int32 faceOriginBaseIdx = INDEX_NONE;
				for (int32 basePointIdx = 0; basePointIdx < numBasePoints; ++basePointIdx)
				{
					auto& basePoint = basePoints[basePointIdx];
					if (basePoint.Equals(projectedFaceOrigin, PLANAR_DOT_EPSILON))
					{
						faceOriginBaseIdx = basePointIdx;
						break;
					}
				}

				// We expect to find a point that's close enough on the base, regardless of the base shape
				if (!ensure(faceOriginBaseIdx != INDEX_NONE))
				{
					return false;
				}

				// From that base point, find which direction to traverse around the base corresponds to a valid cabinet box bottom, if any.
				int32 prevBaseIdx = (faceOriginBaseIdx - 1 + numBasePoints) % numBasePoints;
				int32 nextBaseIdx = (faceOriginBaseIdx + 1) % numBasePoints;
				const FVector& faceOriginBasePoint = basePoints[faceOriginBaseIdx];
				const FVector& prevBasePoint = basePoints[prevBaseIdx];
				const FVector& nextBasePoint = basePoints[nextBaseIdx];
				FVector prevBaseDelta = (prevBasePoint - faceOriginBasePoint);
				FVector nextBaseDelta = (nextBasePoint - faceOriginBasePoint);
				FVector prevBaseDir = prevBaseDelta.GetSafeNormal();
				FVector nextBaseDir = nextBaseDelta.GetSafeNormal();
				FVector newBaseBottomDelta(ForceInitToZero);

				if (FVector::Coincident(prevBaseDir, faceUp) && FVector::Coincident(nextBaseDir, -faceNormal))
				{
					newBaseBottomDelta = nextBaseDelta;
				}
				else if (FVector::Coincident(nextBaseDir, faceUp) && FVector::Coincident(prevBaseDir, -faceNormal))
				{
					newBaseBottomDelta = prevBaseDelta;
				}

				// If we didn't find a valid direction, then the original cabinet base host polygon is not rectangular, and cannot host a toe kick.
				// We can extrude it from the base anyway, but we'll have to zero out the toe kick dimensions in order to not have an error.
				if (newBaseBottomDelta.IsNearlyZero())
				{
					toeKickDimensions.Set(0.0f, 0.0f);
				}
				// Otherwise, we can re-orient the base points and front index in order to extrude the cabinet from the new bottom,
				// orthogonal with both the selected cabinet face and the original cabinet base host polygon.
				else
				{
					basePoints = {
						faceOriginBasePoint,
						faceOriginBasePoint + extrusionDelta,
						faceOriginBasePoint + newBaseBottomDelta + extrusionDelta,
						faceOriginBasePoint + newBaseBottomDelta,
					};
					extrusionDelta = faceUp * faceScale3D.Z;
					FrontFaceIndex = 0;
				}
			}
		}
		// Otherwise, if the new front face is considered the "top" of the extruded base polygon, then create the base points as bottom rectangular extruded face
		else
		{
			FVector frontFaceDeltaX = faceRot.GetAxisX() * faceScale3D.X;

			basePoints = {
				faceOrigin,
				faceOrigin + frontFaceDeltaX,
				faceOrigin + frontFaceDeltaX - extrusionDelta,
				faceOrigin - extrusionDelta
			};
			extrusionDelta = faceUp * faceScale3D.Z;
			FrontFaceIndex = 0;
		}

		FVector portalNativeSize = Assembly.GetRiggedAssemblyNativeSize();
		if (portalNativeSize.GetMin() < KINDA_SMALL_NUMBER)
		{
			return false;
		}

		FVector portalDesiredSize(faceScale3D.X, portalNativeSize.Y, faceScale3D.Z - toeKickDimensions.Y);
		if (portalDesiredSize.GetMin() < KINDA_SMALL_NUMBER)
		{
			return false;
		}

		// Now make the portal's parts from the cabinet's combined assembly
		FVector portalScale = portalDesiredSize / portalNativeSize;
		CabinetFaceActor->MakeFromAssembly(Assembly, portalScale, bFaceLateralInverted, bUpdateCollision && bEnableCollision);

		// Update the face inset distance, now that it's been calculated by the cabinet's part layout
		// TODO: update the part layout (if necessary and not already cached) before having to fully update the whole portal actor.
		faceInsetDist = CabinetFaceActor->CachedPartLayout.CabinetPanelAssemblyConceptualSizeY;

		// Position the portal where it's supposed to go, now that toe kick and face inset have been calculated
		FVector toeKickOffset = toeKickDimensions.Y * faceUp;
		FVector insetDelta = -faceInsetDist * faceNormal;
		portalOrigin = toeKickOffset + faceOrigin + insetDelta;
	}
	else
	{
		FrontFaceIndex = INDEX_NONE;
	}

	// Set up the cabinet box geometry, which will recenter the actor around its base points
	auto& extrusion = extrusions[0];
	CabinetBoxActor->SetupCabinetGeometry(basePoints, extrusionDelta, extrusion.Material, bUpdateCollision, bEnableCollision,
		toeKickDimensions, faceInsetDist, FrontFaceIndex);

	// Then, either position the newly-set up cabinet face actor, or hide it if it's disabled, since it's attached to the cabinet box actor.
	if (bOutFaceValid)
	{
		CabinetFaceActor->SetActorLocationAndRotation(portalOrigin, faceRot);
	}
	else
	{
		CabinetFaceActor->SetActorHiddenInGame(true);
		CabinetFaceActor->SetActorEnableCollision(false);
	}

	return true;
}

bool FMOICabinetImpl::UpdateCachedGeometryData()
{
	const FModumateObjectInstance* parentObj = MOI ? MOI->GetParentObject() : nullptr;
	if (parentObj && (parentObj->GetObjectType() == EObjectType::OTSurfacePolygon))
	{
		CachedBaseOrigin = parentObj->GetWorldTransform();

		int32 numBasePoints = parentObj->GetNumCorners();
		CachedBasePoints.Reset(numBasePoints);
		for (int32 cornerIdx = 0; cornerIdx < numBasePoints; ++cornerIdx)
		{
			CachedBasePoints.Add(parentObj->GetCorner(cornerIdx));
		}

		FVector mostVerticalEdge;
		RectangleCheck(CachedBasePoints, bBaseIsRectangular, mostVerticalEdge);

		CachedExtrusionDelta = InstanceData.ExtrusionDist * GetNormal();

		return true;
	}

	return false;
}

void FMOICabinetImpl::SetupAdjustmentHandles(AEditModelPlayerController_CPP *controller)
{
	int32 numBasePoints = CachedBasePoints.Num();
	for (int32 i = 0; i <= numBasePoints; ++i)
	{
		auto selectFrontHandle = MOI->MakeHandle<ACabinetFrontFaceHandle>();
		selectFrontHandle->SetTargetIndex(i);

		FrontSelectionHandles.Add(selectFrontHandle);
	}

	auto topExtrusionHandle = MOI->MakeHandle<ACabinetExtrusionHandle>();
	topExtrusionHandle->SetSign(1.0f);

	// parent handles
	FModumateObjectInstance *parent = MOI->GetParentObject();
	if (!ensureAlways(parent && (parent->GetObjectType() == EObjectType::OTSurfacePolygon)))
	{
		return;
	}

	int32 numCorners = parent->GetNumCorners();
	for (int32 i = 0; i < numCorners; ++i)
	{
		auto cornerHandle = MOI->MakeHandle<AAdjustPolyPointHandle>();
		cornerHandle->SetTargetIndex(i);
		cornerHandle->SetTargetMOI(parent);

		auto edgeHandle = MOI->MakeHandle<AAdjustPolyPointHandle>();
		edgeHandle->SetTargetIndex(i);
		edgeHandle->SetAdjustPolyEdge(true);
		edgeHandle->SetTargetMOI(parent);
	}
}

void FMOICabinetImpl::ShowAdjustmentHandles(AEditModelPlayerController_CPP *Controller, bool bShow)
{
	FModumateObjectInstanceImplBase::ShowAdjustmentHandles(Controller, bShow);

	AdjustmentHandlesVisible = bShow;

	// make sure that front selection handles are only visible if they are valid choices
	for (auto frontHandle : FrontSelectionHandles)
	{
		if (frontHandle.IsValid())
		{
			// Show a handle if requested, and:
			// - either a front hasn't been set or the handle is the front, and
			// - either the handle targets the side of the cabinet, or the top of the cabinet is rectangular
			bool bHandleEnabled = bShow &&
				((InstanceData.FrontFaceIndex == INDEX_NONE) || (InstanceData.FrontFaceIndex == frontHandle->TargetIndex)) &&
				((frontHandle->TargetIndex != CachedBasePoints.Num()) || bBaseIsRectangular);

			frontHandle->SetEnabled(bHandleEnabled);
		}
	}
}

using namespace Modumate;

void FMOICabinetImpl::GetDraftingLines(const TSharedPtr<FDraftingComposite> &ParentPage, const FPlane &Plane,
	const FVector &AxisX, const FVector &AxisY, const FVector &Origin, const FBox2D &BoundingBox,
	TArray<TArray<FVector>> &OutPerimeters) const
{
	static const Units::FThickness lineThickness = FThickness::Points(0.25f);
	static const FMColor lineColor = FMColor::Black;
	static FModumateLayerType dwgLayerType = FModumateLayerType::kCabinetCutCarcass;
	OutPerimeters.Reset();

	const ADynamicMeshActor * actor = DynamicMeshActor.Get();
	const TArray<FLayerGeomDef>& LayerGeometries = actor->LayerGeometries;
	const FVector actorOrigin = actor->GetActorLocation();

	const bool bGetFarLines = ParentPage->lineClipping.IsValid();
	if (!bGetFarLines)
	{   // Cut-plane lines:
		TArray<TPair<FVector2D, FVector2D>> cutPlaneLines;
		TArray<FVector2D> previousPoints;
		int32 numLayerSides = LayerGeometries.Num() * 2;
		for (int32 layerIdx = 0; layerIdx < numLayerSides; layerIdx++)
		{
			bool usePointsA = layerIdx % 2 == 0;
			const TArray<FVector> layerPoints = usePointsA ? LayerGeometries[layerIdx / 2].OriginalPointsA : LayerGeometries[layerIdx / 2].OriginalPointsB;
			TArray<FVector2D> intersections;
			int32 numPoints = layerPoints.Num();
			for (int32 point = 0; point < numPoints; ++point)
			{
				FVector vertex1 = actorOrigin + layerPoints[point];
				FVector vertex2 = actorOrigin + layerPoints[(point + 1) % numPoints];

				FVector intersection;
				if (FMath::SegmentPlaneIntersection(vertex1, vertex2, Plane, intersection))
				{
					intersections.Add(UModumateGeometryStatics::ProjectPoint2D(Origin, -AxisX, -AxisY, intersection));
				}
			}

			const int numIntersections = intersections.Num();
			if (numIntersections % 2 == 0)
			{
				for (int32 line = 0; line < numIntersections; line += 2)
				{
					cutPlaneLines.Emplace(intersections[line], intersections[line + 1]);
				}
			}

			if (usePointsA)
			{
				previousPoints = intersections;
			}
			else
			{
				for (int32 p = 0; p < intersections.Num() && p < previousPoints.Num(); ++p)
				{
					if (p < previousPoints.Num())
					{
						cutPlaneLines.Emplace(previousPoints[p], intersections[p]);
					}
				}
			}
		}

		for (const auto& line : cutPlaneLines)
		{
			FVector2D lineStart(line.Key);
			FVector2D lineEnd(line.Value);
			FVector2D clippedStart;
			FVector2D clippedEnd;

			if (UModumateFunctionLibrary::ClipLine2DToRectangle(lineStart, lineEnd, BoundingBox, clippedStart, clippedEnd))
			{
				TSharedPtr<Modumate::FDraftingLine> clippedLine = MakeShared<Modumate::FDraftingLine>(
					Modumate::Units::FCoordinates2D::WorldCentimeters(clippedStart),
					Modumate::Units::FCoordinates2D::WorldCentimeters(clippedEnd),
					lineThickness, lineColor);
				ParentPage->Children.Add(clippedLine);
				clippedLine->SetLayerTypeRecursive(dwgLayerType);
			}
		}

		// Cabinet door:
		if (FrontFacePortalActor.IsValid())
		{
			FrontFacePortalActor->GetCutPlaneDraftingLines(ParentPage, Plane, AxisX, AxisY, Origin);
		}
	}
	else
	{   // Beyond lines:
		TArray<FEdge> cabinetEdges;

		int32 numLayers = LayerGeometries.Num();
		for (const auto& layer : LayerGeometries)
		{
			int32 numPoints = layer.OriginalPointsA.Num();
			for (int point = 0; point < numPoints; ++point)
			{
				cabinetEdges.Emplace(actorOrigin + layer.OriginalPointsA[point], actorOrigin + layer.OriginalPointsA[(point + 1) % numPoints]);
				cabinetEdges.Emplace(actorOrigin + layer.OriginalPointsB[point], actorOrigin + layer.OriginalPointsB[(point + 1) % numPoints]);
				cabinetEdges.Emplace(actorOrigin + layer.OriginalPointsA[point], actorOrigin + layer.OriginalPointsB[point]);
			}
		}

		for (const auto& edge : cabinetEdges)
		{
			TArray<FEdge> viewLines = ParentPage->lineClipping->ClipWorldLineToView(edge);
			for (const auto& viewLine : viewLines)
			{
				FVector2D start(viewLine.Vertex[0]);
				FVector2D end(viewLine.Vertex[1]);
				FVector2D boxClipped0;
				FVector2D boxClipped1;

				if (UModumateFunctionLibrary::ClipLine2DToRectangle(start, end, BoundingBox, boxClipped0, boxClipped1))
				{

					TSharedPtr<FDraftingLine> line = MakeShared<FDraftingLine>(
						Units::FCoordinates2D::WorldCentimeters(boxClipped0),
						Units::FCoordinates2D::WorldCentimeters(boxClipped1),
						Units::FThickness::Points(0.15f), FMColor::Gray144);
					ParentPage->Children.Add(line);
					line->SetLayerTypeRecursive(FModumateLayerType::kCabinetBeyond);
				}

			}
		}

		if (FrontFacePortalActor.IsValid())
		{
			FrontFacePortalActor->GetFarDraftingLines(ParentPage, Plane, BoundingBox);
		}

	}
}

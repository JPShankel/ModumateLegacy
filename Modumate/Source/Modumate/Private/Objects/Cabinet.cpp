// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "Objects/Cabinet.h"

#include "DocumentManagement/ModumateCommands.h"
#include "Drafting/ModumateClippingTriangles.h"
#include "Drafting/ModumateDraftingElements.h"
#include "DrawDebugHelpers.h"
#include "Math/ConvexHull2d.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "ToolsAndAdjustments/Handles/AdjustPolyPointHandle.h"
#include "ToolsAndAdjustments/Handles/AdjustPolyExtrusionHandle.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UI/AdjustmentHandleAssetData.h"
#include "UI/EditModelPlayerHUD.h"

using namespace Modumate::Units;

FName FMOICabinetImpl::CabinetGeometryMatName(TEXT("Cabinet_Exterior_Finish"));

FMOICabinetImpl::FMOICabinetImpl(FModumateObjectInstance *moi)
	: FModumateObjectInstanceImplBase(moi)
	, AdjustmentHandlesVisible(false)
	, ToeKickDimensions(ForceInitToZero)
	, CachedExtrusionDelta(ForceInitToZero)
{
}

FMOICabinetImpl::~FMOICabinetImpl()
{
}

FVector FMOICabinetImpl::GetCorner(int32 index) const
{
	int32 numBasePoints = CachedBasePoints.Num();
	if ((numBasePoints < 3) || (index < 0) || (index >= GetNumCorners()))
	{
		return GetLocation();
	}

	bool bIsBasePoint = index < numBasePoints;
	const FVector& basePoint = CachedBasePoints[index % numBasePoints];

	return basePoint + (bIsBasePoint ? FVector::ZeroVector : CachedExtrusionDelta);
}

int32 FMOICabinetImpl::GetNumCorners() const
{
	return CachedBasePoints.Num() * 2;
}

FVector FMOICabinetImpl::GetNormal() const
{
	return CachedBaseOrigin.GetRotation().GetAxisZ();
}

bool FMOICabinetImpl::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<TSharedPtr<Modumate::FDelta>>* OutSideEffectDeltas)
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
		FrontFacePortalActor->SetActorHiddenInGame(!bOutVisible);
		FrontFacePortalActor->SetActorEnableCollision(bOutCollisionEnabled);
	}
}

void FMOICabinetImpl::SetupDynamicGeometry()
{
	if (CachedBasePoints.Num() == 0)
	{
		return;
	}

	int32 frontFaceIndex = (MOI->GetControlPointIndices().Num() > 0) ? MOI->GetControlPointIndex(0) : INDEX_NONE;

	// TODO: get material from assembly spec
	bool bUpdateCollision = !MOI->GetIsInPreviewMode();
	bool bEnableCollision = !MOI->GetIsInPreviewMode();
	DynamicMeshActor->SetupCabinetGeometry(CachedBasePoints, CachedExtrusionDelta, FArchitecturalMaterial(), bUpdateCollision, bEnableCollision,
		ToeKickDimensions, frontFaceIndex);

	// refresh handle visibility, don't destroy & recreate handles
	AEditModelPlayerController_CPP *controller = DynamicMeshActor->GetWorld()->GetFirstPlayerController<AEditModelPlayerController_CPP>();
	// TODO: revisit the handle paradigm for cabinets
	MOI->ShowAdjustmentHandles(controller, AdjustmentHandlesVisible);

	UpdateCabinetPortal();
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

		CachedExtrusionDelta = MOI->GetExtents().Y * GetNormal();
		UModumateFunctionLibrary::GetCabinetToeKickDimensions(MOI->GetAssembly(), ToeKickDimensions);

		return true;
	}

	return false;
}

void FMOICabinetImpl::UpdateCabinetPortal()
{
	int32 frontFaceIndex = (MOI->GetControlPointIndices().Num() > 0) ? MOI->GetControlPointIndex(0) : INDEX_NONE;
	if ((frontFaceIndex < 0) && FrontFacePortalActor.IsValid())
	{
		FrontFacePortalActor->Destroy();
	}
	else if ((frontFaceIndex >= 0) && !FrontFacePortalActor.IsValid())
	{
		FrontFacePortalActor = DynamicMeshActor->GetWorld()->SpawnActor<ACompoundMeshActor>(ACompoundMeshActor::StaticClass());
		FrontFacePortalActor->AttachToActor(DynamicMeshActor.Get(), FAttachmentTransformRules::SnapToTargetIncludingScale);
	}

	if (!FrontFacePortalActor.IsValid())
	{
		return;
	}

	// Get enough geometric information to set up the portal assembly
	FVector extrusionDir = GetNormal();
	FPlane plane;
	if (!UModumateGeometryStatics::GetPlaneFromPoints(CachedBasePoints, plane))
	{
		return;
	}
	if (!FVector::Parallel(plane, extrusionDir))
	{
		return;
	}
	bool bCoincident = FVector::Coincident(FVector(plane), extrusionDir);

	const FVector &p1 = CachedBasePoints[frontFaceIndex];
	const FVector &p2 = CachedBasePoints[(frontFaceIndex + 1) % CachedBasePoints.Num()];
	FVector edgeDelta = p2 - p1;
	float edgeLength = edgeDelta.Size();
	if (!ensureAlways(!FMath::IsNearlyZero(edgeLength)))
	{
		return;
	}

	FrontFacePortalActor->MakeFromAssembly(MOI->GetAssembly(), FVector::OneVector, MOI->GetObjectInverted(), true);

	// Now position the portal where it's supposed to go
	FVector edgeDir = edgeDelta / edgeLength;
	FVector faceNormal = (edgeDir ^ extrusionDir) * (bCoincident ? 1.0f : -1.0f);
	FVector toeKickOffset = ToeKickDimensions.Y * extrusionDir;
	FVector portalOrigin = toeKickOffset + (bCoincident ? p2 : p1);
	FQuat portalRot = FRotationMatrix::MakeFromYZ(faceNormal, extrusionDir).ToQuat();

	FrontFacePortalActor->SetActorLocationAndRotation(portalOrigin, portalRot);
}

void FMOICabinetImpl::SetupAdjustmentHandles(AEditModelPlayerController_CPP *controller)
{
	int32 numBasePoints = CachedBasePoints.Num();
	for (int32 i = 0; i < numBasePoints; ++i)
	{
		// TODO: show the adjustment handles for the underlying SurfacePolygon, so its shape can stretch without going into SurfaceGraph view mode

		auto selectFrontHandle = MOI->MakeHandle<ASelectCabinetFrontHandle>();
		selectFrontHandle->SetTargetIndex(i);

		FrontSelectionHandles.Add(selectFrontHandle);
	}

	auto topExtrusionHandle = MOI->MakeHandle<AAdjustPolyExtrusionHandle>();
	topExtrusionHandle->SetSign(1.0f);
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
			bool bHandleEnabled = bShow && ((MOI->GetControlPointIndices().Num() == 0) || (MOI->GetControlPointIndex(0) == frontHandle->TargetIndex));
			frontHandle->SetEnabled(bHandleEnabled);
		}
	}
}

using namespace Modumate;

const float ASelectCabinetFrontHandle::FaceCenterHeightOffset = 20.0f;

bool ASelectCabinetFrontHandle::BeginUse()
{
	if (!Super::BeginUse())
	{
		return false;
	}

	TArray<int32> newControlIndices;

	if ((TargetMOI->GetControlPointIndices().Num() == 0) || (TargetMOI->GetControlPointIndex(0) != TargetIndex))
	{
		newControlIndices.Add(TargetIndex);
	}

	TargetMOI->SetControlPointIndices(newControlIndices);

	EndUse();

	TArray<TSharedPtr<FDelta>> deltas;
	deltas.Add(MakeShareable(new FMOIDelta({ TargetMOI })));
	AEditModelGameState_CPP* gameState = Controller->GetWorld()->GetGameState<AEditModelGameState_CPP>();
	FModumateDocument* doc = &gameState->Document;

	return doc->ApplyDeltas(deltas, GetWorld());
}

FVector ASelectCabinetFrontHandle::GetHandlePosition() const
{
	const FModumateObjectInstance* cabinetParent = TargetMOI ? TargetMOI->GetParentObject() : nullptr;
	if (!ensure(cabinetParent && (cabinetParent->GetObjectType() == EObjectType::OTSurfacePolygon)))
	{
		return FVector::ZeroVector;
	}

	int32 numBasePoints = cabinetParent->GetNumCorners();
	const auto &points = TargetMOI->GetControlPoints();
	FVector edgeCenter = 0.5f * (TargetMOI->GetCorner(TargetIndex) + TargetMOI->GetCorner((TargetIndex + 1) % numBasePoints));
	FVector extrusionDelta = ((0.5f * TargetMOI->GetExtents().Y) + FaceCenterHeightOffset) * TargetMOI->GetNormal();
	return edgeCenter + extrusionDelta;
}

bool ASelectCabinetFrontHandle::GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const
{
	OutButtonStyle = PlayerHUD->HandleAssets->CabinetFrontFaceStyle;
	return true;
}

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

	const bool bGetFarLines = ParentPage->lineClipping.IsValid();
	if (!bGetFarLines)
	{   // Cut-plane lines:
		TArray<TPair<FVector2D, FVector2D>> cutPlaneLines;
		TArray<FVector2D> previousPoints;
		int32 numLayerSides = LayerGeometries.Num() * 2;
		for (int32 layerIdx = 0; layerIdx < numLayerSides; layerIdx++)
		{
			bool usePointsA = layerIdx % 2 == 0;
			const TArray<FVector> layerPoints = usePointsA ? LayerGeometries[layerIdx / 2].PointsA : LayerGeometries[layerIdx / 2].PointsB;
			TArray<FVector2D> intersections;
			int32 numPoints = layerPoints.Num();
			for (int32 point = 0; point < numPoints; ++point)
			{
				FVector vertex1 = layerPoints[point];
				FVector vertex2 = layerPoints[(point + 1) % numPoints];

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
				TSharedPtr<Modumate::FDraftingLine> clippedLine = MakeShareable(new Modumate::FDraftingLine(
					Modumate::Units::FCoordinates2D::WorldCentimeters(clippedStart),
					Modumate::Units::FCoordinates2D::WorldCentimeters(clippedEnd),
					lineThickness, lineColor));
				ParentPage->Children.Add(clippedLine);
				clippedLine->SetLayerTypeRecursive(dwgLayerType);
			}
		}
	}
	else
	{   // Beyond lines:
		TArray<FEdge> cabinetEdges;

		int32 numLayers = LayerGeometries.Num();
		for (const auto& layer : LayerGeometries)
		{
			int32 numPoints = layer.PointsA.Num();
			for (int point = 0; point < numPoints; ++point)
			{
				cabinetEdges.Emplace(layer.PointsA[point], layer.PointsA[(point + 1) % numPoints]);
				cabinetEdges.Emplace(layer.PointsB[point], layer.PointsB[(point + 1) % numPoints]);
				cabinetEdges.Emplace(layer.PointsA[point], layer.PointsB[point]);
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

					TSharedPtr<FDraftingLine> line = MakeShareable(new FDraftingLine(
						Units::FCoordinates2D::WorldCentimeters(boxClipped0),
						Units::FCoordinates2D::WorldCentimeters(boxClipped1),
						Units::FThickness::Points(0.15f), FMColor::Gray144));
					ParentPage->Children.Add(line);
					line->SetLayerTypeRecursive(FModumateLayerType::kCabinetBeyond);
				}

			}
		}

	}
}

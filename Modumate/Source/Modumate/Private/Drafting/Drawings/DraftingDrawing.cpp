#include "Drafting/Drawings/DraftingDrawing.h"
#include "Drafting/DraftingManager.h"
#include "Drafting/Drawings/DrawingArea.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Drafting/ModumateDraftingTags.h"
#include "Drafting/ModumateDimensions.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "Objects/ModumateObjectInstance.h"
#include "Algo/Copy.h"

#define LOCTEXT_NAMESPACE "ModumateDefaultDrawing"

FDraftingDrawing::FDraftingDrawing(const UModumateDocument *doc, UWorld *world, SceneCaptureID captureObjID) :
	Doc(doc),
	World(world),
	CaptureObjID(captureObjID)
{
	TSharedPtr<FDrawingArea> viewArea = MakeShareable(new FDrawingArea());
	Children.Add(viewArea);
	DrawingContent = TWeakPtr<FDrawingArea>(viewArea);
}

namespace {
		TArray<FVector> GetCorners(const AModumateObjectInstance * Object)
		{
			TArray<FVector> corners;
			if (ensureAlways(Object->GetNumCorners() == 4))
			{
				for (int32 c = 0; c < 4; ++c)
				{
					corners.Add(Object->GetCorner(c));
				}

			}
			return corners;
		}
}


EDrawError FDraftingDrawing::Draw(IModumateDraftingDraw *drawingInterface,
	FModumateUnitCoord2D position,
	ModumateUnitParams::FAngle orientation,
	float scale)
{
	EDrawError error = EDrawError::ErrorNone;
	if (drawingInterface == nullptr)
	{
		error = EDrawError::ErrorBadParam;
		return error;
	}

	// set the drawing scale to the interface, then continue with the normal composite behavior
	drawingInterface->DrawingScale = DrawingScale;

	return FDraftingComposite::Draw(drawingInterface,position,orientation,scale);
}

bool FDraftingDrawing::InitializeDimensions(FModumateUnitCoord2D drawingSize, FModumateUnitCoord2D drawingMargin)
{
	DrawingSize = drawingSize;
	DrawingMargin = drawingMargin;

	Dimensions = DrawingSize;
		
	auto viewArea = DrawingContent.Pin();
	viewArea->SetLocalPosition(DrawingMargin);
	viewArea->Dimensions = Dimensions - (DrawingMargin * 2.0f);

	return true;
}

bool FDraftingDrawing::MakeTitleTag(IModumateDraftingDraw *drawingInterface)
{	
	// empty by default
	return true;
}

bool FDraftingDrawing::MakeScaleTag(IModumateDraftingDraw *drawingInterface)
{
	// empty by default
	return true;
}

// TODO: this function is similar to GetForegroundLines in the cut plane implementation, the main
// difference is this operates on the intersection between a scopebox and a cutplane as opposed
// to the entire cutplane.  It is possible both functions could be cleaned up by making some of 
// the code static and shared
void FDraftingDrawing::GetForegroundLines(TSharedPtr<FDraftingComposite> ParentPage, const FVector &AxisX, const FVector &AxisY, bool bIsDrafting)
{
	auto cutPlane = Doc->GetObjectById(CaptureObjID.Key);
	TArray<FVector> controlPoints;

	FVector cutPlaneOrigin = cutPlane->GetCorner(0);

	// for now, the entire cut plane is used instead of using the scope box at all
#if 0
	auto scopeBox = Doc->GetObjectById(CaptureObjID.Value);
	FVector2D scopeBoxOrigin2D = UModumateGeometryStatics::ProjectPoint2D(controlPoints[0], AxisX, AxisY, cutPlaneOrigin);
	FVector scopeBoxOrigin = cutPlaneOrigin + (scopeBoxOrigin2D.X * AxisX) + (scopeBoxOrigin2D.Y * AxisY);
	controlPoints = scopeBox->GetControlPoints();
#else
	FVector scopeBoxOrigin = cutPlaneOrigin;
	if (DraftingType == UDraftingManager::kDWG)
	{
		FBox sceneBounds(Doc->CalculateProjectBounds().GetBox());
		sceneBounds = sceneBounds.ExpandBy(1000.0f);
		UModumateGeometryStatics::GetBoxCorners(sceneBounds, controlPoints);
	}
	else
	{
		for (int32 cp = 0; cp < cutPlane->GetNumCorners(); ++cp)
		{
			controlPoints.Add(cutPlane->GetCorner(cp));
		}
	}
#endif

	TArray<FVector2D> boxPoints;
	for (auto& point : controlPoints)
	{
		FVector2D point2D = UModumateGeometryStatics::ProjectPoint2D(point, AxisX, AxisY, scopeBoxOrigin);
		boxPoints.Add(point2D);
	}

	FBox2D drawingBox = FBox2D(boxPoints);

	auto plane = FPlane(scopeBoxOrigin, cutPlane->GetNormal());

	TArray<TArray<FVector>> WallCutPerimeters;
	TArray<const AModumateObjectInstance*> inplaneDraftTypes(Doc->GetObjectsOfType(
		{ EObjectType::OTCabinet, EObjectType::OTStructureLine, EObjectType::OTMullion, EObjectType::OTFinish, EObjectType::OTTrim,
			EObjectType::OTTerrain, EObjectType::OTPointHosted, EObjectType::OTEdgeHosted, EObjectType::OTWallSegment,
			EObjectType::OTFloorSegment, EObjectType::OTCeiling, EObjectType::OTRoofFace, EObjectType::OTWindow,
			EObjectType::OTDoor, EObjectType::OTStaircase }));

	for (const auto* moi: inplaneDraftTypes)
	{
		moi->GetDraftingLines(ParentPage, plane, AxisX, AxisY, scopeBoxOrigin, drawingBox, WallCutPerimeters);
	}

	// Clipping of beyond-cut-plane lines.
	ParentPage->lineClipping.Reset(new FModumateClippingTriangles(*cutPlane));
	ParentPage->lineClipping->SetTransform(cutPlane->GetCorner(0), AxisX, 1.0f);
	ParentPage->lineClipping->AddTrianglesFromDoc(Doc);

//#define MODUMATE_DRAW_OCCLUDERS
#ifdef MODUMATE_DRAW_OCCLUDERS

	TArray<FEdge> occluders;
	ParentPage->lineClipping->GetTriangleEdges(occluders);
	for (const auto& edge: occluders)
	{
	TSharedPtr<FDraftingLine> line = MakeShared<FDraftingLine>(
			FModumateUnitCoord2D::WorldCentimeters(FVector2D(edge.Vertex[0])),
			FModumateUnitCoord2D::WorldCentimeters(FVector2D(edge.Vertex[1])),
			lineThickness, lineColor);
		ParentPage->Children.Add(line);
	line->SetLayerTypeRecursive(FModumateLayerType::kDebug1);
	}
#endif


	// Draw all separators, portals.
	TArray<const AModumateObjectInstance*> beyondCutObjects(Doc->GetObjectsOfType({
		EObjectType::OTWallSegment, EObjectType::OTFloorSegment, EObjectType::OTCeiling,
		EObjectType::OTRoofFace, EObjectType::OTWindow, EObjectType::OTDoor,
		EObjectType::OTCabinet, EObjectType::OTStructureLine, EObjectType::OTRailSegment,
		EObjectType::OTSystemPanel, EObjectType::OTMullion, EObjectType::OTStaircase,
		EObjectType::OTFinish, EObjectType::OTCountertop, EObjectType::OTTrim,
		EObjectType::OTTerrain, EObjectType::OTPointHosted, EObjectType::OTEdgeHosted
		}));

	for (auto object: beyondCutObjects)
	{
		object->GetDraftingLines(ParentPage, plane, AxisX, AxisY, scopeBoxOrigin, drawingBox, WallCutPerimeters);
	}
	// Cut plane holds traced FFE lines.
	cutPlane->GetDraftingLines(ParentPage, plane, AxisX, AxisY, scopeBoxOrigin, drawingBox, WallCutPerimeters);

}

bool FDraftingDrawing::MakeWorldObjects()
{
	TSharedPtr<FDraftingComposite> totalPage = MakeShareable(new FDraftingComposite());
	TSharedPtr<FDraftingComposite> section = MakeShareable(new FDraftingComposite());

	auto cutPlane = Doc->GetObjectById(CaptureObjID.Key);
#if 0
	auto scopeBox = Doc->GetObjectById(CaptureObjID.Value);
	FVector scopeBoxNormal = scopeBox->GetNormal();
#else
	FVector scopeBoxNormal = cutPlane->GetNormal();
#endif
		

	FPlane plane = FPlane(cutPlane->GetCorner(0), cutPlane->GetNormal());
	int32 numPoints = 4;
	TArray<FVector> intersection;
#if 0
	bool bValidIntersection = true;
	intersection.SetNumZeroed(numPoints);
	for (int32 cornerIdx = 0; cornerIdx < numPoints; cornerIdx++)
	{
		FVector corner = scopeBox->GetCorner(cornerIdx);
		FVector extrudedCorner = corner + (scopeBoxNormal * scopeBox->GetExtents().Y);

		bool bIntersects = FMath::SegmentPlaneIntersection(corner, extrudedCorner, plane, intersection[cornerIdx]);
		bValidIntersection = bValidIntersection && bIntersects;
	}
#else
	intersection = GetCorners(cutPlane);
#endif

	FVector axisX, axisY, center;
	TArray<FVector2D> cached2DPositions;
	FVector origin = intersection[0];
	UModumateGeometryStatics::AnalyzeCachedPositions(intersection, plane, axisX, axisY, cached2DPositions, center);
	const bool bHorizontalPlan = FVector::Parallel(plane, FVector::UpVector);

	TArray<FVector2D> boxPoints;
	for (auto& point : intersection)
	{
		FVector2D point2D = UModumateGeometryStatics::ProjectPoint2D(point, axisX, axisY, origin);
		boxPoints.Add(point2D);
	}

	FBox2D drawingBox = FBox2D(boxPoints);

	float orthoWidth = drawingBox.GetSize().X;
	float orthoHeight = drawingBox.GetSize().Y;

	FModumateUnitCoord2D dimensions = FModumateUnitCoord2D(ModumateUnitParams::FXCoord::WorldCentimeters(orthoWidth), ModumateUnitParams::FYCoord::WorldCentimeters(orthoHeight));

	TSharedPtr<FDraftingComposite> foregroundLines = MakeShareable(new FDraftingComposite());

	if (!bHorizontalPlan)
	{
		axisX *= -1.0f;
		axisY *= -1.0f;
	}

	GetForegroundLines(foregroundLines, axisX, axisY, true);

	foregroundLines->SetLocalPosition(FModumateUnitCoord2D::WorldCentimeters(drawingBox.Min) * -1.0f);

	if (!bHorizontalPlan)
	{
		foregroundLines->MoveXTo(dimensions.X);
		foregroundLines->MoveYTo(dimensions.Y);
	}
	totalPage->Children.Add(foregroundLines);

	// Plan dimensions MOD-726.
	TSharedPtr<FDraftingComposite> planDimensions = MakeShared<FDraftingComposite>();
	planDimensions->SetLocalPosition(FModumateUnitCoord2D::WorldCentimeters(drawingBox.Min) * -1.0f);
	FModumateDimensions dimensionsCreator;

	if (dimensionsCreator.AddDimensionsFromCutPlane(planDimensions, Doc, plane, origin, axisX))
	{
		if (!bHorizontalPlan)
		{
			planDimensions->MoveXTo(dimensions.X);
			planDimensions->MoveYTo(dimensions.Y);
		}
		totalPage->Children.Add(planDimensions);
	}

	section->Children.Add(totalPage);

	section->BoundingBox = drawingBox + totalPage->BoundingBox;

	WorldObjects = TWeakPtr<FDraftingComposite>(section);

	auto viewArea = DrawingContent.Pin();
	ensureAlways(viewArea.IsValid());

	viewArea->Children.Add(section);

	return true;
}

void FDraftingDrawing::GetFacesConnectedToRooms(TMap<int32, TArray<int32>> &OutFacesToRooms) const
{
	const auto& volumeGraph = *Doc->GetVolumeGraph();

	for (auto* room : Doc->GetObjectsOfType(EObjectType::OTRoom))
	{
		int32 roomID = room->ID;
		TSet<int32> roomConnectedFaces;
		// TODO: refactor room faces using strongly-typed InstanceProperties
		for (int32 faceID : { MOD_ID_NONE })//room->GetControlPointIndices())
		{
			auto face = volumeGraph.FindFace(faceID);
			if (!face)
			{
				continue;
			}

			face->GetAdjacentFaceIDs(roomConnectedFaces);
		}

		for (int32 id : roomConnectedFaces)
		{
			TArray<int32> &roomIDs = OutFacesToRooms.FindOrAdd(id);
			roomIDs.Add(roomID);
		}
	}
}

void FDraftingDrawing::GetVisibleRoomsAndLocations(TMap<int32, FVector2D> &OutRoomsAndLocations) const
{
	const auto& volumeGraph = *Doc->GetVolumeGraph();
	auto cutPlane = Doc->GetObjectById(CaptureObjID.Key);
	// scope boxes are deprecated for now
#if 0
	auto scopeBox = Doc->GetObjectById(CaptureObjID.Value);
#endif

	FPlane plane;
	FVector axisX, axisY, center;
	TArray<FVector2D> cached2DPositions;
	FVector origin = cutPlane->GetCorner(0);
	auto cutPlanePoints = GetCorners(cutPlane);
	UModumateGeometryStatics::AnalyzeCachedPositions(cutPlanePoints, plane, axisX, axisY, cached2DPositions, center);
	TMap<int32, int32> objMap;
	auto graph = MakeShared<FGraph2D>();

	TArray<FVector2D> points2D;
#if 0
	FVector2D scopeBoxOrigin2D = UModumateGeometryStatics::ProjectPoint2D(scopeBox->GetControlPoint(0), axisX, axisY, origin);
	FVector scopeBoxOrigin = origin + (scopeBoxOrigin2D.X * axisX) + (scopeBoxOrigin2D.Y * axisY);

	for (auto& point : scopeBox->GetControlPoints())
	{
		FVector2D point2D = UModumateGeometryStatics::ProjectPoint2D(point, axisX, axisY, scopeBoxOrigin);
		points2D.Add(point2D);
	}
#else
	for (auto& point : cutPlanePoints)
	{
		FVector2D point2D = UModumateGeometryStatics::ProjectPoint2D(point, axisX, axisY, origin);
		points2D.Add(point2D);
	}
#endif

	// bounding box is defined by the dimensions of the cut plane as opposed to the contents of the graph
	FBox2D drawingBox = FBox2D(points2D);
	plane = FPlane(origin, cutPlane->GetNormal());

	volumeGraph.Create2DGraph(plane, axisX, axisY, origin, drawingBox, graph, objMap);

	TSet<int32> foundRooms;

	TMap<int32, TArray<int32>> roomMap;
	GetFacesConnectedToRooms(roomMap);

	for (auto& edgekvp : graph->GetEdges())
	{
		int32 metaplaneID = objMap[edgekvp.Key];
		if (roomMap.Contains(metaplaneID))
		{
			for (int32 roomID : roomMap[metaplaneID])
			{
				foundRooms.Add(roomID);
			}
		}
	}

	// visible rooms have a connected wall that intersects the plane, and a centroid that is above the plane
	for (int32 roomID : foundRooms)
	{
		auto room = Doc->GetObjectById(roomID);
		if (room == nullptr)
		{
			continue;
		}

		// TODO: refactor room faces using strongly-typed InstanceProperties
		for (int32 faceID : { MOD_ID_NONE })//room->GetControlPointIndices())
		{
			auto face = volumeGraph.FindFace(faceID);
			if (face == nullptr)
			{
				continue;
			}

			FVector2D centroid2D;
			UModumateGeometryStatics::FindPointFurthestFromPolyEdge(face->Cached2DPositions, centroid2D);
			FVector centroid = face->DeprojectPosition(centroid2D);

			if (plane.PlaneDot(centroid) > 0)
			{
				FVector2D centroidOnPlane = UModumateGeometryStatics::ProjectPoint2D(origin, -axisX, -axisY, centroid);
				OutRoomsAndLocations.Add(roomID, centroidOnPlane);
				break;
			}
		}
	}
}


#undef LOCTEXT_NAMESPACE

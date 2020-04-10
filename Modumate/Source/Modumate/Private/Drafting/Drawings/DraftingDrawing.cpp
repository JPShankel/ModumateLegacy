#include "DraftingDrawing.h"
#include "DraftingManager.h"
#include "DrawingArea.h"
#include "ModumateDocument.h"
#include "ModumateDraftingTags.h"
#include "ModumateGeometryStatics.h"
#include "ModumateObjectInstance.h"

#define LOCTEXT_NAMESPACE "ModumateDefaultDrawing"

namespace Modumate {

	FDraftingDrawing::FDraftingDrawing(const FModumateDocument *doc, UWorld *world, SceneCaptureID captureObjID) :
		Doc(doc), World(world), CaptureObjID(captureObjID) {}

	EDrawError FDraftingDrawing::Draw(IModumateDraftingDraw *drawingInterface,
		Units::FCoordinates2D position,
		Units::FAngle orientation,
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

	bool FDraftingDrawing::InitializeDimensions(Units::FCoordinates2D drawingSize, Units::FCoordinates2D drawingMargin)
	{
		DrawingSize = drawingSize;
		DrawingMargin = drawingMargin;

		Dimensions = DrawingSize;
		
		TSharedPtr<FDrawingArea> viewArea = MakeShareable(new FDrawingArea());
		viewArea->SetLocalPosition(DrawingMargin);
		viewArea->Dimensions = Dimensions - (DrawingMargin * 2.0f);
		DrawingContent = TWeakPtr<FDrawingArea>(viewArea);

		Children.Add(viewArea);
		Children.Add(MakeShareable(new FDraftingRectangle(Dimensions)));

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
		auto scopeBox = Doc->GetObjectById(CaptureObjID.Value);

		FVector cutPlaneOrigin = cutPlane->GetControlPoint(0);

		FVector2D scopeBoxOrigin2D = UModumateGeometryStatics::ProjectPoint2D(scopeBox->GetControlPoint(0), AxisX, AxisY, cutPlaneOrigin);
		FVector scopeBoxOrigin = cutPlaneOrigin + (scopeBoxOrigin2D.X * AxisX) + (scopeBoxOrigin2D.Y * AxisY);

		TArray<FVector2D> boxPoints;
		for (auto& point : scopeBox->GetControlPoints())
		{
			FVector2D point2D = UModumateGeometryStatics::ProjectPoint2D(point, AxisX, AxisY, scopeBoxOrigin);
			boxPoints.Add(point2D);
		}

		FBox2D drawingBox = FBox2D(boxPoints);

		auto plane = FPlane(scopeBoxOrigin, cutPlane->GetNormal());

		FGraph graph;
		TMap<int32, int32> objMap;
		auto volumeGraph = Doc->GetVolumeGraph();
		volumeGraph.Create2DGraph(plane, AxisX, AxisY, scopeBoxOrigin, drawingBox, graph, objMap);

		Units::FThickness lineThickness = Units::FThickness::Points(0.15f);
		FMColor lineColor = FMColor::Black;

		for (auto& edgekvp : graph.GetEdges())
		{
			auto& edge = edgekvp.Value;

			auto startVertex = graph.FindVertex(edge.StartVertexID);
			auto endVertex = graph.FindVertex(edge.EndVertexID);

			Units::FCoordinates2D start = Units::FCoordinates2D::WorldCentimeters(startVertex->Position);
			Units::FCoordinates2D end = Units::FCoordinates2D::WorldCentimeters(endVertex->Position);

			int32 metaplaneID = objMap[edge.ID];
			auto metaplane = Doc->GetObjectById(metaplaneID);
			if (metaplane == nullptr)
			{
				continue;
			}

			auto children = metaplane->GetChildObjects();
			if (!children.Num() == 1)
			{
				continue;
			}
			auto wall = children[0];
			if (wall == nullptr)
			{
				continue;
			}

			TArray<TPair<FVector2D, FVector2D>> OutLines;

			// TODO: this only is implemented for plane hosted objects right now, this function should be
			// a part of ModumateObjectInstance instead and should propagate down through the children
			TArray<TArray<FVector>> WallCutPerimeters;
			wall->GetDraftingLines(ParentPage, plane, AxisX, AxisY, scopeBoxOrigin, drawingBox, WallCutPerimeters);

			if (bIsDrafting)
			{
				for (int32 objID : wall->GetChildIDs())
				{
					auto moi = Doc->GetObjectById(objID);
					if (!moi) continue;
					
					// not used
					TArray<TArray<FVector>> portalPerimeters;
					moi->GetDraftingLines(ParentPage, plane, AxisX, AxisY, scopeBoxOrigin, drawingBox, portalPerimeters);
				}
			}
		}

	}

	bool FDraftingDrawing::MakeWorldObjects()
	{
		TSharedPtr<FDraftingComposite> totalPage = MakeShareable(new FDraftingComposite());
		TSharedPtr<FDraftingComposite> section = MakeShareable(new FDraftingComposite());

		// Create image primitive background
		FString file;
		UDraftingManager::GetImageFullPath(CaptureObjID, file);

		auto cutPlane = Doc->GetObjectById(CaptureObjID.Key);
		auto scopeBox = Doc->GetObjectById(CaptureObjID.Value);
		FVector scopeBoxNormal = scopeBox->GetNormal();


		FPlane plane = FPlane(cutPlane->GetControlPoint(0), cutPlane->GetNormal());
		bool bValidIntersection = true;
		int32 numPoints = 4;
		TArray<FVector> intersection;
		intersection.SetNumZeroed(numPoints);
		for (int32 cornerIdx = 0; cornerIdx < numPoints; cornerIdx++)
		{
			FVector corner = scopeBox->GetCorner(cornerIdx);
			FVector extrudedCorner = corner + (scopeBoxNormal * scopeBox->GetExtents().Y);

			bool bIntersects = FMath::SegmentPlaneIntersection(corner, extrudedCorner, plane, intersection[cornerIdx]);
			bValidIntersection = bValidIntersection && bIntersects;
		}

		FVector axisX, axisY, center;
		TArray<FVector2D> cached2DPositions;
		FVector origin = intersection[0];
		UModumateGeometryStatics::AnalyzeCachedPositions(intersection, plane, axisX, axisY, cached2DPositions, center);

		TArray<FVector2D> boxPoints;
		for (auto& point : intersection)
		{
			FVector2D point2D = UModumateGeometryStatics::ProjectPoint2D(point, axisX, axisY, origin);
			boxPoints.Add(point2D);
		}

		FBox2D drawingBox = FBox2D(boxPoints);

		float orthoWidth = drawingBox.GetSize().X;
		float orthoHeight = drawingBox.GetSize().Y;

		Units::FCoordinates2D dimensions = Units::FCoordinates2D(Units::FXCoord::WorldCentimeters(orthoWidth), Units::FYCoord::WorldCentimeters(orthoHeight));

		TSharedPtr<FImagePrimitive> image = MakeShareable(new FImagePrimitive(file, dimensions));
		totalPage->Children.Add(image);


		TSharedPtr<FDraftingComposite> foregroundLines = MakeShareable(new FDraftingComposite());
		TArray<TArray<FVector>> outPerimeters;

		if (!FVector::Parallel(cutPlane->GetNormal(), FVector::UpVector))
		{
			axisX *= -1.0f;
			axisY *= -1.0f;
		}
		GetForegroundLines(foregroundLines, axisX, axisY, true);

		foregroundLines->SetLocalPosition(Units::FCoordinates2D::WorldCentimeters(drawingBox.Min) * -1.0f);
		if (!FVector::Parallel(FVector(plane), FVector::UpVector))
		{
			foregroundLines->MoveXTo(dimensions.X);
			foregroundLines->MoveYTo(dimensions.Y);
		}
		totalPage->Children.Add(foregroundLines);

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
		auto volumeGraph = Doc->GetVolumeGraph();

		for (auto* room : Doc->GetObjectsOfType(EObjectType::OTRoom))
		{
			int32 roomID = room->ID;
			TSet<int32> roomConnectedFaces;
			for (int32 faceID : room->GetControlPointIndices())
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
		auto volumeGraph = Doc->GetVolumeGraph();
		auto cutPlane = Doc->GetObjectById(CaptureObjID.Key);
		auto scopeBox = Doc->GetObjectById(CaptureObjID.Value);

		FPlane plane;
		FVector axisX, axisY, center;
		TArray<FVector2D> cached2DPositions;
		FVector origin = cutPlane->GetControlPoint(0);
		UModumateGeometryStatics::AnalyzeCachedPositions(cutPlane->GetControlPoints(), plane, axisX, axisY, cached2DPositions, center);
		TMap<int32, int32> objMap;
		FGraph graph;

		FVector2D scopeBoxOrigin2D = UModumateGeometryStatics::ProjectPoint2D(scopeBox->GetControlPoint(0), axisX, axisY, origin);
		FVector scopeBoxOrigin = origin + (scopeBoxOrigin2D.X * axisX) + (scopeBoxOrigin2D.Y * axisY);

		TArray<FVector2D> boxPoints;
		for (auto& point : scopeBox->GetControlPoints())
		{
			FVector2D point2D = UModumateGeometryStatics::ProjectPoint2D(point, axisX, axisY, scopeBoxOrigin);
			boxPoints.Add(point2D);
		}

		// bounding box is defined by the dimensions of the cut plane as opposed to the contents of the graph
		FBox2D drawingBox = FBox2D(boxPoints);
		plane = FPlane(scopeBoxOrigin, cutPlane->GetNormal());

		volumeGraph.Create2DGraph(plane, axisX, axisY, scopeBoxOrigin, drawingBox, graph, objMap);

		TSet<int32> foundRooms;

		TMap<int32, TArray<int32>> roomMap;
		GetFacesConnectedToRooms(roomMap);

		for (auto& edgekvp : graph.GetEdges())
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

			for (int32 faceID : room->GetControlPointIndices())
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
					FVector2D centroidOnPlane = UModumateGeometryStatics::ProjectPoint2D(scopeBoxOrigin, -axisX, -axisY, centroid);
					OutRoomsAndLocations.Add(roomID, centroidOnPlane);
					break;
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE

// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "Database/ModumateObjectEnums.h"

#define LOCTEXT_NAMESPACE "ModumateTypes"

const EObjectDirtyFlags UModumateTypeStatics::OrderedDirtyFlags[3] = {
	EObjectDirtyFlags::Structure,
	EObjectDirtyFlags::Mitering,
	EObjectDirtyFlags::Visuals,
};

EToolCategories UModumateTypeStatics::GetToolCategory(EToolMode ToolMode)
{
	switch (ToolMode)
	{
	case EToolMode::VE_METAPLANE:
		return EToolCategories::MetaGraph;
	case EToolMode::VE_WALL:
	case EToolMode::VE_FLOOR:
	case EToolMode::VE_DOOR:
	case EToolMode::VE_WINDOW:
	case EToolMode::VE_STAIR:
	case EToolMode::VE_RAIL:
	case EToolMode::VE_ROOF_FACE:
	case EToolMode::VE_STRUCTURELINE:
		return EToolCategories::Separators;
	case EToolMode::VE_PLACEOBJECT:
	case EToolMode::VE_CABINET:
	case EToolMode::VE_FINISH:
	case EToolMode::VE_COUNTERTOP:
	case EToolMode::VE_TRIM:
		return EToolCategories::Attachments;
	case EToolMode::VE_SURFACEGRAPH:
		return EToolCategories::SurfaceGraphs;
	default:
		return EToolCategories::Unknown;
	}
}

EObjectType UModumateTypeStatics::ObjectTypeFromToolMode(EToolMode tm)
{
	switch (tm)
	{
	case EToolMode::VE_NONE:return EObjectType::OTUnknown;
	case EToolMode::VE_SELECT:return EObjectType::OTUnknown;
	case EToolMode::VE_PLACEOBJECT:return EObjectType::OTFurniture;
	case EToolMode::VE_MOVEOBJECT: return EObjectType::OTUnknown;
	case EToolMode::VE_ROTATE: return EObjectType::OTUnknown;
	case EToolMode::VE_SCALE: return EObjectType::OTUnknown;
	case EToolMode::VE_SPLIT: return EObjectType::OTUnknown;
	case EToolMode::VE_WALL: return EObjectType::OTWallSegment;
	case EToolMode::VE_FLOOR: return EObjectType::OTFloorSegment;
	case EToolMode::VE_DOOR: return EObjectType::OTDoor;
	case EToolMode::VE_WINDOW: return EObjectType::OTWindow;
	case EToolMode::VE_STAIR: return EObjectType::OTStaircase;
	case EToolMode::VE_RAIL: return EObjectType::OTRailSegment;
	case EToolMode::VE_CABINET: return EObjectType::OTCabinet;
	case EToolMode::VE_WAND: return EObjectType::OTUnknown;
	case EToolMode::VE_FINISH: return EObjectType::OTFinish;
	case EToolMode::VE_COUNTERTOP: return EObjectType::OTCountertop;
	case EToolMode::VE_TRIM: return EObjectType::OTTrim;
	case EToolMode::VE_ROOF_FACE: return EObjectType::OTRoofFace;
	case EToolMode::VE_ROOF_PERIMETER: return EObjectType::OTRoofPerimeter;
	case EToolMode::VE_METAPLANE: return EObjectType::OTMetaPlane;
	case EToolMode::VE_CUTPLANE: return EObjectType::OTCutPlane;
	case EToolMode::VE_SCOPEBOX: return EObjectType::OTScopeBox;
	case EToolMode::VE_JOIN: return EObjectType::OTUnknown;
	case EToolMode::VE_STRUCTURELINE: return EObjectType::OTStructureLine;
	case EToolMode::VE_DRAWING: return EObjectType::OTDrawing;
	case EToolMode::VE_SURFACEGRAPH: return EObjectType::OTSurfaceGraph;
	}
	return EObjectType::OTUnknown;
}

EToolMode UModumateTypeStatics::ToolModeFromObjectType(EObjectType ot)
{
	switch (ot)
	{
	case EObjectType::OTWallSegment: return EToolMode::VE_WALL;
	case EObjectType::OTRailSegment: return EToolMode::VE_RAIL;
	case EObjectType::OTFloorSegment: return EToolMode::VE_FLOOR;
	case EObjectType::OTRoofFace: return EToolMode::VE_ROOF_FACE;
	case EObjectType::OTDoor: return EToolMode::VE_DOOR;
	case EObjectType::OTWindow: return EToolMode::VE_WINDOW;
	case EObjectType::OTFurniture: return EToolMode::VE_PLACEOBJECT;
	case EObjectType::OTCabinet: return EToolMode::VE_CABINET;
	case EObjectType::OTStaircase: return EToolMode::VE_STAIR;
	case EObjectType::OTFinish: return EToolMode::VE_FINISH;
	case EObjectType::OTGroup: return EToolMode::VE_NONE;
	case EObjectType::OTRoom: return EToolMode::VE_NONE;
	case EObjectType::OTCountertop: return EToolMode::VE_COUNTERTOP;
	case EObjectType::OTTrim: return EToolMode::VE_TRIM;
	case EObjectType::OTMetaVertex:
	case EObjectType::OTMetaEdge:
	case EObjectType::OTMetaPlane: return EToolMode::VE_METAPLANE;
	case EObjectType::OTSurfaceGraph:
	case EObjectType::OTSurfaceVertex:
	case EObjectType::OTSurfaceEdge:
	case EObjectType::OTSurfacePolygon: return EToolMode::VE_SURFACEGRAPH;
	case EObjectType::OTCutPlane: return EToolMode::VE_CUTPLANE;
	case EObjectType::OTScopeBox: return EToolMode::VE_SCOPEBOX;
	case EObjectType::OTStructureLine: return EToolMode::VE_STRUCTURELINE;
	case EObjectType::OTDrawing: return EToolMode::VE_DRAWING;
	case EObjectType::OTRoofPerimeter: return EToolMode::VE_ROOF_PERIMETER;
	case EObjectType::OTUnknown: return EToolMode::VE_NONE;
	};
	return EToolMode::VE_NONE;
}

FText UModumateTypeStatics::GetTextForObjectType(EObjectType ObjectType, bool bPlural)
{
	switch (ObjectType)
	{
	case EObjectType::OTWallSegment:
		return bPlural ? LOCTEXT("OTWallSegments", "Walls") : LOCTEXT("OTWallSegment", "Wall");
	case EObjectType::OTRailSegment:
		return bPlural ? LOCTEXT("OTRailSegments", "Rails") : LOCTEXT("OTRailSegment", "Rail");
	case EObjectType::OTFloorSegment:
		return bPlural ? LOCTEXT("OTFloorSegments", "Floors") : LOCTEXT("OTFloorSegment", "Floor");
	case EObjectType::OTRoofFace:
		return bPlural ? LOCTEXT("OTRoofs", "Roofs") : LOCTEXT("OTRoof", "Roof");
	case EObjectType::OTDoor:
		return bPlural ? LOCTEXT("OTDoors", "Doors") : LOCTEXT("OTDoor", "Door");
	case EObjectType::OTWindow:
		return bPlural ? LOCTEXT("OTWindows", "Windows") : LOCTEXT("OTWindow", "Window");
	case EObjectType::OTFurniture:
		return bPlural ? LOCTEXT("OTFurnitures", "FF&E") : LOCTEXT("OTFurniture", "FF&E");
	case EObjectType::OTCabinet:
		return bPlural ? LOCTEXT("OTCabinets", "Cabinets") : LOCTEXT("OTCabinet", "Cabinet");
	case EObjectType::OTStaircase:
		return bPlural ? LOCTEXT("OTStaircases", "Staircases") : LOCTEXT("OTStaircase", "Staircase");
	case EObjectType::OTFinish:
		return bPlural ? LOCTEXT("OTFinishs", "Finishes") : LOCTEXT("OTFinish", "Finish");
	case EObjectType::OTGroup:
		return bPlural ? LOCTEXT("OTGroups", "Groups") : LOCTEXT("OTGroup", "Group");
	case EObjectType::OTRoom:
		return bPlural ? LOCTEXT("OTRooms", "Rooms") : LOCTEXT("OTRoom", "Room");
	case EObjectType::OTCountertop:
		return bPlural ? LOCTEXT("OTCountertops", "Countertops") : LOCTEXT("OTCountertop", "Countertop");
	case EObjectType::OTTrim:
		return bPlural ? LOCTEXT("OTTrims", "Trims") : LOCTEXT("OTTrim", "Trim");
	case EObjectType::OTMetaVertex:
		return bPlural ? LOCTEXT("OTMetaVertexs", "Meta Vertices") : LOCTEXT("OTMetaVertex", "Meta Vertex");
	case EObjectType::OTMetaEdge:
		return bPlural ? LOCTEXT("OTMetaEdges", "Meta Edges") : LOCTEXT("OTMetaEdge", "Meta Edge");
	case EObjectType::OTMetaPlane:
		return bPlural ? LOCTEXT("OTMetaPlanes", "Meta Planes") : LOCTEXT("OTMetaPlane", "Meta Plane");
	case EObjectType::OTSurfaceGraph:
		return bPlural ? LOCTEXT("OTSurfaceGraphs", "Surface Graphs") : LOCTEXT("OTSurfaceGraph", "Surface Graph");
	case EObjectType::OTSurfaceVertex:
		return bPlural ? LOCTEXT("OTSurfaceVertexs", "Surface Graph Vertices") : LOCTEXT("OTSurfaceVertex", "Surface Graph Vertex");
	case EObjectType::OTSurfaceEdge:
		return bPlural ? LOCTEXT("OTSurfaceEdges", "Surface Graph Edges") : LOCTEXT("OTSurfaceEdge", "Surface Graph Edge");
	case EObjectType::OTSurfacePolygon:
		return bPlural ? LOCTEXT("OTSurfacePolygons", "Surface Graph Polygons") : LOCTEXT("OTSurfacePolygon", "Surface Graph Polygon");
	case EObjectType::OTCutPlane:
		return bPlural ? LOCTEXT("OTCutPlanes", "Cut Planes") : LOCTEXT("OTCutPlane", "Cut Plane");
	case EObjectType::OTScopeBox:
		return bPlural ? LOCTEXT("OTScopeBoxes", "Scope Boxes") : LOCTEXT("OTScopeBox", "Scope Box");
	case EObjectType::OTStructureLine:
		return bPlural ? LOCTEXT("OTStructureLines", "Beams & Columns") : LOCTEXT("OTStructureLine", "Beam/Column");
	case EObjectType::OTDrawing:
		return bPlural ? LOCTEXT("OTDrawings", "Drawings") : LOCTEXT("OTDrawing", "Drawing");
	case EObjectType::OTUnknown:
	default:
		return bPlural ? LOCTEXT("OTUnknowns", "Unknowns") : LOCTEXT("OTUnknown", "Unknown");
	}
}

using namespace Modumate;

EGraph3DObjectType UModumateTypeStatics::Graph3DObjectTypeFromObjectType(EObjectType ot)
{
	switch (ot)
	{
	case EObjectType::OTMetaVertex:
		return EGraph3DObjectType::Vertex;
	case EObjectType::OTMetaEdge:
		return EGraph3DObjectType::Edge;
	case EObjectType::OTMetaPlane:
		return EGraph3DObjectType::Face;
	default:
		return EGraph3DObjectType::None;
	}
}

EGraphObjectType UModumateTypeStatics::Graph2DObjectTypeFromObjectType(EObjectType ObjectType)
{
	switch (ObjectType)
	{
	case EObjectType::OTSurfaceVertex:
		return EGraphObjectType::Vertex;
	case EObjectType::OTSurfaceEdge:
		return EGraphObjectType::Edge;
	case EObjectType::OTSurfacePolygon:
		return EGraphObjectType::Polygon;
	default:
		return EGraphObjectType::None;
	}
}

ECollisionChannel UModumateTypeStatics::CollisionTypeFromObjectType(EObjectType ot)
{
	switch (ot)
	{
	case EObjectType::OTMetaVertex:
	case EObjectType::OTMetaEdge:
	case EObjectType::OTMetaPlane:
	case EObjectType::OTRoofPerimeter:
		return COLLISION_META_MOI;
	case EObjectType::OTFinish:
	case EObjectType::OTTrim:
		return COLLISION_DECORATOR_MOI;
	default:
		return COLLISION_DEFAULT_MOI;
	}
}

FText UModumateTypeStatics::GetAreaTypeText(EAreaType AreaType)
{
	switch (AreaType)
	{
	case EAreaType::None:
		return LOCTEXT("AreaType_None", "None");
	case EAreaType::Net:
		return LOCTEXT("AreaType_Net", "Net");
	case EAreaType::Gross:
		return LOCTEXT("AreaType_Gross", "Gross");
	default:
		return FText::GetEmpty();
	}
}

FText UModumateTypeStatics::GetToolCategoryText(EToolCategories ToolCategory)
{
	switch (ToolCategory)
	{
	case EToolCategories::Unknown:
		return LOCTEXT("ToolCategory_Unknown", "Unknown");
	case EToolCategories::MetaGraph:
		return LOCTEXT("ToolCategory_MetaGraph", "MetaGraph");
	case EToolCategories::Separators:
		return LOCTEXT("ToolCategory_Separators", "Separators");
	case EToolCategories::SurfaceGraphs:
		return LOCTEXT("ToolCategory_SurfaceGraphs", "SurfaceGraphs");
	case EToolCategories::Attachments:
		return LOCTEXT("ToolCategory_Attachments", "Attachments");
	default:
		return FText::GetEmpty();
	}
}

#undef LOCTEXT_NAMESPACE

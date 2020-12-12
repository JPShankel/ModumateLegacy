// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/MOIFactory.h"

#include "Objects/Cabinet.h"
#include "Objects/CutPlane.h"
#include "Objects/FFE.h"
#include "Objects/Finish.h"
#include "Objects/Group.h"
#include "Objects/MetaEdge.h"
#include "Objects/MetaPlane.h"
#include "Objects/MetaVertex.h"
#include "Objects/PlaneHostedObj.h"
#include "Objects/Portal.h"
#include "Objects/RoofPerimeter.h"
#include "Objects/Room.h"
#include "Objects/ScopeBox.h"
#include "Objects/Stairs.h"
#include "Objects/StructureLine.h"
#include "Objects/SurfaceEdge.h"
#include "Objects/SurfaceGraph.h"
#include "Objects/SurfacePolygon.h"
#include "Objects/SurfaceVertex.h"
#include "Objects/Trim.h"
#include "Objects/Mullion.h"

UClass* FMOIFactory::GetMOIClass(EObjectType ObjectType)
{
	switch (ObjectType)
	{
	case EObjectType::OTWallSegment:
	case EObjectType::OTRailSegment:
	case EObjectType::OTFloorSegment:
	case EObjectType::OTCeiling:
	case EObjectType::OTRoofFace:
	case EObjectType::OTCountertop:
	case EObjectType::OTSystemPanel: return AMOIPlaneHostedObj::StaticClass();
	case EObjectType::OTDoor:
	case EObjectType::OTWindow: return AMOIPortal::StaticClass();
	case EObjectType::OTFurniture: return AMOIFFE::StaticClass();
	case EObjectType::OTCabinet: return AMOICabinet::StaticClass();
	case EObjectType::OTStaircase: return AMOIStaircase::StaticClass();
	case EObjectType::OTFinish: return AMOIFinish::StaticClass();
	case EObjectType::OTGroup: return AMOIGroup::StaticClass();
	case EObjectType::OTRoom: return AMOIRoom::StaticClass();
	case EObjectType::OTTrim: return AMOITrim::StaticClass();
	case EObjectType::OTMetaVertex: return AMOIMetaVertex::StaticClass();
	case EObjectType::OTMetaEdge: return AMOIMetaEdge::StaticClass();
	case EObjectType::OTMetaPlane: return AMOIMetaPlane::StaticClass();
	case EObjectType::OTSurfaceGraph: return AMOISurfaceGraph::StaticClass();
	case EObjectType::OTSurfaceVertex: return AMOISurfaceVertex::StaticClass();
	case EObjectType::OTSurfaceEdge: return AMOISurfaceEdge::StaticClass();
	case EObjectType::OTSurfacePolygon: return AMOISurfacePolygon::StaticClass();
	case EObjectType::OTCutPlane: return AMOICutPlane::StaticClass();
	case EObjectType::OTScopeBox: return AMOIScopeBox::StaticClass();
	case EObjectType::OTStructureLine: return AMOIStructureLine::StaticClass();
	case EObjectType::OTRoofPerimeter: return AMOIRoofPerimeter::StaticClass();
	case EObjectType::OTMullion: return AMOIMullion::StaticClass();
	default:
	{
		FString objectTypeString = EnumValueString(EObjectType, ObjectType);
		ensureAlwaysMsgf(false, TEXT("Tried to create a MOI from an unsupported ObjectType: %s!"),
			*objectTypeString);
		return nullptr;
	}
	}
}

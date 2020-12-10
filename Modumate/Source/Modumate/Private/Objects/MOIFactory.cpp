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

FModumateObjectInstance *FMOIFactory::MakeMOI(EObjectType ObjectType)
{
	switch (ObjectType)
	{
		case EObjectType::OTWallSegment:
		case EObjectType::OTRailSegment:
		case EObjectType::OTFloorSegment:
		case EObjectType::OTCeiling:
		case EObjectType::OTRoofFace:
		case EObjectType::OTCountertop:
		case EObjectType::OTSystemPanel: return new FMOIPlaneHostedObjImpl();
		case EObjectType::OTDoor:
		case EObjectType::OTWindow: return new FMOIPortalImpl();
		case EObjectType::OTFurniture: return new FMOIFFEImpl();
		case EObjectType::OTCabinet: return new FMOICabinetImpl();
		case EObjectType::OTStaircase: return new FMOIStaircaseImpl();
		case EObjectType::OTFinish: return new FMOIFinishImpl();
		case EObjectType::OTGroup: return new FMOIGroupImpl();
		case EObjectType::OTRoom: return new FMOIRoomImpl();
		case EObjectType::OTTrim: return new FMOITrimImpl();
		case EObjectType::OTMetaVertex: return new FMOIMetaVertexImpl();
		case EObjectType::OTMetaEdge: return new FMOIMetaEdgeImpl();
		case EObjectType::OTMetaPlane: return new FMOIMetaPlaneImpl();
		case EObjectType::OTSurfaceGraph: return new FMOISurfaceGraphImpl();
		case EObjectType::OTSurfaceVertex: return new FMOISurfaceVertexImpl();
		case EObjectType::OTSurfaceEdge: return new FMOISurfaceEdgeImpl();
		case EObjectType::OTSurfacePolygon: return new FMOISurfacePolygonImpl();
		case EObjectType::OTCutPlane: return new FMOICutPlaneImpl();
		case EObjectType::OTScopeBox: return new FMOIScopeBoxImpl();
		case EObjectType::OTStructureLine: return new FMOIStructureLine();
		case EObjectType::OTRoofPerimeter: return new FMOIRoofPerimeterImpl();
		case EObjectType::OTMullion: return new FMOIMullion();
		default:
		{
			FString objectTypeString = EnumValueString(EObjectType, ObjectType);
			ensureAlwaysMsgf(false, TEXT("Tried to create a MOI from an unsupported ObjectType: %s!"),
				*objectTypeString);
			return nullptr;
		}
	}
}

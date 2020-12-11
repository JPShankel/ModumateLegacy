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

AModumateObjectInstance *FMOIFactory::MakeMOI(EObjectType ObjectType)
{
	switch (ObjectType)
	{
		case EObjectType::OTWallSegment:
		case EObjectType::OTRailSegment:
		case EObjectType::OTFloorSegment:
		case EObjectType::OTCeiling:
		case EObjectType::OTRoofFace:
		case EObjectType::OTCountertop:
		case EObjectType::OTSystemPanel: return new AMOIPlaneHostedObj();
		case EObjectType::OTDoor:
		case EObjectType::OTWindow: return new AMOIPortal();
		case EObjectType::OTFurniture: return new AMOIFFE();
		case EObjectType::OTCabinet: return new AMOICabinet();
		case EObjectType::OTStaircase: return new AMOIStaircase();
		case EObjectType::OTFinish: return new AMOIFinish();
		case EObjectType::OTGroup: return new AMOIGroup();
		case EObjectType::OTRoom: return new AMOIRoom();
		case EObjectType::OTTrim: return new AMOITrim();
		case EObjectType::OTMetaVertex: return new AMOIMetaVertex();
		case EObjectType::OTMetaEdge: return new AMOIMetaEdge();
		case EObjectType::OTMetaPlane: return new AMOIMetaPlane();
		case EObjectType::OTSurfaceGraph: return new AMOISurfaceGraph();
		case EObjectType::OTSurfaceVertex: return new AMOISurfaceVertex();
		case EObjectType::OTSurfaceEdge: return new AMOISurfaceEdge();
		case EObjectType::OTSurfacePolygon: return new AMOISurfacePolygon();
		case EObjectType::OTCutPlane: return new AMOICutPlane();
		case EObjectType::OTScopeBox: return new AMOIScopeBox();
		case EObjectType::OTStructureLine: return new FMOIStructureLine();
		case EObjectType::OTRoofPerimeter: return new AMOIRoofPerimeter();
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

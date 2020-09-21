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

IModumateObjectInstanceImpl *FMOIFactory::MakeMOIImplementation(EObjectType ObjectType, FModumateObjectInstance *MOI)
{
	switch (ObjectType)
	{
		case EObjectType::OTWallSegment:
		case EObjectType::OTRailSegment:
		case EObjectType::OTFloorSegment:
		case EObjectType::OTCeiling:
		case EObjectType::OTRoofFace:
		case EObjectType::OTCountertop: return new FMOIPlaneHostedObjImpl(MOI);
		case EObjectType::OTDoor:
		case EObjectType::OTWindow: return new FMOIPortalImpl(MOI);
		case EObjectType::OTFurniture: return new FMOIFFEImpl(MOI);
		case EObjectType::OTCabinet: return new FMOICabinetImpl(MOI);
		case EObjectType::OTStaircase: return new FMOIStaircaseImpl(MOI);
		case EObjectType::OTFinish: return new FMOIFinishImpl(MOI);
		case EObjectType::OTGroup: return new FMOIGroupImpl(MOI);
		case EObjectType::OTRoom: return new FMOIRoomImpl(MOI);
		case EObjectType::OTTrim: return new FMOITrimImpl(MOI);
		case EObjectType::OTMetaVertex: return new FMOIMetaVertexImpl(MOI);
		case EObjectType::OTMetaEdge: return new FMOIMetaEdgeImpl(MOI);
		case EObjectType::OTMetaPlane: return new FMOIMetaPlaneImpl(MOI);
		case EObjectType::OTSurfaceGraph: return new FMOISurfaceGraphImpl(MOI);
		case EObjectType::OTSurfaceVertex: return new FMOISurfaceVertexImpl(MOI);
		case EObjectType::OTSurfaceEdge: return new FMOISurfaceEdgeImpl(MOI);
		case EObjectType::OTSurfacePolygon: return new FMOISurfacePolygonImpl(MOI);
		case EObjectType::OTCutPlane: return new FMOICutPlaneImpl(MOI);
		case EObjectType::OTScopeBox: return new FMOIScopeBoxImpl(MOI);
		case EObjectType::OTStructureLine: return new FMOIStructureLine(MOI);
		case EObjectType::OTRoofPerimeter: return new FMOIRoofPerimeterImpl(MOI);
		default:
		{
			FString objectTypeString = EnumValueString(EObjectType, ObjectType);
			ensureAlwaysMsgf(false, TEXT("Tried to create a MOI from an unsupported ObjectType: %s!"),
				*objectTypeString);
			return nullptr;
		}
	}
}

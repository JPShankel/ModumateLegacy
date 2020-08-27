// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/Objects/MOIFactory.h"

#include "DocumentManagement/ModumateObjectGroup.h"
#include "DocumentManagement/ModumateObjectInstanceCabinets.h"
#include "DocumentManagement/ModumateObjectInstanceCutPlane.h"
#include "DocumentManagement/ModumateObjectInstanceFinish.h"
#include "DocumentManagement/ModumateObjectInstanceFlatPoly.h"
#include "DocumentManagement/ModumateObjectInstanceFurniture.h"
#include "DocumentManagement/ModumateObjectInstancePlaneHostedObj.h"
#include "DocumentManagement/ModumateObjectInstancePortal.h"
#include "DocumentManagement/ModumateObjectInstanceRails.h"
#include "DocumentManagement/ModumateObjectInstanceRoofPerimeter.h"
#include "DocumentManagement/ModumateObjectInstanceRooms.h"
#include "DocumentManagement/ModumateObjectInstanceScopeBox.h"
#include "DocumentManagement/ModumateObjectInstanceStairs.h"
#include "DocumentManagement/ModumateObjectInstanceStructureLine.h"
#include "DocumentManagement/ModumateObjectInstanceTrim.h"
#include "DocumentManagement/Objects/MetaEdge.h"
#include "DocumentManagement/Objects/MetaPlane.h"
#include "DocumentManagement/Objects/MetaVertex.h"
#include "DocumentManagement/Objects/SurfaceEdge.h"
#include "DocumentManagement/Objects/SurfaceGraph.h"
#include "DocumentManagement/Objects/SurfacePolygon.h"
#include "DocumentManagement/Objects/SurfaceVertex.h"

IModumateObjectInstanceImpl *FMOIFactory::MakeMOIImplementation(EObjectType ObjectType, FModumateObjectInstance *MOI)
{
	switch (ObjectType)
	{
		case EObjectType::OTWallSegment: return new FMOIPlaneHostedObjImpl(MOI);
		case EObjectType::OTRailSegment: return new FMOIRailImpl(MOI);
		case EObjectType::OTFloorSegment: return new FMOIPlaneHostedObjImpl(MOI);
		case EObjectType::OTCeiling: return new FMOIPlaneHostedObjImpl(MOI);
		case EObjectType::OTRoofFace: return new FMOIPlaneHostedObjImpl(MOI);
		case EObjectType::OTCountertop: return new FMOIFlatPolyImpl(MOI, false); // false = no invert handle
		case EObjectType::OTDoor:
		case EObjectType::OTWindow: return new FMOIPortalImpl(MOI);
		case EObjectType::OTFurniture: return new FMOIObjectImpl(MOI);
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

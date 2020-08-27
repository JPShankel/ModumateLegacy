#pragma once

#include "CoreMinimal.h"
#include "BIMKernel/BIMAssemblySpec.h"
#include "ModumateCore/ModumateGeometryStatics.h"

class FModumateObjectInstance;

namespace Modumate
{
	class FGraph3DFace;

	namespace Mitering
	{
		struct FMiterHelpers
		{
			// Given a plane object, get its child layered object, if it exists.
			static const FModumateObjectInstance *GetChildLayeredObj(const FModumateObjectInstance *PlaneObj);

			// Given a plane-hosted object (and its world-space holes), create layer geometry definitions
			// that use miter data generated from a central miter node.
			static bool UpdateMiteredLayerGeoms(const FModumateObjectInstance *PlaneHostedObj, const FGraph3DFace *PlaneFace,
				const TArray<FPolyHole3D> *Holes, TArray<FLayerGeomDef> &OutLayerGeometries);
		};
	}
}

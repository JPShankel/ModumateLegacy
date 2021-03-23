#pragma once

#include "CoreMinimal.h"
#include "BIMKernel/AssemblySpec/BIMAssemblySpec.h"
#include "ModumateCore/LayerGeomDef.h"

class AModumateObjectInstance;

namespace Modumate
{
	class FGraph3DFace;

	namespace Mitering
	{
		struct FMiterHelpers
		{
			// Given a plane object, get its child layered object, if it exists.
			static const AModumateObjectInstance *GetChildLayeredObj(const AModumateObjectInstance *PlaneObj);

			// Given a plane-hosted object (and its world-space holes), create layer geometry definitions
			// that use miter data generated from a central miter node.
			static bool UpdateMiteredLayerGeoms(const AModumateObjectInstance *PlaneHostedObj, const FGraph3DFace *PlaneFace,
				const TArray<FPolyHole3D> *Holes, TArray<FLayerGeomDef> &OutLayerGeometries, TPair<TArray<FVector>, TArray<FVector>>& OutExtendedSurfaceFaces);
		};
	}
}

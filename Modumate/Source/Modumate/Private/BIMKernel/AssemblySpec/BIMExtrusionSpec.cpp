// Copyright 2018 Modumate, Inc.All Rights Reserved.

#include "BIMKernel/AssemblySpec/BIMExtrusionSpec.h"
#include "BIMKernel/Presets/BIMPresetCollection.h"
#include "BIMKernel/Presets/CustomData/BIMDimensions.h"
#include "BIMKernel/Presets/CustomData/BIMProfileRef.h"
#include "ModumateCore/ModumateDimensionStatics.h"

EBIMResult FBIMExtrusionSpec::BuildFromProperties(const FBIMPresetCollectionProxy& PresetCollection)
{
	float xDim, yDim;
	if (Dimensions.TryGetDimension(BIMPropertyNames::Diameter, xDim))
	{
		yDim = xDim;
	}
	else
	{
		Dimensions.TryGetDimension(BIMPropertyNames::Width, xDim);
		Dimensions.TryGetDimension(BIMPropertyNames::Depth, yDim);
	}
	
	
	if (ensureAlways(ProfileRef.Source.IsValid()))
	{
		const FSimpleMeshRef* trimMesh = PresetCollection.GetSimpleMeshByGUID(ProfileRef.Source);

		if (ensureAlways(trimMesh != nullptr))
		{
			SimpleMeshes.Add(*trimMesh);
		}
	}

	// TODO: re-orient column meshes so width is along X instead of depth
	FVector profileSize(xDim, yDim, 1);

	if (ensureAlways(SimpleMeshes.Num() > 0 && SimpleMeshes[0].Asset.Get()->Polygons.Num() > 0))
	{
		FSimplePolygon& polygon = SimpleMeshes[0].Asset.Get()->Polygons[0];
		FVector polyExtents(polygon.Extents.Max.X - polygon.Extents.Min.X, polygon.Extents.Max.Y - polygon.Extents.Min.Y, 1);
		Scale = profileSize / polyExtents;
	}

	return EBIMResult::Success;
}

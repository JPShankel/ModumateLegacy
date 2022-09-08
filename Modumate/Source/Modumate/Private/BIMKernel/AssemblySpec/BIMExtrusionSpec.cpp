// Copyright 2018 Modumate, Inc.All Rights Reserved.

#include "BIMKernel/AssemblySpec/BIMExtrusionSpec.h"
#include "BIMKernel/Presets/BIMPresetCollection.h"
#include "ModumateCore/ModumateDimensionStatics.h"

EBIMResult FBIMExtrusionSpec::BuildFromProperties(const FBIMPresetCollectionProxy& PresetCollection)
{
	FModumateUnitValue xDim, yDim;
	if (Properties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Diameter, xDim))
	{
		yDim = xDim;
	}
	else
	{
		Properties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Width, xDim);
		Properties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Depth, yDim);
	}

	FGuid profileKey;
	if (ensureAlways(Properties.TryGetProperty(EBIMValueScope::Profile, BIMPropertyNames::AssetID,profileKey) && profileKey.IsValid()))
	{
		const FSimpleMeshRef* trimMesh = PresetCollection.GetSimpleMeshByGUID(profileKey);

		if (ensureAlways(trimMesh != nullptr))
		{
			SimpleMeshes.Add(*trimMesh);
		}
	}

	// TODO: re-orient column meshes so width is along X instead of depth
	FVector profileSize(xDim.AsWorldCentimeters(), yDim.AsWorldCentimeters(), 1);

	if (ensureAlways(SimpleMeshes.Num() > 0 && SimpleMeshes[0].Asset.Get()->Polygons.Num() > 0))
	{
		FSimplePolygon& polygon = SimpleMeshes[0].Asset.Get()->Polygons[0];
		FVector polyExtents(polygon.Extents.Max.X - polygon.Extents.Min.X, polygon.Extents.Max.Y - polygon.Extents.Min.Y, 1);
		Scale = profileSize / polyExtents;
	}

	return EBIMResult::Success;
}

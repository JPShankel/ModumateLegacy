// Copyright 2018 Modumate, Inc.All Rights Reserved.

#include "BIMKernel/BIMExtrusionSpec.h"
#include "Database/ModumateObjectDatabase.h"
#include "ModumateCore/ModumateDimensionStatics.h"

EBIMResult FBIMExtrusionSpec::BuildFromProperties(const FModumateDatabase& InDB)
{
	FString diameterString;
	FModumateFormattedDimension xDim, yDim;
	if (Properties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Diameter, diameterString))
	{
		xDim = UModumateDimensionStatics::StringToFormattedDimension(Properties.GetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Diameter));
		yDim = xDim;
	}
	else
	{
		xDim = UModumateDimensionStatics::StringToFormattedDimension(Properties.GetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Width));
		yDim = UModumateDimensionStatics::StringToFormattedDimension(Properties.GetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Depth));
	}

	FBIMKey profileKey = FBIMKey(Properties.GetProperty(EBIMValueScope::Profile, BIMPropertyNames::AssetID).AsString());

	if (ensureAlways(!profileKey.IsNone()))
	{
		const FSimpleMeshRef* trimMesh = InDB.GetSimpleMeshByKey(profileKey);

		if (ensureAlways(trimMesh != nullptr))
		{
			SimpleMeshes.Add(*trimMesh);
		}
	}

	// TODO: re-orient column meshes so width is along X instead of depth
	FVector profileSize(xDim.Centimeters, yDim.Centimeters, 1);

	if (ensureAlways(SimpleMeshes.Num() > 0 && SimpleMeshes[0].Asset.Get()->Polygons.Num() > 0))
	{
		FSimplePolygon& polygon = SimpleMeshes[0].Asset.Get()->Polygons[0];
		FVector polyExtents(polygon.Extents.Max.X - polygon.Extents.Min.X, polygon.Extents.Max.Y - polygon.Extents.Min.Y, 1);
		Scale = profileSize / polyExtents;
	}

	return EBIMResult::Success;
}

// Copyright 2018 Modumate, Inc.All Rights Reserved.

#include "BIMKernel/BIMExtrusionSpec.h"
#include "Database/ModumateObjectDatabase.h"
#include "ModumateCore/ModumateDimensionStatics.h"

EBIMResult FBIMExtrusionSpec::BuildFromProperties(const FModumateDatabase& InDB)
{
	Modumate::Units::FUnitValue xDim, yDim;
	if (Properties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Diameter, xDim))
	{
		yDim = xDim;
	}
	else
	{
		Properties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Width, xDim);
		Properties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Depth, yDim);
	}

	FBIMKey profileKey = FBIMKey(Properties.GetProperty<FString>(EBIMValueScope::Profile, BIMPropertyNames::AssetID));

	if (ensureAlways(!profileKey.IsNone()))
	{
		const FSimpleMeshRef* trimMesh = InDB.GetSimpleMeshByKey(profileKey);

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

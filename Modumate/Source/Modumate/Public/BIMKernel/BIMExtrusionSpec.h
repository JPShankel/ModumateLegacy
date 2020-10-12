// Copyright 2018 Modumate, Inc.All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Database/ModumateObjectEnums.h"
#include "BIMKernel/BIMProperties.h"
#include "BIMKernel/BIMEnums.h"
#include "BIMKernel/BIMKey.h"

#include "ModumateCore/ExpressionEvaluator.h"

#include "Database/ModumateArchitecturalMaterial.h"
#include "Database/ModumateSimpleMesh.h"

class FModumateDatabase;

/*
An extrusion is a simple 2D mesh extruded along a hosted edge
*/
class MODUMATE_API FBIMExtrusionSpec
{
	friend class FBIMAssemblySpec;
private:
	FBIMPropertySheet Properties;
	ECraftingResult BuildFromProperties(const FModumateDatabase& InDB);

public:
	FArchitecturalMaterial Material;
	TArray<FSimpleMeshRef> SimpleMeshes;
};
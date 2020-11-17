// Copyright 2018 Modumate, Inc.All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Database/ModumateObjectEnums.h"
#include "BIMKernel/Core/BIMProperties.h"
#include "BIMKernel/Core/BIMEnums.h"
#include "BIMKernel/Core/BIMKey.h"

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
	EBIMResult BuildFromProperties(const FModumateDatabase& InDB);

public:
	FArchitecturalMaterial Material;
	TArray<FSimpleMeshRef> SimpleMeshes;
	FVector Scale { FVector::OneVector };
};
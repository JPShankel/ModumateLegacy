// Copyright 2018 Modumate, Inc.All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Objects/ModumateObjectEnums.h"
#include "BIMKernel/Core/BIMProperties.h"
#include "BIMKernel/Core/BIMEnums.h"
#include "BIMKernel/Core/BIMKey.h"
#include "BIMKernel/Presets/BIMPresetInstance.h"

#include "ModumateCore/ExpressionEvaluator.h"

#include "Database/ModumateArchitecturalMaterial.h"
#include "Database/ModumateSimpleMesh.h"

#include "BIMExtrusionSpec.generated.h"

class FModumateDatabase;

/*
An extrusion is a simple 2D mesh extruded along a hosted edge
*/
USTRUCT()
struct MODUMATE_API FBIMExtrusionSpec
{
	GENERATED_BODY()
	friend struct FBIMAssemblySpec;
private:

	UPROPERTY()
	FBIMPropertySheet Properties;

	EBIMResult BuildFromProperties(const FBIMPresetCollectionProxy& PresetCollection);

public:
	UPROPERTY()
	FArchitecturalMaterial Material;
	
	UPROPERTY()
	TArray<FSimpleMeshRef> SimpleMeshes;
	
	UPROPERTY()
	FVector Scale { FVector::OneVector };

	UPROPERTY()
	FGuid PresetGUID;

	UPROPERTY()
	EPresetMeasurementMethod MeasurementMethod = EPresetMeasurementMethod::None;
};

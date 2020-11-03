// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Database/ModumateObjectEnums.h"
#include "BIMKernel/BIMProperties.h"
#include "BIMKernel/BIMEnums.h"
#include "BIMKernel/BIMKey.h"

#include "BIMKernel/BIMLayerSpec.h"
#include "BIMKernel/BIMPartSlotSpec.h"
#include "BIMKernel/BIMExtrusionSpec.h"

// All needed by ObjectAssembly, first step is to replace all inclusions of the object assembly header with this one
#include "ModumateCore/ModumateUnits.h"
#include "ModumateCore/ModumateConsoleCommand.h"
#include "ModumateCore/ExpressionEvaluator.h"
#include "Database/ModumateArchitecturalMaterial.h"
#include "Database/ModumateArchitecturalMesh.h"
#include "Database/ModumateSimpleMesh.h"
#include "BIMKernel/BIMLegacyPattern.h"
#include "DocumentManagement/ModumateSerialization.h"

struct FBIMPresetCollection;
class FModumateDatabase;

/*
Assemblies have three categories of subcomponent: layers, part slots and extrusions

An assembly layer (for walls, floors and other sandwich objects) references an FArchitecturalMaterial and some metadata
*/

class MODUMATE_API FBIMAssemblySpec
{
private:
	EBIMResult MakeLayeredAssembly(const FModumateDatabase& InDB);
	EBIMResult MakeExtrudedAssembly(const FModumateDatabase& InDB);
	EBIMResult MakeRiggedAssembly(const FModumateDatabase& InDB);	
	EBIMResult MakeCabinetAssembly(const FModumateDatabase& InDB);
	EBIMResult DoMakeAssembly(const FModumateDatabase& InDB, const FBIMPresetCollection& PresetCollection);

	FBIMPropertySheet RootProperties;

public:

	EObjectType ObjectType = EObjectType::OTNone;

	FBIMKey RootPreset;

	TArray<FBIMLayerSpec> Layers,TreadLayers,RiserLayers;
	TArray<FBIMPartSlotSpec> Parts;
	TArray<FBIMExtrusionSpec> Extrusions;

	FString DisplayName,Comments,CodeName;

	FVector Normal = FVector(0, 0, 1);
	FVector Tangent = FVector(0, 1, 0);

	Modumate::Units::FUnitValue ToeKickDepth = Modumate::Units::FUnitValue::WorldCentimeters(0);
	Modumate::Units::FUnitValue ToeKickHeight = Modumate::Units::FUnitValue::WorldCentimeters(0);
	Modumate::Units::FUnitValue TreadDepth = Modumate::Units::FUnitValue::WorldCentimeters(0);

	// For DataCollection support in preset manager
	FBIMKey UniqueKey() const { return RootPreset; }

	void Reset();

	EBIMResult FromPreset(const FModumateDatabase& InDB, const FBIMPresetCollection& PresetCollection, const FBIMKey& PresetID);

	Modumate::Units::FUnitValue CalculateThickness() const;

	void ReverseLayers();

	FVector GetRiggedAssemblyNativeSize() const;
};

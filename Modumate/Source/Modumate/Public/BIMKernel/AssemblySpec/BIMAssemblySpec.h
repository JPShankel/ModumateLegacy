// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Database/ModumateObjectEnums.h"
#include "BIMKernel/Core/BIMProperties.h"
#include "BIMKernel/Core/BIMEnums.h"
#include "BIMKernel/Core/BIMKey.h"

#include "BIMKernel/AssemblySpec/BIMLayerSpec.h"
#include "BIMKernel/AssemblySpec/BIMPartSlotSpec.h"
#include "BIMKernel/AssemblySpec/BIMExtrusionSpec.h"

#include "Database/ModumateDataCollection.h"

#include "ModumateCore/EdgeDetailData.h"

#include "BIMAssemblySpec.generated.h"

struct FBIMPresetCollection;
class FModumateDatabase;

/*
Assemblies have three categories of subcomponent: layers, part slots and extrusions

An assembly layer (for walls, floors and other sandwich objects) references an FArchitecturalMaterial and some metadata
*/

static constexpr int32 BIM_ROOT_PART = 0;

USTRUCT()
struct MODUMATE_API FBIMAssemblySpec
{
	GENERATED_BODY()
private:
	EBIMResult MakeLayeredAssembly(const FModumateDatabase& InDB);
	EBIMResult MakeExtrudedAssembly(const FModumateDatabase& InDB);
	EBIMResult MakeRiggedAssembly(const FModumateDatabase& InDB);	
	EBIMResult MakeCabinetAssembly(const FModumateDatabase& InDB);
	EBIMResult DoMakeAssembly(const FModumateDatabase& InDB, const FBIMPresetCollection& PresetCollection);

	UPROPERTY()
	FBIMPropertySheet RootProperties;

public:

	UPROPERTY()
	EObjectType ObjectType = EObjectType::OTNone;

	UPROPERTY()
	FGuid PresetGUID;

	UPROPERTY()
	TArray<FBIMLayerSpec> Layers;

	UPROPERTY()
	TArray<FBIMLayerSpec> TreadLayers;

	UPROPERTY()
	TArray<FBIMLayerSpec> RiserLayers;

	UPROPERTY()
	TArray<FBIMPartSlotSpec> Parts;
	
	UPROPERTY()
	TArray<FBIMExtrusionSpec> Extrusions;

	UPROPERTY()
	FString DisplayName;

	UPROPERTY()
	FString Comments;

	UPROPERTY()
	FString CodeName;

	UPROPERTY()
	FVector Normal = FVector(0, 0, 1);
	
	UPROPERTY()
	FVector Tangent = FVector(0, 1, 0);

	UPROPERTY()
	FString SlotConfigConceptualSizeY;

	UPROPERTY()
	FBIMTagPath SlotConfigTagPath;

	UPROPERTY()
	float ToeKickDepthCentimeters = 0.0f;
	
	UPROPERTY()
	float ToeKickHeightCentimeters = 0.0f;
	
	UPROPERTY()
	float TreadDepthCentimeters = 0.0f;

	UPROPERTY()
	FEdgeDetailData EdgeDetailData;

	UPROPERTY()
	EPresetMeasurementMethod MeasurementMethod = EPresetMeasurementMethod::None;

	// For DataCollection support in preset manager
	FGuid UniqueKey() const { return PresetGUID; }

	EBIMResult Reset();

	EBIMResult FromPreset(const FModumateDatabase& InDB, const FBIMPresetCollection& PresetCollection, const FGuid& InPresetID);

	FModumateUnitValue CalculateThickness() const;

	EBIMResult ReverseLayers();

	FVector GetRiggedAssemblyNativeSize() const;
};

using FAssemblyDataCollection = TModumateDataCollection<FBIMAssemblySpec>;

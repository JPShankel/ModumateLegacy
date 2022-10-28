// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Objects/ModumateObjectEnums.h"
#include "BIMKernel/Core/BIMProperties.h"
#include "BIMKernel/Core/BIMEnums.h"
#include "BIMKernel/AssemblySpec/BIMLayerSpec.h"
#include "BIMKernel/AssemblySpec/BIMPartSlotSpec.h"
#include "BIMKernel/AssemblySpec/BIMExtrusionSpec.h"

#include "BIMKernel/Presets/BIMPresetPatternDefinition.h"
#include "BIMKernel/Presets/CustomData/BIMDimensions.h"
#include "BIMKernel/Presets/CustomData/BIMMeshRef.h"
#include "BIMKernel/Presets/CustomData/BIMPart.h"

#include "Database/ModumateDataCollection.h"

#include "ModumateCore/EdgeDetailData.h"

#include "BIMAssemblySpec.generated.h"

struct FBIMPresetCollection;
class FModumateDatabase;
class FBIMPresetCollectionProxy;

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
	EBIMResult MakeLayeredAssembly(const FBIMPresetCollectionProxy& InDB);
	EBIMResult MakeExtrudedAssembly(const FBIMPresetCollectionProxy& InDB);
	EBIMResult MakeRiggedAssembly(const FBIMPresetCollectionProxy& InDB);
	EBIMResult MakeCabinetAssembly(const FBIMPresetCollectionProxy& InDB);
	EBIMResult DoMakeAssembly(const FBIMPresetCollectionProxy& PresetCollection);

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
	bool bZalign = false;

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
	FBIMPattern PatternData;
	
	UPROPERTY()
	FBIMPartConfig PartData;

	UPROPERTY()
	FBIMMeshRef MeshRef;

	UPROPERTY()
	FBIMDimensions PresetDimensions;

	// Cabinets have material bindings at the assembly level
	UPROPERTY()
	FBIMPresetMaterialBindingSet MaterialBindingSet;

	UPROPERTY()
	EPresetMeasurementMethod MeasurementMethod = EPresetMeasurementMethod::None;

	UPROPERTY()
	FLightConfiguration LightConfiguration;

	// For DataCollection support in preset manager
	FGuid UniqueKey() const { return PresetGUID; }

	EBIMResult Reset();

	EBIMResult FromPreset(const FBIMPresetCollectionProxy& PresetCollection, const FGuid& InPresetID);

	FModumateUnitValue CalculateThickness() const;

	EBIMResult ReverseLayers();

	FVector GetCompoundAssemblyNativeSize() const;
};

using FAssemblyDataCollection = TModumateDataCollection<FBIMAssemblySpec>;

// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Database/ModumateObjectEnums.h"
#include "BIMKernel/BIMProperties.h"
#include "BIMKernel/BIMEnums.h"
#include "BIMKernel/BIMKey.h"

// All needed by ObjectAssembly, first step is to replace all inclusions of the object assembly header with this one
#include "ModumateCore/ModumateUnits.h"
#include "ModumateCore/ModumateConsoleCommand.h"
#include "ModumateCore/ExpressionEvaluator.h"
#include "Database/ModumateArchitecturalMaterial.h"
#include "Database/ModumateArchitecturalMesh.h"
#include "Database/ModumateSimpleMesh.h"
#include "BIMKernel/BIMLegacyPattern.h"
#include "DocumentManagement/ModumateSerialization.h"

class FBIMPresetCollection;
class FModumateDatabase;

/*
Assemblies have three categories of subcomponent: layers, part slots and extrusions

An assembly layer (for walls, floors and other sandwich objects) references an FArchitecturalMaterial and some metadata
*/
class MODUMATE_API FBIMLayerSpec
{
	friend class FBIMAssemblySpec;
private:
	FBIMPropertySheet Properties;
	ECraftingResult BuildFromProperties(const FModumateDatabase& InDB);

public:
	ELayerFunction Function = ELayerFunction::None;

	FString CodeName;
	FString PresetSequence;

	Modumate::Units::FUnitValue Thickness;

	// TODO: Modules with materials, patterns that reference modules
	FArchitecturalMaterial Material;

};

/*
An assembly part slot contains a mesh and local transform
TODO: transforms will be derived from hosting plane/moi parameters...to be hard coded initially
*/
class MODUMATE_API FBIMPartSlotSpec
{
	friend class FBIMAssemblySpec;
private:
	FBIMPropertySheet Properties;
	ECraftingResult BuildFromProperties(const FModumateDatabase& InDB);


public:
	Modumate::Expression::FVectorExpression Translation, Size, Orientation;
	using FBooleanVector = bool[3];
	FBooleanVector Flip {false, false, false};

	int32 ParentSlotIndex = 0;

	FArchitecturalMesh Mesh;
	TMap<FName, FArchitecturalMaterial> ChannelMaterials;
};

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

class MODUMATE_API FBIMAssemblySpec
{
private:
	ECraftingResult MakeLayeredAssembly(const FModumateDatabase& InDB);
	ECraftingResult MakeExtrudedAssembly(const FModumateDatabase& InDB);
	ECraftingResult MakeRiggedAssembly(const FModumateDatabase& InDB);	
	ECraftingResult DoMakeAssembly(const FModumateDatabase& InDB, const FBIMPresetCollection& PresetCollection);

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

	ECraftingResult FromPreset(const FModumateDatabase& InDB, const FBIMPresetCollection& PresetCollection, const FBIMKey& PresetID);

	Modumate::Units::FUnitValue CalculateThickness() const;

	void ReverseLayers();

	FVector GetRiggedAssemblyNativeSize() const;
};

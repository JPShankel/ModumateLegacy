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
struct MODUMATE_API FBIMLayerSpec
{
	FBIMPropertySheet Properties;

	ELayerFunction Function = ELayerFunction::None;

	FString CodeName;
	FString PresetSequence;

	Modumate::Units::FUnitValue Thickness;

	// TODO: Modules with materials, patterns that reference modules
	FArchitecturalMaterial Material;

	ECraftingResult BuildFromProperties(const FModumateDatabase& InDB);
};

/*
An assembly part slot contains a mesh and local transform
TODO: transforms will be derived from hosting plane/moi parameters...to be hard coded initially
*/
struct MODUMATE_API FBIMPartSlotSpec
{
	FBIMPropertySheet Properties;

	Modumate::Expression::FVectorExpression Translation, Scale, Orientation;

	FArchitecturalMesh Mesh;
	ECraftingResult BuildFromProperties(const FModumateDatabase& InDB);
};

/*
An extrusion is a simple 2D mesh extruded along a hosted edge
*/
struct MODUMATE_API FBIMExtrusionSpec
{
	FBIMPropertySheet Properties;
	FArchitecturalMaterial Material;
	TArray<FSimpleMeshRef> SimpleMeshes;
	ECraftingResult BuildFromProperties(const FModumateDatabase& InDB);
};

class MODUMATE_API FBIMAssemblySpec
{
private:
	ECraftingResult MakeLayeredAssembly(const FModumateDatabase& InDB);
	ECraftingResult MakeExtrudedAssembly(const FModumateDatabase& InDB);
	ECraftingResult MakeRiggedAssembly(const FModumateDatabase& InDB);	
	ECraftingResult DoMakeAssembly(const FModumateDatabase& InDB, const FBIMPresetCollection& PresetCollection);

public:
	EObjectType ObjectType = EObjectType::OTNone;

	FBIMKey RootPreset;
	FBIMPropertySheet RootProperties;

	TArray<FBIMLayerSpec> Layers;
	TArray<FBIMPartSlotSpec> Parts;
	TArray<FBIMExtrusionSpec> Extrusions;

	// For DataCollection support in preset manager
	FBIMKey UniqueKey() const { return RootPreset; }

	void Reset();

	ECraftingResult FromPreset(const FModumateDatabase& InDB, const FBIMPresetCollection& PresetCollection, const FBIMKey& PresetID);

	// Helper functions for getting properties in the Assembly scope
	// TODO: refactor for typesafe properties
	Modumate::FModumateCommandParameter GetProperty(const FBIMNameType& Name) const;
	void SetProperty(const FBIMNameType& Name, const Modumate::FModumateCommandParameter& Value);
	bool HasProperty(const FBIMNameType& Name) const;

	template <class T>
	bool TryGetProperty(const FBIMNameType& Name, T& OutT) const
	{
		if (HasProperty(Name))
		{
			OutT = GetProperty(Name);
			return true;
		}
		return false;
	}

	Modumate::Units::FUnitValue CalculateThickness() const;

	void ReverseLayers();
};

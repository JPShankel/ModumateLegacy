// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TextureLightProfile.h"
#include "BIMKernel/BIMEnums.h"
#include "BIMKernel/BIMLegacyPattern.h"
#include "BIMKernel/BIMSerialization.h"
#include "BIMKernel/BIMProperties.h"
#include "Database/ModumateArchitecturalMesh.h"
#include "Database/ModumateSimpleMesh.h"
#include "DocumentManagement/ModumateSerialization.h"
#include "ModumateObjectAssembly.generated.h"

class FModumateDatabase;
class FBIMAssemblySpec;

USTRUCT()
struct FLightConfiguration
{
	GENERATED_BODY();

	FName Key;
	float LightIntensity = 0.f;
	FLinearColor LightColor = FLinearColor::White;
	TWeakObjectPtr<UTextureLightProfile> LightProfile = nullptr;
	bool bAsSpotLight = false;

	FName UniqueKey() const { return Key; }
};

/*
An assembly layer (for walls, floors and other sandwich objects) references an FArchitecturalMaterial and some metadata
*/
USTRUCT()
struct MODUMATE_API FModumateObjectAssemblyLayer
{
	GENERATED_BODY()

	ELayerFunction Function = ELayerFunction::None;
	ELayerFormat Format = ELayerFormat::None;

	TArray<FString> SubcategoryDisplayNames;

	FString DisplayName = FString();
	FString Comments = FString();

	FString CodeName = FString();
	FString PresetSequence = FString();

	Modumate::Units::FUnitValue Thickness;

	FArchitecturalMaterial Material = FArchitecturalMaterial();
	FCustomColor BaseColor = FCustomColor();

	FArchitecturalMesh Mesh = FArchitecturalMesh();
	TArray<FArchitecturalMaterial> ExtraMaterials;

	FVector LightLocation;
	FRotator LightRotation;
	FLightConfiguration LightConfiguration;

	TArray<FSimpleMeshRef> SimpleMeshes;

	FVector SlotScale = FVector::OneVector;
};

/*
An Object assembly is a collection of layers
*/

class FPresetManager;
class FModumateDatabase;
class FModumateDocument;

USTRUCT()
struct MODUMATE_API FModumateObjectAssembly
{
	GENERATED_BODY();

	EObjectType ObjectType = EObjectType::OTUnknown;
	FName DatabaseKey, RootPreset;

	FBIMPropertySheet Properties;

	TArray<FModumateObjectAssemblyLayer> Layers;

	Modumate::Units::FUnitValue CalculateThickness() const;

	// Helper functions for getting properties in the Assembly scope
	Modumate::FModumateCommandParameter GetProperty(const FBIMNameType &name) const;
	void SetProperty(const FBIMNameType &name, const Modumate::FModumateCommandParameter &v);
	bool HasProperty(const FBIMNameType &name) const;

	template <class T>
	bool TryGetProperty(const FBIMNameType &name, T &outT) const
	{
		if (HasProperty(name))
		{
			outT = GetProperty(name);
			return true;
		}
		return false;
	}

	void ReverseLayers();
};

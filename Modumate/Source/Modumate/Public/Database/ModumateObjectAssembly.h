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
struct FBIMAssemblySpec;

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

	FLayerPattern Pattern = FLayerPattern();
	FLayerPatternGap Gap;
	TArray<FLayerPatternModule> Modules;
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

UCLASS()
class MODUMATE_API UModumateObjectAssemblyStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	private:
		/*
		Support functions for producing assemblies of various categorical types
		TODO: consider moving these to individual MOI modules after all categories are covered for DDL2
		*/
		static ECraftingResult MakeLayeredAssembly(
			const FModumateDatabase &InDB,
			const FBIMAssemblySpec &InSpec,
			FModumateObjectAssembly &OutMOA);

		static ECraftingResult MakeStructureLineAssembly(
			const FModumateDatabase &InDB,
			const FBIMAssemblySpec &InSpec,
			FModumateObjectAssembly &OutMOA);

		static ECraftingResult MakeStubbyAssembly(
			const FModumateDatabase& InDB,
			const FBIMAssemblySpec& InSpec,
			FModumateObjectAssembly& OutMOA);

public:
		static bool CheckCanMakeAssembly(
			EObjectType OT,
			const FModumateDatabase &InDB,
			const FBIMAssemblySpec &InSpec);

		static ECraftingResult DoMakeAssembly(
			const FModumateDatabase &InDB,
			const FPresetManager &PresetManager,
			const FBIMAssemblySpec &InSpec,
			FModumateObjectAssembly &OutMOA,
			const int32 InShowOnlyLayerID = -1);
};
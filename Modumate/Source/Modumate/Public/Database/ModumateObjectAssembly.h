// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TextureLightProfile.h"
#include "BIMKernel/BIMEnums.h"
#include "BIMKernel/BIMLegacyPattern.h"
#include "BIMKernel/BIMLegacyPortals.h"
#include "BIMKernel/BIMSerialization.h"
#include "BIMKernel/BIMProperties.h"
#include "Database/ModumateArchitecturalMesh.h"
#include "DocumentManagement/ModumateSerialization.h"
#include "ModumateObjectAssembly.generated.h"



USTRUCT(BlueprintType)
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
A part used for portals (doors/windows) that specifies its mesh, compatible slots,
and other portal-specific data that allows it to be 9-sliced and manipulated.
*/
USTRUCT(BlueprintType)
struct MODUMATE_API FPortalPart
{
	GENERATED_BODY()

	FName Key;
	FText DisplayName;
	TSet<EPortalSlotType> SupportedSlotTypes;
	FArchitecturalMesh Mesh = FArchitecturalMesh();

	// The original bounds of the entire mesh
	Modumate::Units::FUnitValue NativeSizeX = Modumate::Units::FUnitValue::WorldCentimeters(0.0f);
	Modumate::Units::FUnitValue NativeSizeY = Modumate::Units::FUnitValue::WorldCentimeters(0.0f);
	Modumate::Units::FUnitValue NativeSizeZ = Modumate::Units::FUnitValue::WorldCentimeters(0.0f);

	// The bounding box that represents the region of the mesh that can be stretched by 9-slicing, in cm.
	FBox NineSliceInterior = FBox(ForceInitToZero);

	TMap<FName, Modumate::Units::FUnitValue> ConfigurationDimensions;
	TSet<FName> SupportedMaterialChannels;

	FName UniqueKey() const { return Key; }
};

struct FPortalConfiguration
{
	EPortalFunction PortalFunction = EPortalFunction::None;
	TMap<FName, Modumate::FPortalReferencePlane> ReferencePlanes;
	TArray<Modumate::FPortalAssemblyConfigurationSlot> Slots;
	Modumate::FPortalConfigDimensionSet Width, Height;
	Modumate::FCraftingPortalPartOption PartSet;
	TMap<FName, FArchitecturalMaterial> MaterialsPerChannel;
	FText DisplayName;

	// Coalesced mapping of slot type and config dimension to fixed unit values
	// e.g. Frame.GapBehind -> 1.25in, Panel.JambThickness -> 2in
	TMap<FName, Modumate::Units::FUnitValue> CachedDimensions;

	static const FName RefPlaneNameMinX, RefPlaneNameMaxX, RefPlaneNameMinZ, RefPlaneNameMaxZ;

	bool IsValid() const;
	void CacheRefPlaneValues();
};

/*
An Object assembly is a collection of layers
*/

class FPresetManager;
class FModumateDatabase;
class FModumateDocument;

USTRUCT(BlueprintType)
struct MODUMATE_API FModumateObjectAssembly
{
	GENERATED_BODY();

	EObjectType ObjectType = EObjectType::OTUnknown;
	FName DatabaseKey, RootPreset;

	Modumate::BIM::FBIMPropertySheet Properties;

	TArray<FModumateObjectAssemblyLayer> Layers;

	Modumate::Units::FUnitValue CalculateThickness() const;

	// Helper functions for getting properties in the Assembly scope
	Modumate::FModumateCommandParameter GetProperty(const Modumate::BIM::FNameType &name) const;
	void SetProperty(const Modumate::BIM::FNameType &name, const Modumate::FModumateCommandParameter &v);
	bool HasProperty(const Modumate::BIM::FNameType &name) const;

	template <class T>
	bool TryGetProperty(const Modumate::BIM::FNameType &name, T &outT) const
	{
		if (HasProperty(name))
		{
			outT = GetProperty(name);
			return true;
		}
		return false;
	}

	void ReverseLayers();

	FPortalConfiguration PortalConfiguration;
	TMap<int32, FPortalPart> PortalParts;
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
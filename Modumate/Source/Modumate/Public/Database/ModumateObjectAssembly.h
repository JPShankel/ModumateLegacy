// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ModumateUnits.h"
#include "ModumateConsoleCommand.h"
#include "ModumateArchitecturalMaterial.h"
#include "ModumateArchitecturalMesh.h"
#include "ModumateSimpleMesh.h"
#include "ModumateLayerPattern.h"
#include "ModumateShoppingItem.h"
#include "ModumateCrafting.h"
#include "ModumateSerialization.h"
#include "ModumateObjectAssembly.generated.h"


USTRUCT(BlueprintType)
struct FLightConfiguration
{
	GENERATED_USTRUCT_BODY();

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
	GENERATED_USTRUCT_BODY();

	FName DatabaseKey = FName();

	FName PresetKey = FName();

	ELayerFunction Function = ELayerFunction::None;
	FText FunctionDisplayName;

	ELayerFormat Format = ELayerFormat::None;
	FText FormatDisplayName;

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

	TSet<EObjectType> CompatibleObjectTypes;

	FVector SlotScale = FVector::OneVector;

	// WHEN UPDATING FIELDS, UPDATE THE FCustomAssemblyLayerRecord STRUCT!!!

	FShoppingItem AsShoppingItem() const;

	// Used by database to sort assemblies
	FName UniqueKey() const { return DatabaseKey; }

	// Helper functions for getting properties in the Layer scope
	Modumate::FModumateCommandParameter GetProperty(const Modumate::BIM::EScope &scope, const Modumate::BIM::FNameType &name) const;

private: 	
	friend struct FModumateObjectAssembly;
	friend class FLayerMaker;
	Modumate::BIM::FBIMPropertySheet Properties;

};

/*
A part used for portals (doors/windows) that specifies its mesh, compatible slots,
and other portal-specific data that allows it to be 9-sliced and manipulated.
*/
USTRUCT(BlueprintType)
struct FPortalPart
{
	GENERATED_USTRUCT_BODY();

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

USTRUCT(BlueprintType)
struct FPortalConfiguration
{
	GENERATED_USTRUCT_BODY();

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
namespace Modumate
{
	class MODUMATE_API IPresetManager;
	class MODUMATE_API ModumateObjectDatabase;
	class MODUMATE_API FModumateDocument;
	class MODUMATE_API FPresetManager;
}

USTRUCT(BlueprintType)
struct FModumateObjectAssembly
{
	GENERATED_USTRUCT_BODY();

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

	FPortalConfiguration PortalConfiguration;
	TMap<int32, FPortalPart> PortalParts;

	// Used by database to sort assemblies
	FName UniqueKey() const { return DatabaseKey; }
	FShoppingItem AsShoppingItem() const;
	FString GetGenomeString() const;
	FCustomAssemblyRecord ToDataRecord() const;


	void FillSpec(Modumate::BIM::FModumateAssemblyPropertySpec &spec) const;

	// Used by DDL 1.0 to import assemblies from marketplace and update assemblies in crafting
	// Under DDL 2.0, we just rely on FModumateAssemblyPropertySpec
	bool ToParameterSet_DEPRECATED(Modumate::FModumateFunctionParameterSet &params) const;

	// DDL 1.0 presets
	void GatherPresets_DEPRECATED(const Modumate::FPresetManager &presetManager, TArray<FName> &presetKeys) const;
	void ReplacePreset_DEPRECATED(const FName &oldPreset, const FName &newPreset);
	bool UsesPreset_DEPRECATED(const FName &presetKey) const;

	// Reverse the assembly's layer list. This modification has no connection to the assembly's crafting/BIM definition,
	// and is purely meant to serve as a reinterpretation of cached layer properties by an assembly's owner.
	// As such, the owner needs to keep track of whether the layers have been reversed.
	void InvertLayers();

	//TODO: move to UModumateObjectAssemblyStatics and refactor each object type to do its own assembly synthesis/analysis
	static bool FromCraftingProperties_DEPRECATED(
		EObjectType ot,
		const Modumate::ModumateObjectDatabase &db,
		const Modumate::FPresetManager &presetManager,
		const Modumate::BIM::FModumateAssemblyPropertySpec &spec,
		FModumateObjectAssembly &outMOA,
		const int32 showOnlyLayerID = -1);

	//TODO: in DDL 2.0, runtime assemblies will not need to be serialized
	static bool FromDataRecord_DEPRECATED(
		const FCustomAssemblyRecord &record, 
		const Modumate::ModumateObjectDatabase &objectDB,
		const Modumate::FPresetManager &presetManager,
		FModumateObjectAssembly &outMOA);
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
			const Modumate::ModumateObjectDatabase &InDB,
			const Modumate::BIM::FModumateAssemblyPropertySpec &InSpec,
			FModumateObjectAssembly &OutMOA);

		static ECraftingResult MakeStructureLineAssembly(
			const Modumate::ModumateObjectDatabase &InDB,
			const Modumate::BIM::FModumateAssemblyPropertySpec &InSpec,
			FModumateObjectAssembly &OutMOA);

	public:
		static bool CheckCanMakeAssembly(
			EObjectType OT,
			const Modumate::ModumateObjectDatabase &InDB,
			const Modumate::BIM::FModumateAssemblyPropertySpec &InSpec);

		static ECraftingResult DoMakeAssembly(
			const Modumate::ModumateObjectDatabase &InDB,
			const Modumate::FPresetManager &PresetManager,
			const Modumate::BIM::FModumateAssemblyPropertySpec &InSpec,
			FModumateObjectAssembly &OutMOA,
			const int32 InShowOnlyLayerID = -1);

		// TODO: remove after DDL2 migration
		static bool ObjectTypeSupportsDDL2(EObjectType OT);

};
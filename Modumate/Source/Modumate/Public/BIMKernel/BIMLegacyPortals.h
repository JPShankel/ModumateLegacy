// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"

#include "ModumateCore/ModumateUnits.h"
#include "ModumateCore/ModumateConsoleCommand.h"
#include "Database/ModumateArchitecturalMaterial.h"
#include "Database/ModumateObjectEnums.h"
#include "Database/ModumateSimpleMesh.h"
#include "BIMKernel/BIMProperties.h"
#include "BIMKernel/BIMWidgetStatics.h"
#include "BIMLegacyPortals.generated.h"

UENUM(BlueprintType)
enum class EPortalFunction : uint8
{
	None,
	CasedOpening,
	Swing,
	Barn,
	Pocket,
	Sliding,
	Folding,
	Fixed,
	Casement,
	Awning,
	Hopper,
	Hung,
	Jalousie,
	Cabinet
};

UENUM(BlueprintType)
enum class EPortalSlotType : uint8
{
	None,
	Frame,
	Panel,
	Hinge,
	Handle,
	Lock,
	Closer,
	Knocker,
	Peephole,
	Protection,
	Silencer,
	Stop,
	DogDoor,
	MailSlot,
	Astragal,
	Sidelite,
	Transom,
	Screen,
	Hole,
	Mullion,
	Track,
	Caster
};

class MODUMATE_API FModumateDatabase;

namespace Modumate
{

	struct MODUMATE_API FCraftingOptionBase
	{
		FName Key;
		FText DisplayName;

		TWeakObjectPtr<UTexture2D> Icon;
		TWeakObjectPtr<UMaterialInterface> EngineMaterial;
		FCustomColor CustomColor;
		FSimpleMeshRef ProfileMesh;

		Modumate::BIM::FBIMPropertySheet CraftingParameters;

		FName UniqueKey() const { return Key; }
	};

	namespace CraftingParameters
	{
		extern const FString ThicknessValue;
		extern const FString ThicknessUnits;
		extern const FString MaterialColorMaterial;
		extern const FString MaterialColorColor;
		extern const FString DimensionLength;
		extern const FString DimensionWidth;
		extern const FString DimensionHeight;
		extern const FString DimensionDepth;
		extern const FString DimensionThickness;
		extern const FString DimensionBevelWidth;
		extern const FString PatternModuleCount;
		extern const FString PatternExtents;
		extern const FString PatternThickness;
		extern const FString PatternGap;
		extern const FString PatternName;
		extern const FString PatternModuleDimensions;
		extern const FString TrimProfileNativeSizeX;
		extern const FString TrimProfileNativeSizeY;
	}

	struct MODUMATE_API FCraftingSubcategoryData : public FCraftingOptionBase
	{
		FString SheetName;
		FString IDCodeLine1;
		ELayerFunction LayerFunction;
		ELayerFormat LayerFormat;
	};

	template<class T>
	struct MODUMATE_API TCraftingOptionSet
	{
		typedef T OptionType;
		FName Key;
		TArray<T> Options;
		FName UniqueKey() const { return Key; }
	};

	typedef TCraftingOptionSet<FCraftingOptionBase> FCraftingOptionSet;

	struct MODUMATE_API FCraftingPortalPartOption : public FCraftingOptionBase
	{
		TSet<FName> ConfigurationWhitelist;
		TMap<EPortalSlotType, FName> PartsBySlotType;
		static const FString TableName;
	};

	typedef TCraftingOptionSet<FCraftingPortalPartOption> FCraftingPortalPartOptionSet;

	struct MODUMATE_API FPortalAssemblyConfigurationSlot
	{
		EPortalSlotType Type = EPortalSlotType::None;
		FString LocationX, LocationY, LocationZ;
		FString SizeX, SizeY, SizeZ;
		float RotateX = 0, RotateY = 0, RotateZ = 0;
		bool FlipX = false, FlipY = false, FlipZ = false;
	};

	struct MODUMATE_API FPortalReferencePlane
	{
		Modumate::Units::FUnitValue FixedValue = Modumate::Units::FUnitValue::WorldCentimeters(0);
		FString ValueExpression = FString();
		EAxis::Type Axis = EAxis::None;
		int32 Index = 0;
		FName Name;
	};

	struct MODUMATE_API FPortalConfigDimensionSet
	{
		FName Key;
		FText DisplayName;
		TMap<FName, Modumate::Units::FUnitValue> DimensionMap;
	};

	struct MODUMATE_API FPortalAssemblyConfigurationOption : public FCraftingOptionBase
	{
		EPortalFunction PortalFunction = EPortalFunction::None;
		TArray<FPortalReferencePlane> ReferencePlanes[3];
		TArray<FPortalAssemblyConfigurationSlot> Slots;
		TArray<FPortalConfigDimensionSet> SupportedWidths;
		TArray<FPortalConfigDimensionSet> SupportedHeights;

		static const FString TableName;

		bool IsValid() const;
	};

	typedef TCraftingOptionSet<FPortalAssemblyConfigurationOption> FPortalAssemblyConfigurationOptionSet;
}

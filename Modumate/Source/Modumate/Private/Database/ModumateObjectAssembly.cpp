// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "Database/ModumateObjectAssembly.h"

#include "ModumateCore/ExpressionEvaluator.h"
#include "DocumentManagement/ModumateDocument.h"
#include "DocumentManagement/ModumateDocument.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "DocumentManagement/ModumateCommands.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "Database/ModumateObjectDatabase.h"
#include "Algo/Accumulate.h"
#include "Algo/Reverse.h"

using namespace Modumate;
using namespace Modumate::Units;

Modumate::Units::FUnitValue FModumateObjectAssembly::CalculateThickness() const
{
	return Modumate::Units::FUnitValue::WorldCentimeters(Algo::TransformAccumulate(
		Layers,
		[](const FModumateObjectAssemblyLayer &l)
		{
			return l.Thickness.AsWorldCentimeters();
		},
		0.0f,
		[](float lhs, float rhs)
		{
			return lhs + rhs;
		}
	));
}

Modumate::FModumateCommandParameter FModumateObjectAssembly::GetProperty(const FBIMNameType &name) const
{
	return Properties.GetProperty(EBIMValueScope::Assembly, name);
}

void FModumateObjectAssembly::SetProperty(const FBIMNameType &name, const Modumate::FModumateCommandParameter &val)
{
	Properties.SetProperty(EBIMValueScope::Assembly, name, val);
}

bool FModumateObjectAssembly::HasProperty(const FBIMNameType &name) const
{
	return Properties.HasProperty(EBIMValueScope::Assembly, name);
}

class MODUMATE_API FLayerMaker
{
public:
	TArray<FName> Subcategories;
	FString CodeName;

	ELayerFunction FunctionEnum = ELayerFunction::None;
	FText FunctionName;

	ELayerFormat FormatEnum = ELayerFormat::None;
	FText FormatName;

	FUnitValue Thickness;

	FString LayerMaterialKey;
	FString LayerColorKey;

	FName ProfileKey;

	FBIMPropertySheet Properties;
	TMap<FBIMNameType, FModumateFormattedDimension> Dimensions;

	void SetValue(
		const FBIMPropertyValue &var,
		const FModumateCommandParameter &value)
	{
		auto convertDimension = [this](const FString &dimStr,const FBIMNameType &property)
		{
			FModumateFormattedDimension dim = UModumateDimensionStatics::StringToFormattedDimension(dimStr);
			Dimensions.Add(property, dim);
			return FUnitValue::WorldCentimeters(dim.Centimeters);
		};

		Properties.SetValue(var.QN().ToString(), value);

		//TODO: deprecated DDL 1.0 .. DDL 2 tracks all layer values in the Module or Layer scope

		if (var.Name == BIMPropertyNames::AssetID)
		{
			if (var.Scope == EBIMValueScope::RawMaterial)
			{
				LayerMaterialKey = value;
			}
			if (var.Scope == EBIMValueScope::Color)
			{
				LayerColorKey = value;
			}
		}

		if (var.Scope == EBIMValueScope::Dimension)
		{
			if (var.Name == BIMPropertyNames::Thickness)
			{
				Thickness = convertDimension(value, var.QN());
			}
		}
	}

	FModumateObjectAssemblyLayer Make(const FModumateDatabase &db)
	{
		FModumateObjectAssemblyLayer ret;

		ret.Format = FormatEnum;
		ret.Function = FunctionEnum;
		ret.Thickness = Thickness;
		ret.DisplayName = CodeName;

		if (!ProfileKey.IsNone())
		{
			ret.Function = ELayerFunction::Finish;
			ret.Format = ELayerFormat::Board;

			const FSimpleMeshRef *trimMesh = db.GetSimpleMeshByKey(ProfileKey);

			if (ensureAlways(trimMesh != nullptr))
			{
				ret.SimpleMeshes.Add(*trimMesh);
			}
		}

		if (!LayerMaterialKey.IsEmpty())
		{
			const FArchitecturalMaterial *mat = db.GetArchitecturalMaterialByKey(*LayerMaterialKey);
			if (ensureAlways(mat != nullptr))
			{
				ret.Material = *mat;
				ensureAlways(ret.Material.EngineMaterial != nullptr);
			}
		}

		if (!LayerColorKey.IsEmpty())
		{
			const FCustomColor* color = db.GetCustomColorByKey(*LayerColorKey);
			if (ensureAlways(color != nullptr))
			{
				ret.BaseColor = *color;
				ret.Material.DefaultBaseColor = *color;
			}
		}

		if (ret.BaseColor.Key.IsNone())
		{
			ret.BaseColor.Key = TEXT("COLORKEY");
		}

		return ret;
	}
};

void FModumateObjectAssembly::ReverseLayers()
{
	Algo::Reverse(Layers);
}

bool UModumateObjectAssemblyStatics::CheckCanMakeAssembly(
	EObjectType OT,
	const FModumateDatabase &InDB,
	const FBIMAssemblySpec &InSpec)
{
	// TODO: Generalize required property specification and move check functions to tools or object types (TBD)
	return !InSpec.RootPreset.IsNone();
}

ECraftingResult UModumateObjectAssemblyStatics::MakeStubbyAssembly(
	const FModumateDatabase& InDB,
	const FBIMAssemblySpec& InSpec,
	FModumateObjectAssembly& OutMOA)
{
	FName meshKey = InSpec.RootProperties.GetProperty(EBIMValueScope::Mesh, BIMPropertyNames::AssetID);
	const FArchitecturalMesh* mesh = InDB.GetArchitecturalMeshByKey(meshKey);
	if (mesh == nullptr)
	{
		return ECraftingResult::Error;
	}

	FModumateObjectAssemblyLayer &layer = OutMOA.Layers.AddDefaulted_GetRef();
	layer.Mesh = *mesh;

	OutMOA.SetProperty(BIMPropertyNames::Normal, FVector(0, 0, 1));
	OutMOA.SetProperty(BIMPropertyNames::Tangent, FVector(0, 1, 0));

	OutMOA.ObjectType = InSpec.ObjectType;

	return ECraftingResult::Success;
}

ECraftingResult UModumateObjectAssemblyStatics::MakeLayeredAssembly(
	const FModumateDatabase &InDB,
	const FBIMAssemblySpec &InSpec,
	FModumateObjectAssembly &OutMOA)
{
	OutMOA = FModumateObjectAssembly();

	OutMOA.Properties = InSpec.RootProperties;

	for (auto &layerProperties : InSpec.LayerProperties)
	{
		FLayerMaker layerMaker;

		layerProperties.ForEachProperty([&layerMaker](const FString &propName, const FModumateCommandParameter &val)
		{
			if (!propName.IsEmpty())
			{
				layerMaker.SetValue(FBIMPropertyValue(*propName), val);
			}
		});

		FModumateObjectAssemblyLayer &layer = OutMOA.Layers.Add_GetRef(layerMaker.Make(InDB));
	}

	return ECraftingResult::Success;
}

ECraftingResult UModumateObjectAssemblyStatics::MakeStructureLineAssembly(
	const FModumateDatabase &InDB,
	const FBIMAssemblySpec &InSpec,
	FModumateObjectAssembly &OutMOA)
{
	OutMOA = FModumateObjectAssembly();
	OutMOA.Properties = InSpec.RootProperties;

	FLayerMaker layerMaker;

	layerMaker.FormatEnum = ELayerFormat::None;
	layerMaker.FunctionEnum = ELayerFunction::Structure;

	layerMaker.CodeName = OutMOA.GetProperty(BIMPropertyNames::Name);
	OutMOA.SetProperty(BIMPropertyNames::Name, layerMaker.CodeName);

	FString diameterString;
	FModumateFormattedDimension xDim, yDim;
	if (OutMOA.Properties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Diameter, diameterString))
	{
		xDim = UModumateDimensionStatics::StringToFormattedDimension(OutMOA.Properties.GetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Diameter));
		yDim = xDim;
	}
	else
	{
		xDim = UModumateDimensionStatics::StringToFormattedDimension(OutMOA.Properties.GetProperty(EBIMValueScope::Dimension, BIMPropertyNames::XExtents));
		yDim = UModumateDimensionStatics::StringToFormattedDimension(OutMOA.Properties.GetProperty(EBIMValueScope::Dimension, BIMPropertyNames::YExtents));
	}

	if (ensureAlways(xDim.Format != EDimensionFormat::Error))
	{
		layerMaker.Dimensions.Add(BIMPropertyNames::XExtents, xDim);
	}

	if (ensureAlways(yDim.Format != EDimensionFormat::Error))
	{
		layerMaker.Dimensions.Add(BIMPropertyNames::YExtents, yDim);
	}

	layerMaker.LayerMaterialKey = OutMOA.Properties.GetProperty(EBIMValueScope::RawMaterial, BIMPropertyNames::AssetID);
	layerMaker.ProfileKey = OutMOA.Properties.GetProperty(EBIMValueScope::Profile, BIMPropertyNames::AssetID);

	FModumateObjectAssemblyLayer &layer = OutMOA.Layers.Add_GetRef(layerMaker.Make(InDB));

	// TODO: re-orient column meshes so width is along X instead of depth
	FVector profileSize(xDim.Centimeters, yDim.Centimeters, 1);

	if (ensureAlways(layer.SimpleMeshes.Num() > 0 && layer.SimpleMeshes[0].Asset.Get()->Polygons.Num() > 0))
	{
		FSimplePolygon &polygon = layer.SimpleMeshes[0].Asset.Get()->Polygons[0];
		FVector polyExtents(polygon.Extents.Max.X - polygon.Extents.Min.X, polygon.Extents.Max.Y - polygon.Extents.Min.Y, 1);
		OutMOA.SetProperty(BIMPropertyNames::Scale, profileSize / polyExtents);
	}

	return ECraftingResult::Success; 
}


ECraftingResult UModumateObjectAssemblyStatics::DoMakeAssembly(
	const FModumateDatabase &InDB,
	const FPresetManager &PresetManager,
	const FBIMAssemblySpec &InSpec,
	FModumateObjectAssembly &OutMOA,
	const int32 InShowOnlyLayerID)
{
	// TODO: move assembly synthesis to each tool mode or MOI implementation (TBD)
	ECraftingResult result = ECraftingResult::Error;
	switch(InSpec.ObjectType)
	{
	case EObjectType::OTStructureLine:
		result = MakeStructureLineAssembly(InDB,InSpec,OutMOA);
		break;

	case EObjectType::OTFloorSegment:
	case EObjectType::OTWallSegment:
	case EObjectType::OTRoofFace:
	case EObjectType::OTFinish:
		result =  MakeLayeredAssembly(InDB,InSpec,OutMOA);
		break;

	case EObjectType::OTCountertop:
	case EObjectType::OTCabinet:
	case EObjectType::OTFurniture:
	case EObjectType::OTDoor:
	case EObjectType::OTWindow:
		result = MakeStubbyAssembly(InDB, InSpec, OutMOA);
		break;

	default:
		ensureAlways(false);
	};

	if (result != ECraftingResult::Error)
	{			
		OutMOA.SetProperty(BIMPropertyNames::Name, InSpec.RootProperties.GetProperty(EBIMValueScope::Preset, BIMPropertyNames::Name));
	}
	return result;
}





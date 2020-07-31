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

Modumate::FModumateCommandParameter FModumateObjectAssembly::GetProperty(const Modumate::BIM::FNameType &name) const
{
	return Properties.GetProperty(BIM::EScope::Assembly, name);
}

void FModumateObjectAssembly::SetProperty(const Modumate::BIM::FNameType &name, const Modumate::FModumateCommandParameter &val)
{
	Properties.SetProperty(BIM::EScope::Assembly, name, val);
}

bool FModumateObjectAssembly::HasProperty(const Modumate::BIM::FNameType &name) const
{
	return Properties.HasProperty(BIM::EScope::Assembly, name);
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

	FLayerPattern Pattern;
	FLayerPatternModule Module;
	FString ModuleMaterialKey;
	FString ModuleColorKey;

	FLayerPatternGap Gap;
	FString GapMaterialKey;
	FString GapColorKey;

	FName ProfileKey;

	BIM::FBIMPropertySheet Properties;
	TMap<Modumate::BIM::FNameType, FModumateFormattedDimension> Dimensions;

	void SetValue(
		const BIM::FValueSpec &var,
		const FModumateCommandParameter &value)
	{
		auto convertDimension = [this](const FString &dimStr,const BIM::FNameType &property)
		{
			FModumateFormattedDimension dim = UModumateDimensionStatics::StringToFormattedDimension(dimStr);
			Dimensions.Add(property, dim);
			return FUnitValue::WorldCentimeters(dim.Centimeters);
		};

		Properties.SetValue(var.QN().ToString(), value);

		//TODO: deprecated DDL 1.0 .. DDL 2 tracks all layer values in the Module or Layer scope
		if (var.Scope == BIM::EScope::MaterialColor)
		{
			if (var.Name == BIM::Parameters::Color)
			{
				LayerColorKey = value;
			}
			else if (var.Name == BIM::Parameters::MaterialKey)
			{
				LayerMaterialKey = value;
			}
		}
		else if (var.Scope == BIM::EScope::Layer)
		{
			if (var.Name == BIM::Parameters::Color)
			{
				LayerColorKey = value;
			}
			else if (var.Name == BIM::Parameters::MaterialKey)
			{
				LayerMaterialKey = value;
			}
			else if (var.Name == BIM::Parameters::TrimProfile)
			{
				ProfileKey = value;
			}
			else if (var.Name == BIM::Parameters::Thickness)
			{
				Thickness = convertDimension(value,var.QN());
			}
			else if (var.Name == BIM::Parameters::Pattern)
			{
				Pattern.Key = *value.AsString();
			}
			else if (var.Name == BIM::Parameters::Function)
			{
				if (!ensureAlways(TryFindEnumValueByName<ELayerFunction>(TEXT("ELayerFunction"), value.AsName(), FunctionEnum)))
				{
					FunctionEnum = ELayerFunction::None;
				}
			}
		}
		else if (var.Scope == BIM::EScope::Module)
		{
			if (var.Name == BIM::Parameters::MaterialKey)
			{
				ModuleMaterialKey = value;
				LayerMaterialKey = ModuleMaterialKey;
			}
			else if (var.Name == BIM::Parameters::Color)
			{
				ModuleColorKey = value;
				LayerColorKey = ModuleColorKey;
			}
			else if (var.Name == BIM::Parameters::Height || var.Name == BIM::Parameters::ZExtents)
			{
				Module.ModuleExtents.Z = convertDimension(value, var.QN()).AsWorldCentimeters();
			}
			else if (var.Name == BIM::Parameters::Width || var.Name == BIM::Parameters::YExtents || var.Name == BIM::Parameters::Depth)
			{
				Module.ModuleExtents.Y = convertDimension(value, var.QN()).AsWorldCentimeters();
			}
			else if (var.Name == BIM::Parameters::Length || var.Name == BIM::Parameters::XExtents)
			{
				Module.ModuleExtents.X = convertDimension(value, var.QN()).AsWorldCentimeters();
			}
			else if (var.Name == BIM::Parameters::BevelWidth)
			{
				Module.BevelWidth = convertDimension(value, var.QN());
			}
		}
		else if (var.Scope == BIM::EScope::Gap)
		{
			if (var.Name == BIM::Parameters::Color)
			{
				GapColorKey = value;
			}
			else if (var.Name == BIM::Parameters::MaterialKey)
			{
				GapMaterialKey = value;
			}
			else if (var.Name == BIM::Parameters::Name)
			{
				Gap.DisplayName = FText::FromString(value);
			}
			else if (var.Name == BIM::Parameters::Width || var.Name == BIM::Parameters::XExtents)
			{
				Gap.GapExtents.X = convertDimension(value,var.QN()).AsWorldCentimeters();
			}
			else if (var.Name == BIM::Parameters::Depth || var.Name == BIM::Parameters::YExtents)
			{
				Gap.GapExtents.Y = convertDimension(value,var.QN()).AsWorldCentimeters();
			}
		}
		else if (var.Scope == BIM::EScope::Pattern)
		{
			if (var.Name == BIM::Parameters::Name)
			{
				Pattern.DisplayName = FText::FromString(value.AsString());
			}
			else if (var.Name == BIM::Parameters::ModuleCount)
			{
				Pattern.ModuleCount = value;
			}
			else if (var.Name == BIM::Parameters::Extents)
			{
				Pattern.ParameterizedExtents = value;
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

		ret.Modules.Add(Module);
		ret.Gap = Gap;
		ret.Pattern = Pattern;

		if (!ProfileKey.IsNone())
		{
			ret.Function = ELayerFunction::Finish;
			ret.Format = ELayerFormat::Board;

			//TODO: legacy trim were keyed based on their display name, so check against both name and key

			const FSimpleMeshRef *trimMesh = db.GetSimpleMeshByKey(ProfileKey);

			if (ensureAlways(trimMesh != nullptr))
			{
				ret.SimpleMeshes.Add(*trimMesh);
			}
		}

		if (!LayerMaterialKey.IsEmpty())
		{
			const FArchitecturalMaterial *mat = db.GetArchitecturalMaterialByKey(*LayerMaterialKey);

			//TODO: ensure removed for DDL 2.0 refactor, missing materials okay in interim
			//if (ensureAlways(mat != nullptr))
			if (mat != nullptr)
			{
				ret.Material = *mat;
				ensureAlways(ret.Material.EngineMaterial != nullptr);
			}
		}

		if (!ModuleMaterialKey.IsEmpty())
		{
			const FArchitecturalMaterial *modMat = db.GetArchitecturalMaterialByKey(*ModuleMaterialKey);
			ensureAlways(modMat != nullptr);
			ensureAlways(ret.Modules.Num() > 0);
			for (auto &m : ret.Modules)
			{
				m.Material = *modMat;
				ensureAlways(m.Material.EngineMaterial != nullptr);
			}
		}

		if (!GapMaterialKey.IsEmpty())
		{
			const FArchitecturalMaterial *gapMat = db.GetArchitecturalMaterialByKey(*GapMaterialKey);
			ensureAlways(gapMat != nullptr);
			if (gapMat != nullptr)
			{
				ret.Gap.Material = *gapMat;
				ensureAlways(ret.Gap.Material.EngineMaterial != nullptr);
			}
		}

		auto tryAssignColor = [&db](FCustomColor &target, const FString &key)
		{
			const FCustomColor *color = db.GetCustomColorByKey(*key);
			ensureAlways(color != nullptr);
			if (color != nullptr)
			{
				target = *color;
			}
		};

		if (!LayerColorKey.IsEmpty())
		{
			tryAssignColor(ret.BaseColor, LayerColorKey);
			tryAssignColor(ret.Material.DefaultBaseColor, LayerColorKey);
		}

		if (!ModuleColorKey.IsEmpty())
		{
			for (auto &m : ret.Modules)
			{
				tryAssignColor(m.Material.DefaultBaseColor, ModuleColorKey);
			}
		}

		if (!GapColorKey.IsEmpty())
		{
			tryAssignColor(ret.Gap.BaseColor, GapColorKey);
		}

		if (ret.Material.EngineMaterial == nullptr &&
			ret.Modules.Num() > 0 &&
			ret.Modules[0].Material.EngineMaterial != nullptr)
		{
			ret.Material = ret.Modules[0].Material;
		}

		if (ret.Pattern.Key.IsNone())
		{
			ret.Pattern.Key = ret.Modules.Num() == 0 ? TEXT("Continuous") : TEXT("PATKEY");
		}

		if (ret.Gap.Key.IsNone())
		{
			ret.Gap.Key = TEXT("GAPKEY");
		}

		for (auto &m : ret.Modules)
		{
			if (m.Key.IsNone())
			{
				m.Key = TEXT("MODKEY");
			}
		}
		if (ret.BaseColor.Key.IsNone())
		{
			ret.BaseColor.Key = TEXT("COLORKEY");
		}

		ret.Pattern.DefaultGap = ret.Gap;
		ret.Pattern.DefaultModules = ret.Modules;

		ensureAlways(ret.Pattern.ModuleCount == ret.Modules.Num());

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
	FName meshKey = InSpec.RootProperties.GetProperty(Modumate::BIM::EScope::Preset, Modumate::BIM::Parameters::Name);
	const FArchitecturalMesh* mesh = InDB.GetArchitecturalMeshByKey(meshKey);
	if (mesh == nullptr)
	{
		return ECraftingResult::Error;
	}

	FModumateObjectAssemblyLayer &layer = OutMOA.Layers.AddDefaulted_GetRef();
	layer.Mesh = *mesh;

	OutMOA.SetProperty(BIM::Parameters::Normal, FVector(0, 0, 1));
	OutMOA.SetProperty(BIM::Parameters::Tangent, FVector(0, 1, 0));

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
				layerMaker.SetValue(BIM::FValueSpec(*propName), val);
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
	layerMaker.CodeName = OutMOA.GetProperty(BIM::Parameters::Name);

	FString diameterString;
	FModumateFormattedDimension xDim, yDim;
	if (OutMOA.Properties.TryGetProperty(BIM::EScope::Assembly, BIM::Parameters::Diameter, diameterString))
	{
		xDim = UModumateDimensionStatics::StringToFormattedDimension(OutMOA.Properties.GetProperty(BIM::EScope::Assembly, BIM::Parameters::Diameter));
		yDim = xDim;
	}
	else
	{
		xDim = UModumateDimensionStatics::StringToFormattedDimension(OutMOA.Properties.GetProperty(BIM::EScope::Assembly, BIM::Parameters::XExtents));
		yDim = UModumateDimensionStatics::StringToFormattedDimension(OutMOA.Properties.GetProperty(BIM::EScope::Assembly, BIM::Parameters::YExtents));
	}

	if (ensureAlways(xDim.Format != EDimensionFormat::Error))
	{
		layerMaker.Dimensions.Add(BIM::Parameters::XExtents, xDim);
	}

	if (ensureAlways(yDim.Format != EDimensionFormat::Error))
	{
		layerMaker.Dimensions.Add(BIM::Parameters::YExtents, yDim);
	}

	layerMaker.FormatEnum = ELayerFormat::None;
	layerMaker.FunctionEnum = ELayerFunction::Structure;
	layerMaker.LayerMaterialKey = OutMOA.Properties.GetProperty(BIM::EScope::Assembly, BIM::Parameters::MaterialKey);
	layerMaker.ProfileKey = OutMOA.Properties.GetProperty(BIM::EScope::Assembly, BIM::Parameters::ProfileKey);

	FModumateObjectAssemblyLayer &layer = OutMOA.Layers.Add_GetRef(layerMaker.Make(InDB));

	FString colorHex = OutMOA.Properties.GetProperty(BIM::EScope::MaterialColor, BIM::Parameters::HexValue);
	layer.Material.DefaultBaseColor.Color = FColor::FromHex(colorHex);
	layer.Material.DefaultBaseColor.bValid = true;

	OutMOA.SetProperty(BIM::Parameters::Name, OutMOA.Properties.GetProperty(BIM::EScope::Preset, BIM::Parameters::Name));

	// TODO: re-orient column meshes so width is along X instead of depth
	FVector profileSize(xDim.Centimeters, yDim.Centimeters, 1);

	if (ensureAlways(layer.SimpleMeshes.Num() > 0 && layer.SimpleMeshes[0].Asset.Get()->Polygons.Num() > 0))
	{
		FSimplePolygon &polygon = layer.SimpleMeshes[0].Asset.Get()->Polygons[0];
		FVector polyExtents(polygon.Extents.Max.X - polygon.Extents.Min.X, polygon.Extents.Max.Y - polygon.Extents.Min.Y, 1);
		OutMOA.SetProperty(BIM::Parameters::Scale, profileSize / polyExtents);
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
		OutMOA.SetProperty(BIM::Parameters::Name, InSpec.RootProperties.GetProperty(BIM::EScope::Preset, BIM::Parameters::Name));
	}
	return result;
}





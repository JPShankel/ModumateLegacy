// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/BIMAssemblySpec.h"
#include "BIMKernel/BIMPresets.h"
#include "BIMKernel/BIMNodeEditor.h"
#include "Database/ModumateObjectDatabase.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "ModumateCore/ModumateConsoleCommand.h"
#include "ModumateCore/ModumateUnits.h"

ECraftingResult FBIMAssemblySpec::FromPreset(const FModumateDatabase& InDB, const FBIMPresetCollection& PresetCollection, const FName& PresetID)
{
	ECraftingResult ret = ECraftingResult::Success;
	RootPreset = PresetID;

	FBIMPropertySheet* currentSheet = &RootProperties;

	TArray<FName> presetStack;
	presetStack.Push(PresetID);

	TArray<EBIMValueScope> scopeStack;
	scopeStack.Push(EBIMValueScope::Assembly);

	while (presetStack.Num() > 0)
	{
		FName presetID = presetStack.Pop();
		EBIMValueScope pinScope = scopeStack.Pop();

		const FBIMPreset* preset = PresetCollection.Presets.Find(presetID);
		if (preset == nullptr)
		{
			ret = ECraftingResult::Error;
		}
		else
		{
			const FBIMPresetNodeType* nodeType = PresetCollection.NodeDescriptors.Find(preset->NodeType);
			if (ensureAlways(nodeType != nullptr))
			{
				if (nodeType->Scope == EBIMValueScope::Layer)
				{
					LayerProperties.AddDefaulted();
					currentSheet = &LayerProperties.Last();
				}
				if (nodeType->Scope != EBIMValueScope::None)
				{
					pinScope = nodeType->Scope;
				}
			}

			EObjectType objectType = PresetCollection.GetPresetObjectType(presetID);
			if (objectType != EObjectType::OTNone && ensureAlways(ObjectType == EObjectType::OTNone))
			{
				ObjectType = objectType;
			}

			preset->Properties.ForEachProperty([this, &currentSheet, &preset, pinScope, PresetID](const FString& Name, const Modumate::FModumateCommandParameter& MCP)
			{
				FBIMPropertyValue vs(*Name);

				// 'Node' scope values inherit their scope from their parents, specified on the pin
				EBIMValueScope nodeScope = vs.Scope == EBIMValueScope::Node ? pinScope : vs.Scope;

				// Preset properties only apply to the preset itself, not to its children
				if (nodeScope != EBIMValueScope::Preset || preset->PresetID == PresetID)
				{
					currentSheet->SetProperty(nodeScope, vs.Name, MCP);
				}
			});

			for (auto& childPreset : preset->ChildPresets)
			{
				presetStack.Push(childPreset.PresetID);
				scopeStack.Push(pinScope);
			}
		}
	}

	// All assembly specs must bind to an object type
	if (ObjectType == EObjectType::OTNone)
	{
		return ECraftingResult::Error;
	}

	return DoMakeAssembly(InDB, PresetCollection);
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

	Modumate::Units::FUnitValue Thickness;

	FString LayerMaterialKey;
	FString LayerColorKey;

	FName ProfileKey;

	FBIMPropertySheet Properties;
	TMap<FBIMNameType, FModumateFormattedDimension> Dimensions;

	void SetValue(
		const FBIMPropertyValue& var,
		const Modumate::FModumateCommandParameter& value)
	{
		auto convertDimension = [this](const FString& dimStr, const FBIMNameType& property)
		{
			FModumateFormattedDimension dim = UModumateDimensionStatics::StringToFormattedDimension(dimStr);
			Dimensions.Add(property, dim);
			return Modumate::Units::FUnitValue::WorldCentimeters(dim.Centimeters);
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

	FModumateObjectAssemblyLayer Make(const FModumateDatabase& db)
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

			const FSimpleMeshRef* trimMesh = db.GetSimpleMeshByKey(ProfileKey);

			if (ensureAlways(trimMesh != nullptr))
			{
				ret.SimpleMeshes.Add(*trimMesh);
			}
		}

		if (!LayerMaterialKey.IsEmpty())
		{
			const FArchitecturalMaterial* mat = db.GetArchitecturalMaterialByKey(*LayerMaterialKey);
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

ECraftingResult FBIMAssemblySpec::MakeStubbyAssembly(const FModumateDatabase& InDB)
{
	CachedAssembly = FModumateObjectAssembly();

	FName meshKey = RootProperties.GetProperty(EBIMValueScope::Mesh, BIMPropertyNames::AssetID);
	const FArchitecturalMesh* mesh = InDB.GetArchitecturalMeshByKey(meshKey);
	if (mesh == nullptr)
	{
		return ECraftingResult::Error;
	}

	FModumateObjectAssemblyLayer& layer = CachedAssembly.Layers.AddDefaulted_GetRef();
	layer.Mesh = *mesh;

	CachedAssembly.SetProperty(BIMPropertyNames::Normal, FVector(0, 0, 1));
	CachedAssembly.SetProperty(BIMPropertyNames::Tangent, FVector(0, 1, 0));

	CachedAssembly.ObjectType = ObjectType;
	return ECraftingResult::Success;
}

ECraftingResult FBIMAssemblySpec::MakeLayeredAssembly(const FModumateDatabase& InDB)
{
	CachedAssembly = FModumateObjectAssembly();

	CachedAssembly.Properties = RootProperties;

	for (auto& layerProperties : LayerProperties)
	{
		FLayerMaker layerMaker;

		layerProperties.ForEachProperty([&layerMaker](const FString& propName, const Modumate::FModumateCommandParameter& val)
		{
			if (!propName.IsEmpty())
			{
				layerMaker.SetValue(FBIMPropertyValue(*propName), val);
			}
		});

		FModumateObjectAssemblyLayer& layer = CachedAssembly.Layers.Add_GetRef(layerMaker.Make(InDB));
	}

	return ECraftingResult::Success;
}


ECraftingResult FBIMAssemblySpec::MakeStructureLineAssembly(const FModumateDatabase& InDB)
{
	CachedAssembly = FModumateObjectAssembly();
	CachedAssembly.Properties = RootProperties;

	FLayerMaker layerMaker;

	layerMaker.FormatEnum = ELayerFormat::None;
	layerMaker.FunctionEnum = ELayerFunction::Structure;

	layerMaker.CodeName = CachedAssembly.GetProperty(BIMPropertyNames::Name);
	CachedAssembly.SetProperty(BIMPropertyNames::Name, layerMaker.CodeName);

	FString diameterString;
	FModumateFormattedDimension xDim, yDim;
	if (CachedAssembly.Properties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Diameter, diameterString))
	{
		xDim = UModumateDimensionStatics::StringToFormattedDimension(CachedAssembly.Properties.GetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Diameter));
		yDim = xDim;
	}
	else
	{
		xDim = UModumateDimensionStatics::StringToFormattedDimension(CachedAssembly.Properties.GetProperty(EBIMValueScope::Dimension, BIMPropertyNames::XExtents));
		yDim = UModumateDimensionStatics::StringToFormattedDimension(CachedAssembly.Properties.GetProperty(EBIMValueScope::Dimension, BIMPropertyNames::YExtents));
	}

	if (ensureAlways(xDim.Format != EDimensionFormat::Error))
	{
		layerMaker.Dimensions.Add(BIMPropertyNames::XExtents, xDim);
	}

	if (ensureAlways(yDim.Format != EDimensionFormat::Error))
	{
		layerMaker.Dimensions.Add(BIMPropertyNames::YExtents, yDim);
	}

	layerMaker.LayerMaterialKey = CachedAssembly.Properties.GetProperty(EBIMValueScope::RawMaterial, BIMPropertyNames::AssetID);
	layerMaker.ProfileKey = CachedAssembly.Properties.GetProperty(EBIMValueScope::Profile, BIMPropertyNames::AssetID);

	FModumateObjectAssemblyLayer& layer = CachedAssembly.Layers.Add_GetRef(layerMaker.Make(InDB));

	// TODO: re-orient column meshes so width is along X instead of depth
	FVector profileSize(xDim.Centimeters, yDim.Centimeters, 1);

	if (ensureAlways(layer.SimpleMeshes.Num() > 0 && layer.SimpleMeshes[0].Asset.Get()->Polygons.Num() > 0))
	{
		FSimplePolygon& polygon = layer.SimpleMeshes[0].Asset.Get()->Polygons[0];
		FVector polyExtents(polygon.Extents.Max.X - polygon.Extents.Min.X, polygon.Extents.Max.Y - polygon.Extents.Min.Y, 1);
		CachedAssembly.SetProperty(BIMPropertyNames::Scale, profileSize / polyExtents);
	}
	return ECraftingResult::Success;
}

ECraftingResult FBIMAssemblySpec::DoMakeAssembly(const FModumateDatabase& InDB, const FBIMPresetCollection& PresetCollection)
{
	// TODO: move assembly synthesis to each tool mode or MOI implementation (TBD)
	ECraftingResult result = ECraftingResult::Error;
	switch (ObjectType)
	{
		case EObjectType::OTStructureLine:
			return MakeStructureLineAssembly(InDB);

		case EObjectType::OTFloorSegment:
		case EObjectType::OTWallSegment:
		case EObjectType::OTRoofFace:
		case EObjectType::OTFinish:
			return MakeLayeredAssembly(InDB);

		case EObjectType::OTCountertop:
		case EObjectType::OTCabinet:
		case EObjectType::OTFurniture:
		case EObjectType::OTDoor:
		case EObjectType::OTWindow:
			return MakeStubbyAssembly(InDB);

		default:
			ensureAlways(false);
	};

	return ECraftingResult::Error;
}
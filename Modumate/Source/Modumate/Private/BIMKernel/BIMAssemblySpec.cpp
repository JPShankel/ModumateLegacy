// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/BIMAssemblySpec.h"
#include "BIMKernel/BIMPresets.h"
#include "BIMKernel/BIMNodeEditor.h"
#include "Database/ModumateObjectDatabase.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "ModumateCore/ModumateConsoleCommand.h"
#include "ModumateCore/ModumateUnits.h"
#include "Algo/Reverse.h"
#include "Algo/Accumulate.h"

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
				// TODO: Part scope for FFE
				if (nodeType->Scope == EBIMValueScope::Profile)
				{
					Extrusions.AddDefaulted();
					currentSheet = &Extrusions.Last().Properties;
				}
				else if (nodeType->Scope == EBIMValueScope::Layer)
				{
					Layers.AddDefaulted();
					currentSheet = &Layers.Last().Properties;
				}
				
				// All nodes should have a scope
				if (ensureAlways(nodeType->Scope != EBIMValueScope::None))
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

bool FBIMAssemblySpec::HasProperty(const FBIMNameType& Name) const
{
	return RootProperties.HasProperty(EBIMValueScope::Assembly, Name);
}

Modumate::FModumateCommandParameter FBIMAssemblySpec::GetProperty(const FBIMNameType& Name) const
{
	return RootProperties.GetProperty(EBIMValueScope::Assembly, Name);
}

void FBIMAssemblySpec::SetProperty(const FBIMNameType& Name, const Modumate::FModumateCommandParameter& Value)
{
	RootProperties.SetProperty(EBIMValueScope::Assembly, Name, Value);
}

void FBIMAssemblySpec::ReverseLayers()
{
	Algo::Reverse(Layers);
}

ECraftingResult FBIMLayerSpec::BuildFromProperties(const FModumateDatabase& InDB)
{
	const FCustomColor* customColor = nullptr;
	Properties.ForEachProperty([this,&InDB,&customColor](const FString& VarQN, const Modumate::FModumateCommandParameter& Value)
	{
		FBIMPropertyValue Var(*VarQN);
		if (Var.Name == BIMPropertyNames::AssetID)
		{
			// TODO: to be replaced with modules & gaps with their own materials & colors
			if (Var.Scope == EBIMValueScope::RawMaterial || Var.Scope == EBIMValueScope::Material)
			{
				const FArchitecturalMaterial* mat = InDB.GetArchitecturalMaterialByKey(Value);
				if (ensureAlways(mat != nullptr))
				{
					Material = *mat;
					ensureAlways(Material.EngineMaterial != nullptr);
				}
			}

			// Color may get set before or after material, so cache it
			if (Var.Scope == EBIMValueScope::Color)
			{
				customColor = InDB.GetCustomColorByKey(Value);
			}
		}

		if (Var.Scope == EBIMValueScope::Dimension)
		{
			if (Var.Name == BIMPropertyNames::Thickness)
			{
				FModumateFormattedDimension dim = UModumateDimensionStatics::StringToFormattedDimension(Value);
				Thickness = Modumate::Units::FUnitValue::WorldCentimeters(dim.Centimeters);
			}
		}	
	});

	if (customColor != nullptr)
	{
		Material.DefaultBaseColor = *customColor;
	}

	return ECraftingResult::Success;
}

ECraftingResult FBIMExtrusionSpec::BuildFromProperties(const FModumateDatabase& InDB)
{
	FString diameterString;
	FModumateFormattedDimension xDim, yDim;
	if (Properties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Diameter, diameterString))
	{
		xDim = UModumateDimensionStatics::StringToFormattedDimension(Properties.GetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Diameter));
		yDim = xDim;
	}
	else
	{
		xDim = UModumateDimensionStatics::StringToFormattedDimension(Properties.GetProperty(EBIMValueScope::Dimension, BIMPropertyNames::XExtents));
		yDim = UModumateDimensionStatics::StringToFormattedDimension(Properties.GetProperty(EBIMValueScope::Dimension, BIMPropertyNames::YExtents));
	}

	FName layerMaterialKey = Properties.GetProperty(EBIMValueScope::RawMaterial, BIMPropertyNames::AssetID);
	FName profileKey = Properties.GetProperty(EBIMValueScope::Profile, BIMPropertyNames::AssetID);

	if (!profileKey.IsNone())
	{
		const FSimpleMeshRef* trimMesh = InDB.GetSimpleMeshByKey(profileKey);

		if (ensureAlways(trimMesh != nullptr))
		{
			SimpleMeshes.Add(*trimMesh);
		}
	}

	if (!layerMaterialKey.IsNone())
	{
		const FArchitecturalMaterial* mat = InDB.GetArchitecturalMaterialByKey(layerMaterialKey);
		if (ensureAlways(mat != nullptr))
		{
			Material = *mat;
			ensureAlways(Material.EngineMaterial != nullptr);
		}
	}

	// TODO: re-orient column meshes so width is along X instead of depth
	FVector profileSize(xDim.Centimeters, yDim.Centimeters, 1);

	if (ensureAlways(SimpleMeshes.Num() > 0 && SimpleMeshes[0].Asset.Get()->Polygons.Num() > 0))
	{
		FSimplePolygon& polygon = SimpleMeshes[0].Asset.Get()->Polygons[0];
		FVector polyExtents(polygon.Extents.Max.X - polygon.Extents.Min.X, polygon.Extents.Max.Y - polygon.Extents.Min.Y, 1);
		Properties.SetProperty(EBIMValueScope::Assembly,BIMPropertyNames::Scale, profileSize / polyExtents);
	}	
	return ECraftingResult::Success;
}

ECraftingResult FBIMPartSlotSpec::BuildFromProperties(const FModumateDatabase& InDB)
{
	// TODO: not yet implemented
	ensureAlways(false);
	return ECraftingResult::Error;
}

ECraftingResult FBIMAssemblySpec::MakeStubbyAssembly(const FModumateDatabase& InDB)
{
	FName meshKey = RootProperties.GetProperty(EBIMValueScope::Mesh, BIMPropertyNames::AssetID);
	const FArchitecturalMesh* mesh = InDB.GetArchitecturalMeshByKey(meshKey);
	if (mesh == nullptr)
	{
		return ECraftingResult::Error;
	}

	FBIMPartSlotSpec& partSlot = Parts.AddDefaulted_GetRef();
	partSlot.Mesh = *mesh;

	SetProperty(BIMPropertyNames::Normal, FVector(0, 0, 1));
	SetProperty(BIMPropertyNames::Tangent, FVector(0, 1, 0));

	return ECraftingResult::Success;
}

ECraftingResult FBIMAssemblySpec::MakeLayeredAssembly(const FModumateDatabase& InDB)
{
	for (auto& layer : Layers)
	{
		ECraftingResult res = layer.BuildFromProperties(InDB);
		if (!ensureAlways(res == ECraftingResult::Success))
		{
			return res;
		}
	}

	return ECraftingResult::Success;
}

ECraftingResult FBIMAssemblySpec::MakeStructureLineAssembly(const FModumateDatabase& InDB)
{
	for (auto& extrusion : Extrusions)
	{
		ECraftingResult res = extrusion.BuildFromProperties(InDB);
		if (!ensureAlways(res == ECraftingResult::Success))
		{
			return res;
		}

		// TODO: currently only one extrusion per assembly so only one scale
		FVector scale = extrusion.Properties.GetProperty(EBIMValueScope::Assembly, BIMPropertyNames::Scale);
		SetProperty(BIMPropertyNames::Scale, scale);
	}
	return ECraftingResult::Success;
}

Modumate::Units::FUnitValue FBIMAssemblySpec::CalculateThickness() const
{
	return Modumate::Units::FUnitValue::WorldCentimeters(Algo::TransformAccumulate(
		Layers,
		[](const FBIMLayerSpec& l)
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
		case EObjectType::OTCeiling:
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
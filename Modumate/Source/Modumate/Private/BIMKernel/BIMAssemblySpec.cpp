// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/BIMAssemblySpec.h"
#include "BIMKernel/BIMPresets.h"
#include "BIMKernel/BIMNodeEditor.h"
#include "Database/ModumateObjectDatabase.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "ModumateCore/ModumateConsoleCommand.h"
#include "ModumateCore/ModumateUnits.h"
#include "ModumateCore/ExpressionEvaluator.h"
#include "Algo/Reverse.h"
#include "Algo/Accumulate.h"

ECraftingResult FBIMAssemblySpec::FromPreset(const FModumateDatabase& InDB, const FBIMPresetCollection& PresetCollection, const FBIMKey& PresetID)
{
	Reset();
	ECraftingResult ret = ECraftingResult::Success;
	RootPreset = PresetID;

	FBIMPropertySheet* currentSheet = &RootProperties;

	TArray<FBIMKey> presetStack;
	presetStack.Push(PresetID);

	// Depth first walk through the preset and its descendents
	while (presetStack.Num() > 0)
	{
		FBIMKey presetID = presetStack.Pop();

		const FBIMPreset* preset = PresetCollection.Presets.Find(presetID);
		if (preset == nullptr)
		{
			ret = ECraftingResult::Error;
			continue;
		}

		// There are 3 sections to an assembly spec: extrusions, layers and parts
		// Extrusions and layers are added when we detect a node that defines them
		// Parts are added below if we have any filled in part slots 
		
		// TODO: ignore gaps until we can reconcile gap vs module dimensions
		if (preset->NodeScope == EBIMValueScope::Gap)
		{
			continue;
		}
		else if (preset->NodeScope == EBIMValueScope::Profile)
		{
			// TODO: until we can combine extrusions and layers, ignore extrusions on layered assemblies
			if (Layers.Num() > 0)
			{
				continue;
			}
			Extrusions.AddDefaulted();
			currentSheet = &Extrusions.Last().Properties;
		}
		else if (preset->NodeScope == EBIMValueScope::Layer)
		{
			Layers.AddDefaulted();
			currentSheet = &Layers.Last().Properties;
		}

		// All nodes should have a scope
		if (!ensureAlways(preset->NodeScope != EBIMValueScope::None))
		{
			ret = ECraftingResult::Error;
			continue;
		}

		// Set the object type for this assembly, should only happen once 
		if (preset->ObjectType != EObjectType::OTNone && ensureAlways(ObjectType == EObjectType::OTNone))
		{
			ObjectType = preset->ObjectType;
		}

		// TODO: refactor for type-safe properties, 
		preset->Properties.ForEachProperty([this, &currentSheet, &preset, PresetID](const FString& Name, const Modumate::FModumateCommandParameter& MCP)
		{
			FBIMPropertyValue vs(*Name);
			currentSheet->SetProperty(vs.Scope, vs.Name, MCP);
		});

		// Add our own children to DFS stack
		for (auto& childPreset : preset->ChildPresets)
		{
			presetStack.Push(childPreset.PresetID);
		}

		// A preset with PartSlots represents a compound object like a door, window or FFE ensemble
		if (preset->PartSlots.Num() > 0)
		{
			const FBIMPreset* slotConfigPreset = PresetCollection.Presets.Find(preset->SlotConfigPresetID);
			if (!ensureAlways(slotConfigPreset != nullptr))
			{
				ret = ECraftingResult::Error;
				continue;
			}

			// Create a part entry in the assembly spec for each part in the preset
			for (auto& partSlot : preset->PartSlots)
			{
				FBIMPartSlotSpec &partSpec = Parts.AddDefaulted_GetRef();
				partSpec.Properties = preset->Properties;

				// Find the preset for the part in the slot...this will contain the mesh, material and color information
				const FBIMPreset* partPreset = PresetCollection.Presets.Find(partSlot.PartPreset);
					
				// Each child of a part represents one component (mesh, material or color)
				// TODO: for now, ignoring material and color
				for (auto& cp : partPreset->ChildPresets)
				{
					if (cp.PresetID.IsNone())
					{
						ret = ECraftingResult::Error;
						continue;
					}

					const FBIMPreset* childPreset = PresetCollection.Presets.Find(cp.PresetID);

					if (!ensureAlways(childPreset != nullptr))
					{
						ret = ECraftingResult::Error;
						continue;
					}

					// If this child has a mesh asset ID, this fetch the mesh and use it 
					FName meshAsset;
					if (childPreset->Properties.TryGetProperty(EBIMValueScope::Mesh, BIMPropertyNames::AssetID, meshAsset))
					{
						const FArchitecturalMesh *mesh = InDB.GetArchitecturalMeshByKey(childPreset->PresetID);
						if (!ensureAlways(mesh != nullptr))
						{
							ret = ECraftingResult::Error;
							break;
						}

						partSpec.Mesh = *mesh;

						// Now find the slot in the config that corresponds to the slot in the preset and fetch its parameterized transform
						for (auto& childSlot : slotConfigPreset->ChildPresets)
						{
							if (childSlot.PresetID == partSlot.SlotName)
							{
								const FBIMPreset* childSlotPreset = PresetCollection.Presets.Find(childSlot.PresetID);
								if (!ensureAlways(childSlotPreset != nullptr))
								{
									break;
								}

								// FVectorExpression holds 3 values as formula strings (ie "Parent.NativeSizeX * 0.5) that are evaluated in CompoundMeshActor

								partSpec.Translation = Modumate::Expression::FVectorExpression(
									childSlotPreset->GetProperty(TEXT("LocationX")),
									childSlotPreset->GetProperty(TEXT("LocationY")),
									childSlotPreset->GetProperty(TEXT("LocationZ")));

								partSpec.Orientation = Modumate::Expression::FVectorExpression(
									childSlotPreset->GetProperty(TEXT("RotationX")),
									childSlotPreset->GetProperty(TEXT("RotationY")),
									childSlotPreset->GetProperty(TEXT("RotationZ")));

								partSpec.Size = Modumate::Expression::FVectorExpression(
									childSlotPreset->GetProperty(TEXT("SizeX")),
									childSlotPreset->GetProperty(TEXT("SizeY")),
									childSlotPreset->GetProperty(TEXT("SizeZ")));

								partSpec.Flip[0] = !childSlotPreset->GetProperty(TEXT("FlipX")).AsString().IsEmpty();
								partSpec.Flip[1] = !childSlotPreset->GetProperty(TEXT("FlipY")).AsString().IsEmpty();
								partSpec.Flip[2] = !childSlotPreset->GetProperty(TEXT("FlipZ")).AsString().IsEmpty();

								break;
							}
						}
					}
				}
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

void FBIMAssemblySpec::Reset()
{
	ObjectType = EObjectType::OTNone;

	RootPreset = FBIMKey();
	RootProperties = FBIMPropertySheet();

	Layers.Empty();
	Parts.Empty();
	Extrusions.Empty();
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
				const FArchitecturalMaterial* mat = InDB.GetArchitecturalMaterialByKey(FBIMKey(Value.AsString()));
				if (ensureAlways(mat != nullptr))
				{
					Material = *mat;
					ensureAlways(Material.EngineMaterial != nullptr);
				}
			}

			// Color may get set before or after material, so cache it
			if (Var.Scope == EBIMValueScope::Color)
			{
				customColor = InDB.GetCustomColorByKey(FBIMKey(Value.AsString()));
			}
		}
	});

	FString thickness;
	if (!Properties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Thickness, thickness))
	{
		if (!Properties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Depth, thickness))
		{
			Properties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Width, thickness);
		}
	}

	if (ensureAlways(!thickness.IsEmpty()))
	{
		FModumateFormattedDimension dim = UModumateDimensionStatics::StringToFormattedDimension(thickness);
		Thickness = Modumate::Units::FUnitValue::WorldCentimeters(dim.Centimeters);
	}
	else
	{
		Thickness = Modumate::Units::FUnitValue::WorldCentimeters(1.0f);
	}

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
		xDim = UModumateDimensionStatics::StringToFormattedDimension(Properties.GetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Width));
		yDim = UModumateDimensionStatics::StringToFormattedDimension(Properties.GetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Depth));
	}

	FBIMKey layerMaterialKey = FBIMKey(Properties.GetProperty(EBIMValueScope::RawMaterial, BIMPropertyNames::AssetID).AsString());
	FBIMKey profileKey = FBIMKey(Properties.GetProperty(EBIMValueScope::Profile, BIMPropertyNames::AssetID).AsString());

	if (ensureAlways(!profileKey.IsNone()))
	{
		const FSimpleMeshRef* trimMesh = InDB.GetSimpleMeshByKey(profileKey);

		if (ensureAlways(trimMesh != nullptr))
		{
			SimpleMeshes.Add(*trimMesh);
		}
	}

	if (ensureAlways(!layerMaterialKey.IsNone()))
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

ECraftingResult FBIMAssemblySpec::MakeRiggedAssembly(const FModumateDatabase& InDB)
{
	// TODO: "Stubby" temporary FFE don't have parts, just one mesh on their root
	if (Parts.Num() == 0)
	{
		FBIMKey meshKey = FBIMKey(RootProperties.GetProperty(EBIMValueScope::Mesh, BIMPropertyNames::AssetID).AsString());
		const FArchitecturalMesh* mesh = InDB.GetArchitecturalMeshByKey(meshKey);
		if (mesh == nullptr)
		{
			return ECraftingResult::Error;
		}

		FBIMPartSlotSpec& partSlot = Parts.AddDefaulted_GetRef();
		partSlot.Mesh = *mesh;

		return ECraftingResult::Success;
	}
	else
	{
		// TODO: slot transformations
	}

	// TODO: get orientation information from BIM system, express as properties
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

ECraftingResult FBIMAssemblySpec::MakeExtrudedAssembly(const FModumateDatabase& InDB)
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
	case EObjectType::OTTrim:
	case EObjectType::OTStructureLine:
		return MakeExtrudedAssembly(InDB);

	case EObjectType::OTFloorSegment:
	case EObjectType::OTWallSegment:
	case EObjectType::OTRoofFace:
	case EObjectType::OTFinish:
	case EObjectType::OTCeiling:
	case EObjectType::OTRailSegment:
	case EObjectType::OTCountertop:
		return MakeLayeredAssembly(InDB);

	case EObjectType::OTCabinet:
	case EObjectType::OTDoor:
	case EObjectType::OTWindow:
	case EObjectType::OTFurniture:
		return MakeRiggedAssembly(InDB);

	default:
		ensureAlways(false);
	};

	return ECraftingResult::Error;
}
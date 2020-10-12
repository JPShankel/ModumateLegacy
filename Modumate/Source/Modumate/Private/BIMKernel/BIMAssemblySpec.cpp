// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/BIMAssemblySpec.h"
#include "BIMKernel/BIMPresets.h"
#include "BIMKernel/BIMLayerSpec.h"
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

	/*
	We build an assembly spec by iterating through the tree of presets and assigning BIM values to specific targets like structural layers, risers, treads, etc		
	Layers for stair tread and risers can be in embedded layered assemblies...when we get to those layers we need to know where they land in the top level assembly
	*/
	enum ELayerTarget
	{
		None = 0,
		Assembly,
		TreadLayer,
		RiserLayer
	};

	// Structure used to walk the tree of presets
	struct FPresetIterator
	{
		FBIMKey PresetID;
		const FBIMPreset* Preset = nullptr;

		// As we iterate, we keep track of which layer and which property sheet to set modifier values on
		ELayerTarget Target = ELayerTarget::Assembly;
		FBIMLayerSpec* TargetLayer = nullptr;
		FBIMPropertySheet* TargetProperties = nullptr;
	};

	// We use a stack of iterators to perform a depth-first visitation of child presets
	TArray<FPresetIterator> iteratorStack;
	FPresetIterator rootIterator;
	rootIterator.PresetID = PresetID;
	rootIterator.TargetProperties = &RootProperties;
	iteratorStack.Push(rootIterator);

	while (iteratorStack.Num() > 0)
	{
		// Make a copy of the top iterator
		//It will get modified below to set the context for subsequent children
		FPresetIterator presetIterator = iteratorStack.Pop();

		presetIterator.Preset = PresetCollection.Presets.Find(presetIterator.PresetID);
		if (!ensureAlways(presetIterator.Preset != nullptr))
		{
			ret = ECraftingResult::Error;
			continue;
		}

		// All nodes should have a scope
		if (!ensureAlways(presetIterator.Preset->NodeScope != EBIMValueScope::None))
		{
			ret = ECraftingResult::Error;
			continue;
		}

		// Set the object type for this assembly, should only happen once 
		if (presetIterator.Preset->ObjectType != EObjectType::OTNone && ensureAlways(ObjectType == EObjectType::OTNone))
		{
			ObjectType = presetIterator.Preset->ObjectType;
		}

		/*
		Every preset has a scope (defined in the CSV sheets)
		Based on the scope of the current preset, determine how to modify the iterator and set properties		
		*/

		// When we encounter a Layer node, figure out if it's the main assembly layer or a tread or riser and track the target layer
		if (presetIterator.Preset->NodeScope == EBIMValueScope::Layer)
		{
			switch (presetIterator.Target)
			{
			case ELayerTarget::Assembly:
				Layers.AddDefaulted_GetRef();
				presetIterator.TargetLayer = &Layers.Last();
				presetIterator.TargetProperties = &Layers.Last().LayerProperties;
				break;
			case ELayerTarget::TreadLayer:
				TreadLayers.AddDefaulted_GetRef();
				presetIterator.TargetLayer = &TreadLayers.Last();
				presetIterator.TargetProperties = &TreadLayers.Last().LayerProperties;
				break;
			case ELayerTarget::RiserLayer:
				RiserLayers.AddDefaulted_GetRef();
				presetIterator.TargetLayer = &RiserLayers.Last();
				presetIterator.TargetProperties = &RiserLayers.Last().LayerProperties;
				break;
			default:
				ensureAlways(false);
				break;
			};
		}
		// Patterns only apply to layers, so we can go ahead and look it up and assign it to the target
		// TODO: if patterns gain a wider scope, we may just need to track it as a property
		else if (presetIterator.Preset->NodeScope == EBIMValueScope::Pattern)
		{
			const FLayerPattern* pattern = InDB.GetLayerByKey(presetIterator.PresetID);

			if (ensureAlways(pattern != nullptr && presetIterator.TargetLayer != nullptr))
			{
				presetIterator.TargetLayer->Pattern = *pattern;
			}
		}
		// TODO: until we can combine extrusions and layers, ignore extrusions on layered assemblies
		// This limits extrusions to Trim, Beams, Columns and Mullions for the time being
		// For extrusions not hosted on a metabeam, we will need assembly-level geometric data
		else if (presetIterator.Preset->NodeScope == EBIMValueScope::Profile)
		{
			if (Layers.Num() == 0 && TreadLayers.Num() == 0 && RiserLayers.Num() == 0)
			{
				presetIterator.TargetProperties = &Extrusions.AddDefaulted_GetRef().Properties;
			}
		}
		// Color can be applied to a number of targets, so just set the asset to the current property sheet and DoMakeAssembly will sort it
		else if (presetIterator.Preset->NodeScope == EBIMValueScope::Color)
		{
			if (ensureAlways(presetIterator.TargetProperties != nullptr))
			{
				presetIterator.TargetProperties->SetProperty(EBIMValueScope::Color, BIMPropertyNames::AssetID, presetIterator.Preset->PresetID.StringValue);
			}
		}
		// When we encounter a module, we'll expect children to apply color, material and dimension data
		else if (presetIterator.Preset->NodeScope == EBIMValueScope::Module)
		{
			if (ensureAlways(presetIterator.TargetLayer != nullptr))
			{
				presetIterator.TargetProperties = &presetIterator.TargetLayer->ModuleProperties.AddDefaulted_GetRef();
			}
		}
		// Gaps have the same rules as modules
		else if (presetIterator.Preset->NodeScope == EBIMValueScope::Gap)
		{
			if (ensureAlways(presetIterator.TargetLayer != nullptr))
			{
				presetIterator.TargetProperties = &presetIterator.TargetLayer->GapProperties;
			}
		}

		// Having analyzed the preset and retargeted (if necessary) the properties, apply all of the preset's properties to the target
		presetIterator.Preset->Properties.ForEachProperty([this, presetIterator](const FString& Name, const Modumate::FModumateCommandParameter& MCP)
		{
			FBIMPropertyValue vs(*Name);
			presetIterator.TargetProperties->SetProperty(vs.Scope, vs.Name, MCP);
		});

		// Add our own children to DFS stack
		for (auto childPreset : presetIterator.Preset->ChildPresets)
		{
			// Each child inherits the targeting information from its parent (presetIterator)
			FPresetIterator childIterator = presetIterator;
			childIterator.PresetID = childPreset.PresetID;
			childIterator.Preset = nullptr;

			/*
			If the preset pin target suggests a change in layer target, take it
			This is the only current use case of pin targets, so we may look for ways to use one of the other mechanisms for this choice
			*/

			switch (childPreset.Target)
			{
			case EBIMPinTarget::Tread:
				childIterator.Target = ELayerTarget::TreadLayer;
				break;
			case EBIMPinTarget::Riser:
				childIterator.Target = ELayerTarget::RiserLayer;
				break;
			default:
				break;
			}
			iteratorStack.Push(childIterator);
		}

		// A preset with PartSlots represents a compound object like a door, window or FFE ensemble
		// TBD: It is unclear whether part slots should be stored as pins. They're treated as pins by the Node Editor
		if (presetIterator.Preset->PartSlots.Num() > 0)
		{
			const FBIMPreset* slotConfigPreset = PresetCollection.Presets.Find(presetIterator.Preset->SlotConfigPresetID);
			if (!ensureAlways(slotConfigPreset != nullptr))
			{
				ret = ECraftingResult::Error;
				continue;
			}

			// Some assemblies have parts that are themselves assemblies
			// Cache these sub-assemblies and extract their parts below
			TArray<FBIMAssemblySpec> subSpecs;

			// Create a part entry in the assembly spec for each part in the preset
			for (auto& partSlot : presetIterator.Preset->PartSlots)
			{
				FBIMPartSlotSpec &partSpec = Parts.AddDefaulted_GetRef();
				partSpec.Properties = presetIterator.Preset->Properties;

				// Find the preset for the part in the slot...this will contain the mesh, material and color information
				const FBIMPreset* partPreset = PresetCollection.Presets.Find(partSlot.PartPreset);

				if (!ensureAlways(partPreset != nullptr))
				{
					continue;
				}

				// If we encounter a sub-assembly (ie a complex panel on a door), recurse and stash
				if (partPreset->NodeScope == EBIMValueScope::Assembly)
				{
					FBIMAssemblySpec& spec = subSpecs.AddDefaulted_GetRef();
					ensureAlways(spec.FromPreset(InDB, PresetCollection, partPreset->PresetID) == ECraftingResult::Success);
					continue;
				}
					
				// Each child of a part represents one component (mesh, material or color)
				// TODO: for now, ignoring material and color
				for (auto& cp : partPreset->ChildPresets)
				{
					if (!ensureAlways(!cp.PresetID.IsNone()))
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

					// If this preset has a material asset ID, then its ID is an architectural material
					FString materialAsset;
					if (childPreset->Properties.TryGetProperty(EBIMValueScope::Material, BIMPropertyNames::AssetID, materialAsset))
					{
						// Pin channels are defined in the DDL spreadsheet
						// Each row of a preset can define a separate "channel" to which its pin assignments apply
						// The only use case for this right now is binding material assignments in parts, 
						// so all parts must define pin channels.
						if (ensureAlways(!cp.PinChannel.IsNone()))
						{
							// Pin channel names are set in the spreadsheet/database
							// They do NOT conform to channel names in the mesh itself
							// the Material property binds a channel by name to a material
							// the Mesh property binds a channel by name to a material index in the engine mesh
							const FArchitecturalMaterial* material = InDB.GetArchitecturalMaterialByKey(childPreset->PresetID);
							if (ensureAlways(material != nullptr))
							{
								partSpec.ChannelMaterials.Add(cp.PinChannel, *material);
							}
						}
					}

					// If this child has a mesh asset ID, this fetch the mesh and use it 
					FString meshAsset;
					if (childPreset->Properties.TryGetProperty(EBIMValueScope::Mesh, BIMPropertyNames::AssetID, meshAsset))
					{
						const FArchitecturalMesh* mesh = InDB.GetArchitecturalMeshByKey(childPreset->PresetID);
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

			// TODO: for now, sub-assemblies only care about parts
			// Some complex types (ie stairs) may need similar treatment for extrusions and layers
			// To be determined: continue with flat layout or build hierarchy of specs?
			for (auto& subSpec : subSpecs)
			{
				for (auto& part : subSpec.Parts)
				{
					part.ParentSlotIndex += Parts.Num();
				}
				Parts.Append(subSpec.Parts);
			}
		}
	}

	// All assembly specs must bind to an object type
	if (ObjectType != EObjectType::OTNone)
	{
		return DoMakeAssembly(InDB, PresetCollection);
	}

	return ECraftingResult::Success;
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

void FBIMAssemblySpec::ReverseLayers()
{
	Algo::Reverse(Layers);
}

FVector FBIMAssemblySpec::GetRiggedAssemblyNativeSize() const
{
	if (ensure(Parts.Num() > 0))
	{
		return Parts[0].Mesh.NativeSize * Modumate::InchesToCentimeters;
	}
	else
	{
		return FVector::ZeroVector;
	}
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

	return ECraftingResult::Success;
}

ECraftingResult FBIMAssemblySpec::MakeLayeredAssembly(const FModumateDatabase& InDB)
{
	auto buildLayers = [InDB](TArray<FBIMLayerSpec>& Layers)
	{
		for (auto& layer : Layers)
		{
			ECraftingResult res = layer.BuildFromProperties(InDB);
			if (res != ECraftingResult::Success)
			{
				return res;
			}
		}
		return ECraftingResult::Success;
	};

	ECraftingResult res = buildLayers(Layers);
	
	if (res == ECraftingResult::Success)
	{
		res = buildLayers(TreadLayers);
	}

	if (res == ECraftingResult::Success)
	{
		res = buildLayers(RiserLayers);
	}

	return res;
}

ECraftingResult FBIMAssemblySpec::MakeExtrudedAssembly(const FModumateDatabase& InDB)
{
	for (auto& extrusion : Extrusions)
	{
		FString materialAsset;
		if (RootProperties.TryGetProperty(EBIMValueScope::Material, BIMPropertyNames::AssetID, materialAsset))
		{
			const FArchitecturalMaterial* mat = InDB.GetArchitecturalMaterialByKey(materialAsset);
			if (ensureAlways(mat != nullptr))
			{
				extrusion.Material = *mat;
				if (RootProperties.TryGetProperty(EBIMValueScope::Color, BIMPropertyNames::AssetID, materialAsset))
				{
					const FCustomColor* color = InDB.GetCustomColorByKey(materialAsset);
					if (ensureAlways(color != nullptr))
					{
						extrusion.Material.DefaultBaseColor = *color;
					}
				}
			}
		}
		ECraftingResult res = extrusion.BuildFromProperties(InDB);
		if (!ensureAlways(res == ECraftingResult::Success))
		{
			return res;
		}
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
	DisplayName = RootProperties.GetProperty(EBIMValueScope::Assembly,BIMPropertyNames::Name);
	Comments = RootProperties.GetProperty(EBIMValueScope::Assembly, BIMPropertyNames::Comments);
	CodeName = RootProperties.GetProperty(EBIMValueScope::Assembly, BIMPropertyNames::Code);

	FString depth, height;
	if (RootProperties.TryGetProperty(EBIMValueScope::ToeKick, BIMPropertyNames::Depth, depth)
		&& RootProperties.TryGetProperty(EBIMValueScope::ToeKick, BIMPropertyNames::Height, height))
	{
		ToeKickDepth = Modumate::Units::FUnitValue::WorldCentimeters(UModumateDimensionStatics::StringToFormattedDimension(depth).Centimeters);
		ToeKickHeight = Modumate::Units::FUnitValue::WorldCentimeters(UModumateDimensionStatics::StringToFormattedDimension(height).Centimeters);
	}

	if (RootProperties.TryGetProperty(EBIMValueScope::Assembly, TEXT("IdealTreadDepth"), depth))
	{
		TreadDepth = Modumate::Units::FUnitValue::WorldCentimeters(UModumateDimensionStatics::StringToFormattedDimension(depth).Centimeters);
	}

	// TODO: move assembly synthesis to each tool mode or MOI implementation (TBD)
	ECraftingResult result = ECraftingResult::Error;
	switch (ObjectType)
	{
	case EObjectType::OTTrim:
	case EObjectType::OTStructureLine:
	case EObjectType::OTMullion:
		return MakeExtrudedAssembly(InDB);

	case EObjectType::OTFloorSegment:
	case EObjectType::OTWallSegment:
	case EObjectType::OTRoofFace:
	case EObjectType::OTFinish:
	case EObjectType::OTCeiling:
	case EObjectType::OTRailSegment:
	case EObjectType::OTCountertop:
	case EObjectType::OTSystemPanel:
	case EObjectType::OTStaircase:
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
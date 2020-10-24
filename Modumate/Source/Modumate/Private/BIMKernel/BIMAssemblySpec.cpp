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
#include "Containers/Queue.h"

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

	// When we encounter a part hierarchy, we want to iterate breadth-first, so use a queue of part iterators
	struct FPartIterator
	{
		FBIMPreset::FPartSlot Slot;
		FBIMKey SlotConfigPreset;
		int32 ParentSlotIndex = 0;
	};
	TQueue<FPartIterator> partIteratorQueue;

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
		for (const auto& childPreset : presetIterator.Preset->ChildPresets)
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

		// If a preset has parts, add them to the part queue for processing below
		for (const auto& part : presetIterator.Preset->PartSlots)
		{
			FPartIterator partIterator;
			partIterator.SlotConfigPreset = presetIterator.Preset->SlotConfigPresetID;
			partIterator.Slot = part;
			partIteratorQueue.Enqueue(partIterator);
		}
	}

	// If we encountered any parts, add a root part to the top of the list to be the parent of the others
	if (!partIteratorQueue.IsEmpty())
	{
		const FBIMPreset* assemblyPreset = PresetCollection.Presets.Find(PresetID);

		FBIMPartSlotSpec& partSpec = Parts.AddDefaulted_GetRef();
		partSpec.ParentSlotIndex = INDEX_NONE;
		partSpec.NodeCategoryPath = assemblyPreset->MyTagPath;
		partSpec.Translation = Modumate::Expression::FVectorExpression(TEXT("0"), TEXT("0"), TEXT("0"));
		partSpec.Size = Modumate::Expression::FVectorExpression(TEXT("Self.ScaledSizeX"), TEXT("Self.ScaledSizeY"), TEXT("Self.ScaledSizeZ"));
#if WITH_EDITOR //for debugging
		//TODO: add _DEBUG tag
		partSpec.NodeScope = EBIMValueScope::Assembly;
		partSpec.PresetID = PresetID;
#endif
	}

	// For each part in the iterator queue, add a part to the assembly list, set its parents and add children to back of queue
	// This ensures that parents will always appear ahead of children in the array so we can process it from front to back
	while (!partIteratorQueue.IsEmpty())
	{
		FPartIterator partIterator;
		partIteratorQueue.Dequeue(partIterator);
		const FBIMPreset* partPreset = PresetCollection.Presets.Find(partIterator.Slot.PartPreset);
		const FBIMPreset* slotConfigPreset = PresetCollection.Presets.Find(partIterator.SlotConfigPreset);

		if (ensureAlways(partPreset != nullptr) && ensureAlways(slotConfigPreset != nullptr))
		{
			// Each child of a part represents one component (mesh, material or color)
			FBIMPartSlotSpec& partSpec = Parts.AddDefaulted_GetRef();
			partSpec.ParentSlotIndex = partIterator.ParentSlotIndex;
			partSpec.NodeCategoryPath = partPreset->MyTagPath;

#if WITH_EDITOR //for debugging
			//TODO: add _DEBUG tag

			partSpec.NodeScope = partPreset->NodeScope;
			partSpec.PresetID = partPreset->PresetID;
			partSpec.SlotName = partIterator.Slot.SlotName;
#endif
			// Look for asset child presets (mesh and material) and cache assets in the part
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
				}
			}

			// Find which slot this child belongs to and fetch transform data
			for (auto& childSlot : slotConfigPreset->ChildPresets)
			{
				if (childSlot.PresetID == partIterator.Slot.SlotName)
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

			// If this part is an assembly, then establish this part as the parent of its children, otherwise use the current parent
			int32 parentSlotIndex = partPreset->NodeScope == EBIMValueScope::Assembly ? Parts.Num() - 1 : partIterator.ParentSlotIndex;
			for (const auto& part : partPreset->PartSlots)
			{
				FPartIterator nextPartIterator;
				nextPartIterator.ParentSlotIndex = parentSlotIndex;
				nextPartIterator.Slot = part;
				nextPartIterator.SlotConfigPreset = partPreset->SlotConfigPresetID;
				partIteratorQueue.Enqueue(nextPartIterator);
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
	//The first part with a mesh defines the assembly native size
	//Parts without meshes represent parents of sets of mesh parts, so we skip those
	//TODO: read an assembly's native size from data
	for (auto& part : Parts)
	{
		if (part.Mesh.EngineMesh.IsValid())
		{
			return part.Mesh.NativeSize * Modumate::InchesToCentimeters;
		}
	}
	return FVector::ZeroVector;
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
		partSlot.ParentSlotIndex = INDEX_NONE;
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
// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/AssemblySpec/BIMAssemblySpec.h"
#include "BIMKernel/Presets/BIMPresetInstance.h"
#include "BIMKernel/AssemblySpec/BIMLayerSpec.h"
#include "BIMKernel/AssemblySpec/BIMPartLayout.h"
#include "BIMKernel/Presets/BIMPresetEditor.h"
#include "Database/ModumateObjectDatabase.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "ModumateCore/ModumateConsoleCommand.h"
#include "ModumateCore/ModumateUnits.h"
#include "ModumateCore/ExpressionEvaluator.h"
#include "Algo/Reverse.h"
#include "Algo/Accumulate.h"
#include "Containers/Queue.h"

EBIMResult FBIMAssemblySpec::FromPreset(const FModumateDatabase& InDB, const FBIMPresetCollection& PresetCollection, const FBIMKey& PresetID)
{
	Reset();
	EBIMResult ret = EBIMResult::Success;
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
		RiserLayer,
		Cabinet
	};

	// Structure used to walk the tree of presets
	struct FPresetIterator
	{
		FBIMKey PresetID;
		const FBIMPresetInstance* Preset = nullptr;

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
		FBIMPresetPartSlot Slot;
		FBIMKey SlotConfigPreset;
		int32 ParentSlotIndex = 0;
	};
	TQueue<FPartIterator> partIteratorQueue;

#if WITH_EDITOR
	DEBUG_GUID = PresetCollection.Presets.Find(PresetID)->GUID;
#endif

	const FBIMPresetInstance* assemblyPreset = PresetCollection.Presets.Find(PresetID);

	if (!ensureAlways(assemblyPreset != nullptr))
	{
		return EBIMResult::Error;
	}

	if (!assemblyPreset->SlotConfigPresetID.IsNone())
	{
		const FBIMPresetInstance* slotConfigPreset = PresetCollection.Presets.Find(assemblyPreset->SlotConfigPresetID);
		if (ensureAlways(slotConfigPreset != nullptr))
		{
			slotConfigPreset->TryGetProperty(BIMPropertyNames::ConceptualSizeY, SlotConfigConceptualSizeY);
			SlotConfigTagPath = slotConfigPreset->MyTagPath;
		}
	}

	while (iteratorStack.Num() > 0)
	{
		// Make a copy of the top iterator
		//It will get modified below to set the context for subsequent children
		FPresetIterator presetIterator = iteratorStack.Pop();

		presetIterator.Preset = PresetCollection.Presets.Find(presetIterator.PresetID);
		if (!ensureAlways(presetIterator.Preset != nullptr))
		{
			ret = EBIMResult::Error;
			continue;
		}

		// All nodes should have a scope
		if (!ensureAlways(presetIterator.Preset->NodeScope != EBIMValueScope::None))
		{
			ret = EBIMResult::Error;
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
		else if (
			presetIterator.Preset->ObjectType == EObjectType::OTStructureLine ||
			presetIterator.Preset->ObjectType == EObjectType::OTMullion ||
			presetIterator.Preset->ObjectType == EObjectType::OTTrim
			)
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
		presetIterator.TargetProperties->AddProperties(presetIterator.Preset->Properties);

		// Add our own children to DFS stack
		// Iterate in reverse order because stack operations are LIFO
		for (int32 i=presetIterator.Preset->ChildPresets.Num()-1;i>=0;--i)
		{
			const FBIMPresetPinAttachment& childPreset = presetIterator.Preset->ChildPresets[i];
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
			case EBIMPinTarget::Cabinet:
				childIterator.Target = ELayerTarget::Cabinet;
				break;
			default:
				break;
			}
			iteratorStack.Push(childIterator);
		}

		// If a preset has parts, add them to the part queue for processing below

		for (const auto& part : presetIterator.Preset->PartSlots)
		{
			if (!part.PartPreset.IsNone())
			{
				FPartIterator partIterator;
				partIterator.SlotConfigPreset = presetIterator.Preset->SlotConfigPresetID;
				partIterator.Slot = part;
				partIteratorQueue.Enqueue(partIterator);

				const FBIMPresetInstance* slotConfigPreset = PresetCollection.Presets.Find(partIterator.SlotConfigPreset);
				if (slotConfigPreset != nullptr)
				{
					if (SlotConfigConceptualSizeY.IsEmpty())
					{
						slotConfigPreset->TryGetProperty(BIMPropertyNames::ConceptualSizeY, SlotConfigConceptualSizeY);
					}

					if (SlotConfigTagPath.Tags.Num() == 0)
					{
						SlotConfigTagPath = slotConfigPreset->MyTagPath;
					}
				}
			}
		}
	}

	// If we encountered any parts, add a root part to the top of the list to be the parent of the others
	if (!partIteratorQueue.IsEmpty())
	{
		FBIMPartSlotSpec& partSpec = Parts.AddDefaulted_GetRef();
		partSpec.ParentSlotIndex = INDEX_NONE;
		partSpec.NodeCategoryPath = assemblyPreset->MyTagPath;
		partSpec.Translation = Modumate::Expression::FVectorExpression(TEXT("0"), TEXT("0"), TEXT("0"));
		partSpec.Size = Modumate::Expression::FVectorExpression(TEXT("Self.ScaledSizeX"), TEXT("Self.ScaledSizeY"), TEXT("Self.ScaledSizeZ"));
#if WITH_EDITOR //for debugging
		partSpec.DEBUGNodeScope = EBIMValueScope::Assembly;
		partSpec.DEBUGPresetID = PresetID;
		partSpec.DEBUG_GUID = assemblyPreset->GUID;
#endif
	}

	// For each part in the iterator queue, add a part to the assembly list, set its parents and add children to back of queue
	// This ensures that parents will always appear ahead of children in the array so we can process it from front to back
	while (!partIteratorQueue.IsEmpty())
	{
		FPartIterator partIterator;
		partIteratorQueue.Dequeue(partIterator);
		const FBIMPresetInstance* partPreset = PresetCollection.Presets.Find(partIterator.Slot.PartPreset);
		const FBIMPresetInstance* slotPreset = PresetCollection.Presets.Find(partIterator.Slot.SlotPreset);
		const FBIMPresetInstance* slotConfigPreset = PresetCollection.Presets.Find(partIterator.SlotConfigPreset);

		if (ensureAlways(partPreset != nullptr) && ensureAlways(slotPreset != nullptr) && ensureAlways(slotConfigPreset != nullptr))
		{
			// Each child of a part represents one component (mesh, material or color)
			FBIMPartSlotSpec& partSpec = Parts.AddDefaulted_GetRef();
			partSpec.ParentSlotIndex = partIterator.ParentSlotIndex;
			partSpec.NodeCategoryPath = partPreset->MyTagPath;
			ensureAlways(slotPreset->Properties.TryGetProperty(EBIMValueScope::Slot, BIMPropertyNames::ID, partSpec.SlotID));

#if WITH_EDITOR //for debugging
			partSpec.DEBUGNodeScope = partPreset->NodeScope;
			partSpec.DEBUGPresetID = partPreset->PresetID;
			partSpec.DEBUGSlotName = partIterator.Slot.SlotPreset;
			partSpec.DEBUG_GUID = partPreset->GUID;
#endif
			// If this child has a mesh asset ID, this fetch the mesh and use it 
			FBIMKey meshAsset;
			if (partPreset->Properties.TryGetProperty(EBIMValueScope::Mesh, BIMPropertyNames::AssetID, meshAsset))
			{
				const FArchitecturalMesh* mesh = InDB.GetArchitecturalMeshByKey(meshAsset);
				if (!ensureAlways(mesh != nullptr))
				{
					ret = EBIMResult::Error;
					break;
				}

				partSpec.Mesh = *mesh;
			}

			for (auto& matBinding : partPreset->MaterialChannelBindings)
			{
				FBIMKey matKey = matBinding.SurfaceMaterial.IsNone() ? matBinding.InnerMaterial : matBinding.SurfaceMaterial;
				const FArchitecturalMaterial* material = InDB.GetArchitecturalMaterialByKey(matKey);
				if (ensureAlways(material != nullptr))
				{
					FArchitecturalMaterial newMat = *material;
					newMat.Color = matBinding.ColorHexValue.IsEmpty() ? FColor::White : FColor::FromHex(matBinding.ColorHexValue);
					partSpec.ChannelMaterials.Add(*matBinding.Channel, newMat);
				}
			}	

			// Find which slot this child belongs to and fetch transform data
			for (auto& childSlot : slotConfigPreset->ChildPresets)
			{
				if (childSlot.PresetID == partIterator.Slot.SlotPreset)
				{
					const FBIMPresetInstance* childSlotPreset = PresetCollection.Presets.Find(childSlot.PresetID);
					if (!ensureAlways(childSlotPreset != nullptr))
					{
						break;
					}

					// FVectorExpression holds 3 values as formula strings (ie "Parent.NativeSizeX * 0.5) that are evaluated in CompoundMeshActor
					partSpec.Translation = Modumate::Expression::FVectorExpression(
						childSlotPreset->GetProperty<FString>(TEXT("LocationX")),
						childSlotPreset->GetProperty<FString>(TEXT("LocationY")),
						childSlotPreset->GetProperty<FString>(TEXT("LocationZ")));

					partSpec.Orientation = Modumate::Expression::FVectorExpression(
						childSlotPreset->GetProperty<FString>(TEXT("RotationX")),
						childSlotPreset->GetProperty<FString>(TEXT("RotationY")),
						childSlotPreset->GetProperty<FString>(TEXT("RotationZ")));

					partSpec.Size = Modumate::Expression::FVectorExpression(
						childSlotPreset->GetProperty<FString>(TEXT("SizeX")),
						childSlotPreset->GetProperty<FString>(TEXT("SizeY")),
						childSlotPreset->GetProperty<FString>(TEXT("SizeZ")));

					partSpec.Flip[0] = !childSlotPreset->GetProperty<FString>(TEXT("FlipX")).IsEmpty();
					partSpec.Flip[1] = !childSlotPreset->GetProperty<FString>(TEXT("FlipY")).IsEmpty();
					partSpec.Flip[2] = !childSlotPreset->GetProperty<FString>(TEXT("FlipZ")).IsEmpty();

					break;
				}
			}

			// If this part is an assembly, then establish this part as the parent of its children, otherwise use the current parent
			int32 parentSlotIndex = partPreset->NodeScope == EBIMValueScope::Assembly ? Parts.Num() - 1 : partIterator.ParentSlotIndex;
			for (const auto& part : partPreset->PartSlots)
			{
				if (!part.PartPreset.IsNone())
				{
					FPartIterator nextPartIterator;
					nextPartIterator.ParentSlotIndex = parentSlotIndex;
					nextPartIterator.Slot = part;
					nextPartIterator.SlotConfigPreset = partPreset->SlotConfigPresetID;
					partIteratorQueue.Enqueue(nextPartIterator);
				}
			}
		}
	}

	// All assembly specs must bind to an object type
	if (ObjectType != EObjectType::OTNone)
	{
		return DoMakeAssembly(InDB, PresetCollection);
	}

	return EBIMResult::Success;
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
	FVector nativeSize = FVector::ZeroVector;

	// TODO: only needed for Y, retire when Depth is available
	for (auto& part : Parts)
	{
		if (part.Mesh.EngineMesh.IsValid())
		{
			nativeSize = part.Mesh.NativeSize * Modumate::InchesToCentimeters;
			break;
		}
	}

	RootProperties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Width, nativeSize.X);
	RootProperties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Height, nativeSize.Z);

	return nativeSize;
}

EBIMResult FBIMAssemblySpec::MakeCabinetAssembly(const FModumateDatabase& InDB)
{
	if (!RootProperties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::ToeKickDepth, ToeKickDepth))
	{
		ToeKickDepth = Modumate::Units::FUnitValue::WorldCentimeters(0);
	}

	if (!RootProperties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::ToeKickHeight, ToeKickHeight))
	{
		ToeKickHeight = Modumate::Units::FUnitValue::WorldCentimeters(0);
	}

	// Cabinets consist of a list of parts (per rigged assembly) and a single material added to an extrusion for the prism
	FString materialAsset;
	if (ensureAlways(RootProperties.TryGetProperty(EBIMValueScope::RawMaterial, BIMPropertyNames::AssetID, materialAsset)))
	{
		const FArchitecturalMaterial* mat = InDB.GetArchitecturalMaterialByKey(materialAsset);
		if (ensureAlways(mat != nullptr))
		{
			FBIMExtrusionSpec& extrusion = Extrusions.AddDefaulted_GetRef();
			extrusion.Material = *mat;

			FString colorHexValue;
			if (RootProperties.TryGetProperty(EBIMValueScope::Color, BIMPropertyNames::HexValue, colorHexValue) && !colorHexValue.IsEmpty())
			{
				extrusion.Material.Color = FColor::FromHex(colorHexValue);
			}

#if WITH_EDITOR
			FBIMPartLayout layout;
			ensureAlways(layout.FromAssembly(*this, FVector::OneVector) == EBIMResult::Success);
#endif
			return EBIMResult::Success;
		}
	}

	return EBIMResult::Error;
}

EBIMResult FBIMAssemblySpec::MakeRiggedAssembly(const FModumateDatabase& InDB)
{
	// TODO: "Stubby" temporary FFE don't have parts, just one mesh on their root
	if (Parts.Num() == 0)
	{
		FBIMKey meshKey;

		auto stringToAxis = [](const FString& InStr, FVector& OutVector,const FVector& InDefault)
		{
			if (InStr.Len() == 2)
			{
				float v = InStr[0] == TCHAR('+') ? 1.0f : -1.0f;
				switch (InStr[1])
				{
				case TCHAR('X'): OutVector = FVector(v, 0, 0); return true;
				case TCHAR('Y'): OutVector = FVector(0, v, 0); return true;
				case TCHAR('Z'): OutVector = FVector(0, 0, v); return true;
				default: OutVector = InDefault; return false;
				};
			}
			OutVector = InDefault; 
			return false;
		};

		// Normal and Tangent have default values set in class def
		FString vectorStr;
		if (RootProperties.TryGetProperty(EBIMValueScope::Mesh, BIMPropertyNames::Normal, vectorStr))
		{
			ensureAlways(stringToAxis(vectorStr, Normal, Normal));
		}

		if (RootProperties.TryGetProperty(EBIMValueScope::Mesh, BIMPropertyNames::Tangent, vectorStr))
		{
			ensureAlways(stringToAxis(vectorStr, Tangent, Tangent));
		}

		if (!RootProperties.TryGetProperty(EBIMValueScope::Mesh, BIMPropertyNames::AssetID, meshKey))
		{
			return EBIMResult::Error;
		}

		const FArchitecturalMesh* mesh = InDB.GetArchitecturalMeshByKey(meshKey);
		if (mesh == nullptr)
		{
			return EBIMResult::Error;
		}

		FBIMPartSlotSpec& partSlot = Parts.AddDefaulted_GetRef();
		partSlot.Mesh = *mesh;
		partSlot.Translation = Modumate::Expression::FVectorExpression(TEXT("0"), TEXT("0"), TEXT("0"));
		partSlot.Size = Modumate::Expression::FVectorExpression(TEXT("Self.ScaledSizeX"), TEXT("Self.ScaledSizeY"), TEXT("Self.ScaledSizeZ"));
		partSlot.Flip[0] = false;
		partSlot.Flip[1] = false;
		partSlot.Flip[2] = false;
		partSlot.ParentSlotIndex = INDEX_NONE;
	}

#if WITH_EDITOR
	FBIMPartLayout layout;
	ensureAlways(layout.FromAssembly(*this, FVector::OneVector) == EBIMResult::Success);
#endif

	return EBIMResult::Success;
}

EBIMResult FBIMAssemblySpec::MakeLayeredAssembly(const FModumateDatabase& InDB)
{
	auto buildLayers = [InDB](TArray<FBIMLayerSpec>& Layers)
	{
		for (auto& layer : Layers)
		{
			EBIMResult res = layer.BuildFromProperties(InDB);
			if (res != EBIMResult::Success)
			{
				return res;
			}
		}
		return EBIMResult::Success;
	};

	EBIMResult res = buildLayers(Layers);
	
	if (res == EBIMResult::Success)
	{
		res = buildLayers(TreadLayers);
	}

	if (res == EBIMResult::Success)
	{
		res = buildLayers(RiserLayers);
	}

	// NOTE: Layers for finishes are designed veneer first, then barriers and adhesives
	// but should be built adhesive first, then barriers and veneers, so we reverse layer order
	if (ObjectType == EObjectType::OTFinish)
	{
		Algo::Reverse(Layers);
	}

	return res;
}

EBIMResult FBIMAssemblySpec::MakeExtrudedAssembly(const FModumateDatabase& InDB)
{
	for (auto& extrusion : Extrusions)
	{
		FString materialAsset;
		if (extrusion.Properties.TryGetProperty(EBIMValueScope::RawMaterial, BIMPropertyNames::AssetID, materialAsset))
		{
			const FArchitecturalMaterial* mat = InDB.GetArchitecturalMaterialByKey(materialAsset);
			if (ensureAlways(mat != nullptr))
			{
				extrusion.Material = *mat;
				FString colorHexValue;
				if (extrusion.Properties.TryGetProperty(EBIMValueScope::Color, BIMPropertyNames::HexValue, colorHexValue))
				{
					extrusion.Material.Color = FColor::FromHex(colorHexValue);
				}
			}
		}
		EBIMResult res = extrusion.BuildFromProperties(InDB);
		if (!ensureAlways(res == EBIMResult::Success))
		{
			return res;
		}
	}
	return EBIMResult::Success;
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

EBIMResult FBIMAssemblySpec::DoMakeAssembly(const FModumateDatabase& InDB, const FBIMPresetCollection& PresetCollection)
{
	RootProperties.TryGetProperty(EBIMValueScope::Assembly,BIMPropertyNames::Name, DisplayName);
	RootProperties.TryGetProperty(EBIMValueScope::Assembly, BIMPropertyNames::Comments, Comments);
	RootProperties.TryGetProperty(EBIMValueScope::Assembly, BIMPropertyNames::Code, CodeName);
	RootProperties.TryGetProperty(EBIMValueScope::Assembly, TEXT("IdealTreadDepth"), TreadDepth);

	// TODO: move assembly synthesis to each tool mode or MOI implementation (TBD)
	EBIMResult result = EBIMResult::Error;
	switch (ObjectType)
	{
	case EObjectType::OTTrim:
	case EObjectType::OTStructureLine:
	case EObjectType::OTMullion:
		return MakeExtrudedAssembly(InDB);

	case EObjectType::OTFloorSegment:
	case EObjectType::OTWallSegment:
	case EObjectType::OTRoofFace:
	case EObjectType::OTCeiling:
	case EObjectType::OTRailSegment:
	case EObjectType::OTCountertop:
	case EObjectType::OTSystemPanel:
	case EObjectType::OTFinish:
	case EObjectType::OTStaircase:
		return MakeLayeredAssembly(InDB);

	case EObjectType::OTDoor:
	case EObjectType::OTWindow:
	case EObjectType::OTFurniture:
		return MakeRiggedAssembly(InDB);

	case EObjectType::OTCabinet:
		return MakeCabinetAssembly(InDB);

	default:
		ensureAlways(false);
	};

	return EBIMResult::Error;
}
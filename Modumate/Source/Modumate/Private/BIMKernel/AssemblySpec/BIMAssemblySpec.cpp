// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/AssemblySpec/BIMAssemblySpec.h"
#include "BIMKernel/Presets/BIMPresetInstance.h"
#include "BIMKernel/AssemblySpec/BIMLayerSpec.h"
#include "BIMKernel/AssemblySpec/BIMPartLayout.h"
#include "BIMKernel/Presets/BIMPresetEditor.h"
#include "BIMKernel/Presets/BIMPresetMaterialBinding.h"
#include "Database/ModumateObjectDatabase.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "ModumateCore/ModumateConsoleCommand.h"
#include "ModumateCore/ModumateUnits.h"
#include "ModumateCore/ExpressionEvaluator.h"
#include "Algo/Reverse.h"
#include "Algo/Accumulate.h"
#include "Containers/Queue.h"

EBIMResult FBIMAssemblySpec::FromPreset(const FModumateDatabase& InDB, const FBIMPresetCollectionProxy& PresetCollection, const FGuid& InPresetGUID)
{
	Reset();
	EBIMResult ret = EBIMResult::Success;
	PresetGUID = InPresetGUID;

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
		FGuid PresetGUID;
		const FBIMPresetInstance* Preset = nullptr;

		// As we iterate, we keep track of which layer and which property sheet to set modifier values on
		ELayerTarget Target = ELayerTarget::Assembly;
		FBIMLayerSpec* TargetLayer = nullptr;
		FBIMPropertySheet* TargetProperties = nullptr;
	};

	// We use a stack of iterators to perform a depth-first visitation of child presets
	TArray<FPresetIterator> iteratorStack;
	FPresetIterator rootIterator;
	rootIterator.PresetGUID = PresetGUID;
	rootIterator.TargetProperties = &RootProperties;
	iteratorStack.Push(rootIterator);

	// When we encounter a part hierarchy, we want to iterate breadth-first, so use a queue of part iterators
	struct FPartIterator
	{
		FBIMPresetPartSlot Slot;
		FGuid SlotConfigPreset;
		int32 ParentSlotIndex = 0;
	};
	TQueue<FPartIterator> partIteratorQueue;

	const FBIMPresetInstance* assemblyPreset = PresetCollection.PresetFromGUID(PresetGUID);
	const FBIMPresetInstance* dimensionsPreset = assemblyPreset;

	if (!ensureAlways(assemblyPreset != nullptr))
	{
		return EBIMResult::Error;
	}

	switch (assemblyPreset->ObjectType)
	{
		case EObjectType::OTEdgeDetail: ensureAlways(assemblyPreset->TryGetCustomData(EdgeDetailData)); break;
		case EObjectType::OTCabinet: ensureAlways(assemblyPreset->TryGetCustomData(MaterialBindingSet)); break;
	};

	if (assemblyPreset->SlotConfigPresetGUID.IsValid())
	{
		const FBIMPresetInstance* slotConfigPreset = PresetCollection.PresetFromGUID(assemblyPreset->SlotConfigPresetGUID);
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

		presetIterator.Preset = PresetCollection.PresetFromGUID(presetIterator.PresetGUID);
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

		// We should only encounter a single object type in the child tree UNLESS we're making a cabinet and we encounter a door
		if (presetIterator.Preset->ObjectType != EObjectType::OTNone)
		{
			if (ObjectType == EObjectType::OTNone)
			{
				ObjectType = presetIterator.Preset->ObjectType;
			}
			else
			{
				if (!ensureAlways(ObjectType == EObjectType::OTCabinet && presetIterator.Preset->ObjectType == EObjectType::OTDoor))
				{
					ret = EBIMResult::Error;
				}
			}
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
				presetIterator.TargetLayer = &Layers.AddDefaulted_GetRef();
				break;
			case ELayerTarget::TreadLayer:
				presetIterator.TargetLayer = &TreadLayers.AddDefaulted_GetRef();
				break;
			case ELayerTarget::RiserLayer:
				presetIterator.TargetLayer = &RiserLayers.AddDefaulted_GetRef();
				break;
			default:
				ensureAlways(false);
				continue;
				break;
			};

			presetIterator.TargetProperties = nullptr;
			presetIterator.TargetLayer->MeasurementMethod = presetIterator.Preset->MeasurementMethod;
			presetIterator.TargetLayer->PresetGUID = presetIterator.PresetGUID;
			presetIterator.TargetLayer->UpdatePatternFromPreset(InDB, *presetIterator.Preset);
		}
		else if (presetIterator.Preset->NodeScope == EBIMValueScope::Module || presetIterator.Preset->NodeScope == EBIMValueScope::Gap)
		{
			if (ensureAlways(presetIterator.TargetLayer != nullptr))
			{
				presetIterator.TargetLayer->UpdatePatternFromPreset(InDB, *presetIterator.Preset);
			}
		}
		else if (
			presetIterator.Preset->ObjectType == EObjectType::OTStructureLine ||
			presetIterator.Preset->ObjectType == EObjectType::OTMullion ||
			presetIterator.Preset->ObjectType == EObjectType::OTTrim
			)
		{
			auto &extrusion = Extrusions.AddDefaulted_GetRef();
			presetIterator.TargetProperties = &extrusion.Properties;
			extrusion.PresetGUID = presetIterator.PresetGUID;
			presetIterator.Preset->TryGetCustomData(MaterialBindingSet);
		}

		// If we're targeting a cabinet, this is the cabinet's face which means:
		// 1) We want to derive named dimensions from this preset and not the top level assembly
		// 2) We do not want to clobber the top level assembly's properties with the face's
		// We need this exception because cabinet faces are the only complex assemblies that are attached as a child instead of a part
		if (presetIterator.Target == ELayerTarget::Cabinet)
		{
			dimensionsPreset = presetIterator.Preset;
		}
		else if (presetIterator.TargetProperties != nullptr)
		{
			presetIterator.TargetProperties->AddProperties(presetIterator.Preset->Properties);
		}

		// Add our own children to DFS stack
		// Iterate in reverse order because stack operations are LIFO
		for (int32 i=presetIterator.Preset->ChildPresets.Num()-1;i>=0;--i)
		{
			const FBIMPresetPinAttachment& childPreset = presetIterator.Preset->ChildPresets[i];
			// Each child inherits the targeting information from its parent (presetIterator)
			FPresetIterator childIterator = presetIterator;
			childIterator.PresetGUID = childPreset.PresetGUID;
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
			if (part.PartPresetGUID.IsValid())
			{
				FPartIterator partIterator;
				partIterator.SlotConfigPreset = presetIterator.Preset->SlotConfigPresetGUID;
				partIterator.Slot = part;
				partIteratorQueue.Enqueue(partIterator);

				const FBIMPresetInstance* slotConfigPreset = PresetCollection.PresetFromGUID(partIterator.SlotConfigPreset);
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
		partSpec.GetNamedDimensionValuesFromPreset(dimensionsPreset);
		partSpec.Translation = FVectorExpression(TEXT("0"), TEXT("0"), TEXT("0"));
		partSpec.Size = FVectorExpression(TEXT("Self.ScaledSizeX"), TEXT("Self.ScaledSizeY"), TEXT("Self.ScaledSizeZ"));
#if WITH_EDITOR //for debugging
		partSpec.DEBUGNodeScope = EBIMValueScope::Assembly;
#endif
		partSpec.PresetGUID = PresetGUID;
	}

	// For each part in the iterator queue, add a part to the assembly list, set its parents and add children to back of queue
	// This ensures that parents will always appear ahead of children in the array so we can process it from front to back
	while (!partIteratorQueue.IsEmpty())
	{
		FPartIterator partIterator;
		partIteratorQueue.Dequeue(partIterator);
		const FBIMPresetInstance* partPreset = PresetCollection.PresetFromGUID(partIterator.Slot.PartPresetGUID);
		const FBIMPresetInstance* slotPreset = PresetCollection.PresetFromGUID(partIterator.Slot.SlotPresetGUID);
		const FBIMPresetInstance* slotConfigPreset = PresetCollection.PresetFromGUID(partIterator.SlotConfigPreset);

		if (ensureAlways(partPreset != nullptr) && ensureAlways(slotPreset != nullptr) && ensureAlways(slotConfigPreset != nullptr))
		{
			// Each child of a part represents one component (mesh, material or color)
			FBIMPartSlotSpec& partSpec = Parts.AddDefaulted_GetRef();
			partSpec.ParentSlotIndex = partIterator.ParentSlotIndex;
			partSpec.NodeCategoryPath = partPreset->MyTagPath;
			partSpec.MeasurementMethod = partPreset->MeasurementMethod;
			partSpec.GetNamedDimensionValuesFromPreset(partPreset);
			ensureAlways(slotPreset->Properties.TryGetProperty(EBIMValueScope::Slot, BIMPropertyNames::ID, partSpec.SlotID));

#if WITH_EDITOR //for debugging
			partSpec.DEBUGNodeScope = partPreset->NodeScope;
#endif
			partSpec.SlotGUID = partIterator.Slot.SlotPresetGUID;
			partSpec.PresetGUID = partPreset->GUID;
			// If this child has a mesh asset ID, this fetch the mesh and use it 
			FGuid meshAsset;
			if (partPreset->Properties.TryGetProperty(EBIMValueScope::Mesh, BIMPropertyNames::AssetID, meshAsset))
			{
				const FArchitecturalMesh* mesh = InDB.GetArchitecturalMeshByGUID(meshAsset);
				if (!ensureAlways(mesh != nullptr))
				{
					ret = EBIMResult::Error;
					break;
				}

				partSpec.Mesh = *mesh;
			}

			FBIMPresetMaterialBindingSet bindingSet;
			if (partPreset->TryGetCustomData(bindingSet))
			{
				for (auto& matBinding : bindingSet.MaterialBindings)
				{
					FArchitecturalMaterial newMat;
					if (ensureAlways(matBinding.GetEngineMaterial(InDB, newMat) == EBIMResult::Success))
					{
						partSpec.ChannelMaterials.Add(matBinding.Channel, newMat);
					}
				}
			}

			// Find which slot this child belongs to and fetch transform data
			for (auto& childSlot : slotConfigPreset->ChildPresets)
			{
				if (childSlot.PresetGUID == partIterator.Slot.SlotPresetGUID)
				{
					const FBIMPresetInstance* childSlotPreset = PresetCollection.PresetFromGUID(childSlot.PresetGUID);
					if (!ensureAlways(childSlotPreset != nullptr))
					{
						break;
					}

					// FVectorExpression holds 3 values as formula strings (ie "Parent.NativeSizeX * 0.5) that are evaluated in CompoundMeshActor
					partSpec.Translation = FVectorExpression(
						childSlotPreset->GetProperty<FString>(TEXT("LocationX")),
						childSlotPreset->GetProperty<FString>(TEXT("LocationY")),
						childSlotPreset->GetProperty<FString>(TEXT("LocationZ")));

					partSpec.Orientation = FVectorExpression(
						childSlotPreset->GetProperty<FString>(TEXT("RotationX")),
						childSlotPreset->GetProperty<FString>(TEXT("RotationY")),
						childSlotPreset->GetProperty<FString>(TEXT("RotationZ")));

					partSpec.Size = FVectorExpression(
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
				if (part.PartPresetGUID.IsValid())
				{
					FPartIterator nextPartIterator;
					nextPartIterator.ParentSlotIndex = parentSlotIndex;
					nextPartIterator.Slot = part;
					nextPartIterator.SlotConfigPreset = partPreset->SlotConfigPresetGUID;
					partIteratorQueue.Enqueue(nextPartIterator);
				}
			}
		}
	}

	// All assembly specs must bind to an object type
	if (ret == EBIMResult::Success && ObjectType != EObjectType::OTNone)
	{
		return DoMakeAssembly(InDB, PresetCollection);
	}

	return ret;
}

EBIMResult FBIMAssemblySpec::Reset()
{
	*this = FBIMAssemblySpec();
	return EBIMResult::Success;
}

EBIMResult FBIMAssemblySpec::ReverseLayers()
{
	Algo::Reverse(Layers);
	return EBIMResult::Success;
}

FVector FBIMAssemblySpec::GetRiggedAssemblyNativeSize() const
{
	FVector nativeSize = FVector::ZeroVector;

	// TODO: only needed for Y, retire when Depth is available
	for (auto& part : Parts)
	{
		if (part.Mesh.EngineMesh.IsValid())
		{
			nativeSize = part.Mesh.NativeSize * UModumateDimensionStatics::InchesToCentimeters;
			break;
		}
	}

	// TODO: Read dimension properties for point hosted objs
	if (ObjectType == EObjectType::OTPointHosted)
	{
		return nativeSize;
	}

	RootProperties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Width, nativeSize.X);
	RootProperties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Height, nativeSize.Z);

	return nativeSize;
}

EBIMResult FBIMAssemblySpec::MakeCabinetAssembly(const FModumateDatabase& InDB)
{
	FModumateUnitValue dimension;
	if (RootProperties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::ToeKickDepth, dimension))
	{
		ToeKickDepthCentimeters = dimension.AsWorldCentimeters();
	}
	else
	{
		ToeKickDepthCentimeters = 0.0f;
	}

	if (RootProperties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::ToeKickHeight, dimension))
	{
		ToeKickHeightCentimeters = dimension.AsWorldCentimeters();
	}
	else
	{
		ToeKickHeightCentimeters = 0.0f;
	}

	// Cabinets consist of a list of parts (per rigged assembly) and a single material added to an extrusion for the prism
	FGuid materialAsset;
	if (ensureAlways(MaterialBindingSet.MaterialBindings.Num() > 0))
	{
		const auto& binding = MaterialBindingSet.MaterialBindings[0];

		FBIMExtrusionSpec& extrusion = Extrusions.AddDefaulted_GetRef();
		if (ensureAlways(binding.GetEngineMaterial(InDB, extrusion.Material) == EBIMResult::Success))
		{
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
		FGuid meshKey;

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
		if (RootProperties.TryGetProperty(EBIMValueScope::Part, BIMPropertyNames::Normal, vectorStr))
		{
			ensureAlways(stringToAxis(vectorStr, Normal, Normal));
		}

		if (RootProperties.TryGetProperty(EBIMValueScope::Part, BIMPropertyNames::Tangent, vectorStr))
		{
			ensureAlways(stringToAxis(vectorStr, Tangent, Tangent));
		}

		if (RootProperties.TryGetProperty(EBIMValueScope::Part, BIMPropertyNames::Zalign, vectorStr))
		{
			bZalign = !vectorStr.IsEmpty();
		}

		if (!RootProperties.TryGetProperty(EBIMValueScope::Mesh, BIMPropertyNames::AssetID, meshKey))
		{
			return EBIMResult::Error;
		}

		const FArchitecturalMesh* mesh = InDB.GetArchitecturalMeshByGUID(meshKey);
		if (mesh == nullptr)
		{
			return EBIMResult::Error;
		}

		FBIMPartSlotSpec& partSlot = Parts.AddDefaulted_GetRef();
		partSlot.Mesh = *mesh;
		partSlot.Translation = FVectorExpression(TEXT("0"), TEXT("0"), TEXT("0"));
		partSlot.Size = FVectorExpression(TEXT("Self.ScaledSizeX"), TEXT("Self.ScaledSizeY"), TEXT("Self.ScaledSizeZ"));
		partSlot.Flip[0] = false;
		partSlot.Flip[1] = false;
		partSlot.Flip[2] = false;
		partSlot.ParentSlotIndex = INDEX_NONE;
	}

	FBIMPartLayout layout;
	return layout.FromAssembly(*this, FVector::OneVector);
}

EBIMResult FBIMAssemblySpec::MakeLayeredAssembly(const FModumateDatabase& InDB)
{
	auto buildLayers = [InDB](TArray<FBIMLayerSpec>& LayersToBuild)
	{
		for (auto& layer : LayersToBuild)
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
	if (MaterialBindingSet.MaterialBindings.Num() == 0)
	{
		return EBIMResult::Success;
	}

	const auto& binding = MaterialBindingSet.MaterialBindings[0];

	FArchitecturalMaterial newMat;
	if (ensureAlways(binding.GetEngineMaterial(InDB, newMat) == EBIMResult::Success))
	{
		for (auto& extrusion : Extrusions)
		{
			extrusion.Material = newMat;
			EBIMResult res = extrusion.BuildFromProperties(InDB);
			if (!ensureAlways(res == EBIMResult::Success))
			{
				return res;
			}
		}
		return EBIMResult::Success;
	}
	return EBIMResult::Error;
}

FModumateUnitValue FBIMAssemblySpec::CalculateThickness() const
{
	return FModumateUnitValue::WorldCentimeters(Algo::TransformAccumulate(
		Layers,
		[](const FBIMLayerSpec& l)
		{
			return l.ThicknessCentimeters;
		},
			0.0f,
			[](float lhs, float rhs)
		{
			return lhs + rhs;
		}
	));
}

EBIMResult FBIMAssemblySpec::DoMakeAssembly(const FModumateDatabase& InDB, const FBIMPresetCollectionProxy& PresetCollection)
{
	RootProperties.TryGetProperty(EBIMValueScope::Assembly,BIMPropertyNames::Name, DisplayName);
	RootProperties.TryGetProperty(EBIMValueScope::Assembly, BIMPropertyNames::Comments, Comments);
	RootProperties.TryGetProperty(EBIMValueScope::Assembly, BIMPropertyNames::Code, CodeName);

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
		return MakeLayeredAssembly(InDB);

	case EObjectType::OTStaircase:
	{
		FModumateUnitValue dimension;
		if (ensureAlways(RootProperties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::TreadDepthIdeal, dimension) && dimension.AsWorldCentimeters() > 0.0f))
		{
			TreadDepthCentimeters = dimension.AsWorldCentimeters();
		}
		else
		{
			// Prevent divide by zero errors with treads of ludicrous size
			TreadDepthCentimeters = 1.0f;
		}
		return MakeLayeredAssembly(InDB);
	}

	case EObjectType::OTDoor:
	case EObjectType::OTWindow:
	case EObjectType::OTFurniture:
	case EObjectType::OTPointHosted:
		return MakeRiggedAssembly(InDB);

	case EObjectType::OTCabinet:
		return MakeCabinetAssembly(InDB);

	case EObjectType::OTEdgeDetail:
		// Edge detail assemblies carry data directly from their preset, no construction step
		// TODO: when property sheets are deprecated, construction should become unnecessary for other types as well
		return EBIMResult::Success;

	default:
		ensureAlways(false);
	};

	return EBIMResult::Error;
}

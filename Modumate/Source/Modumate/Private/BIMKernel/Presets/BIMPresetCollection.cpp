// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/Presets/BIMPresetCollection.h"

#include "BIMKernel/AssemblySpec/BIMAssemblySpec.h"
#include "BIMKernel/Presets/BIMPresetDocumentDelta.h"
#include "BIMKernel/Presets/BIMSymbolPresetData.h"

#include "DocumentManagement/ModumateSerialization.h"
#include "ModumateCore/ModumateScriptProcessor.h"
#include "Online/ModumateAnalyticsStatics.h"
#include "BIMKernel/AssemblySpec/BIMPartLayout.h"
#include "BIMKernel/Presets/CustomData/BIMDimensions.h"
#include "BIMKernel/Presets/CustomData/BIMIESProfile.h"
#include "BIMKernel/Presets/CustomData/BIMMesh.h"
#include "BIMKernel/Presets/CustomData/BIMMeshRef.h"
#include "BIMKernel/Presets/CustomData/BIMNamedDimension.h"
#include "BIMKernel/Presets/CustomData/BIMPattern.h"
#include "BIMKernel/Presets/CustomData/BIMProfile.h"
#include "BIMKernel/Presets/CustomData/BIMRawMaterial.h"
#include "BIMKernel/Presets/CustomData/BIMSlot.h"
#include "DocumentManagement/ModumateDocument.h"
#include "ModumateCore/EnumHelpers.h"
#include "Objects/ModumateObjectStatics.h"

#include "Online/ModumateCloudConnection.h"
#include "Engine/AssetManager.h"
#include "Objects/TerrainMaterial.h"
#include "UnrealClasses/EditModelGameState.h"

#define LOCTEXT_NAMESPACE "BIMPresetCollection"

TModumateDataCollection<FArchitecturalMaterial> FBIMPresetCollection::AMaterials;
TModumateDataCollection<FArchitecturalMesh> FBIMPresetCollection::AMeshes;
TModumateDataCollection<FSimpleMeshRef> FBIMPresetCollection::SimpleMeshes;
TModumateDataCollection<FStaticIconTexture> FBIMPresetCollection::StaticIconTextures;
TModumateDataCollection<FLayerPattern> FBIMPresetCollection::Patterns;
TModumateDataCollection<FArchitecturalLight> FBIMPresetCollection::Lights;
TMap<FString, FLightConfiguration> FBIMPresetCollection::LightConfigurations;
TMap<EObjectType, FBIMAssemblySpec> FBIMPresetCollection::DefaultAssembliesByObjectType;
TMap<EObjectType, FAssemblyDataCollection> FBIMPresetCollection::AssembliesByObjectType;
TSet<FGuid> FBIMPresetCollection::UsedGUIDs;

//TODO: Merge this in to the GetAllDescendents method below. They are pretty much copy/paste -JN
EBIMResult FBIMPresetCollection::GetDirectCanonicalDescendents(const FGuid& PresetID, TSet<FGuid>& OutCanonicals) const
{
	auto* preset = PresetFromGUID(PresetID);
	
	if(!ensure(preset != nullptr)) return EBIMResult::Error;
	
	for (auto &childNode : preset->ChildPresets)
	{
		OutCanonicals.Add(childNode.PresetGUID);
	}

	if (preset->SlotConfigPresetGUID.IsValid())
	{
		OutCanonicals.Add(preset->SlotConfigPresetGUID);
	}

	for (auto& part : preset->PartSlots)
	{
		if (part.PartPresetGUID.IsValid())
		{
			OutCanonicals.Add(part.PartPresetGUID);
		}
		if (part.SlotPresetGUID.IsValid())
		{
			OutCanonicals.Add(part.SlotPresetGUID);
		}
	}
	
	preset->Properties_DEPRECATED.ForEachProperty([&](const FBIMPropertyKey& PropKey,const FString& Value) {
		FGuid guid;
		if (!Value.IsEmpty() && FGuid::Parse(Value, guid) && guid.IsValid())
		{
			OutCanonicals.Add(guid);
		}
	});

	FBIMPresetMaterialBindingSet materialBindingSet;
	if (preset->TryGetCustomData(materialBindingSet))
	{
		for (auto& binding : materialBindingSet.MaterialBindings)
		{
			if (binding.SurfaceMaterialGUID.IsValid())
			{
				OutCanonicals.Add(binding.SurfaceMaterialGUID);
			}
			if (binding.InnerMaterialGUID.IsValid())
			{
				OutCanonicals.Add(binding.InnerMaterialGUID);
			}
		}
	}
	return EBIMResult::Success;
}

/*
Given a preset ID, recurse through all its children and gather all other presets that this one depends on
*/
EBIMResult FBIMPresetCollection::GetAllDescendentPresets(const FGuid& PresetID, TSet<FGuid>& OutPresets) const
{
	if (!PresetID.IsValid())
	{
		return EBIMResult::Error;
	}
	TArray<FGuid> presetStack;
	TSet<FGuid> processed;
	presetStack.Push(PresetID);
	while (presetStack.Num() > 0)
	{
		FGuid presetID = presetStack.Pop();
		if (processed.Contains(presetID))
		{
			continue;
		}

		processed.Add(presetID);

		const FBIMPresetInstance* preset = PresetFromGUID(presetID);
		
		if (preset == nullptr)
		{
			continue;
		}
		
		if (presetID != PresetID && presetID.IsValid())
		{
			OutPresets.Add(VDPTable.TranslateToDerived(presetID));
		}
		
		for (auto &childNode : preset->ChildPresets)
		{
			presetStack.Push(childNode.PresetGUID);
		}

		if (preset->SlotConfigPresetGUID.IsValid())
		{
			presetStack.Push(preset->SlotConfigPresetGUID);
		}

		for (auto& part : preset->PartSlots)
		{
			if (part.PartPresetGUID.IsValid())
			{
				presetStack.Push(part.PartPresetGUID);
			}
		}

		preset->Properties_DEPRECATED.ForEachProperty([&](const FBIMPropertyKey& PropKey,const FString& Value) {
			FGuid guid;
			if (!Value.IsEmpty() && FGuid::Parse(Value, guid) && guid.IsValid())
			{
				presetStack.Push(guid);
			}
		});

		FBIMPresetMaterialBindingSet materialBindingSet;
		if (preset->TryGetCustomData(materialBindingSet))
		{
			for (auto& binding : materialBindingSet.MaterialBindings)
			{
				if (binding.SurfaceMaterialGUID.IsValid())
				{
					presetStack.Push(binding.SurfaceMaterialGUID);
				}
				if (binding.InnerMaterialGUID.IsValid())
				{
					presetStack.Push(binding.InnerMaterialGUID);
				}
			}
		}

		for (auto& kvp : preset->CustomDataByClassName)
		{
			TSharedPtr<FJsonObject> jsonObject;
			kvp.Value.GetJsonObject(jsonObject);
			GetAllChildGuidsFromJsonObject(jsonObject, presetStack);
		}

	}
	return EBIMResult::Success;
}

void FBIMPresetCollection::GetAllChildGuidsFromJsonObject(TSharedPtr<FJsonObject>& InJsonObject, TArray<FGuid>& OutGuids)
{
	for (auto& val : InJsonObject->Values)
	{
		if (val.Value->Type == EJson::String)
		{
			FGuid guid;
			if (!val.Value->AsString().IsEmpty() && FGuid::Parse(val.Value->AsString(), guid) && guid.IsValid())
			{
				OutGuids.Push(guid);
			}
		}
		else if (val.Value->Type == EJson::Object)
		{
			auto object = val.Value->AsObject();
			if (object.IsValid())
			{
				GetAllChildGuidsFromJsonObject(object, OutGuids);
			}
		}
	}
}

/*
* Given a PresetID, get all other presets that depend on it
*/
EBIMResult FBIMPresetCollection::GetAllAncestorPresets(const FGuid& PresetGUID, TSet<FGuid>& OutPresets) const
{
	for (auto& preset : PresetsByGUID)
	{
		TSet<FGuid> descendents;
		GetAllDescendentPresets(preset.Key, descendents);
		if (descendents.Contains(PresetGUID))
		{
			OutPresets.Add(preset.Key);
		}
	}
	return EBIMResult::Success;
}

EBIMResult FBIMPresetCollection::GetDescendentPresets(const FGuid& InPresetGUID, TSet<FGuid>& OutPresets) const
{
	const FBIMPresetInstance* preset = PresetFromGUID(InPresetGUID);
	if (!ensureAlways(preset != nullptr))
	{
		return EBIMResult::Error;
	}

	for (auto& childPreset : preset->ChildPresets)
	{
		OutPresets.Add(childPreset.PresetGUID);
	}
	for (auto& partSlot : preset->PartSlots)
	{
		OutPresets.Add(partSlot.PartPresetGUID);
	}

	return EBIMResult::Success;
}

EBIMResult FBIMPresetCollection::GetAncestorPresets(const FGuid& InPresetGUID, TSet<FGuid>& OutPresets) const
{
	for (auto& curPreset : PresetsByGUID)
	{
		for (auto& curChildPreset : curPreset.Value.ChildPresets)
		{
			if (curChildPreset.PresetGUID == InPresetGUID)
			{
				OutPresets.Add(curPreset.Value.GUID);
			}
		}
		for (auto& curPartSlot : curPreset.Value.PartSlots)
		{
			if (curPartSlot.PartPresetGUID == InPresetGUID)
			{
				OutPresets.Add(curPreset.Value.GUID);
			}
		}
	}
	return EBIMResult::Success;
}

EObjectType FBIMPresetCollection::GetPresetObjectType(const FGuid& PresetID) const
{
	const FBIMPresetInstance* preset = PresetFromGUID(PresetID);
	if (preset == nullptr)
	{
		return EObjectType::OTNone;
	}
	return preset->ObjectType;
}

EBIMResult FBIMPresetCollection::GetPresetsByPredicate(const TFunction<bool(const FBIMPresetInstance& Preset)>& Predicate, TArray<FGuid>& OutPresets) const
{
	for (auto& kvp : PresetsByGUID)
	{
		if (Predicate(kvp.Value))
		{
			OutPresets.Add(kvp.Value.GUID);
		}
	}
	return EBIMResult::Success;
}

EBIMResult FBIMPresetCollection::GetPresetsForSlot(const FGuid& SlotPresetGUID, TArray<FGuid>& OutPresets) const
{
	const FBIMPresetInstance* slotPreset = PresetFromGUID(SlotPresetGUID);
	if (slotPreset == nullptr)
	{
		return EBIMResult::Error;
	}

	FBIMSlot slotConfig;
	if (slotPreset->TryGetCustomData(slotConfig))
	{
		FBIMTagPath tagPath;
		EBIMResult res = tagPath.FromString(slotConfig.SupportedNCPs);
		if (res == EBIMResult::Success)
		{
			return GetPresetsByPredicate([tagPath](const FBIMPresetInstance& Preset)
			{
				return Preset.MyTagPath.MatchesExact(tagPath);
			}, OutPresets);
		}
		else
		{
			return res;
		}
	}
	return EBIMResult::Error;
}

EBIMResult FBIMPresetCollection::PostLoad()
{
	UsedGUIDs.Empty();
	AllNCPs.Empty();

	for (auto& kvp : PresetsByGUID)
	{
		UsedGUIDs.Add(kvp.Value.GUID);
		AllNCPs.Add(kvp.Value.MyTagPath);

		// Set up unpopulated slots of rigged assemblies
		if (kvp.Value.SlotConfigPresetGUID.IsValid())
		{
			const FBIMPresetInstance* slotConfig = PresetFromGUID(kvp.Value.SlotConfigPresetGUID);
			if (!ensureAlways(slotConfig != nullptr))
			{
				continue;
			}

			TArray<FBIMPresetPartSlot> EmptySlots;
			for (auto& configSlot : slotConfig->ChildPresets)
			{
				const FBIMPresetInstance* slotPreset = PresetFromGUID(configSlot.PresetGUID);
				if (!ensureAlways(slotPreset != nullptr))
				{
					continue;
				}

				FBIMPresetPartSlot* partSlot = kvp.Value.PartSlots.FindByPredicate([this, slotPreset](const FBIMPresetPartSlot& PartSlot)
				{
					if (PartSlot.SlotPresetGUID == VDPTable.TranslateToCanonical(slotPreset->GUID))
					{
						return true;
					}
					return false;
				});

				if (partSlot == nullptr)
				{
					FBIMPresetPartSlot& newSlot = EmptySlots.AddDefaulted_GetRef();
					newSlot.SlotPresetGUID = slotPreset->GUID;
				}
			}

			if (EmptySlots.Num() > 0)
			{
				kvp.Value.PartSlots.Append(EmptySlots);
			}
		}
	}

	return EBIMResult::Success;
}

EBIMResult FBIMPresetCollection::ProcessNamedDimensions(FBIMPresetInstance& Preset)
{
	if (Preset.NodeScope != EBIMValueScope::Part && !Preset.SlotConfigPresetGUID.IsValid())
	{
		return EBIMResult::Success;
	}
	
	FBIMDimensions dims;
	if (Preset.TryGetCustomData(dims))
	{
		dims.bHasDefaults = true;
		Preset.SetCustomData(dims);
	}
	
	return EBIMResult::Success;
}

EBIMResult FBIMPresetCollection::ProcessAllNamedDimensions()
{
	TArray<FGuid> namedDimensionPresets;

	// Parts may have unique values like "JambLeftSizeX" or "PeepholeDistanceZ"
	// If a part is expected to have one of these values but doesn't, we provide defaults here
	// TODO: typesafe NCPs...bare string for now
	EBIMResult res = EBIMResult::Success;
	if (ensureAlways(GetPresetsForNCP(FBIMTagPath(TEXT("NamedDimension")), namedDimensionPresets) == EBIMResult::Success))
	{
		for (auto& ndp : namedDimensionPresets)
		{
			const FBIMPresetInstance* preset = PresetFromGUID(ndp);

			if (ensureAlways(preset != nullptr))
			{
				FBIMNamedDimension namedDimension;
				if (preset->TryGetCustomData(namedDimension) && !namedDimension.DimensionKey.IsEmpty())
				{
					FPartNamedDimension& dimension = FBIMPartSlotSpec::NamedDimensionMap.Add(namedDimension.DimensionKey);

					dimension.DefaultValue = FModumateUnitValue::WorldCentimeters(namedDimension.DefaultValue);
					dimension.UIType = GetEnumValueByString<EPartSlotDimensionUIType>(namedDimension.UIGroup);
					dimension.DisplayName = FText::FromString(namedDimension.DisplayName);
					dimension.Description =  FText::FromString(namedDimension.Description);
				}
				else
				{
					res = EBIMResult::Error;
				}
				
			}
			else
			{
				res = EBIMResult::Error;
			}
		}
	}

	/*
	* Presets that either have or are themselves parts are given default values for named dimensions so they can be edited if visible
	*/
	TArray<FGuid> dimensionedPresets;
	GetPresetsByPredicate([](const FBIMPresetInstance& Preset)
	{
		return Preset.NodeScope == EBIMValueScope::Part || Preset.SlotConfigPresetGUID.IsValid();
	}
	, dimensionedPresets);
	for (auto& dimPreset : dimensionedPresets)
	{
		FBIMPresetInstance* preset = PresetFromGUID(dimPreset);
		if (ensureAlways(preset != nullptr))
		{
			ProcessNamedDimensions(*preset);
		}
	}

	return res;
}

EBIMResult FBIMPresetCollection::ProcessMeshReferences(FBIMPresetInstance& Preset)
{
	EBIMResult rtn =  EBIMResult::Success;
	
	FBIMMeshRef meshRef;
	if(Preset.TryGetCustomData(meshRef))
	{
		const FBIMPresetInstance* meshPreset = PresetFromGUID(meshRef.Source);
			
		if (ensure(meshPreset != nullptr))
		{
			FBIMDimensions meshDimensions;
			if (meshPreset->TryGetCustomData(meshDimensions))
			{
				FBIMDimensions presetDimensions;
				Preset.TryGetCustomData(presetDimensions);
				meshDimensions.ForEachDimension([&presetDimensions](const FName& DimKey, float Value) {
					// if we are not overriding the mesh dims on the preset, set it
					if (!presetDimensions.HasCustomDimension(DimKey))
					{
						presetDimensions.SetCustomDimension(DimKey, Value);
					}
				});
				Preset.SetCustomData(presetDimensions);
			}
		}
		else
		{
			rtn = EBIMResult::Error;
		}
	}
	
	return rtn;
}

/*
 *  After adding all of the presets and their dependants to the PresetCollection, we need to process the mesh references
 *  and add all dimension properties of a mesh to its parent if it is not already defined.
*/
EBIMResult FBIMPresetCollection::ProcessAllMeshReferences()
{
	EBIMResult rtn = EBIMResult::Success;
	
	for(auto& presetKVP : PresetsByGUID)
	{
		ProcessMeshReferences(presetKVP.Value);
	}

	return rtn;
}

EBIMResult FBIMPresetCollection::ProcessAllAssemblies()
{
	EBIMResult rtn = EBIMResult::Success;
	for(auto& presetKVP : PresetsByGUID)
	{
		auto assemblyResult = ProcessAssemblyForPreset(presetKVP.Value);
		if(!ensureAlways(assemblyResult == EBIMResult::Success))
		{
			rtn = assemblyResult;
			UE_LOG(LogTemp, Error, TEXT("Preset assembly processing error: %d"), assemblyResult);
		}
	}

	return rtn;
}

EBIMResult FBIMPresetCollection::ProcessAssemblyForPreset(FBIMPresetInstance& Preset)
{
	EBIMResult res = EBIMResult::Success;
	if (Preset.ObjectType != EObjectType::OTNone)
	{
		FBIMAssemblySpec outSpec;
		res = outSpec.FromPreset(FBIMPresetCollectionProxy(*this), Preset.GUID);
		if(res == EBIMResult::Success)
		{
			outSpec.ObjectType = Preset.ObjectType;	
			res = UpdateProjectAssembly(outSpec);
			if(res == EBIMResult::Success)
			{
				if (!DefaultAssembliesByObjectType.Contains(outSpec.ObjectType))
				{
					DefaultAssembliesByObjectType.Add(outSpec.ObjectType, outSpec);
				}		
			}
		}
	}
	
	return res;
}

EBIMResult FBIMPresetCollection::GetAvailableGUID(FGuid& OutGUID)
{
	int32 sanity = 0;
	static constexpr int32 sanityCheck = 1000;

	do {
		OutGUID = FGuid::NewGuid();
	} while (UsedGUIDs.Contains(OutGUID) && ++sanity < sanityCheck);

	if (sanity == sanityCheck)
	{
		return EBIMResult::Error;
	}
	UsedGUIDs.Add(OutGUID);
	return EBIMResult::Success;
}

bool FBIMPresetCollection::operator==(const FBIMPresetCollection& RHS) const
{
	if (PresetsByGUID.Num() != RHS.PresetsByGUID.Num())
	{
		return false;
	}

	for (const auto& kvp : RHS.PresetsByGUID)
	{
		const FBIMPresetInstance* preset = PresetFromGUID(kvp.Key);
		if (preset == nullptr || *preset != kvp.Value)
		{
			return false;
		}
	}

	return true;
}

bool FBIMPresetCollection::operator!=(const FBIMPresetCollection& RHS) const
{
	return !(*this == RHS);
}

EBIMResult FBIMPresetCollection::GetNCPForPreset(const FGuid& InPresetGUID, FBIMTagPath& OutNCP) const
{
	const FBIMPresetInstance* preset = PresetFromGUID(InPresetGUID);
	if (preset == nullptr)
	{
		return EBIMResult::Error;
	}
	OutNCP = preset->MyTagPath;
	return EBIMResult::Success;
}

EBIMResult FBIMPresetCollection::GetPresetsForNCP(const FBIMTagPath& InNCP, TArray<FGuid>& OutPresets, bool bExactMatch) const
{
	if (bExactMatch)
	{
		return GetPresetsByPredicate([InNCP](const FBIMPresetInstance& Preset)
			{
				return Preset.MyTagPath.Tags.Num() >= InNCP.Tags.Num() && Preset.MyTagPath.MatchesExact(InNCP);
			}, OutPresets);
	}
	else
	{
		return GetPresetsByPredicate([InNCP](const FBIMPresetInstance& Preset)
			{
				return Preset.MyTagPath.Tags.Num() >= InNCP.Tags.Num() && Preset.MyTagPath.MatchesPartial(InNCP);
			}, OutPresets);
	}
}

EBIMResult FBIMPresetCollection::GetNCPSubcategories(const FBIMTagPath& InNCP, TArray<FString>& OutSubcats) const
{
	for (auto& ncp : AllNCPs)
	{
		if (ncp.Tags.Num() > InNCP.Tags.Num() && ncp.MatchesPartial(InNCP))
		{
			OutSubcats.AddUnique(ncp.Tags[InNCP.Tags.Num()]);
		}
	}
	return EBIMResult::Success;
}

FBIMPresetInstance* FBIMPresetCollection::PresetFromGUID(const FGuid& InGUID)
{
	return PresetsByGUID.Find(VDPTable.TranslateToDerived(InGUID));	
}

const FBIMPresetInstance* FBIMPresetCollection::PresetFromGUID(const FGuid& InGUID) const
{
	return PresetsByGUID.Find(VDPTable.TranslateToDerived(InGUID));
}

EBIMResult FBIMPresetCollection::AddOrUpdatePreset(const FBIMPresetInstance& InPreset, FBIMPresetInstance& OutPreset)
{
	if(!InPreset.EnsurePresetIsValidForUse())
	{
		return EBIMResult::Error;
	}
	
	if(InPreset.Origination != EPresetOrigination::Invented)
	{
		//Update the VDP Table
		VDPTable.Add(InPreset.CanonicalBase, InPreset.GUID);
	}

	//Update the Presets Table
	PresetsByGUID.Add(InPreset.GUID, InPreset);
	OutPreset = PresetsByGUID[InPreset.GUID];
	
	return EBIMResult::Success;
}

EBIMResult FBIMPresetCollection::ProcessPreset(const FGuid& GUID)
{
	auto rtn = EBIMResult::Error;
	if (ensureAlways(GUID.IsValid() && PresetsByGUID.Contains(GUID)))
	{
		FBIMPresetInstance& inMap = PresetsByGUID[GUID];
		if(ensureAlways(inMap.Origination != EPresetOrigination::Canonical))
		{
			FString guidStr = GUID.ToString();
			
			if(!AddAssetsFromPreset(inMap))
			{
				UE_LOG(LogTemp, Warning, TEXT("Failed AddAssetsFromPreset: %s"), *guidStr);
				rtn = EBIMResult::Error;
			}
			else if(ProcessMeshReferences(inMap) != EBIMResult::Success)
			{
				UE_LOG(LogTemp, Warning, TEXT("Failed ProcessMeshReferences: %s"), *guidStr);
				rtn = EBIMResult::Error;
			}
			else if(ProcessNamedDimensions(inMap) != EBIMResult::Success)
			{
				UE_LOG(LogTemp, Warning, TEXT("Failed ProcessNamedDimensions: %s"), *guidStr);
				rtn = EBIMResult::Error;
			}
			else if(SetPartSizesFromMeshes(inMap) != EBIMResult::Success)
			{
				UE_LOG(LogTemp, Warning, TEXT("Failed SetPartSizesFromMeshes: %s"), *guidStr);
				rtn = EBIMResult::Error;
			}
			else if(ProcessAssemblyForPreset(inMap) != EBIMResult::Success)
			{
				UE_LOG(LogTemp, Warning, TEXT("Failed ProcessAssemblyForPreset: %s"), *guidStr);
				rtn = EBIMResult::Error;
			}
			else
			{
				rtn = EBIMResult::Success;
			}
		}
	}

	return rtn;
}

EBIMResult FBIMPresetCollection::RemovePreset(const FGuid& InGUID)
{
	FBIMPresetInstance* preset = PresetFromGUID(InGUID);
	if (preset != nullptr)
	{
		//TODO: Figure out how the UX works around removing VDPs and EDPs -JN
		VDPTable.Remove(InGUID);
		PresetsByGUID.Remove(InGUID);
		return EBIMResult::Success;
	}
	return EBIMResult::Error;
}

bool FBIMPresetCollection::GetAllPresetKeys(TArray<FGuid>& OutKeys) const
{
	PresetsByGUID.GetKeys(OutKeys);
	return true;
}

bool FBIMPresetCollection::ContainsNonCanon(const FGuid& Guid) const
{
	return PresetsByGUID.Contains(Guid);
}

TSharedPtr<FBIMPresetDelta> FBIMPresetCollection::MakeUpdateDelta(const FBIMPresetInstance& UpdatedPreset, UObject* AnalyticsWorldContextObject) const
{
	
	TSharedPtr<FBIMPresetDelta> presetDelta = MakeShared<FBIMPresetDelta>();
	
	presetDelta->NewState = UpdatedPreset;

	const FBIMPresetInstance* oldPreset = PresetFromGUID(UpdatedPreset.GUID);
	if (oldPreset != nullptr)
	{
		presetDelta->OldState = *oldPreset;
	}

	if (AnalyticsWorldContextObject)
	{
		UModumateAnalyticsStatics::RecordPresetUpdate(AnalyticsWorldContextObject, oldPreset);
	}
	return presetDelta;
}

TSharedPtr<FBIMPresetDelta> FBIMPresetCollection::MakeUpdateDelta(const FGuid& PresetID, UObject* AnalyticsWorldContextObject) const
{
	const FBIMPresetInstance* currentPreset = PresetFromGUID(PresetID);
	if (currentPreset == nullptr)
	{
		return nullptr;
	}

	return MakeUpdateDelta(*currentPreset, AnalyticsWorldContextObject);
}

TSharedPtr<FBIMPresetDelta> FBIMPresetCollection::MakeDuplicateDelta(const FGuid& OriginalID, FBIMPresetInstance& NewPreset, UObject* AnalyticsWorldContextObject)
{
	const FBIMPresetInstance* original = PresetFromGUID(OriginalID);
	if (!ensureAlways(original != nullptr))
	{
		return nullptr;
	}

	if (ensureAlways(!NewPreset.GUID.IsValid()))
	{
		NewPreset = *original;
		NewPreset.Origination = EPresetOrigination::Invented;
		NewPreset.ParentGUID = original->GUID;
		NewPreset.DisplayName = FText::Format(LOCTEXT("DisplayName", "Duplicate of {0}"), NewPreset.DisplayName);

		GetAvailableGUID(NewPreset.GUID);

		if (AnalyticsWorldContextObject)
		{
			UModumateAnalyticsStatics::RecordPresetDuplication(AnalyticsWorldContextObject, &NewPreset);
		}

		FBIMSymbolPresetData symbolData;
		if (NewPreset.TryGetCustomData(symbolData))
		{   // Wipe out equivalent MOI IDs in new Preset.
			for (auto& idSet : symbolData.EquivalentIDs)
			{
				idSet.Value.IDSet.Empty();
			}
			NewPreset.SetCustomData(symbolData);
		}

		return MakeUpdateDelta(NewPreset);
	}

	return nullptr;
}


TSharedPtr<FBIMPresetDelta> FBIMPresetCollection::MakeCreateNewDelta(FBIMPresetInstance& NewPreset, UObject* AnalyticsWorldContextObject)
{
		if (!NewPreset.GUID.IsValid())
		{
			GetAvailableGUID(NewPreset.GUID);
		}

		if (AnalyticsWorldContextObject)
		{
			UModumateAnalyticsStatics::RecordPresetCreation(AnalyticsWorldContextObject, &NewPreset);
		}

		return MakeUpdateDelta(NewPreset);
}

EBIMResult FBIMPresetCollection::ForEachPreset(const TFunction<void(const FBIMPresetInstance& Preset)>& Operation) const
{
	for (auto& kvp : PresetsByGUID)
	{
		Operation(kvp.Value);
	}
	return EBIMResult::Success;
}

EBIMResult FBIMPresetCollection::GetAvailablePresetsForSwap(const FGuid& ParentPresetID, const FGuid& PresetIDToSwap, TArray<FGuid>& OutAvailablePresets) const
{
	const FBIMPresetInstance* preset = PresetFromGUID(PresetIDToSwap);
	if (!ensureAlways(preset != nullptr))
	{
		return EBIMResult::Error;
	}

	return GetPresetsByPredicate([preset](const FBIMPresetInstance& Preset)
	{
		return Preset.MyTagPath.MatchesExact(preset->MyTagPath);
	}, OutAvailablePresets);

	return EBIMResult::Success;
}

EBIMResult FBIMPresetCollection::GetProjectAssembliesForObjectType(EObjectType ObjectType, TArray<FBIMAssemblySpec>& OutAssemblies) const
{
	const FAssemblyDataCollection* db = AssembliesByObjectType.Find(ObjectType);
	if (db == nullptr)
	{
		return EBIMResult::Success;
	}
	for (auto& kvp : db->DataMap)
	{
		OutAssemblies.Add(kvp.Value);
	}
	return EBIMResult::Success;
}

EBIMResult FBIMPresetCollection::RemoveProjectAssemblyForPreset(const FGuid& PresetID)
{
	FBIMAssemblySpec assembly;
	EObjectType objectType = GetPresetObjectType(PresetID);
	if (TryGetProjectAssemblyForPreset(objectType, PresetID, assembly))
	{
		FAssemblyDataCollection* db = AssembliesByObjectType.Find(objectType);
		if (ensureAlways(db != nullptr))
		{
			db->RemoveData(assembly);
			return EBIMResult::Success;
		}
	}
	return EBIMResult::Error;
}

EBIMResult FBIMPresetCollection::UpdateProjectAssembly(const FBIMAssemblySpec& Assembly)
{
	FAssemblyDataCollection& db = AssembliesByObjectType.FindOrAdd(Assembly.ObjectType);
	db.AddData(Assembly);
	return EBIMResult::Success;
}

bool FBIMPresetCollection::TryGetDefaultAssemblyForToolMode(EToolMode ToolMode, FBIMAssemblySpec& OutAssembly) const
{
	EObjectType objectType = UModumateTypeStatics::ObjectTypeFromToolMode(ToolMode);
	const FAssemblyDataCollection* db = AssembliesByObjectType.Find(objectType);
	if (db != nullptr && db->DataMap.Num() > 0)
	{
		auto iterator = db->DataMap.CreateConstIterator();
		OutAssembly = iterator->Value;
		return true;
	}
	return false;
}

bool FBIMPresetCollection::TryGetProjectAssemblyForPreset(EObjectType ObjectType, const FGuid& PresetID, FBIMAssemblySpec& OutAssembly) const
{
	const TModumateDataCollection<FBIMAssemblySpec>* db = AssembliesByObjectType.Find(ObjectType);

	// It's legal for an object database to not exist
	if (db == nullptr)
	{
		return false;
	}

	const FBIMAssemblySpec* presetAssembly = db->GetData(PresetID);

	// TODO: temp patch to support missing assemblies prior to assemblies being retired
	if (presetAssembly == nullptr)
	{
		presetAssembly = DefaultAssembliesByObjectType.Find(ObjectType);
	}

	if (presetAssembly != nullptr)
	{
		OutAssembly = *presetAssembly;
		return true;
	}
	return false;
}

const FBIMAssemblySpec* FBIMPresetCollection::GetAssemblyByGUID(EToolMode ToolMode, const FGuid& Key) const
{
	EObjectType objectType = UModumateTypeStatics::ObjectTypeFromToolMode(ToolMode);
	const FAssemblyDataCollection* db = AssembliesByObjectType.Find(objectType);
	if (db != nullptr)
	{
		return db->GetData(Key);
	}
	else
	{
		return nullptr;
	}
}

bool FBIMPresetCollection::ReadInitialPresets(const UModumateGameInstance* GameInstance)
{
	// KLUDGE: Static collections
	DefaultAssembliesByObjectType.Empty();
	AMeshes.Reset();

	auto world = GameInstance->GetWorld();
	if(ensure(world))
	{
		auto state = world->GetGameState<AEditModelGameState>();
		if(ensure(state))
		{
			FMOIDocumentRecord& docRecord = state->Document->GetLastSerializedRecord();
			return ReadPresetsFromDocRecord(DocVersion, docRecord, GameInstance);
		}
	}
	return false;
}

/**
 * Read and process all presets.
 * Steps:
 * 1. Get the Taxonomy from the cloud
 * 2. Get the document presets that were saved
 * 3. Get the initial presets from the cloud
 * 4. Merge both initial and document presets together, find what is missing
 * 5. Retrieve the missing presets
 * 6. Do post-processing on all presets
*/
bool FBIMPresetCollection::ReadPresetsFromDocRecord(int32 DocRecordVersion, FMOIDocumentRecord& DocRecord, const UModumateGameInstance* GameInstance)
{
	UE_LOG(LogTemp, Log, TEXT("Reading presets from document record"));
	*this = FBIMPresetCollection();
	
	// Parse the default material GUID
	FGuid::Parse(TEXT("09F17296-2023-944C-A1E7-EEDFE28680E9"), DefaultMaterialGUID);
	PopulateTaxonomyFromCloudSync(*this, GameInstance);
	DocRecord.PresetCollection.PresetTaxonomy = this->PresetTaxonomy;
	
	//Upgrade any presets in the document that need upgrading
	// Because upgrading may change other presets in the collection
	// and Adding presets is done by Value, we need to upgrade ALL
	// before adding to the collection.
	auto world = GameInstance->GetWorld();
	bool bShouldUpgradeData = false;
	if (world)
	{
		auto state = Cast<AEditModelGameState>(world->GetGameState());
		if(state)
		{
			bShouldUpgradeData = DocRecordVersion < DocVersion && !state->IsNetMode(NM_Client);
		}
	}

	//Only the server or standalone clients should upgrade the preset collection
	if(bShouldUpgradeData)
	{
		for (TTuple<FGuid, FBIMPresetInstance>& kvp : DocRecord.PresetCollection.PresetsByGUID)
		{
			kvp.Value.UpgradeData(DocRecord.PresetCollection, DocRecordVersion);
		}
		UpgradeDocRecord(DocRecordVersion, DocRecord, GameInstance);	
	}
	else if(!ensure(DocRecordVersion == DocVersion))
	{
		UE_LOG(LogTemp, Error, TEXT("Could not upgrade BIMPresetCollection"));
		return false;
	}
	
	FBIMPresetCollection docPresets = DocRecord.PresetCollection;
	
	//Read in VDP table -- This needs to occur before any presets are added.
	VDPTable = docPresets.VDPTable;

	auto count = 0;
	for (auto& kvp : docPresets.PresetsByGUID)
	{
		
		if (kvp.Value.Origination == EPresetOrigination::EditedDerived ||
		    kvp.Value.Origination == EPresetOrigination::Invented)
		{
			FBIMPresetInstance CollectionPreset;
			AddOrUpdatePreset(kvp.Value, CollectionPreset);	
		}
		else
		{
			count++;
		}
	}
	if(count > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Ignored %d presets while loading record"), count);
	}

	PopulateInitialCanonicalPresetsFromCloudSync(*this, GameInstance);
	
	//Find out which presets are missing
	TSet<FGuid> neededPresets;
	for (auto& derived : VDPTable.GetDerivedGUIDs())
	{
		if(!PresetsByGUID.Contains(derived))
		{
			neededPresets.Add(VDPTable.TranslateToCanonical(derived));
		}
	}

	//Query the web for said presets
	PopulateMissingCanonicalPresetsFromCloudSync(neededPresets, *this, GameInstance);
	
	//Populate sub-caches (AssemblyType based)
	for(auto& myPreset: PresetsByGUID)
	{
		//This does more than just populate the Mesh cache
		// so its order is important
		ensure(AddAssetsFromPreset(myPreset.Value));
	}

	// Do processing steps for presets en masse
	ProcessAllMeshReferences();
	ProcessAllNamedDimensions();
	SetAllPartSizesFromMeshes();
	ProcessAllAssemblies();
	UE_LOG(LogTemp, Log, TEXT("Done reading presets for document"));
	return PostLoad() == EBIMResult::Success;
}
bool FBIMPresetCollection::UpgradeMoiStateData(FMOIStateData& InOutMoi, const FBIMPresetCollection& OldCollection, const TMap<FGuid, FBIMPresetInstance>& CustomGuidMap, TSet<FGuid>& OutMissingCanonicals) const
{
	int32 count = 0;
	if(InOutMoi.AssemblyGUID.IsValid())
	{
		auto assemblyGuid = InOutMoi.AssemblyGUID;
		//Invented
		if(CustomGuidMap.Contains(assemblyGuid))
		{
			InOutMoi.AssemblyGUID = CustomGuidMap[assemblyGuid].GUID;
		}
		else if(!OldCollection.PresetsByGUID.Contains(assemblyGuid))
		{
			//Do we have it in our table/collection already?
			if(OldCollection.VDPTable.HasCanonical(assemblyGuid))
			{
				//If so, go ahead and correct it.
				auto vdp = OldCollection.VDPTable.TranslateToDerived(assemblyGuid);
				InOutMoi.AssemblyGUID = vdp;
			}
			else
			{
				//no? we will fix it later
				OutMissingCanonicals.Add(assemblyGuid);
				count++;
			}
		}
	}

	//Loop through custom data and pull in any guid's you find.
	switch (InOutMoi.ObjectType)
	{
	case EObjectType::OTTerrainMaterial:
		{
			FMOITerrainMaterialData terrainMaterial;
			InOutMoi.CustomData.LoadStructData(terrainMaterial);
			auto canonical = terrainMaterial.Material;
			if(CustomGuidMap.Contains(canonical))
			{
				terrainMaterial.Material = CustomGuidMap[canonical].GUID;
				InOutMoi.CustomData.SaveStructData(terrainMaterial);
			}
			else if(OldCollection.VDPTable.HasCanonical(canonical))
			{
				auto vdp = OldCollection.VDPTable.TranslateToDerived(canonical);
				ensure(OldCollection.PresetsByGUID.Contains(vdp));
				terrainMaterial.Material = vdp;
				InOutMoi.CustomData.SaveStructData(terrainMaterial);
			}
			else if(!canonical.IsValid())
			{
				UE_LOG(LogTemp, Warning, TEXT("OTTerrainMaterial has Invalid Material GUID"));
			}
			else
			{
				OutMissingCanonicals.Add(canonical);
				count++;
			}
			
			break;
		}
	default:
		break;
	}

	return count == 0;
}

bool FBIMPresetCollection::UpgradeDocRecord(int32 DocRecordVersion, FMOIDocumentRecord& DocRecord,
	const UModumateGameInstance* GameInstance)
{
	//Update VDP Table FIRST
	DocRecord.PresetCollection.VDPTable.UpgradeDocRecord(DocRecordVersion);
	
	// With version 24, all presets carry around what market (canonical) preset they were based on (if any)
	//  all previous bEdited presets become Invented presets. bEdited is deprecated, but we make
	//  the assumption that anything in the document is bEdited already.
	if(DocRecordVersion < 24)
	{
		FBIMPresetCollection& oldCollection = DocRecord.PresetCollection;
		
		//Replace GUIDs in the doc record
		TMap<FGuid, FBIMPresetInstance> oldToInvented;
		for(auto& kvp: oldCollection.PresetsByGUID)
		{
			auto& preset = kvp.Value;
			
			preset.Origination = EPresetOrigination::Invented;
			preset.CanonicalBase.Invalidate();
			FGuid oldGuid = preset.GUID;
			ensure(oldCollection.GetAvailableGUID(preset.GUID) == EBIMResult::Success);
			oldToInvented.Add(oldGuid, preset);
		}

		auto count = oldCollection.PresetsByGUID.Num();
		//Remove and re-add to fix key
		for(auto& kvp : oldToInvented)
		{
			ensure(oldCollection.RemovePreset(kvp.Key) != EBIMResult::Error);
			ensure(oldCollection.AddOrUpdatePreset(kvp.Value, kvp.Value) != EBIMResult::Error);
		}

		ensure(oldCollection.PresetsByGUID.Num() == count);

		//Get all the initial presets that are typical in post-24
		PopulateInitialCanonicalPresetsFromCloudSync(oldCollection, GameInstance);

		//Walk the object list, if an assembly is missing, we need to grab it as well.
		//Also, update any canonical references with vdps (if we have them from the inital
		// pull) or with Invented presets (if we changed them earlier).
		TSet<FGuid> missingCanonicalPresets;
		TArray<FMOIStateData*> fixMeAfterMissingPresetsArePopulated;
		for(FMOIStateData& objectData : DocRecord.ObjectData)
		{
			if(!UpgradeMoiStateData(objectData, oldCollection, oldToInvented, missingCanonicalPresets))
			{
				fixMeAfterMissingPresetsArePopulated.Add(&objectData);
			}
		}
		
		//Find any missing descendent presets, such as those that are downloaded
		// from the marketplace in 3.3 and assigned to a initial preset as a layer
		// or material or what-not.
		for(auto& oti: oldToInvented)
		{
			TSet<FGuid> descendants;
			oldCollection.GetDirectCanonicalDescendents(oti.Value.GUID, descendants);
			for(auto& canonical : descendants)
			{
				if(!oldCollection.VDPTable.HasCanonical(canonical) && !missingCanonicalPresets.Contains(canonical))
				{
					missingCanonicalPresets.Add(canonical);
				}
			}
		}

		//We should have a complete list of what is missing. Go get it.
		PopulateMissingCanonicalPresetsFromCloudSync(missingCanonicalPresets, oldCollection, GameInstance);

		//Walk all the previous objects that we didn't have assembly presets for
		// and fix them to point to the vdps we just got.
		for(auto dataPtr : fixMeAfterMissingPresetsArePopulated)
		{
			missingCanonicalPresets.Empty();
			if(ensure(UpgradeMoiStateData(*dataPtr, oldCollection, oldToInvented, missingCanonicalPresets)))
			{
				if(dataPtr->AssemblyGUID.IsValid())
				{
					ensure(oldCollection.ContainsNonCanon(dataPtr->AssemblyGUID));	
				}
			}
		}

		for(auto& preset : oldCollection.PresetsByGUID)
		{
			for(auto& oti : oldToInvented)
			{
				if(preset.Value.ReplaceImmediateChildGuid(oti.Key, oti.Value.GUID))
				{
					ensure(oldCollection.PresetsByGUID.Contains(oti.Value.GUID));
					if(preset.Value.Origination == EPresetOrigination::VanillaDerived)
					{
						preset.Value.Origination = EPresetOrigination::EditedDerived;
					}
				}
			}
		}
	}
	
	return true;
}

bool FBIMPresetCollection::SavePresetsToDocRecord(FMOIDocumentRecord& DocRecord) const
{
	DocRecord.PresetCollection.VDPTable = VDPTable;
	
	for (auto& kvp : PresetsByGUID)
	{
		//Save all presets in our Document that are Edited (Vanilla Edited) or Invented
		if (kvp.Value.Origination == EPresetOrigination::Invented ||
			kvp.Value.Origination == EPresetOrigination::EditedDerived)
		{

			//The document should never duplicate keys. This is an error condition -JN
			if(ensure(!DocRecord.PresetCollection.PresetsByGUID.Contains(kvp.Key)))
			{
				auto serializedPreset = kvp.Value;

				for (auto& customData : serializedPreset.CustomDataByClassName)
				{
					customData.Value.SaveJsonFromCbor();
				}

				FBIMPresetInstance CollectionPreset;
				DocRecord.PresetCollection.AddOrUpdatePreset(serializedPreset, CollectionPreset);
			}
		}
	}
	return true;
}

EBIMResult FBIMPresetCollection::GetBlankPresetForNCP(FBIMTagPath TagPath, FBIMPresetInstance& OutPreset) const
{
	OutPreset.MyTagPath = TagPath;
	OutPreset.Origination = EPresetOrigination::Invented;
	OutPreset.GUID.Invalidate();
	
	// get ncp node and if it does not exist we need to exit. Can't create preset without a valid ncp
	FBIMPresetTaxonomyNode ncpNode;
	if (PresetTaxonomy.GetExactMatch(TagPath, ncpNode) == EBIMResult::Error)
	{
		return EBIMResult::Error;
	}
	
	// Populate default values, such as assetType, ObjectType
	if (PresetTaxonomy.PopulateDefaults(OutPreset) == EBIMResult::Error)
	{
		return EBIMResult::Error;
	}
	
	return EBIMResult::Success;
}

EBIMResult FBIMPresetCollection::MakeDeleteDeltas(const FGuid& DeleteGUID, const FGuid& ReplacementGUID, TArray<FDeltaPtr>& OutDeltas, UObject* AnalyticsWorldContextObject)
{
	const FBIMPresetInstance* presetToDelete = PresetFromGUID(DeleteGUID);

	if (!ensureAlways(presetToDelete != nullptr))
	{
		return EBIMResult::Error;
	}

	TSet<FGuid> ancestors;
	if (ReplacementGUID.IsValid() && GetAncestorPresets(DeleteGUID,ancestors) == EBIMResult::Success)
	{
		for (auto& ancestor : ancestors)
		{
			const FBIMPresetInstance* presetInstance = PresetFromGUID(ancestor);
			if (ensureAlways(presetInstance != nullptr))
			{
				TSharedPtr<FBIMPresetDelta> presetDelta = MakeShared<FBIMPresetDelta>();
				OutDeltas.Add(presetDelta);
				presetDelta->OldState = *presetInstance;
				presetDelta->NewState = *presetInstance;
				for (auto& child : presetDelta->NewState.ChildPresets)
				{
					if (child.PresetGUID == DeleteGUID)
					{
						child.PresetGUID = ReplacementGUID;
					}
				}
				for (auto& part : presetDelta->NewState.PartSlots)
				{
					if (part.PartPresetGUID == DeleteGUID)
					{
						part.PartPresetGUID = ReplacementGUID;
					}
				}
			}
		}
	}

	TSharedPtr<FBIMPresetDelta> presetDelta = MakeShared<FBIMPresetDelta>();
	OutDeltas.Add(presetDelta);
	presetDelta->OldState = *presetToDelete;

	return EBIMResult::Success;
}

EBIMResult FBIMPresetCollection::MakeNewPresetFromDatasmith(const FString& NewPresetName, const FGuid& ArchitecturalMeshID, FGuid& OutPresetID)
{
	const FBIMAssemblySpec* originalPresetAssembly = DefaultAssembliesByObjectType.Find(EObjectType::OTFurniture);
	const FBIMPresetInstance* original = PresetFromGUID(originalPresetAssembly->UniqueKey());
	if (!ensureAlways(original != nullptr))
	{
		return EBIMResult::Error;
	}

	FBIMPresetInstance NewPreset = *original;
	NewPreset.DisplayName = FText::Format(LOCTEXT("DisplayName", "Datasmith from {0}"), FText::FromString(NewPresetName));
	
	FBIMMeshRef meshRef;
	meshRef.Source = ArchitecturalMeshID;
	NewPreset.SetCustomData(meshRef);

	if (OutPresetID.IsValid())
	{
		NewPreset.GUID = OutPresetID;
	}
	else
	{
		GetAvailableGUID(NewPreset.GUID);
		OutPresetID = NewPreset.GUID;
	}
	AddOrUpdatePreset(NewPreset, NewPreset);

	FAssemblyDataCollection& db = AssembliesByObjectType.FindOrAdd(EObjectType::OTFurniture);
	FBIMAssemblySpec newSpec;
	if (newSpec.FromPreset(FBIMPresetCollectionProxy(*this), NewPreset.GUID) == EBIMResult::Success)
	{
		db.AddData(newSpec);
		return EBIMResult::Success;
	}

	return EBIMResult::Error;
}


FBIMPresetCollectionProxy::FBIMPresetCollectionProxy(const FBIMPresetCollection& InCollection) : BaseCollection(&InCollection)
{}

FBIMPresetCollectionProxy::FBIMPresetCollectionProxy()
{}

const FBIMPresetInstance* FBIMPresetCollectionProxy::PresetFromGUID(const FGuid& InGUID) const
{
	FGuid presetGUID = BaseCollection->VDPTable.TranslateToDerived(InGUID);
	const FBIMPresetInstance* preset = OverriddenPresets.Find(presetGUID);
	
	if (preset == nullptr && BaseCollection != nullptr)
	{
		preset = BaseCollection->PresetFromGUID(presetGUID);
	}
	return preset;
}

EBIMResult FBIMPresetCollectionProxy::OverridePreset(const FBIMPresetInstance& PresetInstance)
{
	ensure(PresetInstance.Origination != EPresetOrigination::Canonical);
	OverriddenPresets.Add(PresetInstance.GUID, PresetInstance);
	return EBIMResult::Success;
}

const FBIMAssemblySpec* FBIMPresetCollectionProxy::AssemblySpecFromGUID(EObjectType ObjectType, const FGuid& InGUID) const
{
	if (BaseCollection != nullptr)
	{
		const FAssemblyDataCollection* db = BaseCollection->AssembliesByObjectType.Find(ObjectType);
		if (db != nullptr)
		{
			return db->GetData(InGUID);
		}
	}
	return nullptr;
}

EBIMResult FBIMPresetCollectionProxy::CreateAssemblyFromLayerPreset(const FGuid& LayerPresetKey, EObjectType ObjectType, FBIMAssemblySpec& OutAssemblySpec)
{

	// Build a temporary top-level assembly preset to host the single layer
	FBIMPresetInstance assemblyPreset;
	assemblyPreset.GUID = FGuid::NewGuid();
	assemblyPreset.NodeScope = EBIMValueScope::Assembly;

	// Give the temporary assembly a single layer child
	FBIMPresetPinAttachment& attachment = assemblyPreset.ChildPresets.AddDefaulted_GetRef();
	attachment.ParentPinSetIndex = 0;
	attachment.ParentPinSetPosition = 0;
	attachment.PresetGUID = LayerPresetKey;
	assemblyPreset.ObjectType = ObjectType;

	// Add the temp assembly and layer presets
	OverridePreset(assemblyPreset);

	return OutAssemblySpec.FromPreset(*this, assemblyPreset.GUID);
}


EBIMResult FBIMPresetCollection::SetPartSizesFromMeshes(FBIMPresetInstance& Preset)
{
	auto checkPartSize = [](FBIMPresetInstance& Preset, const FBIMNameType& PartField, const float NativeSize)
	{
		float v = 0.0f;
			
		FBIMDimensions presetDimensions;
		if (Preset.TryGetCustomData(presetDimensions))
		{
			presetDimensions.TryGetDimension(PartField, v);	
		}

		// The property will always be present but will be 0 if it hasn't been set
		if (v == 0.0f)
		{
			v = NativeSize;
			presetDimensions.SetCustomDimension(PartField, v);
			Preset.SetCustomData(presetDimensions);
		}
	};
	
	FBIMMeshRef meshRef;
	if (!Preset.TryGetCustomData(meshRef))
	{
		return EBIMResult::Success;
	}
	auto* meshPreset = PresetFromGUID(meshRef.Source);
	if (!meshPreset)
	{
		return EBIMResult::Error;
	}

	FBIMMesh meshConfig;
	if (!meshPreset->TryGetCustomData(meshConfig))
	{
		return EBIMResult::Error;
	}

	checkPartSize(Preset, FBIMNameType(FBIMPartLayout::PartSizeX), meshConfig.NativeSizeX);
	checkPartSize(Preset, FBIMNameType(FBIMPartLayout::PartSizeY), meshConfig.NativeSizeY);
	checkPartSize(Preset, FBIMNameType(FBIMPartLayout::PartSizeZ), meshConfig.NativeSizeZ);
	
	return EBIMResult::Success;
}

EBIMResult FBIMPresetCollection::SetAllPartSizesFromMeshes()
{
	/*
	* Each Part preset has PartSize(X,Y,Z) properties, set in the data tables
	* If a Part preset does not define its own size, it gets 0 from the named dimension default
	* If a Part preset has 0 in a size dimension, retrieve the native size from the associated mesh
	*/
	TArray<FGuid> partPresets;
	if (ensureAlways(GetPresetsForNCP(FBIMTagPath(TEXT("Part")), partPresets) == EBIMResult::Success))
	{
		for (auto guid : partPresets)
		{
			auto preset = PresetFromGUID(guid);
			if (preset != nullptr)
			{
				SetPartSizesFromMeshes(*preset);
			}
		}
	}	
	return EBIMResult::Success;
}

EBIMResult FBIMPresetCollection::GetWebPresets(FBIMWebPresetCollection& OutPresets, UWorld* World)
{
	for (auto& kvp : PresetsByGUID)
	{
		FBIMWebPreset& webPreset = OutPresets.presets.Add(kvp.Key);
		kvp.Value.ToWebPreset(webPreset, World);
	}

	OutPresets.ncpTaxonomy = PresetTaxonomy.nodes;

	return EBIMResult::Success;
}

bool FBIMPresetCollection::AddMeshFromPreset(const FBIMPresetInstance& Preset) const
{
	FBIMMesh meshConfig;

	if (ensureAlways(Preset.TryGetCustomData(meshConfig)) && ensureAlways(meshConfig.AssetPath.Len() > 0))
	{
		FVector presetNativeSize(meshConfig.NativeSizeX, meshConfig.NativeSizeY, meshConfig.NativeSizeZ);
		FVector meshNativeSize = presetNativeSize * UModumateDimensionStatics::CentimetersToInches;

		FVector presetNineSliceMin(meshConfig.SliceX1, meshConfig.SliceY1, meshConfig.SliceZ1);
		FVector presetNineSliceMax(meshConfig.SliceX2, meshConfig.SliceY2, meshConfig.SliceZ2);
		FBox meshNineSlice = FBox(
			presetNineSliceMin * UModumateDimensionStatics::CentimetersToInches,
			presetNineSliceMax * UModumateDimensionStatics::CentimetersToInches);
		
		if (meshConfig.AssetPath.StartsWith(TEXT("http")))
		{
			AddArchitecturalMeshFromDatasmith(meshConfig.AssetPath, Preset.GUID);
		}
		else
		{
			AddArchitecturalMesh(Preset.GUID, Preset.DisplayName.ToString(), meshNativeSize, meshNineSlice, meshConfig.AssetPath);
		}
	}
	return true;
}

bool FBIMPresetCollection::AddRawMaterialFromPreset(const FBIMPresetInstance& Preset) const
{
	FBIMRawMaterial rawMaterialData;
	if (ensure(Preset.TryGetCustomData(rawMaterialData)) && ensure(rawMaterialData.AssetPath.Len() != 0))
	{
		AddArchitecturalMaterial(Preset.GUID, Preset.DisplayName.ToString(), rawMaterialData.AssetPath);
		return true;
	}
	return false;
}

bool FBIMPresetCollection::AddMaterialFromPreset(const FBIMPresetInstance& Preset) const
{
	FGuid rawMaterial;

	for (auto& cp : Preset.ChildPresets)
	{
		const FBIMPresetInstance* childPreset = PresetFromGUID(cp.PresetGUID);
		if (childPreset != nullptr)
		{
			if (childPreset->NodeScope == EBIMValueScope::RawMaterial)
			{
				rawMaterial = cp.PresetGUID;
			}
		}
	}

	if (ensure(rawMaterial.IsValid()))
	{
		const FBIMPresetInstance* preset = PresetFromGUID(rawMaterial);
		if (preset != nullptr)
		{
			FBIMRawMaterial rawMaterialConfig;
			if (ensure(Preset.TryGetCustomData(rawMaterialConfig)) && !Preset.DisplayName.IsEmpty())
			{
				AddArchitecturalMaterial(Preset.GUID, Preset.DisplayName.ToString(), rawMaterialConfig.AssetPath);
				return true;
			}
		}
	}
	return false;
}

bool FBIMPresetCollection::AddLightFromPreset(const FBIMPresetInstance& Preset) const
{
	FArchitecturalLight light;
	light.Key = Preset.GUID;
	light.DisplayName = Preset.DisplayName;

	FBIMIESProfile iesProfileConfig;
	if (ensure(Preset.TryGetCustomData(iesProfileConfig)))
	{
		light.IconPath = FName(iesProfileConfig.CraftingIconAssetFilePath);
		light.ProfilePath = FName(iesProfileConfig.AssetPath);
		//fetch IES resource from profile path
		//fill in light configuration
		Lights.AddData(light);	
	}

	return true;
}

bool FBIMPresetCollection::AddProfileFromPreset(const FBIMPresetInstance& Preset) const
{
	FBIMProfile profileConfig;
	if (ensure(Preset.TryGetCustomData(profileConfig)))
	{
		if (profileConfig.AssetPath.Len() != 0 && ensure(!Preset.DisplayName.IsEmpty()))
		{
			AddSimpleMesh(Preset.GUID, Preset.DisplayName.ToString(), profileConfig.AssetPath);
            return true;
		}
	}
	return false;
}

bool FBIMPresetCollection::AddPatternFromPreset(const FBIMPresetInstance& Preset) const
{
	FLayerPattern newPattern;
	newPattern.InitFromCraftingPreset(Preset);
	Patterns.AddData(newPattern);

	FBIMPatternConfig patterneConfig;
	if (Preset.TryGetCustomData(patterneConfig) && !patterneConfig.CraftingIconAssetFilePath.IsEmpty())
	{
		AddStaticIconTexture(Preset.GUID, Preset.DisplayName.ToString(), patterneConfig.CraftingIconAssetFilePath);
		return true;
	}
	return false;
}

void FBIMPresetCollection::AddArchitecturalMesh(const FGuid& Key, const FString& Name, const FVector& InNativeSize, const FBox& InNineSliceBox, const FSoftObjectPath& AssetPath)
{
	if (!ensureAlways(AssetPath.IsAsset() && AssetPath.IsValid()))
	{
		return;
	}

	FArchitecturalMesh mesh;
	mesh.AssetPath = AssetPath;
	mesh.NativeSize = InNativeSize;
	mesh.NineSliceBox = InNineSliceBox;
	mesh.Key = Key;

	mesh.EngineMesh = Cast<UStaticMesh>(AssetPath.TryLoad());
	if (ensureAlways(mesh.EngineMesh.IsValid()))
	{
		mesh.EngineMesh->AddToRoot();
	}

	AMeshes.AddData(mesh);
}

void FBIMPresetCollection::AddArchitecturalMaterial(const FGuid& Key, const FString& Name, const FSoftObjectPath& AssetPath)
{
	if (!ensureAlways(AssetPath.IsAsset() && AssetPath.IsValid()))
	{
		return;
	}

	FArchitecturalMaterial mat;
	mat.Key = Key;
	mat.DisplayName = FText::FromString(Name);

	mat.EngineMaterial = Cast<UMaterialInterface>(AssetPath.TryLoad());
	if (ensureAlways(mat.EngineMaterial.IsValid()))
	{
		mat.EngineMaterial->AddToRoot();
	}

	mat.UVScaleFactor = 1;
	AMaterials.AddData(mat);
}

void FBIMPresetCollection::AddSimpleMesh(const FGuid& Key, const FString& Name, const FSoftObjectPath& AssetPath)
{
	if (!ensureAlways(AssetPath.IsAsset() && AssetPath.IsValid()))
	{
		return;
	}

	FSimpleMeshRef mesh;

	mesh.Key = Key;
	mesh.AssetPath = AssetPath;

	mesh.Asset = Cast<USimpleMeshData>(AssetPath.TryLoad());
	if (ensureAlways(mesh.Asset.IsValid()))
	{
		mesh.Asset->AddToRoot();
	}

	SimpleMeshes.AddData(mesh);
}

/*
* TODO: Refactor for FArchive binary format
*/

void FBIMPresetCollection::AddStaticIconTexture(const FGuid& Key, const FString& Name, const FSoftObjectPath& AssetPath)
{
	if (!ensureAlways(AssetPath.IsAsset() && AssetPath.IsValid()))
	{
		return;
	}

	FStaticIconTexture staticTexture;

	staticTexture.Key = Key;
	staticTexture.DisplayName = FText::FromString(Name);
	staticTexture.Texture = Cast<UTexture2D>(AssetPath.TryLoad());
	if (staticTexture.Texture.IsValid())
	{
		staticTexture.Texture->AddToRoot();
		StaticIconTextures.AddData(staticTexture);
	}
}

void FBIMPresetCollection::AddArchitecturalMeshFromDatasmith(const FString& AssetUrl, const FGuid& InArchitecturalMeshKey)
{
	FArchitecturalMesh mesh;
	//mesh.NativeSize = InNativeSize;
	//mesh.NineSliceBox = InNineSliceBox;
	mesh.Key = InArchitecturalMeshKey;
	mesh.DatasmithUrl = AssetUrl;

	AMeshes.AddData(mesh);
}

bool FBIMPresetCollection::AddAssetsFromPreset(const FBIMPresetInstance& Preset) const
{
	switch (Preset.AssetType)
	{
	case EBIMAssetType::IESProfile:
		AddLightFromPreset(Preset);
		break;
	case EBIMAssetType::Mesh:
		AddMeshFromPreset(Preset);
		break;
	case EBIMAssetType::Profile:
		AddProfileFromPreset(Preset);
		break;
	case EBIMAssetType::RawMaterial:
		AddRawMaterialFromPreset(Preset);
		break;
	case EBIMAssetType::Material:
		AddMaterialFromPreset(Preset);
		break;
	case EBIMAssetType::Pattern:
		AddPatternFromPreset(Preset);
		break;
	default:
		break;
	};

	return true;
}

// Data Access
const FArchitecturalMesh* FBIMPresetCollection::GetArchitecturalMeshByGUID(const FGuid& Key) const
{
	return AMeshes.GetData(VDPTable.TranslateToDerived(Key));
}

const FArchitecturalMaterial* FBIMPresetCollection::GetArchitecturalMaterialByGUID(const FGuid& Key) const
{
	return AMaterials.GetData(VDPTable.TranslateToDerived(Key));
}

const FSimpleMeshRef* FBIMPresetCollection::GetSimpleMeshByGUID(const FGuid& Key) const
{
	return SimpleMeshes.GetData(VDPTable.TranslateToDerived(Key));
}

const FStaticIconTexture* FBIMPresetCollection::GetStaticIconTextureByGUID(const FGuid& Key) const
{
	return StaticIconTextures.GetData(VDPTable.TranslateToDerived(Key));
}

const FLayerPattern* FBIMPresetCollection::GetPatternByGUID(const FGuid& Key) const
{
	return Patterns.GetData(VDPTable.TranslateToDerived(Key));
}

const FArchitecturalMesh* FBIMPresetCollectionProxy::GetArchitecturalMeshByGUID(const FGuid& InGUID) const
{
	if (BaseCollection)
	{
		return BaseCollection->GetArchitecturalMeshByGUID(InGUID);
	}
	return nullptr;
}

const FLayerPattern* FBIMPresetCollectionProxy::GetPatternByGUID(const FGuid& Key) const
{
	if (BaseCollection)
	{
		return BaseCollection->GetPatternByGUID(Key);
	}
	return nullptr;
}

const FArchitecturalMaterial* FBIMPresetCollectionProxy::GetArchitecturalMaterialByGUID(const FGuid& Key) const
{
	if (BaseCollection)
	{
		return BaseCollection->GetArchitecturalMaterialByGUID(Key);
	}
	return nullptr;
}

const FSimpleMeshRef* FBIMPresetCollectionProxy::GetSimpleMeshByGUID(const FGuid& Key) const
{
	if (BaseCollection)
	{
		return BaseCollection->GetSimpleMeshByGUID(Key);
	}
	return nullptr;
}

FGuid FBIMPresetCollectionProxy::GetDefaultMaterialGUID() const
{
	if (BaseCollection)
	{
		return BaseCollection->DefaultMaterialGUID;
	}
	return FGuid();
}

void FBIMPresetCollection::PopulateTaxonomyFromCloudSync(FBIMPresetCollection& Collection,
	const UModumateGameInstance* GameInstance)
{
	auto cloud = GameInstance->GetCloudConnection();
	
	const auto* projectSettings = GetDefault<UGeneralProjectSettings>();
	FString currentVersion = projectSettings->ProjectVersion;
	
	FString endpoint = TEXT("/assets/ncps?version=") + currentVersion;
	cloud->RequestEndpoint(endpoint, FModumateCloudConnection::Get,
		//Customizer
		[](TSharedRef<IHttpRequest, ESPMode::ThreadSafe>& RefRequest)
		{ },
		//Success
		[&](bool bSuccessful, const TSharedPtr<FJsonObject>& Response)
		{
			if (bSuccessful && Response.IsValid())
			{ 
				Collection.PresetTaxonomy.FromWebTaxonomy(Response);
			}
		},
		//Error
		[](int32 ErrorCode, const FString& ErrorString) 
		{
			UE_LOG(LogTemp, Error, TEXT("Preset Request Status Error: %s"), *ErrorString);		
		}
	, true, true);
}

void FBIMPresetCollection::PopulateInitialCanonicalPresetsFromCloudSync(FBIMPresetCollection& Collection,
	const UModumateGameInstance* GameInstance)
{
	auto cloud = GameInstance->GetCloudConnection();

	const auto* projectSettings = GetDefault<UGeneralProjectSettings>();
	FString currentVersion = projectSettings->ProjectVersion;
	//TODO: Fix endpoint for startsInProject to cache so this doesn't take ~10 seconds to load
	FString endpoint = TEXT("/assets/presets?StartsInProject=1&version=") + currentVersion;
	cloud->RequestEndpoint(endpoint, FModumateCloudConnection::Get,
		//Customizer
		[](TSharedRef<IHttpRequest, ESPMode::ThreadSafe>& RefRequest)
		{ },
		//Success
		[&](bool bSuccessful, const TSharedPtr<FJsonObject>& Response)
		{
			if (bSuccessful && Response.IsValid())
			{ 
				ProcessCloudCanonicalPreset(Response, Collection, GameInstance);
			}
		},
		//Error
		[](int32 ErrorCode, const FString& ErrorString) 
		{
			UE_LOG(LogTemp, Error, TEXT("Preset Request Status Error: %s"), *ErrorString);		
		}
	, true, true);

	UE_LOG(LogTemp, Log, TEXT("Initial Preset Request Completed"));
}

bool FBIMPresetCollection::PopulateMissingCanonicalPresetsFromCloudSync(const TSet<FGuid> Presets,
	FBIMPresetCollection& Collection, const UModumateGameInstance* GameInstance)
{
	const int32 MAX_URL_SIZE = 2048;
	
	if(Presets.Num() == 0) return true;
	
	FString endpoint = TEXT("/assets/presets?guid=");
	if(endpoint.Len() >= MAX_URL_SIZE)
	{
		//TODO: Split preset list in 2 and recurse. -JN
		UE_LOG(LogTemp, Error, TEXT("Too many missing presets, count=%d"), Presets.Num());
		return false;
	}
	
	for (auto& preset :Presets)
	{
		endpoint.Append(preset.ToString() + TEXT(","));
	}
	endpoint = endpoint.TrimChar(',');

	const auto* projectSettings = GetDefault<UGeneralProjectSettings>();
	FString currentVersion = projectSettings->ProjectVersion;
	endpoint.Append(TEXT("&version=") + currentVersion);
	
	auto cloud = GameInstance->GetCloudConnection();
	cloud->RequestEndpoint(endpoint, FModumateCloudConnection::Get,
		//Customizer
		[](TSharedRef<IHttpRequest, ESPMode::ThreadSafe>& RefRequest)
		{ },
		//Success
		[&](bool bSuccessful, const TSharedPtr<FJsonObject>& Response)
		{
			if (bSuccessful && Response.IsValid())
			{
				ProcessCloudCanonicalPreset(Response, Collection, GameInstance);
			}
		},
		//Error
		[&](int32 ErrorCode, const FString& ErrorString) 
		{
			UE_LOG(LogTemp, Error, TEXT("Preset Request Status Error: %s"), *ErrorString);
		}
	, true, true);
	
	UE_LOG(LogTemp, Log, TEXT("Mssing Preset Request Completed"));
	return true;
}

/**
 * @brief Translates a JsonObject in to a preset and adds it to collection. 
 * @details This is used to add presets from the cloud on the initial connection to the project.
 * If a VDPTable entry already exists for a canonical it will use the derived GUID the table. If that derived guid
 * already exists in the collection, we do not add anything and this call becomes a no-op.
 * @param JsonObject The object that contains the preset 
 * @param Collection The Collection to add to
 * @param GameInstance The current Game instance
 **/
void FBIMPresetCollection::ProcessCloudCanonicalPreset(TSharedPtr<FJsonObject> JsonObject, FBIMPresetCollection& Collection, const UModumateGameInstance* GameInstance)
{
	TMap<FString, TSharedPtr<FJsonValue>> topLevelEntries = JsonObject->Values;
	for(const auto& entry: topLevelEntries)
	{
		FBIMPresetInstance canonicalPreset;
		FBIMWebPreset webPreset;
		TSharedPtr<FJsonObject> objPreset = entry.Value->AsObject();
		FJsonObjectConverter::JsonObjectToUStruct<FBIMWebPreset>(objPreset.ToSharedRef(), &webPreset);
		webPreset.origination = EPresetOrigination::Canonical;
		webPreset.canonicalBase.Invalidate();
		
		if(ensure(canonicalPreset.FromWebPreset(webPreset, GameInstance->GetWorld()) != EBIMResult::Error))
		{
			if(ensure(canonicalPreset.GUID.IsValid()))
			{
				FBIMPresetInstance vdp = canonicalPreset;
				vdp.GUID = FGuid::NewGuid();
				vdp.CanonicalBase = canonicalPreset.GUID;
				vdp.Origination = EPresetOrigination::VanillaDerived;
			
				if(Collection.VDPTable.HasCanonical(canonicalPreset.GUID))
				{
					//Use the existing GUID
					auto derived = Collection.VDPTable.TranslateToDerived(canonicalPreset.GUID);
					vdp.GUID = derived;
				}

				//If the GUID exists already, don't add it -- it would replace it and we
				// would lose any user edits. This is ONLY for processing cloud presets
				if(!Collection.ContainsNonCanon(vdp.GUID))
				{
					Collection.AddOrUpdatePreset(vdp, vdp);
				}
			}
		}
	}
}

const FVDPTable* FBIMPresetCollectionProxy::GetVDPTable() const
{
	if(ensureAlways(BaseCollection))
	{
		return &BaseCollection->VDPTable;
	}
	return nullptr;
}

#undef LOCTEXT_NAMESPACE

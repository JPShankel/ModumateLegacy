// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/Presets/BIMPresetCollection.h"

#include "BIMKernel/AssemblySpec/BIMAssemblySpec.h"
#include "BIMKernel/Presets/BIMCSVReader.h"
#include "BIMKernel/Presets/BIMPresetDocumentDelta.h"
#include "BIMKernel/Presets/BIMSymbolPresetData.h"

#include "DocumentManagement/ModumateSerialization.h"
#include "ModumateCore/ModumateScriptProcessor.h"
#include "Online/ModumateAnalyticsStatics.h"
#include "BIMKernel/AssemblySpec/BIMPartLayout.h"
#include "DocumentManagement/ModumateDocument.h"
#include "ModumateCore/EnumHelpers.h"
#include "Objects/ModumateObjectStatics.h"
#include "Engine/AssetManager.h"

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


/*
Given a preset ID, recurse through all its children and gather all other presets that this one depends on
*/
EBIMResult FBIMPresetCollection::GetAllDescendentPresets(const FGuid& PresetID, TArray<FGuid>& OutPresets) const
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
			OutPresets.AddUnique(presetID);
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

		preset->Properties.ForEachProperty([&presetStack](const FBIMPropertyKey& PropKey,const FString& Value) {
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

	}
	return EBIMResult::Success;
}

/*
* Given a PresetID, get all other presets that depend on it
*/
EBIMResult FBIMPresetCollection::GetAllAncestorPresets(const FGuid& PresetGUID, TArray<FGuid>& OutPresets) const
{
	for (auto& preset : PresetsByGUID)
	{
		TArray<FGuid> descendents;
		GetAllDescendentPresets(preset.Key, descendents);
		if (descendents.Contains(PresetGUID))
		{
			OutPresets.Add(preset.Key);
		}
	}
	return EBIMResult::Success;
}

EBIMResult FBIMPresetCollection::GetDescendentPresets(const FGuid& InPresetGUID, TArray<FGuid>& OutPresets) const
{
	const FBIMPresetInstance* preset = PresetFromGUID(InPresetGUID);
	if (!ensureAlways(preset != nullptr))
	{
		return EBIMResult::Error;
	}

	for (auto& childPreset : preset->ChildPresets)
	{
		OutPresets.AddUnique(childPreset.PresetGUID);
	}
	for (auto& partSlot : preset->PartSlots)
	{
		OutPresets.AddUnique(partSlot.PartPresetGUID);
	}

	return EBIMResult::Success;
}

EBIMResult FBIMPresetCollection::GetAncestorPresets(const FGuid& InPresetGUID, TArray<FGuid>& OutPresets) const
{
	for (auto& curPreset : PresetsByGUID)
	{
		for (auto& curChildPreset : curPreset.Value.ChildPresets)
		{
			if (curChildPreset.PresetGUID == InPresetGUID)
			{
				OutPresets.AddUnique(curPreset.Value.GUID);
			}
		}
		for (auto& curPartSlot : curPreset.Value.PartSlots)
		{
			if (curPartSlot.PartPresetGUID == InPresetGUID)
			{
				OutPresets.AddUnique(curPreset.Value.GUID);
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

	FString slotPathNCP;
	if (slotPreset->Properties.TryGetProperty(EBIMValueScope::Slot, BIMPropertyNames::SupportedNCPs, slotPathNCP))
	{
		FBIMTagPath tagPath;
		EBIMResult res = tagPath.FromString(slotPathNCP);
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


// TODO: Loading data from a CSV manifest file is not going to be required long term. 
// Ultimately we will develop a compiler from this code that generates a record that can be read more efficiently
// Once this compiler is in the shape we intend, we will determine where in the toolchain this code will reside we can refactor for long term sustainability
// Until then this is a prototypical development space used to prototype the relational database structure being authored in Excel
EBIMResult FBIMPresetCollection::LoadCSVManifest(const FString& ManifestPath, const FString& ManifestFile, TArray<FGuid>& OutStarters, TArray<FString>& OutMessages)
{
	FModumateCSVScriptProcessor processor;

	static const FName kTypeName = TEXT("TYPENAME");
	static const FName kProperty = TEXT("PROPERTY");
	static const FName kTagPaths = TEXT("TAGPATHS");
	static const FName kPreset = TEXT("PRESET");
	static const FName kInputPin = TEXT("INPUTPIN");
	static const FName kFormElement = TEXT("FORMELEMENT");
	static const FName kString = TEXT("String");
	static const FName kDimension = TEXT("Dimension");

	FBIMCSVReader tableData;
	tableData.KeyGuidMap.Empty();
	tableData.GuidKeyMap.Empty();

	processor.AddRule(kFormElement, [&OutMessages, &tableData](const TArray<const TCHAR*> &Row, int32 RowNumber)
	{
		ensureAlways(tableData.ProcessFormElementRow(Row, RowNumber, OutMessages) == EBIMResult::Success);
	});

	processor.AddRule(kTypeName, [&OutMessages, &tableData](const TArray<const TCHAR*> &Row, int32 RowNumber)
	{
		ensureAlways(tableData.ProcessNodeTypeRow(Row, RowNumber, OutMessages) == EBIMResult::Success);
	});

	processor.AddRule(kProperty, [&OutMessages, &tableData](const TArray<const TCHAR*> &Row, int32 RowNumber)
	{
		ensureAlways(tableData.ProcessPropertyDeclarationRow(Row, RowNumber, OutMessages) == EBIMResult::Success);
	});

	processor.AddRule(kTagPaths, [&OutMessages, &tableData, this](const TArray<const TCHAR*> &Row, int32 RowNumber)
	{
		if (tableData.NodeType.TypeName.IsNone())
		{
			OutMessages.Add(FString::Printf(TEXT("No node definition")));
		}

		// tableData.NodeType may be updated by the tag path row, which contains matrices that can add to the form template
		if (ensureAlways(tableData.ProcessTagPathRow(Row, RowNumber, OutMessages) == EBIMResult::Success))
		{
			NodeDescriptors.Add(tableData.NodeType.TypeName, tableData.NodeType);
		}
	});

	processor.AddRule(kPreset, [&OutMessages, &OutStarters, &tableData, &ManifestFile, this](const TArray<const TCHAR*> &Row, int32 RowNumber)
	{
		ensureAlways(tableData.ProcessPresetRow(Row, RowNumber, *this, OutStarters, OutMessages) == EBIMResult::Success);
	});

	processor.AddRule(kInputPin, [&OutMessages, &tableData](const TArray<const TCHAR*> &Row, int32 RowNumber)
	{
		ensureAlways(tableData.ProcessInputPinRow(Row, RowNumber, OutMessages) == EBIMResult::Success);
	});

	TArray<FString> fileList;
	TMap<FBIMKey, FGuid> keyGUIDMap;
	if (FFileHelper::LoadFileToStringArray(fileList, *(ManifestPath / ManifestFile)))
	{
		for (auto &file : fileList)
		{
			// add a blank line in manifest to halt processing
			if (file.IsEmpty())
			{
				break;
			}

			if (file[0] == TCHAR(';'))
			{
				continue;
			}

			tableData = FBIMCSVReader();
			tableData.CurrentFile = file;

			if (!processor.ParseFile(*(ManifestPath / file)))
			{
				OutMessages.Add(FString::Printf(TEXT("Failed to load CSV file %s"), *file));
				return EBIMResult::Error;
			}

			// Make sure the last preset we were reading ends up in the map
			if (tableData.Preset.GUID.IsValid())
			{
				tableData.Preset.TryGetProperty(BIMPropertyNames::Name, tableData.Preset.DisplayName);
				AddPreset(tableData.Preset);
				FBIMCSVReader::KeyGuidMap.Add(tableData.Preset.PresetID, tableData.Preset.GUID);
				FBIMCSVReader::GuidKeyMap.Add(tableData.Preset.GUID, tableData.Preset.PresetID);
			}
		}
		PostLoad();
		return EBIMResult::Success;
	}
	OutMessages.Add(FString::Printf(TEXT("Failed to load manifest file %s"), *ManifestFile));
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
		
		FBIMPresetTypeDefinition* typeDef = NodeDescriptors.Find(kvp.Value.NodeType);
		if (ensureAlways(typeDef != nullptr))
		{
			kvp.Value.TypeDefinition = *typeDef;
		}

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

				FBIMPresetPartSlot* partSlot = kvp.Value.PartSlots.FindByPredicate([slotPreset](const FBIMPresetPartSlot& PartSlot)
				{
					if (PartSlot.SlotPresetGUID == slotPreset->GUID)
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

		TArray<FBIMPresetTaxonomyNode> taxonomyNodes;
		// Get metadata from NCP taxonomy...partial matches are in order from specific to general
		if (PresetTaxonomy.GetAllPartialMatches(kvp.Value.MyTagPath, taxonomyNodes) == EBIMResult::Success)
		{
			for (auto& node : taxonomyNodes)
			{
				if (kvp.Value.CategoryTitle.IsEmpty())
				{
					kvp.Value.CategoryTitle = node.DesignerTitle;
				}

				if (kvp.Value.MeasurementMethod == EPresetMeasurementMethod::None)
				{
					kvp.Value.MeasurementMethod = node.MeasurementMethod;
				}

				if (kvp.Value.ObjectType == EObjectType::OTNone)
				{
					kvp.Value.ObjectType = node.ObjectType;
				}

				if (kvp.Value.DisplayName.IsEmpty())
				{
					kvp.Value.DisplayName = node.DisplayName;
				}

				if (kvp.Value.AssetType == EBIMAssetType::None)
				{
					kvp.Value.AssetType = node.AssetType;
				}
			}
		}
	}

	return ProcessNamedDimensions();
}

EBIMResult FBIMPresetCollection::ProcessNamedDimensions()
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
				FPartNamedDimension& dimension = FBIMPartSlotSpec::NamedDimensionMap.Add(preset->PresetID.ToString());

				float numProp;
				if (preset->TryGetProperty(BIMPropertyNames::DefaultValue, numProp))
				{
					dimension.DefaultValue = FModumateUnitValue::WorldCentimeters(numProp);
				}
				else
				{
					res = EBIMResult::Error;
				}

				FString stringProp;
				if (ensureAlways(preset->TryGetProperty(BIMPropertyNames::UIGroup, stringProp)))
				{
					dimension.UIType = GetEnumValueByString<EPartSlotDimensionUIType>(stringProp);
				}
				else
				{
					res = EBIMResult::Error;
				}

				if (ensureAlways(preset->TryGetProperty(BIMPropertyNames::DisplayName, stringProp)))
				{
					dimension.DisplayName = FText::FromString(stringProp);
				}
				else
				{
					res = EBIMResult::Error;
				}

				if (ensureAlways(preset->TryGetProperty(BIMPropertyNames::Description, stringProp)))
				{
					dimension.Description = FText::FromString(stringProp);
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
			for (auto& kvp : FBIMPartSlotSpec::NamedDimensionMap)
			{
				FBIMPropertyKey propKey(EBIMValueScope::Dimension, *kvp.Key);
				if (!preset->Properties.HasProperty<float>(propKey.Scope, propKey.Name))
				{
					preset->Properties.SetProperty(propKey.Scope, propKey.Name, kvp.Value.DefaultValue.AsWorldCentimeters());
				}
			}
		}
	}

	return res;
}

EBIMResult FBIMPresetCollection::ProcessStarterAssemblies(const TArray<FGuid>& StarterPresets)
{
	/*
	Now build every preset that was returned in 'starters' by the manifest parse and add it to the project
	*/
	EBIMResult res = EBIMResult::Success;
	for (auto& starter : StarterPresets)
	{
		const FBIMPresetInstance* preset = PresetFromGUID(starter);

		// TODO: "starter" presets currently only refer to complete assemblies, will eventually include presets to be shopped from the marketplace
		if (ensureAlways(preset != nullptr) && preset->ObjectType == EObjectType::OTNone)
		{
			continue;
		}

		FBIMAssemblySpec outSpec;
		
		EBIMResult fromRes = outSpec.FromPreset(FBIMPresetCollectionProxy(*this), preset->GUID);
		if (fromRes != EBIMResult::Success)
		{
			res = fromRes;
		}
		
		outSpec.ObjectType = preset->ObjectType;
		UpdateProjectAssembly(outSpec);

		if (!DefaultAssembliesByObjectType.Contains(outSpec.ObjectType))
		{
			DefaultAssembliesByObjectType.Add(outSpec.ObjectType, outSpec);
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

EBIMResult FBIMPresetCollection::GenerateBIMKeyForPreset(const FGuid& PresetID, FBIMKey& OutKey) const
{
	OutKey = FBIMKey();

	TArray<FGuid> idStack;
	idStack.Push(PresetID);

	FString returnKey;

	while (idStack.Num() > 0)
	{
		FGuid id = idStack.Pop();
		const FBIMPresetInstance* preset = PresetFromGUID(id);
		if (!ensureAlways(preset != nullptr))
		{
			return EBIMResult::Error;
		}

		FString key;
		preset->MyTagPath.ToString(key);
		returnKey.Append(key);

		FString name;
		if (preset->TryGetProperty(BIMPropertyNames::Name, name))
		{
			returnKey.Append(name);
		}

		for (auto& childPreset : preset->ChildPresets)
		{
			idStack.Push(childPreset.PresetGUID);
		}

		for (auto& part : preset->PartSlots)
		{
			if (!part.PartPreset.IsNone())
			{
				idStack.Push(part.PartPresetGUID);
			}
		}
	}

	if (returnKey.IsEmpty())
	{
		return EBIMResult::Error;
	}

	returnKey.RemoveSpacesInline();

	OutKey = FBIMKey(returnKey);

	return EBIMResult::Success;
}

bool FBIMPresetCollection::operator==(const FBIMPresetCollection& RHS) const
{
	if (NodeDescriptors.Num() != RHS.NodeDescriptors.Num())
	{
		return false;
	}

	for (const auto& kvp : RHS.NodeDescriptors)
	{
		const FBIMPresetTypeDefinition* typeDef = NodeDescriptors.Find(kvp.Key);
		if (typeDef == nullptr || *typeDef != kvp.Value)
		{
			return false;
		}
	}

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
	return PresetsByGUID.Find(InGUID);
}

const FBIMPresetInstance* FBIMPresetCollection::PresetFromGUID(const FGuid& InGUID) const
{
	return PresetsByGUID.Find(InGUID);
}

EBIMResult FBIMPresetCollection::AddPreset(const FBIMPresetInstance& InPreset)
{
	if (ensureAlways(InPreset.GUID.IsValid()))
	{
		PresetsByGUID.Add(InPreset.GUID, InPreset);
		return EBIMResult::Success;
	}
	return EBIMResult::Error;
}

EBIMResult FBIMPresetCollection::RemovePreset(const FGuid& InGUID)
{
	FBIMPresetInstance* preset = PresetFromGUID(InGUID);
	if (preset != nullptr)
	{
		PresetsByGUID.Remove(preset->GUID);
		return EBIMResult::Success;
	}
	return EBIMResult::Error;
}

TSharedPtr<FBIMPresetDelta> FBIMPresetCollection::MakeUpdateDelta(const FBIMPresetInstance& UpdatedPreset, UObject* AnalyticsWorldContextObject) const
{
	TSharedPtr<FBIMPresetDelta> presetDelta = MakeShared<FBIMPresetDelta>();

	presetDelta->NewState = UpdatedPreset;
	presetDelta->NewState.bEdited = true;
	const FBIMPresetInstance* oldPreset = PresetsByGUID.Find(UpdatedPreset.GUID);
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
	const FBIMPresetInstance* currentPreset = PresetsByGUID.Find(PresetID);
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
		NewPreset.ParentGUID = original->GUID;
		NewPreset = *original;
		NewPreset.DisplayName = FText::Format(LOCTEXT("DisplayName", "Duplicate of {0}"), NewPreset.DisplayName);
		NewPreset.Properties.SetProperty(NewPreset.NodeScope, BIMPropertyNames::Name, NewPreset.DisplayName.ToString());

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

bool FBIMPresetCollection::ReadPresetsFromDocRecord(int32 DocRecordVersion, const FMOIDocumentRecord& DocRecord, const FBIMPresetCollection& UntruncatedCollection)
{
	*this = UntruncatedCollection;

	FBIMPresetCollection docPresets = DocRecord.PresetCollection;

	// Presets in the doc were edited by definition
	// If bEdited comes in false for a custom preset for any reason, it won't get saved
	TArray<FGuid> starters;
	for (auto& kvp : docPresets.PresetsByGUID)
	{
		kvp.Value.bEdited = true;
		AddPreset(kvp.Value);
		if (kvp.Value.ObjectType != EObjectType::OTNone)
		{
			starters.AddUnique(kvp.Key);
		}
	}

	for (auto& kvp : DocRecord.PresetCollection.NodeDescriptors)
	{
		if (!NodeDescriptors.Contains(kvp.Key))
		{
			NodeDescriptors.Add(kvp.Key, kvp.Value);
		}
	}

	FBIMPresetCollectionProxy proxyCollection(*this);

	for (auto& kvp : docPresets.PresetsByGUID)
	{
		proxyCollection.OverridePreset(kvp.Value);
	}

	for (auto& kvp : docPresets.PresetsByGUID)
	{
		kvp.Value.UpgradeData(proxyCollection, DocRecordVersion);
	}

	PresetsByGUID.Append(docPresets.PresetsByGUID);

	for (auto& kvp : PresetsByGUID)
	{
		if (kvp.Value.ObjectType != EObjectType::OTNone)
		{
			starters.AddUnique(kvp.Key);
		}
	}

	ProcessStarterAssemblies(starters);

	SetPartSizesFromMeshes();
	
	// If any presets fail to build their assembly, remove them
	// They'll be replaced by the fallback system and will not be written out again
	TSet<FGuid> incompletePresets;
	for (auto& kvp : PresetsByGUID)
	{
		if (kvp.Value.ObjectType != EObjectType::OTNone)
		{
			FAssemblyDataCollection& db = AssembliesByObjectType.FindOrAdd(kvp.Value.ObjectType);
			FBIMAssemblySpec newSpec;
			if (newSpec.FromPreset(FBIMPresetCollectionProxy(*this), kvp.Value.GUID) == EBIMResult::Success)
			{
				db.AddData(newSpec);
			}
			else
			{
				incompletePresets.Add(kvp.Value.GUID);
			}
		}
	}

	for (auto& incompletePreset : incompletePresets)
	{
		RemovePreset(incompletePreset);
	}

	return PostLoad() == EBIMResult::Success;
}

bool FBIMPresetCollection::SavePresetsToDocRecord(FMOIDocumentRecord& DocRecord) const
{
	for (auto& kvp : PresetsByGUID)
	{
		// Only save presets that have been edited, the rest are in the shared db
		if (kvp.Value.bEdited)
		{
			auto serializedPreset = kvp.Value;

			for (auto& customData : serializedPreset.CustomDataByClassName)
			{
				customData.Value.SaveJsonFromCbor();
			}

			DocRecord.PresetCollection.AddPreset(serializedPreset);
			if (!DocRecord.PresetCollection.NodeDescriptors.Contains(kvp.Value.NodeType))
			{
				const FBIMPresetTypeDefinition* typeDef = NodeDescriptors.Find(kvp.Value.NodeType);
				if (ensureAlways(typeDef != nullptr))
				{
					DocRecord.PresetCollection.NodeDescriptors.Add(kvp.Value.NodeType, *typeDef);
				}
			};
		}
	}

	return true;
}

EBIMResult FBIMPresetCollection::GetBlankPresetForObjectType(EObjectType ObjectType, FBIMPresetInstance& OutPreset) const
{
	TArray<FGuid> guids;
	GetPresetsByPredicate([ObjectType](const FBIMPresetInstance& Preset) {return Preset.ObjectType == ObjectType; },guids);

	// TODO: ensure the first preset is a reasonable blank
	if (guids.Num() > 0)
	{
		const FBIMPresetInstance* preset = PresetFromGUID(guids[0]);
		if (preset != nullptr)
		{
			OutPreset = *preset;
			OutPreset.GUID.Invalidate();
			OutPreset.bEdited = true;
			return EBIMResult::Success;
		}
	}
	return EBIMResult::Error;
}

EBIMResult FBIMPresetCollection::MakeDeleteDeltas(const FGuid& DeleteGUID, const FGuid& ReplacementGUID, TArray<FDeltaPtr>& OutDeltas, UObject* AnalyticsWorldContextObject)
{
	const FBIMPresetInstance* presetToDelete = PresetFromGUID(DeleteGUID);

	if (!ensureAlways(presetToDelete != nullptr))
	{
		return EBIMResult::Error;
	}

	TArray<FGuid> ancestors;
	if (ReplacementGUID.IsValid() && GetAncestorPresets(DeleteGUID,ancestors) == EBIMResult::Success)
	{
		FBIMKey replacementBIMKey;
		const FBIMPresetInstance* replacementInst = PresetFromGUID(ReplacementGUID);
		if (ensureAlways(replacementInst != nullptr))
		{
			replacementBIMKey = replacementInst->PresetID;
		}

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
						part.PartPreset = replacementBIMKey;
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
	NewPreset.Properties.SetProperty(NewPreset.NodeScope, BIMPropertyNames::Name, NewPreset.DisplayName.ToString());
	NewPreset.Properties.SetProperty(EBIMValueScope::Mesh, BIMPropertyNames::AssetID, ArchitecturalMeshID.ToString());

	if (OutPresetID.IsValid())
	{
		NewPreset.GUID = OutPresetID;
	}
	else
	{
		GetAvailableGUID(NewPreset.GUID);
		OutPresetID = NewPreset.GUID;
	}
	AddPreset(NewPreset);

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
	const FBIMPresetInstance* preset = OverriddenPresets.Find(InGUID);
	if (preset == nullptr && BaseCollection != nullptr)
	{
		preset = BaseCollection->PresetFromGUID(InGUID);
	}
	return preset;
}

EBIMResult FBIMPresetCollectionProxy::OverridePreset(const FBIMPresetInstance& PresetInstance)
{
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
	assemblyPreset.PresetID = FBIMKey(TEXT("TempIconPreset"));
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

EBIMResult FBIMPresetCollection::SetPartSizesFromMeshes()
{
	/*
	* Each Part preset has PartSize(X,Y,Z) properties, set in the data tables
	* If a Part preset does not define its own size, it gets 0 from the named dimension default
	* If a Part preset has 0 in a size dimension, retrieve the native size from the associated mesh
	*/
	TArray<FGuid> partPresets;
	if (ensureAlways(GetPresetsForNCP(FBIMTagPath(TEXT("Part")), partPresets) == EBIMResult::Success))
	{
		auto checkPartSize = [](FBIMPresetInstance* Preset, const FBIMPresetInstance* MeshPreset, const FBIMNameType& PartField, const FBIMNameType& MeshField, const FText& DisplayName)
		{
			float v = 0.0f;
			Preset->Properties.TryGetProperty(EBIMValueScope::Dimension, PartField, v);

			// The property will always be present but will be 0 if it hasn't been set
			if (v == 0.0f)
			{
				MeshPreset->Properties.TryGetProperty(EBIMValueScope::Mesh, MeshField, v);
				Preset->Properties.SetProperty(EBIMValueScope::Dimension, PartField, v);
				FBIMPropertyKey propKey(EBIMValueScope::Dimension, PartField);
				// If size dimensions are specified in the part table, the form will already be built, otherwise build it here
				if (!Preset->PresetForm.HasField(propKey.QN()))
				{
					Preset->PresetForm.AddPropertyElement(DisplayName, propKey.QN(), EBIMPresetEditorField::DimensionProperty);
				}
			}
		};

		for (auto guid : partPresets)
		{
			auto* preset = PresetFromGUID(guid);
			FGuid assetGUID;
			if (!preset || !preset->Properties.TryGetProperty(EBIMValueScope::Mesh, TEXT("AssetID"), assetGUID))
			{
				continue;
			}
			auto* meshPreset = PresetFromGUID(assetGUID);
			if (!meshPreset)
			{
				continue;
			}

			checkPartSize(preset, meshPreset, FBIMNameType(FBIMPartLayout::PartSizeX), FBIMNameType(FBIMPartLayout::NativeSizeX), FText::FromString(TEXT("Part Size X")));
			checkPartSize(preset, meshPreset, FBIMNameType(FBIMPartLayout::PartSizeY), FBIMNameType(FBIMPartLayout::NativeSizeY), FText::FromString(TEXT("Part Size Y")));
			checkPartSize(preset, meshPreset, FBIMNameType(FBIMPartLayout::PartSizeZ), FBIMNameType(FBIMPartLayout::NativeSizeZ), FText::FromString(TEXT("Part Size Z")));
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

	OutPresets.ncpTaxonomy = PresetTaxonomy.Nodes;

	return EBIMResult::Success;
}

bool FBIMPresetCollection::AddMeshFromPreset(const FBIMPresetInstance& Preset) const
{
	FString assetPath = Preset.GetScopedProperty<FString>(EBIMValueScope::Mesh, BIMPropertyNames::AssetPath);

	if (ensureAlways(assetPath.Len() > 0))
	{
		FVector meshNativeSize(ForceInitToZero);
		FBox meshNineSlice(ForceInitToZero);

		FVector presetNativeSize(ForceInitToZero);
		if (Preset.Properties.TryGetProperty(EBIMValueScope::Mesh, TEXT("NativeSizeX"), presetNativeSize.X) &&
			Preset.Properties.TryGetProperty(EBIMValueScope::Mesh, TEXT("NativeSizeY"), presetNativeSize.Y) &&
			Preset.Properties.TryGetProperty(EBIMValueScope::Mesh, TEXT("NativeSizeZ"), presetNativeSize.Z))
		{
			meshNativeSize = presetNativeSize * UModumateDimensionStatics::CentimetersToInches;
		}

		FVector presetNineSliceMin(ForceInitToZero), presetNineSliceMax(ForceInitToZero);
		if (Preset.Properties.TryGetProperty(EBIMValueScope::Mesh, TEXT("SliceX1"), presetNineSliceMin.X) &&
			Preset.Properties.TryGetProperty(EBIMValueScope::Mesh, TEXT("SliceY1"), presetNineSliceMin.Y) &&
			Preset.Properties.TryGetProperty(EBIMValueScope::Mesh, TEXT("SliceZ1"), presetNineSliceMin.Z) &&
			Preset.Properties.TryGetProperty(EBIMValueScope::Mesh, TEXT("SliceX2"), presetNineSliceMax.X) &&
			Preset.Properties.TryGetProperty(EBIMValueScope::Mesh, TEXT("SliceY2"), presetNineSliceMax.Y) &&
			Preset.Properties.TryGetProperty(EBIMValueScope::Mesh, TEXT("SliceZ2"), presetNineSliceMax.Z))
		{
			meshNineSlice = FBox(
				presetNineSliceMin * UModumateDimensionStatics::CentimetersToInches,
				presetNineSliceMax * UModumateDimensionStatics::CentimetersToInches);
		}

		FString name;
		Preset.TryGetProperty(BIMPropertyNames::Name, name);
		if (assetPath.StartsWith(TEXT("http")))
		{
			AddArchitecturalMeshFromDatasmith(assetPath, Preset.GUID);
		}
		else
		{
			AddArchitecturalMesh(Preset.GUID, name, meshNativeSize, meshNineSlice, assetPath);
		}
	}
	return true;
}

bool FBIMPresetCollection::AddRawMaterialFromPreset(const FBIMPresetInstance& Preset) const
{
	FString assetPath;
	Preset.TryGetProperty(BIMPropertyNames::AssetPath, assetPath);
	if (assetPath.Len() != 0)
	{
		FString matName;
		if (ensureAlways(Preset.TryGetProperty(BIMPropertyNames::Name, matName)))
		{
			AddArchitecturalMaterial(Preset.GUID, matName, assetPath);
			return true;
		}
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

	if (rawMaterial.IsValid())
	{
		const FBIMPresetInstance* preset = PresetFromGUID(rawMaterial);
		if (preset != nullptr)
		{
			FString assetPath, matName;
			if (ensureAlways(preset->TryGetProperty(BIMPropertyNames::AssetPath, assetPath)
				&& Preset.TryGetProperty(BIMPropertyNames::Name, matName)))
			{
				AddArchitecturalMaterial(Preset.GUID, matName, assetPath);
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

	Preset.TryGetProperty(BIMPropertyNames::Name, light.DisplayName);
	Preset.TryGetProperty(BIMPropertyNames::CraftingIconAssetFilePath, light.IconPath);
	Preset.TryGetProperty(BIMPropertyNames::AssetPath, light.ProfilePath);
	//fetch IES resource from profile path
	//fill in light configuration
	Lights.AddData(light);

	return true;
}

bool FBIMPresetCollection::AddProfileFromPreset(const FBIMPresetInstance& Preset) const
{
	FString assetPath;
	Preset.TryGetProperty(BIMPropertyNames::AssetPath, assetPath);
	if (assetPath.Len() != 0)
	{
		FString name;
		if (ensureAlways(Preset.TryGetProperty(BIMPropertyNames::Name, name)))
		{
			AddSimpleMesh(Preset.GUID, name, assetPath);
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

	FString assetPath;
	Preset.TryGetProperty(BIMPropertyNames::CraftingIconAssetFilePath, assetPath);
	if (assetPath.Len() > 0)
	{
		FString iconName;
		if (Preset.TryGetProperty(BIMPropertyNames::Name, iconName))
		{
			AddStaticIconTexture(Preset.GUID, iconName, assetPath);
			return true;
		}
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

static constexpr TCHAR BIMCacheRecordField[] = TEXT("BIMCacheRecord");
static constexpr TCHAR BIMManifestFileName[] = TEXT("BIMManifest.txt");
static constexpr TCHAR BIMNCPFileName[] = TEXT("NCPTable.csv");

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
	};

	return true;
}

void FBIMPresetCollection::ReadPresetData(bool bTruncate)
{
	*this = FBIMPresetCollection();
	// Make sure abstract material is in for default use
	FGuid::Parse(TEXT("09F17296-2023-944C-A1E7-EEDFE28680E9"), DefaultMaterialGUID);

	TArray<FGuid> starters;
	TArray<FString> errors;

	ManifestDirectoryPath = FPaths::ProjectContentDir() / TEXT("NonUAssets") / TEXT("BIMData");

	if (!ensure(LoadCSVManifest(*ManifestDirectoryPath, BIMManifestFileName, starters, errors) == EBIMResult::Success))
	{
		return;
	}

	if (!ensure(errors.Num() == 0))
	{
		return;
	}

	FString NCPString;
	FString NCPPath = FPaths::Combine(*ManifestDirectoryPath, BIMNCPFileName);
	if (!ensure(FFileHelper::LoadFileToString(NCPString, *NCPPath)))
	{
		return;
	}

	FCsvParser NCPParsed(NCPString);
	const FCsvParser::FRows& NCPRows = NCPParsed.GetRows();

	if (ensure(PresetTaxonomy.LoadCSVRows(NCPRows) == EBIMResult::Success))
	{
		ensure(PostLoad() == EBIMResult::Success);
	}

	// If this preset implies an asset type, load it
	for (auto& preset : PresetsByGUID)
	{
		if (preset.Value.ObjectType != EObjectType::OTNone)
		{
			starters.AddUnique(preset.Key);
		}

		FLightConfiguration lightConfig;
		if (preset.Value.TryGetCustomData(lightConfig))
		{
			const FBIMPresetInstance* bimPresetInstance = PresetsByGUID.Find(lightConfig.IESProfileGUID);
			if (bimPresetInstance != nullptr)
			{
				FString assetPath = bimPresetInstance->GetScopedProperty<FString>(EBIMValueScope::IESProfile, BIMPropertyNames::AssetPath);
				FSoftObjectPath referencePath = FString(TEXT("TextureLightProfile'")).Append(assetPath).Append(TEXT("'"));
				TSharedPtr<FStreamableHandle> SyncStreamableHandle = UAssetManager::GetStreamableManager().RequestSyncLoad(referencePath);
				if (SyncStreamableHandle)
				{
					UTextureLightProfile* IESProfile = Cast<UTextureLightProfile>(SyncStreamableHandle->GetLoadedAsset());
					if (IESProfile)
					{
						lightConfig.LightProfile = IESProfile;
					}
				}
				preset.Value.SetCustomData(lightConfig);
			}
		}
		switch (preset.Value.AssetType)
		{
		case EBIMAssetType::IESProfile:
			AddLightFromPreset(preset.Value);
			break;
		case EBIMAssetType::Mesh:
			AddMeshFromPreset(preset.Value);
			break;
		case EBIMAssetType::Profile:
			AddProfileFromPreset(preset.Value);
			break;
		case EBIMAssetType::RawMaterial:
			AddRawMaterialFromPreset(preset.Value);
			break;
		case EBIMAssetType::Material:
			AddMaterialFromPreset(preset.Value);
			break;
		case EBIMAssetType::Pattern:
			AddPatternFromPreset(preset.Value);
			break;
		};
	}

	SetPartSizesFromMeshes();

	ensure(ProcessStarterAssemblies(starters) == EBIMResult::Success);
}

// Data Access
const FArchitecturalMesh* FBIMPresetCollection::GetArchitecturalMeshByGUID(const FGuid& Key) const
{
	return AMeshes.GetData(Key);
}

const FArchitecturalMaterial* FBIMPresetCollection::GetArchitecturalMaterialByGUID(const FGuid& Key) const
{
	return AMaterials.GetData(Key);
}

const FSimpleMeshRef* FBIMPresetCollection::GetSimpleMeshByGUID(const FGuid& Key) const
{
	return SimpleMeshes.GetData(Key);
}

const FStaticIconTexture* FBIMPresetCollection::GetStaticIconTextureByGUID(const FGuid& Key) const
{
	return StaticIconTextures.GetData(Key);
}

const FLayerPattern* FBIMPresetCollection::GetPatternByGUID(const FGuid& Key) const
{
	return Patterns.GetData(Key);
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



#undef LOCTEXT_NAMESPACE
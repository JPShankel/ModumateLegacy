// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "BIMKernel/BIMTagPath.h"
#include "BIMKernel/BIMNodeEditor.h"
#include "BIMKernel/BIMWidgetStatics.h"
#include "BIMKernel/BIMAssemblySpec.h"

static bool testTags()
{
	Modumate::BIM::FTagGroup group1, group2;

	group1.Add(TEXT("TAG1"));
	group1.Add(TEXT("TAG2"));
	group1.Add(TEXT("TAG3"));

	group2.Add(TEXT("TAG1"));
	group2.Add(TEXT("TAG2"));
	group2.Add(TEXT("TAG3"));

	if (!ensureAlways(group1.MatchesAll(group2)))
	{
		return false;
	}

	if (!ensureAlways(group1.MatchesAny(group2)))
	{
		return false;
	}

	group1 = Modumate::BIM::FTagGroup();
	group2 = Modumate::BIM::FTagGroup();

	group1.Add(TEXT("TAG1"));
	group1.Add(TEXT("TAG2"));

	group2.Add(TEXT("TAG1"));
	group2.Add(TEXT("TAG2"));
	group2.Add(TEXT("TAG3"));

	if (!ensureAlways(!group1.MatchesAll(group2)))
	{
		return false;
	}

	if (!ensureAlways(group1.MatchesAny(group2)))
	{
		return false;
	}

	if (!ensureAlways(!group2.MatchesAll(group1)))
	{
		return false;
	}

	if (!ensureAlways(group2.MatchesAny(group1)))
	{
		return false;
	}

	group1 = Modumate::BIM::FTagGroup();
	group2 = Modumate::BIM::FTagGroup();

	group1.Add(TEXT("TAG1"));
	group1.Add(TEXT("TAG2"));

	group2.Add(TEXT("TAG1"));
	group2.Add(TEXT("TAG2"));
	group2.Add(TEXT("TAG3"));

	if (!ensureAlways(!group1.MatchesAll(group2)))
	{
		return false;
	}

	if (!ensureAlways(group1.MatchesAny(group2)))
	{
		return false;
	}

	if (!ensureAlways(!group2.MatchesAll(group1)))
	{
		return false;
	}

	if (!ensureAlways(group2.MatchesAny(group1)))
	{
		return false;
	}

	group1 = Modumate::BIM::FTagGroup();
	group2 = Modumate::BIM::FTagGroup();

	group1.Add(TEXT("TAG1"));
	group1.Add(TEXT("G1TAG2"));

	group2.Add(TEXT("G2TAG1"));
	group2.Add(TEXT("TAG1"));
	group2.Add(TEXT("TAG3"));

	if (!ensureAlways(!group1.MatchesAll(group2)))
	{
		return false;
	}

	if (!ensureAlways(group1.MatchesAny(group2)))
	{
		return false;
	}

	FString outString1,outString2;

	Modumate::BIM::FTagPath path1, path2;
	path1.Add(group1);
	path1.Add(group2);

	if (!ensureAlways(path1.ToString(outString1) == ECraftingResult::Success))
	{
		return false;
	}

	if (!ensureAlways(path2.FromString(outString1) == ECraftingResult::Success))
	{
		return false;
	}

	if (!ensureAlways(path2.ToString(outString2) == ECraftingResult::Success))
	{
		return false;
	}

	if (!ensureAlways(outString1.Len() > 0 && outString1 == outString2))
	{
		return false;
	}

	if (!ensureAlways(path1.Matches(path2)))
	{
		return false;
	}

	if (!ensureAlways(path2.Matches(path1)))
	{
		return false;
	}

	return true;
}

bool testPreset(const Modumate::BIM::FCraftingPresetCollection &PresetCollection, const FName &PresetID)
{
	const Modumate::BIM::FCraftingTreeNodePreset *preset = PresetCollection.Presets.Find(PresetID);
	if (preset == nullptr)
	{
		return false;
	}

	Modumate::BIM::FCraftingTreeNodePreset outPreset;
	FCraftingPresetRecord record;

	if (preset->ToDataRecord(record) != ECraftingResult::Success)
	{
		return false;
	}

	if (outPreset.FromDataRecord(PresetCollection, record) != ECraftingResult::Success)
	{
		return false;
	}

	if (!ensureAlways(outPreset.Matches(*preset)))
	{
		return false;
	}

	if (!ensureAlways(preset->Matches(outPreset)))
	{
		return false;
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateCraftingUnitTest, "Modumate.Database.ModumateCrafting.UnitTest", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
bool FModumateCraftingUnitTest::RunTest(const FString &Parameters)
{
	if (!testTags())
	{
		return false;
	}

	FString manifestPath = FPaths::ProjectContentDir() / TEXT("NonUAssets") / TEXT("BIMData");

	TArray<FString> errors;
	Modumate::BIM::FCraftingPresetCollection presetCollection;

	if (presetCollection.LoadCSVManifest(manifestPath, TEXT("BIMManifest.txt"), errors) != ECraftingResult::Success)
	{
		return false;
	}

	TArray<FName> layeredAssemblies,materialColorPresets,layerPresets;
	FName layeredType = TEXT("4LayeredAssembly");
	FName layer0Type = TEXT("2Layer0D");
	FName materialColorType = TEXT("1Material");
	for (auto &kvp : presetCollection.Presets)
	{
		if (kvp.Value.NodeType == layeredType)
		{
			kvp.Value.ObjectType = EObjectType::OTWallSegment;
			layeredAssemblies.Add(kvp.Key);
		}
		else if (kvp.Value.NodeType == materialColorType)
		{
			materialColorPresets.Add(kvp.Key);
		}
		else if (kvp.Value.NodeType == layer0Type)
		{
			layerPresets.Add(kvp.Key);
		}
	}

	for (auto &kvp : presetCollection.NodeDescriptors)
	{
		if (kvp.Value.TypeName == layer0Type)
		{
			kvp.Value.Scope = EBIMValueScope::Layer;
		}
	}

	if (!ensureAlways(layeredAssemblies.Num() > 0) || !ensureAlways(materialColorPresets.Num() > 0))
	{
		return false;
	}

	if (!ensureAlways(testPreset(presetCollection, layeredAssemblies[0])))
	{
		return false;
	}

	if (!ensureAlways(testPreset(presetCollection, materialColorPresets[0])))
	{
		return false;
	}

	/*
	Make sure preset keys are consistent between known parents and children
	*/
	Modumate::BIM::FCraftingTreeNodePreset *assemblyPreset = presetCollection.Presets.Find(layeredAssemblies[0]);
	Modumate::BIM::FCraftingTreeNodePreset *layerPreset = presetCollection.Presets.Find(layerPresets[0]);
	FName assemblyPresetFirstLayer = assemblyPreset->ChildPresets[0].PresetID;

	FString str1 = assemblyPresetFirstLayer.ToString();
	FString str2 = layerPresets[0].ToString();

	if (!ensureAlways(layerPresets[0].IsEqual(assemblyPresetFirstLayer)))
	{
		return false;
	}

	Modumate::BIM::FCraftingTreeNodeType *nodeType = presetCollection.NodeDescriptors.Find(assemblyPreset->NodeType);

	Modumate::BIM::FCraftingTreeNodeInstancePool instancePool;
	Modumate::BIM::FCraftingTreeNodeInstanceSharedPtr node = instancePool.CreateNodeInstanceFromPreset(presetCollection, 0, layeredAssemblies[0],0,0);

	FBIMAssemblySpec outSpec;
	if (!ensureAlways(instancePool.PresetToSpec(layeredAssemblies[0], presetCollection, outSpec) == ECraftingResult::Success))
	{
		return false;
	}
	
	return true;
}
// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "BIMKernel/BIMDataModel.h"
#include "BIMKernel/BIMNodeEditor.h"
#include "BIMKernel/BIMWidgetStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateCraftingUnitTest, "Modumate.Database.ModumateCrafting.UnitTest", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
bool FModumateCraftingUnitTest::RunTest(const FString &Parameters)
{
	/*
	Load WALL_ROOT preset
	*/
	TArray<FString> errors;
	Modumate::BIM::FCraftingPresetCollection presetCollection;
	if (!ensureAlways(presetCollection.ParseScriptFile(FPaths::ProjectContentDir() / TEXT("NonUAssets") / TEXT("BIMNodeGraph.mbn"), errors) == ECraftingResult::Success))
	{
		return false;
	}

	TArray<FName> outPresets;
	if (!ensureAlways(presetCollection.SearchPresets(FName(TEXT("WALL_ROOT")), { Modumate::BIM::FCraftingPresetTag(TEXT("Assembly.Default")) }, {}, outPresets) == ECraftingResult::Success))
	{
		return false;
	}

	if (!ensureAlways(outPresets.Num() != 0))
	{
		return false;
	}

	// Find a multilayer preset
	Modumate::BIM::FCraftingTreeNodePreset *preset = nullptr;
	for (auto &presetID : outPresets)
	{
		preset = presetCollection.Presets.Find(presetID);
		if (!ensureAlways(preset != nullptr))
		{
			return false;
		}

		if (preset->ChildNodes.Num() > 1)
		{
			break;
		}
	}

	if (!ensureAlways(preset != nullptr && preset->ChildNodes.Num() > 1))
	{
		return false;
	}

	/*
	Check that preset matches itself
	*/
	if (!ensureAlways(preset->Matches(*preset)))
	{
		return false;
	}

	/*
	Check that serialized preset matches original when deserialized
	*/
	FCraftingPresetRecord record;
	preset->ToDataRecord(record);
	Modumate::BIM::FCraftingTreeNodePreset instancePreset;
	instancePreset.FromDataRecord(presetCollection, record);

	if (!ensureAlways(preset->Matches(instancePreset)))
	{
		return false;
	}

	if (!ensureAlways(instancePreset.Matches(*preset)))
	{
		return false;
	}

	/*
	Check that preset matches original when built from command parameters
	*/
	Modumate::FModumateFunctionParameterSet parameterSet;
	preset->ToParameterSet(parameterSet);
	instancePreset = Modumate::BIM::FCraftingTreeNodePreset();

	instancePreset.FromParameterSet(presetCollection, parameterSet);

	if (!ensureAlways(preset->Matches(instancePreset)))
	{
		return false;
	}

	if (!ensureAlways(instancePreset.Matches(*preset)))
	{
		return false;
	}


	/*
	Create the first default wall preset as a node
	*/
	Modumate::BIM::FCraftingTreeNodeInstancePool instancePool;
	Modumate::BIM::FCraftingTreeNodeInstanceSharedPtr instance = instancePool.CreateNodeInstanceFromPreset(presetCollection, 0, preset->PresetID, true);

	if (!ensureAlways(instance.IsValid()))
	{
		return false;
	}

	/*
	Convert node to preset and verify it matches the original
	*/
	instancePreset = Modumate::BIM::FCraftingTreeNodePreset();
	if (!ensureAlways(presetCollection.GetInstanceDataAsPreset(instance, instancePreset) == ECraftingResult::Success))
	{
		return false;
	}

	if (!ensureAlways(instancePreset.Matches(*preset)))
	{
		return false;
	}

	if (!ensureAlways(preset->Matches(instancePreset)))
	{
		return false;
	}

	/*
	Verify that input pins generate the correct list of eligible presets
	*/

	// Make a wood stud wall
	instancePool.ResetInstances();
	instance = instancePool.CreateNodeInstanceFromPreset(presetCollection, 0, FName(TEXT("Default2x4InteriorWoodStudWallPreset")), true);
	if (!ensureAlways(instance.IsValid()))
	{
		return false;
	}

	// Find the material node
	Modumate::BIM::FCraftingTreeNodeInstanceSharedPtr materialNode = instancePool.FindInstanceByPredicate(
		[](const Modumate::BIM::FCraftingTreeNodeInstanceSharedPtr &Instance)
	{
		return (Instance->PresetID.IsEqual(FName(TEXT("WoodSprucePineFir"))));
	});

	if (!ensureAlways(materialNode.IsValid()))
	{
		return false;
	}

	// Ask for eligible swaps of material node (should only be WoodSprucePineFir)
	TArray<FCraftingNode> craftingNodes;
	UModumateCraftingNodeWidgetStatics::GetEligiblePresetsForSwap(instancePool, presetCollection, materialNode->GetInstanceID(), craftingNodes);

	if (!ensureAlways(craftingNodes.Num() == 1))
	{
		return false;
	}

	if (!ensureAlways(craftingNodes[0].PresetID.IsEqual(FName(TEXT("WoodSprucePineFir")))))
	{
		return false;
	}
	return true;
}
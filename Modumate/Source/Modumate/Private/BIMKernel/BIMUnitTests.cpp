// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/BIMUnitTests.h"
#include "JsonObjectConverter.h"
#include "BIMKernel/Core/BIMTagPath.h"
#include "BIMKernel/Presets/BIMPresetEditor.h"
#include "BIMKernel/AssemblySpec/BIMAssemblySpec.h"
#include "BIMKernel/Presets/BIMPresetCollection.h"
#include "BIMKernel/Presets/BIMPresetInstance.h"
#include "BIMKernel/Core/BIMKey.h"

static bool testKeys()
{
	if (FBIMKey() != FBIMKey(TEXT("")))
	{
		return false;
	}

	FBIMKey key1(TEXT("Value1"));
	FBIMKey key2(TEXT("Value1"));

	if (key1 != key2)
	{
		return false;
	}

	FString ms1, ms2;

	FString characters(TEXT("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-><!@#$%^&*()_+=-"));
	int32 numChars = characters.Len();
	for (int32 i = 0; i < 2000; ++i)
	{
		int32 c = FMath::RandRange(0, numChars - 1);
		ms1.AppendChar(characters[c]);
		int32 d = FMath::RandRange(0, numChars - 1);
		while (d == c)
		{
			d = FMath::RandRange(0, numChars - 1);
		}
		ms2.AppendChar(characters[d]);
	}

	key1 = FBIMKey(ms1);
	key2 = FBIMKey(ms1);

	if (key1 != key2)
	{
		return false;
	}

	key2 = FBIMKey(ms2);

	if (key1 == key2)
	{
		return false;
	}

	TMap<FBIMKey, int32> testMap;

	testMap.Add(key1, 1);
	testMap.Add(key2, 2);

	int32 *mapNum = testMap.Find(key1);

	if (mapNum == nullptr || (*mapNum) != 1)
	{
		return false;
	}

	mapNum = testMap.Find(key2);

	if (mapNum == nullptr || (*mapNum) != 2)
	{
		return false;
	}

	FTestBIMKeyContainer container;
	container.TestMap.Add(FBIMKey("Test1"), 1);
	container.TestMap.Add(FBIMKey("Test2"), 2);
	container.TestMap.Add(FBIMKey("Test3"), 3);

	auto jsonOb = FJsonObjectConverter::UStructToJsonObject<FTestBIMKeyContainer>(container);

	FTestBIMKeyContainer deserialized;
	FJsonObjectConverter::JsonObjectToUStruct<FTestBIMKeyContainer>(jsonOb.ToSharedRef(), &deserialized);

	if (deserialized.TestMap.Num() != container.TestMap.Num())
	{
		return false;
	}

	TArray<FBIMKey> keyArrayContainer,keyArrayDeserialized;
	TArray<int32> valueArrayContainer, valueArrayDeserialized;

	container.TestMap.GenerateKeyArray(keyArrayContainer);
	container.TestMap.GenerateValueArray(valueArrayContainer);

	deserialized.TestMap.GenerateKeyArray(keyArrayDeserialized);
	deserialized.TestMap.GenerateValueArray(valueArrayDeserialized);

	for (int32 i = 0; i < keyArrayContainer.Num(); ++i)
	{
		if (keyArrayContainer[i] != keyArrayDeserialized[i])
		{
			return false;
		}
		if (valueArrayContainer[i] != valueArrayDeserialized[i])
		{
			return false;
		}
	}

	return true;
}

static bool testTags()
{

	FString outString1,outString2;

	FBIMTagPath path1, path2;
	path1.Tags.Add(TEXT("TAG1"));
	path1.Tags.Add(TEXT("TAG2"));
	path1.Tags.Add(TEXT("TAG3"));

	if (!ensureAlways(path1.ToString(outString1) == EBIMResult::Success))
	{
		return false;
	}

	if (!ensureAlways(path2.FromString(outString1) == EBIMResult::Success))
	{
		return false;
	}

	if (!ensureAlways(path2.ToString(outString2) == EBIMResult::Success))
	{
		return false;
	}

	if (!ensureAlways(outString1.Len() > 0 && outString1 == outString2))
	{
		return false;
	}

	if (!ensureAlways(path1.MatchesExact(path2)))
	{
		return false;
	}

	if (!ensureAlways(path2.MatchesExact(path1)))
	{
		return false;
	}


	path2 = FBIMTagPath();
	path2.Tags.Add(TEXT("TAG1"));
	path2.Tags.Add(TEXT("TAG2"));

	if (!ensureAlways(path1.MatchesPartial(path2)))
	{
		return false;
	}

	if (!ensureAlways(path2.MatchesPartial(path1)))
	{
		return false;
	}

	path1 = FBIMTagPath();
	path1.Tags.Add(TEXT("TAG1"));
	path1.Tags.Add(TEXT("TORGO"));

	if (!ensureAlways(!path1.MatchesPartial(path2)))
	{
		return false;
	}

	if (!ensureAlways(!path2.MatchesPartial(path1)))
	{
		return false;
	}

	return true;
}

bool testPreset(const FBIMPresetCollection &PresetCollection, const FBIMKey& PresetID)
{
	const FBIMPresetInstance* preset = PresetCollection.Presets.Find(PresetID);
	if (preset == nullptr)
	{
		return false;
	}
	// TODO: node editor tests
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateCraftingUnitTest, "Modumate.Database.ModumateCrafting.UnitTest", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
bool FModumateCraftingUnitTest::RunTest(const FString &Parameters)
{
	if (!testTags())
	{
		return false;
	}

	if (!testKeys())
	{
		return false;
	}

	FString manifestPath = FPaths::ProjectContentDir() / TEXT("NonUAssets") / TEXT("BIMData");

	TArray<FString> errors;
	FBIMPresetCollection presetCollection;
	TArray<FBIMKey> starters;

	if (presetCollection.LoadCSVManifest(manifestPath, TEXT("BIMManifest.txt"), starters, errors) != EBIMResult::Success)
	{
		return false;
	}

	TArray<FBIMKey> layeredAssemblies,materialColorPresets,layerPresets,riggedPresets;
	FName layeredType(TEXT("4LayeredAssembly"));
	FName layer0Type(TEXT("2Layer0D"));
	FName materialColorType(TEXT("1Material"));
	FName riggedType(TEXT("3RiggedAssembly"));

	for (auto &kvp : presetCollection.Presets)
	{
		if (kvp.Value.NodeType == riggedType)
		{
			riggedPresets.Add(kvp.Key);
		}
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

	TMap<FString, FBIMNameType> propertyForm;
	if (presetCollection.GetPropertyFormForPreset(materialColorPresets[0], propertyForm) != EBIMResult::Success)
	{
		return false;
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
	FBIMPresetInstance* assemblyPreset = presetCollection.Presets.Find(layeredAssemblies[0]);
	FBIMPresetInstance* layerPreset = presetCollection.Presets.Find(layerPresets[0]);
	FBIMKey assemblyPresetFirstLayer = assemblyPreset->ChildPresets[0].PresetID;

	if (!ensureAlways(layerPresets[0] == assemblyPresetFirstLayer))
	{
		return false;
	}

	FBIMPresetTypeDefinition *nodeType = presetCollection.NodeDescriptors.Find(assemblyPreset->NodeType);

	FBIMPresetEditor instancePool;
	FBIMPresetEditorNodeSharedPtr rootNode;
	if (!ensureAlways(instancePool.InitFromPreset(presetCollection, layeredAssemblies[0], rootNode) == EBIMResult::Success))
	{
		return false;
	}

	for (auto& riggedPreset : riggedPresets)
	{
		if (!ensureAlways(testPreset(presetCollection, riggedPreset)))
		{
			return false;
		}

		if (!ensureAlways(instancePool.InitFromPreset(presetCollection, riggedPreset, rootNode) == EBIMResult::Success))
		{
			return false;
		}

		if (!ensureAlways(rootNode->GetPresetStatus() == EBIMPresetEditorNodeStatus::UpToDate))
		{
			return false;
		}
	}

	return true;
}
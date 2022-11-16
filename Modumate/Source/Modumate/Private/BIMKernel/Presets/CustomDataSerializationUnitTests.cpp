#include "CoreMinimal.h"
#include "VectorTypes.h"
#include "BIMKernel/AssemblySpec/BIMLegacyPattern.h"
#include "BIMKernel/Presets/BIMPresetInstance.h"
#include "BIMKernel/Presets/BIMPresetPatternDefinition.h"
#include "BIMKernel/Presets/BIMWebPreset.h"
#include "Misc/AutomationTest.h"
#include "BIMKernel/Presets/CustomData/BIMDimensions.h"
#include "BIMKernel/Presets/CustomData/BIMIESProfile.h"
#include "BIMKernel/Presets/CustomData/BIMMesh.h"
#include "BIMKernel/Presets/CustomData/BIMMeshRef.h"
#include "BIMKernel/Presets/CustomData/BIMNamedDimension.h"
#include "BIMKernel/Presets/CustomData/BIMPart.h"
#include "BIMKernel/Presets/CustomData/BIMPattern.h"
#include "BIMKernel/Presets/CustomData/BIMPatternRef.h"
#include "BIMKernel/Presets/CustomData/BIMPresetInfo.h"
#include "BIMKernel/Presets/CustomData/BIMProfile.h"
#include "BIMKernel/Presets/CustomData/BIMProfileRef.h"
#include "BIMKernel/Presets/CustomData/BIMRawMaterial.h"
#include "BIMKernel/Presets/CustomData/BIMSlotConfig.h"
#include "DocumentManagement/ModumateDocument.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/ModumateGameInstance.h"


class AEditModelPlayerController;
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateWebPresetsSerializes, "Modumate.BIM.WebPresets.Serializes",
                                EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FModumateWebPresetsSerializes::RunTest(const FString& Parameters)
{
	UWorld* world = nullptr;
	for (const FWorldContext& worldContext : GEngine->GetWorldContexts())
	{
		if (worldContext.WorldType == EWorldType::Game || worldContext.WorldType == EWorldType::PIE)
		{
			world = worldContext.World();
			break;
		}
	}
	
	if(world == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("TEST NOT RUN, must have PIE or Standalone context running"));
		return true;
	}
		
	auto controller = world->GetFirstPlayerController<AEditModelPlayerController>();
	if(controller == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("TEST NOT RUN, must have PIE or Standalone context running"));
		return true;
	}

	auto gameInstance = world->GetGameInstance<UModumateGameInstance>();
	if(gameInstance == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("TEST NOT RUN, must have PIE or Standalone context running"));
		return true;
	}
	
	FBIMPresetCollection collection {};
	collection.ReadInitialPresets(gameInstance);

	TArray<FGuid> keys;
	collection.GetAllPresetKeys(keys);
	for(auto& key : keys)
	{
		const FBIMPresetInstance* InPreset = collection.PresetFromGUID(key);
		FBIMPresetInstance OutPreset;
		FBIMWebPreset interim;
		InPreset->ToWebPreset(interim, world);
		OutPreset.FromWebPreset(interim, world);
		collection.RemovePreset(key);
		collection.AddOrUpdatePreset(OutPreset, OutPreset);

		//The VDP table can screw this, for our test we just fix it.
		OutPreset.GUID = InPreset->GUID;

		FString msg = TEXT("Preset ") + InPreset->CanonicalBase.ToString() + TEXT(" serializes and matches deserialization");
		TestTrue(msg, *InPreset == OutPreset);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateCustomDataSerializes, "Modumate.BIM.CustomData.Serializes", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
	bool FModumateCustomDataSerializes::RunTest(const FString& Parameters)
{
	FBIMWebPreset TestPreset;
	
	FBIMDimensions InDims, OutDims;
	FBIMIESProfile InIESProfile, OutIESProfile;
	FBIMMesh InMesh, OutMesh;
	FBIMMeshRef InMeshRef, OutMeshRef;
	FBIMNamedDimension InNamedDimension, OutNamedDimension;
	FBIMPartConfig InPartConfig, OutPartConfig;
	FBIMPatternConfig InPatternConfig, OutPatternConfig;
	FBIMPatternRef InPatternRef, OutPatternRef;
	FBIMPresetInfo InPresetInfo, OutPresetInfo;
	FBIMProfile InProfile, OutProfile;
	FBIMProfileRef InProfileRef, OutProfileRef;
	FBIMRawMaterial InRawMaterial, OutRawMaterial;
	FBIMSlotConfig InSlotConfig, OutSlotConfig;
	
	FBIMPresetMaterialBindingSet InMaterialBindingSet, OutMaterialBindingSet;
	FLightConfiguration InLightConfig, OutLightConfig;

	//TODO: Edge Details
	//TODO: Symbols
	
	InDims.SetCustomDimension(FName(TEXT("Test1")), 1.1f);
	InDims.SetCustomDimension(FName(TEXT("Test2")), 2.2f);
	InDims.SetCustomDimension(FName(TEXT("Test3")), 3.3f);
	InDims.SetCustomDimension(FName(TEXT("Test4")), 4.4f);
	
	InIESProfile.AssetPath = TEXT("Test1//A");
	InIESProfile.CraftingIconAssetFilePath = TEXT("Test2//Icon");
	
	InMesh.Normal = TEXT("");
	InMesh.Tangent = TEXT("");
	InMesh.AssetPath = TEXT("Test1//sagas");
	InMesh.MaterialKey = FGuid::NewGuid().ToString();
	InMesh.SliceX1 = 1;
	InMesh.SliceY1 = 2;
	InMesh.SliceZ1 = 3;
	InMesh.SliceX2 = 4;
	InMesh.SliceY2 = 5;
	InMesh.SliceZ2 = 6;
	InMesh.NativeSizeX = 7;
	InMesh.NativeSizeY = 8;
	InMesh.NativeSizeZ = 9;
	
	InMeshRef.Source = FGuid::NewGuid();
	
	InNamedDimension.Description = TEXT("This is a test");
	InNamedDimension.DefaultValue = 0.1f;
	InNamedDimension.DimensionKey = TEXT("MyKey");
	InNamedDimension.DisplayName = TEXT("Test");
	InNamedDimension.UIGroup = TEXT("what?");
	
	InPartConfig.Normal = TEXT("1");
	InPartConfig.Tangent = TEXT("2");
	InPartConfig.Zalign = true;
	
	InPatternConfig.Extents = TEXT("1");
	InPatternConfig.Thickness = TEXT("2");
	InPatternConfig.ModuleCount = 1;
	InPatternConfig.ModuleDimensions = {TEXT("1")};
	InPatternConfig.CraftingIconAssetFilePath = TEXT("IconPath");
	
	InPatternRef.Source = FGuid::NewGuid();
	
	InPresetInfo.Comments = TEXT("Comments!");
	InPresetInfo.Mark = TEXT("NOpe");
	
	InProfile.AssetPath = TEXT("assetPath");
	
	InProfileRef.Source = FGuid::NewGuid();
	
	InRawMaterial.AssetPath = TEXT("AnotherAssetPath");
	InRawMaterial.TextureTilingSize = 1;
	
	InSlotConfig.ConceptualSizeY = TEXT("1");

	FBIMPresetMaterialBinding binding1, binding2;
	binding1.Channel = FName(TEXT("Channel1"));
	binding1.InnerMaterialGUID = FGuid::NewGuid();
	binding1.SurfaceMaterialGUID = FGuid::NewGuid();
	binding1.ColorHexValue = "000000";
	binding1.ColorTintVariationPercent = 0.5f;

	binding2.Channel = FName(TEXT("Channel2"));
	binding2.InnerMaterialGUID = FGuid::NewGuid();
	binding2.SurfaceMaterialGUID = FGuid::NewGuid();
	binding2.ColorHexValue = "000001";
	binding2.ColorTintVariationPercent = 0.6f;

	InMaterialBindingSet.MaterialBindings.Add(binding1);
	InMaterialBindingSet.MaterialBindings.Add(binding2);

	InLightConfig.Location = {1,2,3};
	InLightConfig.Name = TEXT("LightConfig");
	InLightConfig.Rotation = {5,6,7};
	InLightConfig.Scale = {8,9,10};
	InLightConfig.LightColor = {1,1,1};
	InLightConfig.LightIntensity = 1.f;
	InLightConfig.SourceRadius = 1515;
	InLightConfig.bAsSpotLight = false;
	InLightConfig.IESProfileGUID = FGuid::NewGuid();
	
	InDims.ConvertToWebPreset(TestPreset);
	InIESProfile.ConvertToWebPreset(TestPreset);
	InMesh.ConvertToWebPreset(TestPreset);
	InMeshRef.ConvertToWebPreset(TestPreset);
	InNamedDimension.ConvertToWebPreset(TestPreset);
	InPartConfig.ConvertToWebPreset(TestPreset);
	InPatternConfig.ConvertToWebPreset(TestPreset);
	InPatternRef.ConvertToWebPreset(TestPreset);
	InPresetInfo.ConvertToWebPreset(TestPreset);
	InProfile.ConvertToWebPreset(TestPreset);
	InProfileRef.ConvertToWebPreset(TestPreset);
	InRawMaterial.ConvertToWebPreset(TestPreset);
	InSlotConfig.ConvertToWebPreset(TestPreset);
	InMaterialBindingSet.ConvertToWebPreset(TestPreset);
	InLightConfig.ConvertToWebPreset(TestPreset);
	
	OutDims.ConvertFromWebPreset(TestPreset);
	OutIESProfile.ConvertFromWebPreset(TestPreset);
	OutMesh.ConvertFromWebPreset(TestPreset);
	OutMeshRef.ConvertFromWebPreset(TestPreset);
	OutNamedDimension.ConvertFromWebPreset(TestPreset);
	OutPartConfig.ConvertFromWebPreset(TestPreset);
	OutPatternConfig.ConvertFromWebPreset(TestPreset);
	OutPatternRef.ConvertFromWebPreset(TestPreset);
	OutPresetInfo.ConvertFromWebPreset(TestPreset);
	OutProfile.ConvertFromWebPreset(TestPreset);
	OutProfileRef.ConvertFromWebPreset(TestPreset);
	OutRawMaterial.ConvertFromWebPreset(TestPreset);
	OutSlotConfig.ConvertFromWebPreset(TestPreset);
	OutMaterialBindingSet.ConvertFromWebPreset(TestPreset);
	OutLightConfig.ConvertFromWebPreset(TestPreset);



	TestTrue(TEXT("Dimensions matches"), InDims == OutDims);
	TestTrue(TEXT("IESProfile matches"), InIESProfile == OutIESProfile);
	TestTrue(TEXT("Mesh matches"), InMesh == OutMesh);
	TestTrue(TEXT("MeshRef matches"), InMeshRef == OutMeshRef);
	TestTrue(TEXT("NamedDims matches"), InNamedDimension == OutNamedDimension);
	TestTrue(TEXT("PatternConfig matches"), InPatternConfig == OutPatternConfig);
	TestTrue(TEXT("PatternRef matches"), InPatternRef == OutPatternRef);
	TestTrue(TEXT("PresetInfo matches"), InPresetInfo == OutPresetInfo);
	TestTrue(TEXT("Profile matches"), InProfile == OutProfile);
	TestTrue(TEXT("ProfileRef matches"), InProfileRef == OutProfileRef);
	TestTrue(TEXT("RawMaterial matches"), InRawMaterial == OutRawMaterial);
	TestTrue(TEXT("SlotConfig matches"), InSlotConfig == OutSlotConfig);
	TestTrue(TEXT("Material Bindings matches"), InMaterialBindingSet == OutMaterialBindingSet);
	TestTrue(TEXT("Light Config matches"), InLightConfig == OutLightConfig);
	
	return true;
}
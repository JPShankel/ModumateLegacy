#include "CoreMinimal.h"
#include "ModumateCore/ExpressionEvaluator.h"
#include "Database/ModumateObjectDatabase.h"
#include "BIMKernel/Core/BIMProperties.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Quantities/QuantitiesDimensions.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "BIMKernel/Presets/BIMPresetCollection.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelDatasmithImporter.h"


static bool testVectorFormula()
{
	FVectorExpression vf1(TEXT("Parent.SizeX-2.5"), TEXT("(1/2)*Parent.SizeY"), TEXT("(1/2)*(12+Parent.SizeZ-6)"));

	TMap<FString, float> vars;
	vars.Add(TEXT("Parent.SizeX"), 3.0f);
	vars.Add(TEXT("Parent.SizeY"), 10.0f);
	vars.Add(TEXT("Parent.SizeZ"), 8.0f);

	FVector outVector;
	if (!vf1.Evaluate(vars, outVector))
	{
		return false;
	}
	if (!FMath::IsNearlyEqual(outVector.X, 0.5f))
	{
		return false;
	}
	if (!FMath::IsNearlyEqual(outVector.Y, 5.0f))
	{
		return false;
	}
	if (!FMath::IsNearlyEqual(outVector.Z, 7.0f))
	{
		return false;
	}
	
	FVectorExpression vf2(TEXT("Parent.SizeX-2.5"), TEXT(""), TEXT("(1/2)*(12+Parent.SizeZ-6)"));
	
	outVector = FVector::ZeroVector;
	if (!vf2.Evaluate(vars, outVector))
	{
		return false;
	}

	if (outVector.Y != 0.0f)
	{
		return false;
	}

	return true;
}

#if WITH_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateDatabaseBIMTest, "Modumate.Database.BIM.UnitTest", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::HighPriority)
	bool FModumateDatabaseBIMTest::RunTest(const FString& Parameters)
{
	bool bSuccess = true;
	UE_LOG(LogUnitTest, Display, TEXT("Modumate BIM Schema - Unit Test Started"));

	FBIMPropertyKey vs;
	bSuccess = vs.Scope == EBIMValueScope::None && bSuccess;
	bSuccess = vs.Name == TEXT("") && bSuccess;

	vs = FBIMPropertyKey(EBIMValueScope::Assembly, TEXT("Name"));

	FBIMNameType fqn = vs.QN();
	bSuccess = (fqn == TEXT("Assembly.Name")) && bSuccess;

	bSuccess = testVectorFormula() && bSuccess;

	FModumateDatabase bimDatabase;
	bimDatabase.Init();
	bimDatabase.ReadPresetData();
	bSuccess = bimDatabase.UnitTest() && bSuccess;

	FBIMPresetInstance sourcePreset, targetPreset;

	sourcePreset.SetScopedProperty(EBIMValueScope::Preset, BIMPropertyNames::Description, FString(TEXT("My Preset Condition")));
	sourcePreset.SetScopedProperty(EBIMValueScope::Preset, BIMPropertyNames::Code, 555.0f);

	FLightConfiguration lightConfig;
	lightConfig.Name = TEXT("My Light");
	sourcePreset.SetCustomData(lightConfig);

	FBIMConstructionCost constructionCost;
	constructionCost.LaborCostRate = 777.0f;
	sourcePreset.SetCustomData(constructionCost);

	targetPreset.SetCustomData(FLightConfiguration());
	targetPreset.SetCustomData(FBIMConstructionCost());

	FBIMWebPreset webPreset;
	sourcePreset.ToWebPreset(webPreset, nullptr);

	targetPreset.FromWebPreset(webPreset, nullptr);

	FBIMConstructionCost outConstructionCost;
	FLightConfiguration outLightConfig;

	bSuccess = targetPreset.TryGetCustomData(outConstructionCost) && bSuccess;
	bSuccess = targetPreset.TryGetCustomData(outLightConfig) && bSuccess;

	bSuccess = (outConstructionCost.LaborCostRate == constructionCost.LaborCostRate) && bSuccess;
	bSuccess = (outLightConfig.Name == lightConfig.Name) && bSuccess;

	UE_LOG(LogEngineAutomationTests, Log, TEXT("Modumate BIM Schema - Unit Test Completed %s"), bSuccess ? TEXT("PASSED") : TEXT("FAILED"));
	return bSuccess;
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FWaitForModumateOnlineAssetsLoad, int32*, NumAssets);
bool FWaitForModumateOnlineAssetsLoad::Update()
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

	auto controller = world ? world->GetFirstPlayerController<AEditModelPlayerController>() : nullptr;
	auto importer = controller ? controller->EditModelDatasmithImporter : nullptr;
	if (!importer)
	{
		UE_LOG(LogEngineAutomationTests, Error, TEXT("Cannot get importer"));
		return true;
	}

	if (importer->StaticMeshAssetMap.Num() == *NumAssets)
	{
		UE_LOG(LogEngineAutomationTests, Log, TEXT("All assets loaded for importer: %d out of %d"), importer->StaticMeshAssetMap.Num(), *NumAssets);
		return true;
	}

	float elapsed = (FPlatformTime::Seconds() - StartTime);
	if (elapsed > 10.f)
	{
		UE_LOG(LogEngineAutomationTests, Error, TEXT("Timed out for importer loading assets"));
		return true;
	}

	return false;
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FModumateOnlineAssetsLoadBody, int32*, NumExpectedAssemblies);
bool FModumateOnlineAssetsLoadBody::Update()
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

	auto controller = world ? world->GetFirstPlayerController<AEditModelPlayerController>() : nullptr;
	auto importer = controller ? controller->EditModelDatasmithImporter : nullptr;
	if (!importer)
	{
		UE_LOG(LogEngineAutomationTests, Error, TEXT("Cannot get importer"));
		return true;
	}

	UModumateGameInstance* gameInstance = world->GetGameInstance<UModumateGameInstance>();
	TArray<FBIMAssemblySpec> assemblies;
	gameInstance->ObjectDatabase->GetPresetCollection().GetProjectAssembliesForObjectType(EObjectType::OTFurniture, assemblies);

	// Create callback if needed
#if 0
	auto weakImporter = MakeWeakObjectPtr<AEditModelDatasmithImporter>(importer);
	auto deferredReport = [weakImporter]() {
		if (weakImporter.IsValid())
		{
			UE_LOG(LogEngineAutomationTests, Log, TEXT("Imported: %d"), weakImporter->StaticMeshAssetMap.Num());
		}
	};
#endif

	// Limit number of assemblies to test
	*NumExpectedAssemblies = 10;
	for (int32 i = 0; i < *NumExpectedAssemblies; ++i)
	{
		importer->HandleAssetRequest(FAssetRequest(assemblies[i], nullptr));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateOnlineAssetsLoad, "Modumate.Online.AssetsLoad", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::HighPriority)
bool FModumateOnlineAssetsLoad::RunTest(const FString& Parameters)
{
	static int32 expectedAssemblies = 0;

	ADD_LATENT_AUTOMATION_COMMAND(FLoadGameMapCommand("EditModelLVL"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForMapToLoadCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FModumateOnlineAssetsLoadBody(&expectedAssemblies));
	ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForModumateOnlineAssetsLoad(&expectedAssemblies));
	ADD_LATENT_AUTOMATION_COMMAND(FLoadGameMapCommand("MainMenuLVL"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForMapToLoadCommand());
	return true;
}

#endif
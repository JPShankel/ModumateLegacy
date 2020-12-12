#include "CoreMinimal.h"
#include "UnrealClasses/EditModelInputAutomation.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "DocumentManagement/ModumateCommands.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "Tests/AutomationCommon.h"

namespace Modumate
{
#if WITH_DEV_AUTOMATION_TESTS

	UWorld* GetGameWorld()
	{
		for (const FWorldContext &worldContext : GEngine->GetWorldContexts())
		{
			if (worldContext.WorldType == EWorldType::Game)
			{
				return worldContext.World();
			}
		}

		return nullptr;
	}

	static const FString InputTestFolder(TEXT("Input"));

	DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FStartInputPlaybackCommand, FString, InputLogBaseName, float, PlaybackSpeed);

	bool FStartInputPlaybackCommand::Update()
	{
		UWorld *world = GetGameWorld();
		UModumateGameInstance *gameInstance = world ? world->GetGameInstance<UModumateGameInstance>() : nullptr;
		AEditModelPlayerController_CPP *controller = world ? world->GetFirstPlayerController<AEditModelPlayerController_CPP>() : nullptr;
		AEditModelGameState_CPP *gameState = world ? Cast<AEditModelGameState_CPP>(world->GetGameState()) : nullptr;
		if (ensure(controller && controller->InputAutomationComponent && gameState))
		{
			// Make an empty document, and turn on document debugging
			gameState->Document->MakeNew(world);
			controller->ModumateCommand(
				Modumate::FModumateCommand(Modumate::Commands::kDebug)
				.Param(TEXT("type"), TEXT("document"))
				.Param(TEXT("on"), true)
			);

			FString inputLogPath = FPaths::ProjectDir() / gameInstance->TestScriptRelativePath / InputTestFolder /
				(InputLogBaseName + FEditModelInputLog::LogExtension);
			bool bPlayingBack = controller->InputAutomationComponent->BeginPlayback(inputLogPath, false, PlaybackSpeed);
			ensure(bPlayingBack);
		}

		return true;
	}

	struct FInputPlaybackComparison
	{
		FString ReferenceProjectFileName;
		FString ReferenceScreenshotFileName;
		bool bCompareGraph = false;
		bool bCompareMOIs = false;
		bool bComparePresets = false;

		FInputPlaybackComparison() { }

		FInputPlaybackComparison(const FString& InReferenceProjectFileName, const FString& InReferenceScreenshotFileName, bool bInCompareGraph, bool bInCompareMOIs, bool bInComparePresets)
			: ReferenceProjectFileName(InReferenceProjectFileName)
			, ReferenceScreenshotFileName(InReferenceScreenshotFileName)
			, bCompareGraph(bInCompareGraph)
			, bCompareMOIs(bInCompareMOIs)
			, bComparePresets(bInComparePresets)
		{ }
	};

	DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FWaitForInputPlaybackCommand, FInputPlaybackComparison, ResultComparison);

	bool FWaitForInputPlaybackCommand::Update()
	{
		UWorld *world = GetGameWorld();
		UModumateGameInstance* gameInstance = world ? world->GetGameInstance<UModumateGameInstance>() : nullptr;
		AEditModelPlayerController_CPP *controller = world ? world->GetFirstPlayerController<AEditModelPlayerController_CPP>() : nullptr;
		AEditModelGameState_CPP *gameState = world ? Cast<AEditModelGameState_CPP>(world->GetGameState()) : nullptr;
		if (ensure(gameInstance && controller && controller->InputAutomationComponent && gameState))
		{
			if (controller->InputAutomationComponent->IsPlaying())
			{
				return false;
			}
			else
			{
				// Make good filenames for the result outputs
				FString buildID;
				FParse::Value(FCommandLine::Get(), TEXT("BuildID="), buildID);
				FString resultFileSuffix = buildID.IsEmpty() ? buildID : (FString(TEXT("_Build")) + buildID);

				// Take a screenshot, so we can see what went wrong if the test fails
				bool bShowUI = true;
				bool bAddFilenameSuffix = false;
				FString screenshotFilename = FString(TEXT("Screenshot_InputTest")) + resultFileSuffix;
				FScreenshotRequest::RequestScreenshot(screenshotFilename, bShowUI, bAddFilenameSuffix);

				// Save the current resulting document as a record
				FModumateDocumentHeader resultDocHeader;
				FMOIDocumentRecord resultDocRecord;
				gameState->Document->SerializeRecords(world, resultDocHeader, resultDocRecord);

				// Verify the results
				bool bLoadedReference = false;
				FModumateDocumentHeader referenceDocHeader;
				FMOIDocumentRecord referenceDocRecord;
				if (!ResultComparison.ReferenceProjectFileName.IsEmpty())
				{
					FString referenceFilePath = FPaths::ProjectDir() / gameInstance->TestScriptRelativePath / InputTestFolder / ResultComparison.ReferenceProjectFileName;

					if (FModumateSerializationStatics::TryReadModumateDocumentRecord(referenceFilePath, referenceDocHeader, referenceDocRecord))
					{
						bLoadedReference = true;
					}
					else
					{
						UE_LOG(LogInputAutomation, Error, TEXT("Failed to load reference document record: %s"), *referenceFilePath);
					}
				}

				if (ResultComparison.bCompareGraph && bLoadedReference)
				{
					FGraph3D referenceDocGraph;
					referenceDocGraph.Load(&referenceDocRecord.VolumeGraph);

					if (!referenceDocGraph.Equals(gameState->Document->GetVolumeGraph()))
					{
						UE_LOG(LogInputAutomation, Error, TEXT("Resulting VolumeGraph does not equal reference VolumeGraph!"));
					}
				}

				if (ResultComparison.bCompareMOIs && bLoadedReference)
				{
					bool bEqualMOIs = false;

					TMap<int32, FMOIStateData> referenceMOIStates;
					for (FMOIStateData& MOIState : referenceDocRecord.ObjectData)
					{
						// TODO: clean up this logic; this is copied from the post-deserialization behavior in UModumateDocument::Load,
						// but it's less costly than creating and loading an entirely separate document, with MOIs and all.
						MOIState.CustomData.SaveCborFromJson();
						referenceMOIStates.Add(MOIState.ID, MOIState);
					}

					if (referenceMOIStates.Num() == resultDocRecord.ObjectData.Num())
					{
						bEqualMOIs = true;
						for (auto& resultMOIState : resultDocRecord.ObjectData)
						{
							auto* referenceMOIState = referenceMOIStates.Find(resultMOIState.ID);
							if ((referenceMOIState == nullptr) || (*referenceMOIState != resultMOIState))
							{
								bEqualMOIs = false;
								UE_LOG(LogInputAutomation, Error, TEXT("Resulting MOI States do not match for ID #%d!"), resultMOIState.ID);
							}
						}
					}

					if (!bEqualMOIs)
					{
						UE_LOG(LogInputAutomation, Error, TEXT("Resulting MOI States do not equal reference MOI States!"));
					}
				}

				return true;
			}
		}

		return true;
	}

	// Simple input playback test
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateInputTestSimpleObjects, "Modumate.Input.TestSimpleObjects", EAutomationTestFlags::ClientContext | EAutomationTestFlags::NonNullRHI | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::HighPriority)
	bool FModumateInputTestSimpleObjects::RunTest(const FString& Parameters)
	{
		// TODO: write our own FLoadGameMapCommand and/or FWaitForMapToLoadCommand, so that we don't need to wait
		// an arbitrary amount of time before the previously-loaded level registers as a false positive for FWaitForMapToLoadCommand.
		const FString mapName(TEXT("EditModelLVL"));
		ADD_LATENT_AUTOMATION_COMMAND(FLoadGameMapCommand(mapName));
		ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(1.0f));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitForMapToLoadCommand());

		const FString inputLogName(TEXT("TestSimpleObjects"));
		float playbackSpeed = 4.0f;
		ADD_LATENT_AUTOMATION_COMMAND(FStartInputPlaybackCommand(inputLogName, playbackSpeed));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitForInputPlaybackCommand(FInputPlaybackComparison()));
		ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(5.0f));

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateInputTestComplexRoof, "Modumate.Input.TestComplexRoof", EAutomationTestFlags::ClientContext | EAutomationTestFlags::NonNullRHI | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::HighPriority)
		bool FModumateInputTestComplexRoof::RunTest(const FString& Parameters)
	{
		// TODO: write our own FLoadGameMapCommand and/or FWaitForMapToLoadCommand, so that we don't need to wait
		// an arbitrary amount of time before the previously-loaded level registers as a false positive for FWaitForMapToLoadCommand.
		const FString mapName(TEXT("EditModelLVL"));
		ADD_LATENT_AUTOMATION_COMMAND(FLoadGameMapCommand(mapName));
		ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(1.0f));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitForMapToLoadCommand());

		const FString inputLogName(TEXT("ComplexRoof"));
		float playbackSpeed = 4.0f;
		ADD_LATENT_AUTOMATION_COMMAND(FStartInputPlaybackCommand(inputLogName, playbackSpeed));

		const FString expectedGraphName(TEXT("ComplexRoof.mdmt"));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitForInputPlaybackCommand(FInputPlaybackComparison(expectedGraphName, FString(), true, true, false)));

		ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(5.0f));

		return true;
	}

#endif	// WITH_DEV_AUTOMATION_TESTS
}

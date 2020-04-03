#include "CoreMinimal.h"
#include "EditModelInputAutomation.h"
#include "EditModelGameState_CPP.h"
#include "EditModelPlayerController_CPP.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "ModumateCommands.h"
#include "ModumateGameInstance.h"
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
			gameState->Document.MakeNew(world);
			controller->ModumateCommand(
				Modumate::FModumateCommand(Modumate::Commands::kDebug)
				.Param(TEXT("type"), TEXT("document"))
				.Param(TEXT("on"), true)
			);

			FString inputLogPath = FPaths::ProjectDir() / gameInstance->TestScriptRelativePath /
				(InputLogBaseName + FEditModelInputLog::LogExtension);
			bool bPlayingBack = controller->InputAutomationComponent->BeginPlayback(inputLogPath, false, PlaybackSpeed);
			ensure(bPlayingBack);
		}

		return true;
	}

	DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FWaitForInputPlaybackCommand, int32, ExpectedNumObjects);

	bool FWaitForInputPlaybackCommand::Update()
	{
		UWorld *world = GetGameWorld();
		AEditModelPlayerController_CPP *controller = world ? world->GetFirstPlayerController<AEditModelPlayerController_CPP>() : nullptr;
		AEditModelGameState_CPP *gameState = world ? Cast<AEditModelGameState_CPP>(world->GetGameState()) : nullptr;
		if (ensure(controller && controller->InputAutomationComponent && gameState))
		{
			if (controller->InputAutomationComponent->IsPlaying())
			{
				return false;
			}
			else
			{
				// Take a screenshot, so we can verify the results
				bool bShowUI = true;
				bool bAddFilenameSuffix = false;

				// Make a good filename for the screenshot
				FString buildID;
				FParse::Value(FCommandLine::Get(), TEXT("BuildID="), buildID);
				FString screenshotFileSuffix = buildID.IsEmpty() ? buildID : (FString(TEXT("_Build")) + buildID);
				FString screenshotFilename = FString(TEXT("Screenshot_InputTest")) + screenshotFileSuffix;

				FScreenshotRequest::RequestScreenshot(screenshotFilename, bShowUI, bAddFilenameSuffix);

				// Verify the number of objects
				const FModumateDocument &doc = gameState->Document;
				int32 numObjects = doc.GetObjectInstances().Num();
				if (ExpectedNumObjects != numObjects)
				{
					FString lastLogName = FPaths::GetBaseFilename(controller->InputAutomationComponent->GetLastLogPath());
					UE_LOG(LogInputAutomation, Error, TEXT("Expected %d objects at the ended of playing back \"%s\", found %d objects!"),
						ExpectedNumObjects, *lastLogName, numObjects);
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
		int32 expectedNumObjects = 47;
		ADD_LATENT_AUTOMATION_COMMAND(FStartInputPlaybackCommand(inputLogName, playbackSpeed));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitForInputPlaybackCommand(expectedNumObjects));
		ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(5.0f));

		return true;
	}

#endif	// WITH_DEV_AUTOMATION_TESTS
}

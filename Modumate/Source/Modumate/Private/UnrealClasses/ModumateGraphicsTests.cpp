// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Tests/AutomationCommon.h"
#include "UnrealClasses/EditModelInputAutomation.h"
#include "Stats/Stats.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "Serialization/Csv/CsvParser.h"
#include "HAL/FileManager.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "DocumentManagement/ModumateCommands.h"
#include "UnrealClasses/ModumateGameInstance.h"


namespace Modumate
{
#if WITH_DEV_AUTOMATION_TESTS

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGraphicsPerformanceTest1, "Modumate.Graphics.Performance",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::PerfFilter);

	namespace {
		FString sceneTestFilename("200310_BPHouse_06.mdmt");
		TSharedFuture<FString> writeCsvFuture;

		const float loadToCaptureTime = 2.0f;
		const float captureDuration = 5.0f;
		const TCHAR* requiredStats[] = { TEXT("FrameTime"), TEXT("InitViews_Scene"),  TEXT("InitViews_Shadows")};

		DEFINE_LATENT_AUTOMATION_COMMAND(FGraphicsSceneLoaded);
		bool FGraphicsSceneLoaded::Update()
		{
			const UWorld * currentWorld = GEngine->GetWorldContexts()[0].World();
			const AEditModelGameState* gameState = currentWorld->GetGameState<AEditModelGameState>();
			return !gameState->Document->CurrentProjectName.IsEmpty();
		}

		DEFINE_LATENT_AUTOMATION_COMMAND(FStartCapture);
		bool FStartCapture::Update()
		{
			FCsvProfiler::Get()->BeginCapture();
			return true;
		}

		DEFINE_LATENT_AUTOMATION_COMMAND(FReadStats);
		bool FReadStats::Update()
		{
			FCsvProfiler* csvProfiler = FCsvProfiler::Get();
			writeCsvFuture = csvProfiler->EndCapture();
			return true;
		}

		DEFINE_LATENT_AUTOMATION_COMMAND(FProcessCsvFile);
		bool FProcessCsvFile::Update()
		{
			if (writeCsvFuture.IsReady())
			{
				FString csvFilename = writeCsvFuture.Get();
				writeCsvFuture = TSharedFuture<FString>();

				FString csvContents;
				if (FFileHelper::LoadFileToString(csvContents, *csvFilename))
				{
					FCsvParser csvParser(csvContents);
					const FCsvParser::FRows contents = csvParser.GetRows();
					const int tableSize = contents.Num() - 3;
					if (tableSize > 0)
					{
						const TArray<const TCHAR*>& headers = contents[0];
						for (int rs = 0; rs < sizeof(requiredStats) / sizeof(requiredStats[0]); ++rs)
						{
							const TCHAR* requiredStat = requiredStats[rs];
							int requiredColumn = -1;
							for (int c = 1; c < headers.Num(); ++c)
							{
								if (FCString::Stristr(headers[c], requiredStat))
								{
									requiredColumn = c;
									break;
								}
							}
							if (requiredColumn >= 0)
							{
								double total = 0.0;
								for (int r = 0; r < tableSize; ++r)
								{
									total += FCString::Atod(contents[r + 1][requiredColumn]);
								}
								double stat = total / tableSize;
								UE_LOG(LogPerformance, Display, TEXT("FGraphicsPerformanceTest1: %s = %lf ms"), requiredStat, stat);
							}
						}
					}

					IFileManager::Get().Delete(*csvFilename);
				}
				return true;
			}
			return false;
		}

		DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FGraphicsPerformanceLoadScene, FString, sceneFile);
		bool FGraphicsPerformanceLoadScene::Update()
		{
			UWorld* world = nullptr;

			for (const FWorldContext& worldContext : GEngine->GetWorldContexts())
			{
				if (worldContext.WorldType == EWorldType::Game)
				{
					world = worldContext.World();
					break;
				}
			}

			if (!world)
			{
				return false;
			}

			FString scenePathname =
				FPaths::ProjectDir() / world->GetGameInstance<UModumateGameInstance>()->TestScriptRelativePath / sceneFile;
			
			AEditModelPlayerController * playerController = world->GetFirstPlayerController<AEditModelPlayerController>();
			playerController->LoadModelFilePath(scenePathname, false, false, false);

			return true;
		}

	} // Anonymous namespace


	bool FGraphicsPerformanceTest1::RunTest(const FString& Parameters)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FLoadGameMapCommand("EditModelLVL"));
		ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(2.0f));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitForMapToLoadCommand());
		ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("setres 1600x1200")) );
		ADD_LATENT_AUTOMATION_COMMAND(FGraphicsPerformanceLoadScene(sceneTestFilename));
		ADD_LATENT_AUTOMATION_COMMAND(FGraphicsSceneLoaded);
		ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(loadToCaptureTime));
		ADD_LATENT_AUTOMATION_COMMAND(FStartCapture);
		ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(captureDuration));
		ADD_LATENT_AUTOMATION_COMMAND(FReadStats);
		ADD_LATENT_AUTOMATION_COMMAND(FProcessCsvFile);

		return true;
	}

#endif  // WITH_DEV_AUTOMATION_TESTS
}

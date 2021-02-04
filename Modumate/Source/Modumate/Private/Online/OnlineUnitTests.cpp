// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Database/ModumateObjectEnums.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "Tests/AutomationCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ModumateOnlineTests
{
	UWorld* GetGameWorld()
	{
		for (const FWorldContext& worldContext : GEngine->GetWorldContexts())
		{
			if (worldContext.WorldType == EWorldType::Game)
			{
				return worldContext.World();
			}
		}

		return nullptr;
	}
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FWaitForModumateLoginCommand, float, Timeout);
bool FWaitForModumateLoginCommand::Update()
{
	UWorld* world = ModumateOnlineTests::GetGameWorld();
	UModumateGameInstance* gameInstance = world ? world->GetGameInstance<UModumateGameInstance>() : nullptr;
	if (gameInstance == nullptr)
	{
		return true;
	}

	float elapsed = (FPlatformTime::Seconds() - StartTime);
	ELoginStatus loginStatus = gameInstance->LoginStatus();
	switch (loginStatus)
	{
	case ELoginStatus::Connected:
	{
		UE_LOG(LogUnitTest, Log, TEXT("Successfully logged in after %.2f seconds"), elapsed);
		return true;
	}
	case ELoginStatus::WaitingForRefreshToken:
	case ELoginStatus::WaitingForVerify:
	{
		if (elapsed < Timeout)
		{
			return false;
		}
		else
		{
			UE_LOG(LogUnitTest, Error, TEXT("Timed out waiting for login!"));
			return true;
		}
	}
	default:
	{
		UE_LOG(LogUnitTest, Error, TEXT("Error! Invalid login state: %s"), *EnumValueString(ELoginStatus, loginStatus));
		return true;
	}
	}
}

// Simple input playback test
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateOnlineTestLogin, "Modumate.Online.Login", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::HighPriority)
bool FModumateOnlineTestLogin::RunTest(const FString& Parameters)
{
	UWorld* world = ModumateOnlineTests::GetGameWorld();
	UModumateGameInstance* gameInstance = world ? world->GetGameInstance<UModumateGameInstance>() : nullptr;
	if (gameInstance == nullptr)
	{
		return false;
	}

	FString validUsername, validPassword;
	if (!GConfig->GetString(TEXT("ModumateTestingConfig"), TEXT("ValidUsername"), validUsername, GEditorIni) ||
		!GConfig->GetString(TEXT("ModumateTestingConfig"), TEXT("ValidPassword"), validPassword, GEditorIni))
	{
		return false;
	}

	gameInstance->Login(validUsername, validPassword, FString());

	ADD_LATENT_AUTOMATION_COMMAND(FWaitForModumateLoginCommand(4.0f));

	return true;
}

#endif	// WITH_DEV_AUTOMATION_TESTS

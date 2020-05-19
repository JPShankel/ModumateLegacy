// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/ModumateConsole.h"

#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "DocumentManagement/ModumateCommands.h"
#include "UnrealClasses/ModumateGameInstance.h"


UModumateConsole::UModumateConsole(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UModumateConsole::AugmentRuntimeAutoCompleteList(TArray<FAutoCompleteCommand>& List)
{
	UWorld *world = GetWorld();
	UModumateGameInstance *modGameInst = world ? world->GetGameInstance<UModumateGameInstance>() : nullptr;
	if (modGameInst)
	{
		TMap<FString, FString> registeredCommands;
		modGameInst->GetRegisteredCommands(registeredCommands);

		static const FString cmdPrefix(TEXT("Modumate "));
		static const FColor cmdColor(93, 193, 155);

		for (const auto &kvp : registeredCommands)
		{
			FAutoCompleteCommand autoCompleteCommand;
			autoCompleteCommand.Command = cmdPrefix + kvp.Key;
			autoCompleteCommand.Desc = kvp.Value;
			autoCompleteCommand.Color = cmdColor;

			List.Add(MoveTemp(autoCompleteCommand));
		}
	}
}

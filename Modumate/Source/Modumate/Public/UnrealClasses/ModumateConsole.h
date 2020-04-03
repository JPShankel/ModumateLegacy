// Copyright 2018 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/Console.h"

#include "ModumateConsole.generated.h"

/**
 *
 */
UCLASS()
class MODUMATE_API UModumateConsole : public UConsole
{
	GENERATED_UCLASS_BODY()

	virtual void AugmentRuntimeAutoCompleteList(TArray<FAutoCompleteCommand>& List) override;
};

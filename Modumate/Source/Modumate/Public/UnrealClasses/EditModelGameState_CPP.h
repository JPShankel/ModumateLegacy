// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DocumentManagement/ModumateDocument.h"
#include "ModumateCore/ModumateConsoleCommand.h"
#include "Runtime/Engine/Classes/GameFramework/GameStateBase.h"
#include "ToolsAndAdjustments/Interface/EditModelToolInterface.h"
#include "EditModelGameState_CPP.generated.h"


/**
 *
 */

UCLASS(config=Game)
class MODUMATE_API AEditModelGameState_CPP : public AGameStateBase
{
	GENERATED_BODY()


public:

	AEditModelGameState_CPP();
	~AEditModelGameState_CPP();

	FModumateDocument Document;

	UFUNCTION(BlueprintCallable, Category = "Shopping")
	TArray<float> GetComponentsThicknessWithKey(EToolMode Mode, const FBIMKey& AssemblyKey) const;

	UFUNCTION(BlueprintCallable, Category = "Modumate Document")
	bool GetPortalToolTip(EToolMode Mode, const FBIMKey& AssemblyKey, FString& Type, FString& ConfigName, TArray<FString>& Parts);
};

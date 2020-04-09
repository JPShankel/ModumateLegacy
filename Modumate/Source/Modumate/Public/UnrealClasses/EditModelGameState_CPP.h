// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModumateDocument.h"
#include "ModumateConsoleCommand.h"
#include "ModumateCrafting.h"
#include "Runtime/Engine/Classes/GameFramework/GameStateBase.h"
#include "EditModelToolInterface.h"
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

	Modumate::FModumateDocument Document;

	const FModumateObjectAssembly *GetAssemblyByKey_DEPRECATED(EToolMode mode, const FName &key) const;
	const FModumateObjectAssembly *GetAssemblyByKey(const FName &key) const;

	// Assemblies & Components (aka Layers)
	UFUNCTION(BlueprintCallable, Category = "Shopping")
	TArray<FShoppingItem> GetAssembliesForToolMode(EToolMode mode) const;

	// WIP Marketplace Assemblies
	UFUNCTION(BlueprintCallable, Category = "Shopping")
	TArray<FShoppingItem> GetMarketplaceAssembliesForToolMode(EToolMode mode) const;

	// WIP Import Marketplace Assemblies to normal assemblies db
	UFUNCTION(BlueprintCallable, Category = "Shopping")
	void ImportAssemblyFromMarketplace(EToolMode mode, const FName &key);

	UFUNCTION(BlueprintCallable, Category = "Shopping")
	TArray<FShoppingItem> GetComponentsForAssembly(EToolMode mode, const FShoppingItem &item) const;

	UFUNCTION(BlueprintCallable, Category = "Shopping")
	TArray<float> GetComponentsThicknessWithKey(EToolMode mode, const FString &assemblyKey) const;

	// Return number of affected
	UFUNCTION(BlueprintCallable, Category = "Shopping")
	int32 CheckRemoveAssembly(EToolMode mode, const FString &assemblyKey);

	UFUNCTION(BlueprintCallable, Category = "Shopping")
	bool DoRemoveAssembly(EToolMode mode, const FString &assemblyKey, const FString &replaceAssemblyKey);

	UFUNCTION(BlueprintCallable, Category = "Modumate Document")
	bool GetPortalToolTip(EToolMode mode, const FName &assemblyKey, FString &type, FString &configName, TArray<FString> &parts);
};

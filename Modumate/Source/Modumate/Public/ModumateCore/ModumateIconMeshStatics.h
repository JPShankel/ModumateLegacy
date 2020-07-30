// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UnrealClasses/DynamicMeshActor.h"
#include "ModumateIconMeshStatics.generated.h"


/**
*
*/


UCLASS()
class MODUMATE_API UModumateIconMeshStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "Modumate IconMesh")
		static bool GetMeshesFromShoppingItem(AEditModelPlayerController_CPP *Controller, const FName &AsmKey, EToolMode FromToolMode, TArray<UStaticMesh*>& TargetComps, bool bMarketplaceAsm = false);

	UFUNCTION(BlueprintCallable, Category = "Modumate IconMesh")
		static bool GetEngineMaterialByKey(AEditModelPlayerController_CPP *Controller, const FName &Key, UMaterialInterface* &ModuleMaterial);
	
	UFUNCTION(BlueprintCallable, Category = "Modumate IconMesh")
		static bool GetEngineCustomColorByKey(AEditModelPlayerController_CPP *Controller, const FName &Key, FCustomColor &ModuleColor);

	UFUNCTION(BlueprintCallable, Category = "Modumate IconMesh")
		static bool GetEngineStaticIconTextureByKey(AEditModelPlayerController_CPP *Controller, const FName &Key, FStaticIconTexture &StaticIcon);

	UFUNCTION(BlueprintCallable, Category = "Modumate IconMesh")
		static bool MakeIconMeshFromPofileKey(
			AEditModelPlayerController_CPP *Controller,
			ADynamicMeshActor *DynamicMeshActor,
			EToolMode FromToolMode, 
			const FName &ProfileKey,
			const FVector &RootLoation,
			float Length = 25.f);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "Modumate IconMesh")
		static bool GetEngineMaterialByPresetKey(
			UObject* WorldContextObject, 
			const FName &PresetKey, 
			UMaterialInterface* &ModuleMaterial,
			FCustomColor &ModuleColor);
};

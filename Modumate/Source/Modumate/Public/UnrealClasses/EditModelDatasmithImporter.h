// Copyright 2022 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DatasmithRuntime.h"
#include "Online/ModumateCloudConnection.h"
#include "BIMKernel/AssemblySpec/BIMAssemblySpec.h"
#include "EditModelDatasmithImporter.generated.h"

USTRUCT()
struct MODUMATE_API FAssetRequest
{
	GENERATED_BODY()

	using FCallback = TFunction<void()>;

	FAssetRequest() {}
	FAssetRequest(const FBIMAssemblySpec& InAssembly, const FCallback& InCallBack) :
		Assembly(InAssembly), CallbackTask(InCallBack) {}

	FBIMAssemblySpec Assembly;
	FCallback CallbackTask = nullptr;
};

UENUM(BlueprintType)
enum class EAssetImportLoadStatus : uint8
{
	None,
	Loading,
	Loaded
};

UCLASS()
class MODUMATE_API AEditModelDatasmithImporter : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AEditModelDatasmithImporter();

	UPROPERTY(EditAnywhere)
	USceneComponent* Root;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TSubclassOf<class AEditModelDatasmithRuntimeActor> DatasmithRuntimeActorClass;

	UPROPERTY()
	TWeakObjectPtr<class AEditModelDatasmithRuntimeActor> CurrentDatasmithRuntimeActor = nullptr;

	// This is the time in second for calling a method to check whether a second attempt is necessary
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float CheckAfterImportTime = 5.f;

	TMap<FGuid, TArray<UStaticMesh*>> StaticMeshAssetMap;
	TMap<FGuid, TArray<FTransform>> StaticMeshTransformMap;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TMap<FGuid, EAssetImportLoadStatus> PresetLoadStatusMap;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TMap<FGuid, class AEditModelDatasmithRuntimeActor*> PresetToDatasmithRuntimeActorMap;

	FTimerHandle AssetImportCheckTimer;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:

	UFUNCTION(BlueprintCallable)
	bool ImportDatasmithFromDialogue();

	UFUNCTION(BlueprintCallable)
	bool ImportDatasmithFromURL(const FString& URL);

	void RequestCompleteCallback(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void HandleAssetRequest(const FAssetRequest& InAssetRequest);

	void OnRuntimeActorImportDone(class AEditModelDatasmithRuntimeActor* FromActor);

	UFUNCTION()
	void OnAssetImportCheckTimer();
};
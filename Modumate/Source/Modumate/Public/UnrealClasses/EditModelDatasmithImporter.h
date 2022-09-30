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
	Downloading,
	Loading,
	Loaded
};

UENUM(BlueprintType)
enum class EDatasmithMaterialType : uint8
{
	None,
	PbrOpaque,
	PbrOpaque2Sided,
	Opaque,
	PbrTranslucent,
	PbrTranslucent2Sided,
	Transparent,
	Cutout
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

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "ModumateDatasmith")
	UMaterial* Modumate_DS_Cutout;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "ModumateDatasmith")
	UMaterial* Modumate_DS_Opaque;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "ModumateDatasmith")
	UMaterial* Modumate_DS_PbrOpaque;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "ModumateDatasmith")
	UMaterial* Modumate_DS_PbrOpaque_2Sided;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "ModumateDatasmith")
	UMaterial* Modumate_DS_PbrTranslucent;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "ModumateDatasmith")
	UMaterial* Modumate_DS_PbrTranslucent_2Sided;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "ModumateDatasmith")
	UMaterial* Modumate_DS_Transparent;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "OriginalDatasmith")
	UMaterial* Original_DS_Cutout;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "OriginalDatasmith")
	UMaterial* Original_DS_Opaque;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "OriginalDatasmith")
	UMaterial* Original_DS_PbrOpaque;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "OriginalDatasmith")
	UMaterial* Original_DS_PbrOpaque_2Sided;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "OriginalDatasmith")
	UMaterial* Original_DS_PbrTranslucent;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "OriginalDatasmith")
	UMaterial* Original_DS_PbrTranslucent_2Sided;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "OriginalDatasmith")
	UMaterial* Original_DS_Transparent;

	// This is the time in second for calling a method to check whether a second attempt is necessary
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float CheckAfterImportTime = 5.f;

	TMap<FGuid, TArray<UStaticMesh*>> StaticMeshAssetMap;
	TMap<FGuid, TArray<FTransform>> StaticMeshTransformMap;
	TMap<UStaticMesh*, TArray<UMaterialInstanceDynamic*>> ImportedMaterialMap;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TMap<FGuid, EAssetImportLoadStatus> PresetLoadStatusMap;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TMap<FGuid, class AEditModelDatasmithRuntimeActor*> PresetToDatasmithRuntimeActorMap;

	FTimerHandle AssetImportCheckTimer;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:

	// Command code for testing import via modal dialog
	UFUNCTION(BlueprintCallable)
	bool ImportDatasmithFromDialogue();

	// Command code for testing via URL
	UFUNCTION(BlueprintCallable)
	bool ImportDatasmithFromWeb(const FString& URL);

	UFUNCTION(BlueprintCallable)
	UMaterialInterface* GetModumateDatasmithMaterialByType(EDatasmithMaterialType InType);

	UFUNCTION(BlueprintCallable)
	EDatasmithMaterialType GetTypeFromOriginalDatasmithMaterial(const UMaterialInterface* InOriginalMaterial);

	UFUNCTION()
	bool RequestDownloadFromURL(const FGuid& InGUID, const FString& URL);

	void DownloadCompleteCallback(const FGuid& InGUID, FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void HandleAssetRequest(const FAssetRequest& InAssetRequest);

	void OnRuntimeActorImportDone(class AEditModelDatasmithRuntimeActor* FromActor);

	UFUNCTION()
	void OnAssetImportCheckTimer();

	bool AddDatasmithRuntimeActor(const FGuid& InGUID, const FString& DatasmithImportPath);

	bool GetZipFilePathFromPresetGUID(const FGuid& InGUID, FString& OutFilePath);
	bool GetFolderPathFromPresetGUID(const FGuid& InGUID, FString& OutFolderPath);
	bool ImportDatasmithFilesFromPresetFolder(const FGuid& InGUID);
};
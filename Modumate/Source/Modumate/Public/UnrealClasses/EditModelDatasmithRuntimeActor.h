// Copyright 2022 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DatasmithRuntime.h"
#include "EditModelDatasmithRuntimeActor.generated.h"

USTRUCT(BlueprintType)
struct MODUMATE_API FDatasmithMetaDataMaterial
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 Index = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString TintColor = TEXT("FFFFFF");

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float TintColorFading = 0.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float DiffuseMapFading = 1.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString DiffuseColor = TEXT("BCBCBC");

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString DiffuseMap;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float MetallicMapFading = 0.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float Metallic = 0.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString MetallicMap;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float SpecularMapFading = 0.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float Specular = 0.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString SpecularMap;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float RoughnessMapFading = 0.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float Roughness = 0.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString RoughnessMap;
};

USTRUCT(BlueprintType)
struct MODUMATE_API FDatasmithMetaData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 Version = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FDatasmithMetaDataMaterial> Materials;
};

UCLASS()
class MODUMATE_API AEditModelDatasmithRuntimeActor : public ADatasmithRuntimeActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AEditModelDatasmithRuntimeActor();

	UPROPERTY()
	TWeakObjectPtr<class AEditModelDatasmithImporter> DatasmithImporter = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FGuid PresetKey;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString ImportFilePath;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString AssetFolderPath;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<class UStaticMesh*> StaticMeshRefs;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FTransform> StaticMeshTransforms;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<class UMaterialInterface*> StaticMeshMaterials;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FDatasmithMetaData CurrentMetaData;

	virtual void OnImportEnd() override;

	bool MakeFromImportFilePath();
	void GetAttachedActorsRecursive(AActor* ParentActor, TArray<AActor*>& OutActors);
	bool GetValuesFromMetaData();

	// Helper for writing MetaData
	UFUNCTION(BlueprintCallable)
	static FString DatasmithColorToHex(const FColor& InColor);

	UFUNCTION(BlueprintCallable)
	static FColor DatasmithHexToColor(const FString& InHexString);

	UFUNCTION(BlueprintCallable)
	static FString MakeJsonStringFromDatasmithMetaData(const FDatasmithMetaData& InData);
};
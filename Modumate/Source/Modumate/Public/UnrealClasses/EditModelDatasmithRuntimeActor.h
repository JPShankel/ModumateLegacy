// Copyright 2022 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DatasmithRuntime.h"
#include "EditModelDatasmithRuntimeActor.generated.h"

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
	TArray<class UStaticMesh*> StaticMeshRefs;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FTransform> StaticMeshTransforms;

	virtual void OnImportEnd() override;

	bool MakeFromImportFilePath();
	void GetAttachedActorsRecursive(AActor* ParentActor, TArray<AActor*>& OutActors);
};
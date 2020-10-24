// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Database/ModumateObjectEnums.h"
#include "BIMKernel/BIMAssemblySpec.h"
#include "GameFramework/Actor.h"

#include "CompoundMeshActor.generated.h"

struct FLightConfiguration;
class UPointLightComponent;
class USpotLightComponent;
class UStaticMeshComponent;
class UProceduralMeshComponent;

UCLASS()
class MODUMATE_API ACompoundMeshActor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ACompoundMeshActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	void MakeFromAssembly(const FBIMAssemblySpec &ObAsm, FVector Scale, bool bLateralInvert, bool bMakeCollision);
	void UpdateLightFromLightConfig(UStaticMeshComponent* parentMesh, const FLightConfiguration &lightConfig, const FTransform &lightTransform);
	void RemoveAllLights();

	bool ConvertProcMeshToLinesOnPlane(const FVector &PlanePosition, const FVector &PlaneNormal, TArray<TPair<FVector, FVector>> &OutEdges);
	bool ConvertStaticMeshToLinesOnPlane(const FVector &PlanePosition, const FVector &PlaneNormal, TArray<TPair<FVector, FVector>> &OutEdges);

	void SetIsDynamic(bool DynamicStatus);
	bool GetIsDynamic() const { return bIsDynamic; }

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	TArray<bool> UseSlicedMesh;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	TArray<UStaticMeshComponent*> StaticMeshComps;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	TArray<UProceduralMeshComponent*> NineSliceComps;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	TArray<UProceduralMeshComponent*> NineSliceLowLODComps;

	TArray<TMap<FName, int32>> MaterialIndexMappings;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UPointLightComponent* PointLightComp;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	USpotLightComponent* SpotLightComp;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	bool bIsDynamic = true;


	// Temporary MOI data, used for objects to refer to the data they would use
	// if they were to become a fully-fledged MOI, by systems like tools.
	FBIMKey TempAssemblyKey;
	EToolMode TempObjectToolMode;

private:
	void ResetProcMeshComponents(TArray<UProceduralMeshComponent*> &ProcMeshComps, int32 maxNumMeshes);
	bool InitializeProcMeshComponent(TArray<UProceduralMeshComponent*> &ProcMeshComps, USceneComponent *rootComp, int32 index);
	void CalculateNineSliceComponents(TArray<UProceduralMeshComponent*> &ProcMeshComps, USceneComponent *rootComp, const int32 LodIndex, int32 sliceCompIdxStart, FBox &nineSliceInterior, const FVector &partNativeSize);
};

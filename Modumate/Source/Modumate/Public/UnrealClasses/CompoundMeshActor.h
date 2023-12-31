// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Objects/ModumateObjectEnums.h"
#include "BIMKernel/AssemblySpec/BIMAssemblySpec.h"
#include "BIMKernel/AssemblySpec/BIMPartLayout.h"
#include "UnrealClasses/EditModelDatasmithImporter.h"
#include "GameFramework/Actor.h"

#include "CompoundMeshActor.generated.h"

struct FLightConfiguration;
class UPointLightComponent;
class USpotLightComponent;
class UStaticMeshComponent;
class UProceduralMeshComponent;
class UModumateDocument;
class FDraftingComposite;
class FDrawingDesignerLine;
enum class FModumateLayerType;


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
	static int32 CurrentLightCount;
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	void MakeFromAssemblyPart(const FBIMAssemblySpec& ObAsm, int32 PartIndex, FVector Scale, bool bLateralInvert, bool bMakeCollision);
	void MakeFromAssembly(const FBIMAssemblySpec& ObAsm, FVector Scale, bool bLateralInvert, bool bMakeCollision);
	bool MakeFromAssemblyPartAsync(const FAssetRequest& InAssetRequest, int32 PartIndex, FVector Scale, bool bLateralInvert, bool bMakeCollision);

	void SetupCapGeometry();
	void ClearCapGeometry();

	void UpdateLightFromLightConfig(UStaticMeshComponent* parentMesh, const FLightConfiguration &lightConfig);
	void RemoveAllLights();

	bool ConvertProcMeshToLinesOnPlane(const FVector &PlanePosition, const FVector &PlaneNormal, TArray<TPair<FVector, FVector>> &OutEdges);
	bool ConvertStaticMeshToLinesOnPlane(const FVector &PlanePosition, const FVector &PlaneNormal, TArray<TPair<FVector, FVector>> &OutEdges);

	void SetIsDynamic(bool DynamicStatus);
	bool GetIsDynamic() const { return bIsDynamic; }

	bool GetCutPlaneDraftingLines(const TSharedPtr<FDraftingComposite>& ParentPage, const FPlane& Plane,
		const FVector& AxisX, const FVector& AxisY, const FVector& Origin, FModumateLayerType LayerType) const;
	void GetFarDraftingLines(const TSharedPtr<FDraftingComposite>& ParentPage, const FPlane& Plane, const FBox2D& BoundingBox, FModumateLayerType LayerType) const;
	float GetPortalCenter(const UModumateDocument* Doc, const FGuid& AssemblyGUID) const;

	void GetDrawingDesignerLines(const FVector& ViewDirection, TArray<FDrawingDesignerLine>& Outlines, float MinLength,
		float AngleTolerance = 0.9205f, bool bFastMode = true) const;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	TArray<bool> UseSlicedMesh;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	TArray<UStaticMeshComponent*> StaticMeshComps;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	UStaticMeshComponent* ProxyStaticMesh;

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
	EToolMode TempObjectToolMode;

	// The part layout that's filled and evaluated by a given assembly;
	// cached so it can provide calculated values and potentially avoid re-evaluating itself unnecessarily.
	FBIMPartLayout CachedPartLayout;

	//should be called when ray tracing enabled is toggled
	void RayTracingEnabled_OnToggled();

private:
	//will hold the original light intensity of the light in lumens
	float OriginalLightIntensity = 0.0f;
	//the amount to reduce light intensity when not in RayTracing
	static const float LightReductionFactor;
	void ResetProcMeshComponents(TArray<UProceduralMeshComponent*> &ProcMeshComps, int32 maxNumMeshes);
	void ResetStaticMeshComponents(int32 maxNumMeshes);
	bool InitializeProcMeshComponent(TArray<UProceduralMeshComponent*> &ProcMeshComps, USceneComponent *rootComp, int32 index);
	void CalculateNineSliceComponents(TArray<UProceduralMeshComponent*> &ProcMeshComps, USceneComponent *rootComp, const int32 LodIndex, int32 sliceCompIdxStart, FBox &nineSliceInterior, const FVector &partNativeSize, const UStaticMeshComponent* StaticMeshRef);

	bool GetOrAddProceduralMeshCap(int32 CapId, UProceduralMeshComponent*& OutMesh);
	TArray<UProceduralMeshComponent*> ProceduralMeshCaps;

	FVector ProxyStaticMeshDimension = FVector::OneVector;
};

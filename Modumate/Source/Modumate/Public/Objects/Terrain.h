// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "Objects/ModumateObjectInstance.h"

#include "Terrain.generated.h"


USTRUCT()
struct MODUMATE_API FMOITerrainData
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Origin;

	UPROPERTY()
	FString Name;

	UPROPERTY()
	TMap<int32, double> Heights;
};


class ADynamicTerrainActor;

UCLASS()
class MODUMATE_API AMOITerrain : public AModumateObjectInstance
{
	GENERATED_BODY()

public:
	AMOITerrain();

	virtual FVector GetCorner(int32 index) const override;
	virtual int32 GetNumCorners() const override;
	virtual FVector GetNormal() const override;
	virtual FVector GetLocation() const override;
	virtual FQuat GetRotation() const override;
	virtual bool GetTransformedLocationState(const FTransform Transform, FMOIStateData& OutState) const override;

	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;
	virtual bool GetUpdatedVisuals(bool& bOutVisible, bool& bOutCollisionEnabled) override;
	virtual void PreDestroy() override;

	virtual void GetDraftingLines(const TSharedPtr<FDraftingComposite>& ParentPage, const FPlane& Plane,
		const FVector& AxisX, const FVector& AxisY, const FVector& Origin, const FBox2D& BoundingBox,
		TArray<TArray<FVector>>& OutPerimeters) const override;

	const TArray<ADynamicTerrainActor*>& GetTerrainActors() const
		{ return TerrainActors; }


	UPROPERTY()
	FMOITerrainData InstanceData;

protected:
	UPROPERTY()
	TArray<ADynamicTerrainActor*> TerrainActors;

	void UpdateTerrainActors();
	bool SetupTerrainMaterial(ADynamicTerrainActor* Actor);

	FVector GraphToWorldPosition(FVector2D GraphPos, double Height = 0.0) const;
};

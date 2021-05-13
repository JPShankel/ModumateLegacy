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
	TMap<int32, double> Heights;
};


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

	virtual AActor* CreateActor(const FVector& loc, const FQuat& rot) override;
	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;
	virtual bool GetUpdatedVisuals(bool& bOutVisible, bool& bOutCollisionEnabled) override;

	UPROPERTY()
	FMOITerrainData InstanceData;

protected:
	void UpdateTerrainActor();
	bool SetupTerrainMaterial();

};

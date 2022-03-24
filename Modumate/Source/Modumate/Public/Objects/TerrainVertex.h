// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "Objects/VertexBase.h"

#include "TerrainVertex.generated.h"


USTRUCT()
struct MODUMATE_API FMOITerrainVertexData
{
	GENERATED_BODY()

	FMOITerrainVertexData();

	UPROPERTY()
	double Height = 30.4799995;
};

UCLASS()
class MODUMATE_API AMOITerrainVertex : public AMOIVertexBase
{
	GENERATED_BODY()

public:
	AMOITerrainVertex();

	virtual FVector GetLocation() const override;

	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;

	virtual void RegisterInstanceDataUI(class UToolTrayBlockProperties* PropertiesUI) override;

	virtual bool FromWebMOI(const FString& InJson) override;

	UFUNCTION()
	void OnInstPropUIChangedHeight(float NewHeight);

	UPROPERTY()
	FMOITerrainVertexData InstanceData;

protected:

	FVector CachedLocation { ForceInitToZero };
	float CachedHeight;
};

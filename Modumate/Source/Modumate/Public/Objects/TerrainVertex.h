// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "Objects/VertexBase.h"

#include "TerrainVertex.generated.h"

UCLASS()
class MODUMATE_API AMOITerrainVertex : public AMOIVertexBase
{
	GENERATED_BODY()

public:
	AMOITerrainVertex();

	virtual FVector GetLocation() const override;

	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;

	virtual void RegisterInstanceDataUI(class UToolTrayBlockProperties* PropertiesUI) override;

	UFUNCTION()
	void OnInstPropUIChangedHeight(float NewHeight);

protected:

	FVector CachedLocation { ForceInitToZero };
	float CachedHeight;
};

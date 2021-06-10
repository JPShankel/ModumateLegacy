// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "Objects/PlaneBase.h"

#include "TerrainMaterial.generated.h"

USTRUCT()
struct MODUMATE_API FMOITerrainMaterialData
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid Material;
};

UCLASS()
class MODUMATE_API AMOITerrainMaterial : public AMOIPlaneBase
{
	GENERATED_BODY()

public:
	AMOITerrainMaterial();

	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;
	virtual void SetupAdjustmentHandles(AEditModelPlayerController*) override { };
	virtual void RegisterInstanceDataUI(class UToolTrayBlockProperties* PropertiesUI) override;
	virtual void PostCreateObject(bool bNewObject) override;

	UFUNCTION()
	void OnInstPropUIChangedSelection(int32 NewValue);

	UPROPERTY()
	FMOITerrainMaterialData InstanceData;

protected:
	bool UpdateStructure();
	void SetupSiteMaterials();
	static const FBIMTagPath SiteNcp;

	FTransform CachedTransform;
	TArray<FGuid> MaterialGuids;
	int32 CachedSelection;
};

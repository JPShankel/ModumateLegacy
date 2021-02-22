// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "Objects/PlaneBase.h"

#include "BackgroundImage.generated.h"

USTRUCT()
struct MODUMATE_API FMOIBackgroundImageData
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Location;

	UPROPERTY()
	FQuat Rotation;

	UPROPERTY()
	float Scale = 1.0f;

	UPROPERTY()
	FString SourceFilename;
};

UCLASS()
class MODUMATE_API AMOIBackgroundImage: public AMOIPlaneBase
{
	GENERATED_BODY()

public:
	AMOIBackgroundImage();

	virtual void PostCreateObject(bool bNewObject) override;
	virtual void SetupDynamicGeometry() override;
	virtual void SetupAdjustmentHandles(AEditModelPlayerController* controller) override;
	virtual void GetUpdatedVisuals(bool& bOutVisible, bool& bOutCollisionEnabled) override;

	virtual void RegisterInstanceDataUI(UToolTrayBlockProperties* PropertiesUI) override;
	UFUNCTION()
	void OnInstPropUIChangedScale(float NewScale);

	virtual FVector GetCorner(int32 index) const override;
	virtual int32 GetNumCorners() const override;
	virtual FVector GetNormal() const override;
	virtual FVector GetLocation() const override;
	virtual FQuat GetRotation() const override;

	virtual bool GetTransformedLocationState(const FTransform Transform, FMOIStateData& OutState) const override;

	FVector2D GetImageSize() const { return ImageSize; }

	UPROPERTY()
	FMOIBackgroundImageData InstanceData;

protected:
	void UpdateCachedGeometryData();
	void UpdateImageData();
	bool UpdateImage(const FString& ImageFile);
	void CreateActorMesh(UProceduralMeshComponent* Mesh);

	FString CachedFilename;
	FVector2D ImageSize;
	float ImageScale = 0.0f;

	static const FModumateUnitValue DefaultSize;
	static const FColor UnselectedColor;
	static const FColor SelectedColor;
};

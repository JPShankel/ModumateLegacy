// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "Objects/PlaneBase.h"

#include "BackgroundImage.generated.h"

USTRUCT()
struct MODUMATE_API FMOIBackgroundImageData
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	FQuat Rotation = FQuat::Identity;

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
	void PreDestroy() override;

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


	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint>& outPoints, TArray<FStructureLine>& outLines,
		bool bForSnapping, bool bForSelection) const override;

protected:
	void UpdateCachedGeometryData();
	void UpdateImageData();
	bool UpdateImage(const FString& ImageFile);
	void CreateActorMesh(UProceduralMeshComponent* Mesh);

	FString CachedFilename;
	FVector2D ImageSize;
	float ImageScale = 0.0f;

	static const FModumateUnitValue DefaultSize;
	static const FModumateUnitValue PlaneDisplacement;
	static const FColor UnselectedColor;
	static const FColor SelectedColor;
};

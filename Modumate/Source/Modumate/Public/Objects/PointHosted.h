// Copyright 2021 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/ModumateObjectInstance.h"

#include "PointHosted.generated.h"

USTRUCT()
struct MODUMATE_API FMOIPointHostedData
{
	GENERATED_BODY()

	FMOIPointHostedData();

	UPROPERTY()
	FVector FlipSigns = FVector::OneVector;

	UPROPERTY()
	FDimensionOffset OffsetX;

	UPROPERTY()
	FDimensionOffset OffsetY;

	UPROPERTY()
	FDimensionOffset OffsetZ;

	UPROPERTY()
	FRotator Rotation = FRotator::ZeroRotator;
};

UCLASS()
class MODUMATE_API AMOIPointHosted : public AModumateObjectInstance
{
	GENERATED_BODY()

public:
	AMOIPointHosted();

	virtual AActor* CreateActor(const FVector& loc, const FQuat& rot) override;
	virtual void SetupDynamicGeometry() override;
	virtual void UpdateDynamicGeometry() override;
	virtual void RegisterInstanceDataUI(class UToolTrayBlockProperties* PropertiesUI) override;
	virtual bool GetOffsetState(const FVector& AdjustmentDirection, FMOIStateData& OutState) const override;
	virtual bool GetFlippedState(EAxis::Type FlipAxis, FMOIStateData& OutState) const override;
	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint>& outPoints, TArray<FStructureLine>& outLines, bool bForSnapping, bool bForSelection) const override;
	virtual void ToggleAndUpdateCapGeometry(bool bEnableCap) override;
	virtual void GetDraftingLines(const TSharedPtr<FDraftingComposite>& ParentPage, const FPlane& Plane, const FVector& AxisX, const FVector& AxisY, const FVector& Origin,
		const FBox2D& BoundingBox, TArray<TArray<FVector>>& OutPerimeters) const override;
	virtual void GetDrawingDesignerItems(const FVector& ViewDirection, TArray<FDrawingDesignerLine>& OutDrawingLines, float MinLength = 0.0f) const override;
	virtual bool ProcessQuantities(FQuantitiesCollection& QuantitiesVisitor) const override;

	UPROPERTY()
	FMOIPointHostedData InstanceData;

protected:
	void InternalUpdateGeometry(bool bCreateCollision);

	UFUNCTION()
	void OnInstPropUIChangedFlip(int32 FlippedAxisInt);

	UFUNCTION()
	void OnInstPropUIChangedOffsetX(const FDimensionOffset& NewValue);

	UFUNCTION()
	void OnInstPropUIChangedOffsetY(const FDimensionOffset& NewValue);

	UFUNCTION()
	void OnInstPropUIChangedOffsetZ(const FDimensionOffset& NewValue);

	UFUNCTION()
	void OnInstPropUIChangedRotationX(float NewValue);

	UFUNCTION()
	void OnInstPropUIChangedRotationY(float NewValue);

	UFUNCTION()
	void OnInstPropUIChangedRotationZ(float NewValue);

	virtual void UpdateQuantities() override;
};

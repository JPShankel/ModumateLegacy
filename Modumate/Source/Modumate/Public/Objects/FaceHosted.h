// Copyright 2022 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/ModumateObjectInstance.h"

#include "FaceHosted.generated.h"

USTRUCT()
struct MODUMATE_API FMOIFaceHostedData
{
	GENERATED_BODY()

	FMOIFaceHostedData();

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

	UPROPERTY()
	int32 BasisEdge = 0;

	// this is a read only cached value
	UPROPERTY()
	int32 NumEdges = 0;
};

UCLASS()
class MODUMATE_API AMOIFaceHosted : public AModumateObjectInstance
{
	GENERATED_BODY()

public:
	AMOIFaceHosted();

	virtual AActor* CreateActor(const FVector& loc, const FQuat& rot) override;
	virtual void SetupDynamicGeometry() override;
	virtual void UpdateDynamicGeometry() override;
	virtual void RegisterInstanceDataUI(class UToolTrayBlockProperties* PropertiesUI) override;
	virtual bool GetFlippedState(EAxis::Type FlipAxis, FMOIStateData& OutState) const override;
	virtual bool GetOffsetState(const FVector& AdjustmentDirection, FMOIStateData& OutState) const override;
	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint>& outPoints, TArray<FStructureLine>& outLines, bool bForSnapping, bool bForSelection) const override;
	virtual void ToggleAndUpdateCapGeometry(bool bEnableCap) override;
	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;
	virtual void GetDraftingLines(const TSharedPtr<FDraftingComposite>& ParentPage, const FPlane& Plane,
		const FVector& AxisX, const FVector& AxisY, const FVector& Origin, const FBox2D& BoundingBox,
		TArray<TArray<FVector>>& OutPerimeters) const override;
	virtual void GetDrawingDesignerItems(const FVector& ViewDirection, TArray<FDrawingDesignerLine>& OutDrawingLines, float MinLength = 0.0f) const override;
	virtual bool ProcessQuantities(FQuantitiesCollection& QuantitiesVisitor) const override;

	UPROPERTY()
	FMOIFaceHostedData InstanceData;

protected:

	void InternalUpdateGeometry(bool bCreateCollision);

	UFUNCTION()
	void OnInstPropUIChangedCycle(int32 BasisValue);

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

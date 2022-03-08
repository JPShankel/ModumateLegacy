// Copyright 2021 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/ModumateObjectInstance.h"

#include "EdgeHosted.generated.h"

USTRUCT()
struct MODUMATE_API FMOIEdgeHostedData
{
	GENERATED_BODY()

	FMOIEdgeHostedData();

	UPROPERTY()
	FVector FlipSigns = FVector::OneVector;

	UPROPERTY()
	FDimensionOffset OffsetUp;

	UPROPERTY()
	FDimensionOffset OffsetNormal;

	// X = Extend start length, Y = Extend end length
	UPROPERTY()
	FVector2D Extensions = FVector2D::ZeroVector;

	UPROPERTY()
	float Rotation = 0.f;
};

UCLASS()
class MODUMATE_API AMOIEdgeHosted : public AModumateObjectInstance
{
	GENERATED_BODY()

public:
	AMOIEdgeHosted();

	virtual AActor* CreateActor(const FVector& loc, const FQuat& rot) override;
	virtual void SetupDynamicGeometry() override;
	virtual void UpdateDynamicGeometry() override;
	virtual void RegisterInstanceDataUI(class UToolTrayBlockProperties* PropertiesUI) override;
	virtual bool GetOffsetState(const FVector& AdjustmentDirection, FMOIStateData& OutState) const override;
	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint>& outPoints, TArray<FStructureLine>& outLines, bool bForSnapping, bool bForSelection) const override;
	virtual void ToggleAndUpdateCapGeometry(bool bEnableCap) override;
	virtual void GetDraftingLines(const TSharedPtr<FDraftingComposite>& ParentPage, const FPlane& Plane,
		const FVector& AxisX, const FVector& AxisY, const FVector& Origin, const FBox2D& BoundingBox,
		TArray<TArray<FVector>>& OutPerimeters) const override;
	virtual void GetDrawingDesignerItems(const FVector& ViewDirection, TArray<FDrawingDesignerLine>& OutDrawingLines, float MinLength = 0.0f) const override;

	UPROPERTY()
	FMOIEdgeHostedData InstanceData;

protected:
	FVector LineStartPos, LineEndPos, LineDir, LineNormal, LineUp;
	FVector CachedScale { ForceInit };

	void InternalUpdateGeometry(bool bCreateCollision);
	void OnInstPropUIChangedExtension(float NewValue, int32 ExtensionIdx);

	UFUNCTION()
	void OnInstPropUIChangedFlip(int32 FlippedAxisInt);

	UFUNCTION()
	void OnInstPropUIChangedOffsetUp(const FDimensionOffset& NewValue);

	UFUNCTION()
	void OnInstPropUIChangedOffsetNormal(const FDimensionOffset& NewValue);

	UFUNCTION()
	void OnInstPropUIChangedRotation(float NewValue);

	UFUNCTION()
	void OnInstPropUIChangedExtensionStart(float NewValue);

	UFUNCTION()
	void OnInstPropUIChangedExtensionEnd(float NewValue);
};

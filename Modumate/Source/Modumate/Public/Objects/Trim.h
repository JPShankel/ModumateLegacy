// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Database/ModumateArchitecturalMaterial.h"
#include "Objects/ModumateObjectStatics.h"
#include "Objects/DimensionOffset.h"
#include "Objects/ModumateObjectInstance.h"

#include "Trim.generated.h"

class AEditModelPlayerController;

USTRUCT()
struct MODUMATE_API FMOITrimData
{
	GENERATED_BODY()

	FMOITrimData();
	FMOITrimData(int32 InVersion);

	UPROPERTY()
	int32 Version = 0;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Up inversion is stored in the Y axis of FlipSigns."))
	bool bUpInverted_DEPRECATED = false;

	// FlipSigns.X refers to flipping along the direction of the hosting line, which only flips UVs.
	// FlipSigns.Y refers to flipping about the extrusion's "Up" axis, which for Trim is along the hosting polygon, and is the profile polygon's Y component.
	UPROPERTY()
	FVector2D FlipSigns = FVector2D::UnitVector;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "UpJustification is now stored in OffsetUp."))
	float UpJustification_DEPRECATED = 0.5f;

	// Offset along the extrusion's "Normal" axis, which is the profile polygon's X component.
	UPROPERTY()
	FDimensionOffset OffsetNormal = FDimensionOffset::Positive;

	// Offset along the extrusion's "Up" axis, which is the profile polygon's Y component.
	UPROPERTY()
	FDimensionOffset OffsetUp;

	UPROPERTY()
	FVector2D Extensions = FVector2D::ZeroVector;

	UPROPERTY()
	int32 FlipIndex = 0;

	static constexpr int32 CurrentVersion = 2;
};

UCLASS()
class MODUMATE_API AMOITrim : public AModumateObjectInstance
{
	GENERATED_BODY()
public:
	AMOITrim();

	virtual FVector GetLocation() const override;
	virtual FQuat GetRotation() const override;
	virtual FVector GetNormal() const override;

	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;
	virtual void PreDestroy() override;
	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint>& outPoints, TArray<FStructureLine>& outLines, bool bForSnapping = false, bool bForSelection = false) const override;
	virtual void ToggleAndUpdateCapGeometry(bool bEnableCap) override;

	virtual void SetIsDynamic(bool bIsDynamic) override;
	virtual bool GetIsDynamic() const override;

	virtual bool GetInvertedState(FMOIStateData& OutState) const override;

	// Flipping the X axis (R) has no effect, because it is generalized to affect extrusion "Normal" axis, and Trim can't be flipped into the hosting polygon.
	// Flipping the Y axis (F) flips along the hosting line, negating InstanceData.FlipSigns.X.
	// Flipping the Z axis (V) flips across the hosting line in the hosting polygon plane, negating InstanceData.FlipSigns.Y and flipping InstanceData.UpJustification.
	virtual bool GetFlippedState(EAxis::Type FlipAxis, FMOIStateData& OutState) const override;
	virtual bool GetOffsetState(const FVector& AdjustmentDirection, FMOIStateData& OutState) const override;
	virtual void RegisterInstanceDataUI(class UToolTrayBlockProperties* PropertiesUI) override;

	void GetDraftingLines(const TSharedPtr<FDraftingComposite>& ParentPage, const FPlane& Plane,
		const FVector& AxisX, const FVector& AxisY, const FVector& Origin, const FBox2D& BoundingBox,
		TArray<TArray<FVector>>& OutPerimeters) const override;
	virtual void GetDrawingDesignerItems(const FVector& ViewDirection, TArray<FDrawingDesignerLine>& OutDrawingLines,
		float MinLength = 0.0f) const override;

	virtual bool ProcessQuantities(FQuantitiesCollection& QuantitiesVisitor) const override;

	virtual void PostLoadInstanceData() override;

	virtual bool FromWebMOI(const FString& InJson) override;

	UPROPERTY()
	FMOITrimData InstanceData;

protected:
	// Cached values for the trim, derived from instance properties and the parent SurfaceEdge
	FVector TrimStartPos, TrimEndPos, TrimNormal, TrimUp, TrimDir, TrimExtrusionFlip;
	FVector2D UpperExtensions, OuterExtensions, ProfileOffsetDists, ProfileFlip;
	TArray<FVector2D> CachedProfilePoints;
	FBox2D CachedProfileExtents;

	UFUNCTION()
	void OnInstPropUIChangedFlip(int32 FlippedAxisInt);

	UFUNCTION()
	void OnInstPropUIChangedOffsetUp(const FDimensionOffset& NewValue);

	void OnInstPropUIChangedExtension(float NewValue, int32 ExtensionIdx);

	UFUNCTION()
	void OnInstPropUIChangedExtensionStart(float NewValue);

	UFUNCTION()
	void OnInstPropUIChangedExtensionEnd(float NewValue);

	bool UpdateCachedStructure();
	bool UpdateMitering();
	bool InternalUpdateGeometry(bool bRecreate, bool bCreateCollision);
	virtual void UpdateQuantities() override;
};

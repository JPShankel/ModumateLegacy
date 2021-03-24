// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once


#include "Database/ModumateArchitecturalMaterial.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "UnrealClasses/CompoundMeshActor.h"
#include "Objects/DimensionOffset.h"
#include "Objects/ModumateObjectInstance.h"

#include "StructureLine.generated.h"

class AEditModelPlayerController;
class ADynamicMeshActor;
class AModumateObjectInstance;

USTRUCT()
struct MODUMATE_API FMOIStructureLineData
{
	GENERATED_BODY()

	FMOIStructureLineData();
	FMOIStructureLineData(int32 InVersion);

	UPROPERTY()
	int32 Version = 0;

	// FlipSigns.X refers to flipping about the extrusion's "Normal" axis, which for StructureLines is the basis X axis, and is the profile polygon's Y component.
	// FlipSigns.Y refers to flipping about the extrusion's "Up" axis, which for StructureLines is the basis Y axis, and is the profile polygon's X component.
	// FlipSigns.Z refers to flipping along the direction of the hosting line, which only flips UVs.
	UPROPERTY()
	FVector FlipSigns = FVector::OneVector;

	// Justification.X referred to the offset in the extrusion's "Up" axis, which for StructureLines is the basis Y axis, and is the profile polygon's X component.
	// Justification.Y referred to the offset in the extrusion's "Normal" axis, which for StructureLines is the basis X axis, and is the profile polygon's Y component.
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Justification.X and Justification.Y are now stored in OffsetUp and OffsetNormal, respectively."))
	FVector2D Justification_DEPRECATED = FVector2D(0.5f, 0.5f);

	// OffsetUp refers to the offset in the extrusion's "Up" axis, which for StructureLines is the basis Y axis, and is the profile polygon's X component.
	UPROPERTY()
	FDimensionOffset OffsetUp;

	// OffsetNormal refers to the offset in the extrusion's "Normal" axis, which for StructureLines is the basis X axis, and is the profile polygon's Y component.
	UPROPERTY()
	FDimensionOffset OffsetNormal;

	UPROPERTY()
	float Rotation = 0.0f;

	static constexpr int32 CurrentVersion = 2;
};


UCLASS()
class MODUMATE_API AMOIStructureLine : public AModumateObjectInstance
{
	GENERATED_BODY()

public:
	AMOIStructureLine();

	virtual FQuat GetRotation() const override;
	virtual FVector GetLocation() const override;

	virtual void SetupDynamicGeometry() override;
	virtual void UpdateDynamicGeometry() override;

	// Flipping the X axis (R) flips the polygon about the "Normal" axis, the basis X axis, which is the profile polygon's Y component,
	//     negating InstanceData.FlipSigns.X and flipping InstanceData.Justification.Y.
	// Flipping the Y axis (F) flips along the hosting line, negating InstanceData.FlipSigns.Y.
	// Flipping the Z axis (V) flips the polygon about the "Up" axis, the basis Y axis, which is the profile polygon's X component,
	//     negating InstanceData.FlipSigns.Z and flipping InstanceData.Justification.X.
	virtual bool GetFlippedState(EAxis::Type FlipAxis, FMOIStateData& OutState) const override;
	virtual bool GetOffsetState(const FVector& AdjustmentDirection, FMOIStateData& OutState) const override;
	virtual void RegisterInstanceDataUI(class UToolTrayBlockProperties* PropertiesUI) override;

	virtual void GetDraftingLines(const TSharedPtr<Modumate::FDraftingComposite> &ParentPage, const FPlane &Plane,
		const FVector &AxisX, const FVector &AxisY, const FVector &Origin, const FBox2D &BoundingBox,
		TArray<TArray<FVector>> &OutPerimeters) const override;

	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const override;
	virtual void PostLoadInstanceData() override;

	virtual bool ProcessQuantities(FQuantitiesCollection& QuantitiesVisitor) const override;

	UPROPERTY()
	FMOIStructureLineData InstanceData;

protected:
	FVector LineStartPos, LineEndPos, LineDir, LineNormal, LineUp;
	FVector2D UpperExtensions, OuterExtensions, ProfileFlip;
	TArray<FVector2D> CachedProfilePoints;
	FBox2D CachedProfileExtents;

	UFUNCTION()
	void OnInstPropUIChangedFlip(int32 FlippedAxisInt);

	UFUNCTION()
	void OnInstPropUIChangedOffsetUp(const FDimensionOffset& NewValue);

	UFUNCTION()
	void OnInstPropUIChangedOffsetNormal(const FDimensionOffset& NewValue);

	UFUNCTION()
	void OnInstPropUIChangedRotation(float NewValue);

	bool UpdateCachedGeometry(bool bRecreate, bool bCreateCollision);

	virtual void UpdateQuantities() override;
};

// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "UnrealClasses/CompoundMeshActor.h"
#include "Database/ModumateArchitecturalMaterial.h"
#include "Objects/ModumateObjectInstance.h"
#include "ModumateCore/ModumateObjectStatics.h"

#include "StructureLine.generated.h"

class AEditModelPlayerController;
class ADynamicMeshActor;
class AModumateObjectInstance;

USTRUCT()
struct MODUMATE_API FMOIStructureLineData
{
	GENERATED_BODY()

	// FlipSigns.X refers to flipping about the extrusion's "Normal" axis, which for StructureLines is the basis X axis, and is the profile polygon's Y component.
	// FlipSigns.Y refers to flipping along the direction of the hosting line, which only flips UVs.
	// FlipSigns.Z refers to flipping about the extrusion's "Up" axis, which for StructureLines is the basis Y axis, and is the profile polygon's X component.
	UPROPERTY()
	FVector FlipSigns = FVector::OneVector;

	// Justification.X refers to the offset in the extrusion's "Up" axis, which for StructureLines is the basis Y axis, and is the profile polygon's X component.
	// Justification.Y refers to the offset in the extrusion's "Normal" axis, which for StructureLines is the basis X axis, and is the profile polygon's Y component.
	UPROPERTY()
	FVector2D Justification = FVector2D(0.5f, 0.5f);

	UPROPERTY()
	float Rotation = 0.0f;
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
	virtual bool GetJustifiedState(const FVector& AdjustmentDirection, FMOIStateData& OutState) const override;

	virtual void GetDraftingLines(const TSharedPtr<Modumate::FDraftingComposite> &ParentPage, const FPlane &Plane,
		const FVector &AxisX, const FVector &AxisY, const FVector &Origin, const FBox2D &BoundingBox,
		TArray<TArray<FVector>> &OutPerimeters) const override;

	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const override;

	UPROPERTY()
	FMOIStructureLineData InstanceData;

protected:
	FVector LineStartPos, LineEndPos, LineDir, LineNormal, LineUp;
	FVector2D UpperExtensions, OuterExtensions, ProfileFlip;
	TArray<FVector2D> CachedProfilePoints;
	FBox2D CachedProfileExtents;

	bool UpdateCachedGeometry(bool bRecreate, bool bCreateCollision);
};

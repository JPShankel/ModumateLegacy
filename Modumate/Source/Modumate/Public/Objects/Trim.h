// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "UnrealClasses/CompoundMeshActor.h"
#include "CoreMinimal.h"
#include "Database/ModumateArchitecturalMaterial.h"
#include "Objects/ModumateObjectInstance.h"
#include "ModumateCore/ModumateObjectStatics.h"

#include "Trim.generated.h"

class AEditModelPlayerController_CPP;

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

	UPROPERTY()
	FVector2D FlipSigns = FVector2D::UnitVector;

	UPROPERTY()
	float UpJustification = 0.5f;

	static constexpr int32 CurrentVersion = 1;
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

	virtual void SetupAdjustmentHandles(AEditModelPlayerController_CPP* Controller) override;

	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;
	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping = false, bool bForSelection = false) const override;

	virtual void SetIsDynamic(bool bIsDynamic) override;
	virtual bool GetIsDynamic() const override;

	virtual bool GetInvertedState(FMOIStateData& OutState) const override;
	virtual bool GetFlippedState(EAxis::Type FlipAxis, FMOIStateData& OutState) const override;
	virtual bool GetJustifiedState(const FVector& AdjustmentDirection, FMOIStateData& OutState) const override;

	void GetDraftingLines(const TSharedPtr<Modumate::FDraftingComposite>& ParentPage, const FPlane& Plane,
		const FVector& AxisX, const FVector& AxisY, const FVector& Origin, const FBox2D& BoundingBox,
		TArray<TArray<FVector>>& OutPerimeters) const override;

	virtual void PostLoadInstanceData() override;

	UPROPERTY()
	FMOITrimData InstanceData;

protected:
	// Cached values for the trim, derived from instance properties and the parent SurfaceEdge
	FVector TrimStartPos, TrimEndPos, TrimNormal, TrimUp, TrimDir, TrimScale;
	FVector2D UpperExtensions, OuterExtensions;

	bool UpdateCachedStructure();
	bool UpdateMitering();
	bool InternalUpdateGeometry(bool bRecreate, bool bCreateCollision);
};


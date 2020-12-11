// Copyright 2018 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/ModumateObjectInstance.h"
#include "Objects/LayeredObjectInterface.h"
#include "CoreMinimal.h"

class AEditModelPlayerController_CPP;
class AModumateObjectInstance;

class MODUMATE_API AMOIStaircase : public AModumateObjectInstance
{
public:
	AMOIStaircase();

	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;
	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const override;
	virtual void SetupAdjustmentHandles(AEditModelPlayerController_CPP* controller) override;

	virtual void SetupDynamicGeometry() override;

	virtual void GetDraftingLines(const TSharedPtr<Modumate::FDraftingComposite>& ParentPage, const FPlane& Plane,
		const FVector& AxisX, const FVector& AxisY, const FVector& Origin, const FBox2D& BoundingBox,
		TArray<TArray<FVector>>& OutPerimeters) const override;

protected:
	void GetBeyondLines(const TSharedPtr<Modumate::FDraftingComposite>& ParentPage, const FPlane& Plane,
		const FVector& AxisX, const FVector& AxisY, const FVector& Origin, const FBox2D& BoundingBox) const;
	void GetInPlaneLines(const TSharedPtr<Modumate::FDraftingComposite>& ParentPage, const FPlane& Plane,
		const FVector& AxisX, const FVector& AxisY, const FVector& Origin, const FBox2D& BoundingBox) const;

	bool bCachedUseRisers, bCachedStartRiser, bCachedEndRiser;
	TArray<TArray<FVector>> CachedTreadPolys;
	TArray<TArray<FVector>> CachedRiserPolys;
	TArray<FVector> CachedRiserNormals;
	TArray<FLayerGeomDef> TreadLayers;
	TArray<FLayerGeomDef> RiserLayers;
	FCachedLayerDimsByType CachedTreadDims;
	FCachedLayerDimsByType CachedRiserDims;
	float TreadRun = 0.0f;

	// Empirically derived overlap.
	// TODO: put in assembly spec.
	static constexpr float OpenStairsOverhang = 2.0f * Modumate::InchesToCentimeters;
};

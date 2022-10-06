// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/DimensionOffset.h"
#include "Objects/LayeredObjectInterface.h"
#include "Objects/ModumateObjectInstance.h"
#include "UnrealClasses/DynamicMeshActor.h"

#include "PlaneHostedObj.generated.h"

class AEditModelPlayerController;
class AModumateObjectInstance;

USTRUCT()
struct MODUMATE_API FMOIPlaneHostedObjData
{
	GENERATED_BODY()

	FMOIPlaneHostedObjData();
	FMOIPlaneHostedObjData(int32 InVersion);

	UPROPERTY()
	int32 Version = 0;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Layer order reversal is now stored in the Y component of FlipSigns."))
	bool bLayersInverted_DEPRECATED = false;

	UPROPERTY()
	FVector FlipSigns = FVector::OneVector;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "The data formerly stored in Justification is now in Offset."))
	float Justification_DEPRECATED = 0.5f;

	UPROPERTY()
	FDimensionOffset Offset;

	UPROPERTY()
	int32 OverrideOriginIndex = INDEX_NONE;

	static constexpr int32 CurrentVersion = 2;
};

UCLASS()
class MODUMATE_API AMOIPlaneHostedObj : public AModumateObjectInstance, public ILayeredObject
{
	GENERATED_BODY()

public:
	AMOIPlaneHostedObj();

	virtual FQuat GetRotation() const override;
	virtual FVector GetLocation() const override;
	virtual FVector GetCorner(int32 index) const override;
	virtual FVector GetNormal() const override;
	virtual void PreDestroy() override;
	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;
	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping = false, bool bForSelection = false) const override;
	virtual void ToggleAndUpdateCapGeometry(bool bEnableCap) override;
	virtual bool OnSelected(bool bIsSelected) override;

	virtual const ILayeredObject* GetLayeredInterface() const override { return this; }
	virtual const FCachedLayerDimsByType &GetCachedLayerDims() const override { return CachedLayerDims; }

	virtual bool GetInvertedState(FMOIStateData& OutState) const override;
	virtual bool GetFlippedState(EAxis::Type FlipAxis, FMOIStateData& OutState) const override;
	virtual bool GetOffsetState(const FVector& AdjustmentDirection, FMOIStateData& OutState) const override;
	virtual void RegisterInstanceDataUI(class UToolTrayBlockProperties* PropertiesUI) override;

	virtual void GetDraftingLines(const TSharedPtr<FDraftingComposite> &ParentPage, const FPlane &Plane,
		const FVector &AxisX, const FVector &AxisY, const FVector &Origin, const FBox2D &BoundingBox,
		TArray<TArray<FVector>> &OutPerimeters) const override;

	bool ProcessQuantities(FQuantitiesCollection& QuantitiesVisitor) const override;

	const TArray<FLayerGeomDef>& GetLayerGeoms() const { return LayerGeometries; }
	const TPair<TArray<FVector>, TArray<FVector>>& GetExtendedSurfaceFaces() const { return CachedExtendedSurfaceFaces; }

	UPROPERTY()
	FMOIPlaneHostedObjData InstanceData;

	virtual void GetDrawingDesignerItems(const FVector& viewDirection, TArray<FDrawingDesignerLine>& OutDrawingLines,
		float MinLength = 0.0f) const override;

	virtual bool GetBoundingLines(TArray<FDrawingDesignerLine>& outBounding)  const override;

	virtual bool ToWebMOI(FWebMOI& OutMOI) const override;

protected:
	virtual void PostLoadInstanceData() override;
	virtual void UpdateQuantities() override;
	void UpdateAlignmentTargets();

	void UpdateMeshWithLayers(bool bRecreateMesh, bool bRecalculateEdgeExtensions);
	void UpdateConnectedEdges();
	void MarkEdgesMiterDirty();
	void GetBeyondDraftingLines(const TSharedPtr<FDraftingComposite>& ParentPage, const FPlane& Plane,
		const FBox2D& BoundingBox) const;

	UFUNCTION()
	void OnInstPropUIChangedFlip(int32 FlippedAxisInt);

	UFUNCTION()
	void OnInstPropUIChangedOffset(const FDimensionOffset& NewValue);

	bool bGeometryDirty = false;
	FCachedLayerDimsByType CachedLayerDims;
	TArray<FLayerGeomDef> LayerGeometries;
	TArray<FPolyHole3D> CachedHoles;
	TArray<int32> CachedAlignmentTargets;
	TPair<TArray<FVector>, TArray<FVector>> CachedExtendedSurfaceFaces;
	mutable TArray<FVector> TempHoleRelativePoints;

	TArray<FVector2D> CachedLayerEdgeExtensions;

	UPROPERTY()
	TArray<AModumateObjectInstance*> CachedParentConnectedMOIs;

	UPROPERTY()
	TArray<AModumateObjectInstance*> CachedConnectedEdges;

	bool IsValidParentObjectType(EObjectType ParentObjectType) const;
};

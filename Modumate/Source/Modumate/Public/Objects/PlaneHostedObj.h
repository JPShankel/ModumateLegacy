// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/LayeredObjectInterface.h"
#include "Objects/ModumateObjectInstance.h"
#include "UnrealClasses/DynamicMeshActor.h"

#include "PlaneHostedObj.generated.h"

class AEditModelPlayerController_CPP;
class AModumateObjectInstance;

USTRUCT()
struct MODUMATE_API FMOIPlaneHostedObjData
{
	GENERATED_BODY()

	UPROPERTY()
	bool bLayersInverted = false;

	UPROPERTY()
	float Justification = 0.5f;

	UPROPERTY()
	int32 OverrideOriginIndex = INDEX_NONE;
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
	virtual void GetTypedInstanceData(UScriptStruct*& OutStructDef, void*& OutStructPtr) override;
	virtual void PreDestroy() override;
	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;
	virtual void SetupDynamicGeometry() override;
	virtual void UpdateDynamicGeometry() override;
	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping = false, bool bForSelection = false) const override;
	virtual void SetupAdjustmentHandles(AEditModelPlayerController_CPP *controller) override;
	virtual bool OnSelected(bool bIsSelected) override;

	virtual const ILayeredObject* GetLayeredInterface() const override { return this; }
	virtual const FCachedLayerDimsByType &GetCachedLayerDims() const override { return CachedLayerDims; }

	virtual bool GetInvertedState(FMOIStateData& OutState) const override;
	virtual bool GetFlippedState(EAxis::Type FlipAxis, FMOIStateData& OutState) const override;
	virtual bool GetJustifiedState(const FVector& AdjustmentDirection, FMOIStateData& OutState) const override;

	virtual void GetDraftingLines(const TSharedPtr<Modumate::FDraftingComposite> &ParentPage, const FPlane &Plane,
		const FVector &AxisX, const FVector &AxisY, const FVector &Origin, const FBox2D &BoundingBox,
		TArray<TArray<FVector>> &OutPerimeters) const override;

protected:
	void UpdateMeshWithLayers(bool bRecreateMesh, bool bRecalculateEdgeExtensions);
	void UpdateConnectedEdges();
	void MarkEdgesMiterDirty();
	void GetBeyondDraftingLines(const TSharedPtr<Modumate::FDraftingComposite>& ParentPage, const FPlane& Plane,
		const FBox2D& BoundingBox) const;

	FMOIPlaneHostedObjData InstanceData;

	bool bGeometryDirty = false;
	FCachedLayerDimsByType CachedLayerDims;
	TArray<FLayerGeomDef> LayerGeometries;
	TArray<FPolyHole3D> CachedHoles;
	mutable TArray<FVector> TempHoleRelativePoints;

	TArray<FVector2D> CachedLayerEdgeExtensions;
	TArray<AModumateObjectInstance *> CachedParentConnectedMOIs;
	TArray<AModumateObjectInstance *> CachedConnectedEdges;
};

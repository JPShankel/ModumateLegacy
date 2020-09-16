// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "UnrealClasses/CompoundMeshActor.h"
#include "CoreMinimal.h"
#include "ToolsAndAdjustments/Handles/EditModelPortalAdjustmentHandles.h"
#include "Database/ModumateArchitecturalMaterial.h"
#include "DocumentManagement/ModumateObjectInstance.h"

#include "ModumateObjectInstanceCabinets.generated.h"


class AEditModelPlayerController_CPP;

UCLASS()
class MODUMATE_API ASelectCabinetFrontHandle : public AAdjustmentHandleActor
{
	GENERATED_BODY()

public:
	static const float FaceCenterHeightOffset;

	virtual bool BeginUse() override;
	virtual FVector GetHandlePosition() const override;

protected:
	virtual bool GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const override;
};

class FModumateObjectInstance;

class MODUMATE_API FMOICabinetImpl : public FModumateObjectInstanceImplBase
{
public:
	FMOICabinetImpl(FModumateObjectInstance *moi);
	virtual ~FMOICabinetImpl();

	virtual FVector GetCorner(int32 index) const override;
	virtual int32 GetNumCorners() const override;
	virtual FVector GetNormal() const override;
	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<TSharedPtr<Modumate::FDelta>>* OutSideEffectDeltas) override;
	virtual void UpdateVisibilityAndCollision(bool &bOutVisible, bool &bOutCollisionEnabled) override;
	virtual void SetupDynamicGeometry() override;
	virtual void SetupAdjustmentHandles(AEditModelPlayerController_CPP *controller) override;
	virtual void ShowAdjustmentHandles(AEditModelPlayerController_CPP *Controller, bool bShow);
	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping = false, bool bForSelection = false) const override;
	virtual void GetDraftingLines(const TSharedPtr<Modumate::FDraftingComposite> &ParentPage, const FPlane &Plane,
		const FVector &AxisX, const FVector &AxisY, const FVector &Origin, const FBox2D &BoundingBox,
		TArray<TArray<FVector>> &OutPerimeters) const override;
	static FName CabinetGeometryMatName;

protected:
	bool UpdateCachedGeometryData();
	void UpdateCabinetPortal();

	bool AdjustmentHandlesVisible;
	TArray<TWeakObjectPtr<ASelectCabinetFrontHandle>> FrontSelectionHandles;
	TWeakObjectPtr<ACompoundMeshActor> FrontFacePortalActor;
	FVector2D ToeKickDimensions;

	FTransform CachedBaseOrigin;
	TArray<FVector> CachedBasePoints;
	FVector CachedExtrusionDelta;
};

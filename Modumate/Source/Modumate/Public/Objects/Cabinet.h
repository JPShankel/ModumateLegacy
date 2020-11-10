// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "UnrealClasses/CompoundMeshActor.h"
#include "CoreMinimal.h"
#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"
#include "Database/ModumateArchitecturalMaterial.h"
#include "Objects/ModumateObjectInstance.h"

#include "Cabinet.generated.h"


USTRUCT()
struct MODUMATE_API FMOICabinetData
{
	GENERATED_BODY()

	UPROPERTY()
	float ExtrusionDist = 0.0f;

	UPROPERTY()
	int32 FrontFaceIndex = INDEX_NONE;

	UPROPERTY()
	bool bFrontFaceLateralInverted = false;
};

class AEditModelPlayerController_CPP;

UCLASS()
class MODUMATE_API ASelectCabinetFrontHandle : public AAdjustmentHandleActor
{
	GENERATED_BODY()

public:
	static const float FaceCenterHeightOffset;
	static const float BaseCenterOffset;

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
	virtual void GetTypedInstanceData(UScriptStruct*& OutStructDef, void*& OutStructPtr) override;
	virtual FVector GetNormal() const override;
	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;
	virtual void UpdateVisibilityAndCollision(bool &bOutVisible, bool &bOutCollisionEnabled) override;
	virtual void SetupDynamicGeometry() override;
	virtual void SetupAdjustmentHandles(AEditModelPlayerController_CPP *controller) override;
	virtual void ShowAdjustmentHandles(AEditModelPlayerController_CPP *Controller, bool bShow);
	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping = false, bool bForSelection = false) const override;
	virtual bool GetInvertedState(FMOIStateData& OutState) const override;
	virtual void GetDraftingLines(const TSharedPtr<Modumate::FDraftingComposite> &ParentPage, const FPlane &Plane,
		const FVector &AxisX, const FVector &AxisY, const FVector &Origin, const FBox2D &BoundingBox,
		TArray<TArray<FVector>> &OutPerimeters) const override;

	static bool GetFaceGeometry(const TArray<FVector>& BasePoints, const FVector& ExtrusionDelta, int32 FaceIndex,
		TArray<FVector>& OutFacePoints, FTransform& OutFaceTransform);
	static bool UpdateCabinetActors(const FBIMAssemblySpec& Assembly, const TArray<FVector>& InBasePoints, const FVector& InExtrusionDelta,
		int32 FrontFaceIndex, bool bFaceLateralInverted, bool bUpdateCollision, bool bEnableCollision,
		class ADynamicMeshActor* CabinetBoxActor, class ACompoundMeshActor* CabinetFaceActor, bool& bOutFaceValid);

protected:
	bool UpdateCachedGeometryData();

	bool AdjustmentHandlesVisible;
	TArray<TWeakObjectPtr<ASelectCabinetFrontHandle>> FrontSelectionHandles;
	TWeakObjectPtr<ACompoundMeshActor> FrontFacePortalActor;

	FTransform CachedBaseOrigin;
	TArray<FVector> CachedBasePoints;
	FVector CachedExtrusionDelta;
	bool bCurrentFaceValid;

	FMOICabinetData InstanceData;
};

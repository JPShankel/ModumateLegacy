// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "UnrealClasses/CompoundMeshActor.h"
#include "CoreMinimal.h"
#include "ToolsAndAdjustments/Handles/CabinetFrontFaceHandle.h"
#include "ToolsAndAdjustments/Handles/CabinetExtrusionHandle.h"
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

class AEditModelPlayerController;

class AModumateObjectInstance;

UCLASS()
class MODUMATE_API AMOICabinet : public AModumateObjectInstance
{
	GENERATED_BODY()
public:
	AMOICabinet();

	virtual FVector GetCorner(int32 index) const override;
	virtual int32 GetNumCorners() const override;
	virtual FVector GetNormal() const override;
	virtual void PreDestroy() override;
	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;
	virtual bool GetUpdatedVisuals(bool &bOutVisible, bool &bOutCollisionEnabled) override;
	virtual void SetupDynamicGeometry() override;
	virtual void ToggleAndUpdateCapGeometry(bool bEnableCap) override;
	virtual void SetupAdjustmentHandles(AEditModelPlayerController *controller) override;
	virtual void ShowAdjustmentHandles(AEditModelPlayerController *Controller, bool bShow);
	virtual bool OnSelected(bool bIsSelected) override;
	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping = false, bool bForSelection = false) const override;
	virtual bool GetInvertedState(FMOIStateData& OutState) const override;
	virtual void GetDraftingLines(const TSharedPtr<FDraftingComposite> &ParentPage, const FPlane &Plane,
		const FVector &AxisX, const FVector &AxisY, const FVector &Origin, const FBox2D &BoundingBox,
		TArray<TArray<FVector>> &OutPerimeters) const override;
	virtual bool ProcessQuantities(FQuantitiesCollection& QuantitiesVisitor) const override;

	static void RectangleCheck(const TArray<FVector>& Points, bool& bOutIsRectangular, FVector& OutMostVerticalDir);
	static bool GetFaceGeometry(const TArray<FVector>& BasePoints, const FVector& ExtrusionDelta, int32 FaceIndex,
		TArray<FVector>& OutFacePoints, FTransform& OutFaceTransform);
	static bool UpdateCabinetActors(const FBIMAssemblySpec& Assembly, const TArray<FVector>& InBasePoints, const FVector& InExtrusionDelta,
		int32 FrontFaceIndex, bool bFaceLateralInverted, bool bUpdateCollision, bool bEnableCollision,
		class ADynamicMeshActor* CabinetBoxActor, class ACompoundMeshActor* CabinetFaceActor, bool& bOutFaceValid,
		int32 DoorPartIndex = BIM_ROOT_PART, bool bEnableCabinetBox = true);
	virtual void GetDrawingDesignerItems(const FVector& ViewDirection, TArray<FDrawingDesignerLine>& OutDrawingLines, float MinLength = 0.0f) const override;

	UPROPERTY()
	FMOICabinetData InstanceData;

protected:
	bool UpdateCachedGeometryData();

	bool AdjustmentHandlesVisible;
	TArray<TWeakObjectPtr<ACabinetFrontFaceHandle>> FrontSelectionHandles;
	TWeakObjectPtr<ACompoundMeshActor> FrontFacePortalActor;

	FTransform CachedBaseOrigin;
	TArray<FVector> CachedBasePoints;
	FVector CachedExtrusionDelta;

	bool bCurrentFaceValid;
	bool bBaseIsRectangular;

	int32 ExtrusionDimensionActorID;

	FBox GetBoundingBox() const;

	virtual void UpdateQuantities() override;
};

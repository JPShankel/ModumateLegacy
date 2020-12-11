// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Objects/PlaneBase.h"
#include "Drafting/ModumateHUDDraw.h"
#include "DocumentManagement/ModumateSceneCaptureObjectInterface.h"

#include "CutPlane.generated.h"

USTRUCT()
struct MODUMATE_API FMOICutPlaneData
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Location;

	UPROPERTY()
	FQuat Rotation;

	UPROPERTY()
	FVector2D Extents;

	UPROPERTY()
	FString Name;

	UPROPERTY()
	bool bIsExported { false };
};

class ACutPlaneCaptureActor;

namespace Modumate
{
	class FDraftingComposite;
	class FModumateDraftingView;
}

class MODUMATE_API AMOICutPlane : public FMOIPlaneImplBase, public ISceneCaptureObject
{
public:
	AMOICutPlane();

	virtual void GetTypedInstanceData(UScriptStruct*& OutStructDef, void*& OutStructPtr) override;
	virtual void SetupDynamicGeometry() override;
	virtual void UpdateDynamicGeometry() override;
	virtual bool OnSelected(bool bIsSelected) override;
	virtual void SetupAdjustmentHandles(AEditModelPlayerController_CPP *controller) override;

	virtual bool GetTransformedLocationState(const FTransform Transform, FMOIStateData& OutState) const override;
	virtual void AddDraftingLines(UHUDDrawWidget *HUDDrawWidget) override;
	virtual void GetDraftingLines(const TSharedPtr<Modumate::FDraftingComposite> &ParentPage, const FPlane &Plane, const FVector &AxisX, const FVector &AxisY, const FVector &Origin, const FBox2D &BoundingBox, TArray<TArray<FVector>> &OutPerimeters) const override;
	virtual ISceneCaptureObject* GetSceneCaptureInterface() override { return this; }

	virtual bool AddCaptureArea(int32 ScopeBoxID, TArray<FVector> CaptureArea) override;
	virtual void SetupPendingRenders() override;
	virtual bool StartRender(UModumateDocument* doc = nullptr) override;
	virtual void CaptureComplete() override;
	virtual void TraceRequestComplete(int32 TraceID, FString TraceString) override;

	virtual AActor* CreateActor(UWorld* world, const FVector& loc, const FQuat& rot) override;
	virtual void PostCreateObject(bool bNewObject) override;
	virtual void PreDestroy() override;
	virtual void GetUpdatedVisuals(bool &bOutVisible, bool &bOutCollisionEnabled) override;

	Modumate::FModumateHUDDraw DrawingInterface;

	// These two objects are similar in purpose and use, but there are differences between what the HUD and the PDF 
	// can and should draw. These objects should remain separate while there isn't parity between the two interfaces.
	TSharedPtr<Modumate::FDraftingComposite> PreviewHUDLines = nullptr;
	TSharedPtr<Modumate::FDraftingComposite> DraftingForegroundLines = nullptr;

	TWeakObjectPtr<ADynamicMeshActor> MasksActor;

protected:
	virtual float GetAlpha() const override;
	void UpdateCachedGeometryData();
	void UpdateDraftingPreview();
	void GetForegroundLines(TSharedPtr<Modumate::FDraftingComposite> ParentPage, const FVector &AxisX, const FVector &AxisY, bool bIsDrafting = false);
	void ConvertToOutlines(const FString& renderTargetFilename);

	TWeakObjectPtr<ACutPlaneCaptureActor> CaptureActor;

	FLinearColor EdgeSelectedColor;
	FLinearColor EdgeColor;

	TArray<TPair<int32, TArray<FVector>>> PendingCaptureAreas;
	TQueue<FPendingObjectRender> PendingObjectRenders;
	FPendingObjectRender CurrentObjectRender;
	TMap<int32, FPendingObjectRender> InprocessRenders;

	FMOICutPlaneData InstanceData;
};

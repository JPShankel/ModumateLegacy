// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "DocumentManagement/ModumateObjectInstancePlaneBase.h"
#include "Drafting/ModumateHUDDraw.h"
#include "DocumentManagement/ModumateSceneCaptureObjectInterface.h"

class ACutPlaneCaptureActor;

namespace Modumate
{
	class FDraftingComposite;
	class FModumateDraftingView;

	class MODUMATE_API FMOICutPlaneImpl : public FMOIPlaneImplBase, public ISceneCaptureObject
	{
	public:
		FMOICutPlaneImpl(FModumateObjectInstance *moi);

		virtual void SetupDynamicGeometry() override;
		virtual void UpdateDynamicGeometry() override;
		virtual void OnSelected(bool bNewSelected) override;
		virtual void AddDraftingLines(UHUDDrawWidget *HUDDrawWidget) override;
		virtual void GetDraftingLines(const TSharedPtr<FDraftingComposite> &ParentPage, const FPlane &Plane, const FVector &AxisX, const FVector &AxisY, const FVector &Origin, const FBox2D &BoundingBox, TArray<TArray<FVector>> &OutPerimeters) const override;
		virtual ISceneCaptureObject* GetSceneCaptureInterface() override { return this; }

		virtual bool AddCaptureArea(int32 ScopeBoxID, TArray<FVector> CaptureArea) override;
		virtual bool StartRender(FModumateDocument* doc = nullptr) override;
		virtual void PublishPage() override;

		FModumateHUDDraw DrawingInterface;

		// These two objects are similar in purpose and use, but there are differences between what the HUD and the PDF 
		// can and should draw. These objects should remain separate while there isn't parity between the two interfaces.
		TSharedPtr<FDraftingComposite> PreviewHUDLines = nullptr;
		TSharedPtr<FDraftingComposite> DraftingForegroundLines = nullptr;

		TWeakObjectPtr<ADynamicMeshActor> MasksActor;

	protected:
		virtual float GetAlpha() const override;
		void UpdateCachedGeometryData();
		void UpdateDraftingPreview();
		void GetForegroundLines(TSharedPtr<FDraftingComposite> ParentPage, const FVector &AxisX, const FVector &AxisY, bool bIsDrafting = false);

		TWeakObjectPtr<ACutPlaneCaptureActor> CaptureActor;

		FLinearColor EdgeSelectedColor;
		FLinearColor EdgeColor;

		TArray<TPair<int32, TArray<FVector>>> PendingCaptureAreas;
	};
}

// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "DocumentManagement/ModumateObjectInstance.h"
#include "DocumentManagement/ModumateMiterNodeInterface.h"

class UWorld;
class AAdjustmentHandleActor_CPP;
class ALineActor;

namespace Modumate
{
	class MODUMATE_API FMOIMetaEdgeImpl : public FModumateObjectInstanceImplBase, IMiterNode
	{
	public:
		FMOIMetaEdgeImpl(FModumateObjectInstance *moi);

		virtual void SetLocation(const FVector &p) override;
		virtual FVector GetLocation() const override;
		virtual FVector GetCorner(int32 index) const override;
		virtual void ClearAdjustmentHandles(AEditModelPlayerController_CPP *controller) override;
		virtual void ShowAdjustmentHandles(AEditModelPlayerController_CPP *controller, bool show) override;
		virtual void GetAdjustmentHandleActors(TArray<TWeakObjectPtr<AAdjustmentHandleActor_CPP>>& outHandleActors) override;
		virtual void OnCursorHoverActor(AEditModelPlayerController_CPP *controller, bool bEnableHover) override;
		virtual AActor *CreateActor(UWorld *world, const FVector &loc, const FQuat &rot) override;
		virtual bool CleanObject(EObjectDirtyFlags DirtyFlag) override;
		virtual void SetupDynamicGeometry() override;
		virtual void UpdateDynamicGeometry() override;
		virtual void UpdateVisibilityAndCollision(bool &bOutVisible, bool &bOutCollisionEnabled) override;
		void SetupAdjustmentHandles(AEditModelPlayerController_CPP *controller);
		virtual void OnSelected(bool bNewSelected) override;
		virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping = false, bool bForSelection = false) const override;
		virtual bool ShowStructureOnSelection() const override { return false; }
		virtual bool UseStructureDataForCollision() const override { return true; }
		virtual const IMiterNode* GetMiterInterface() const override { return this; }

		// Begin IMiterNode interface
		virtual const FMiterData &GetMiterData() const;
		// End IMiterNode interface

	protected:
		float GetThicknessMultiplier() const;

		TWeakObjectPtr<UWorld> World;
		TArray<TWeakObjectPtr<AAdjustmentHandleActor_CPP>> AdjustmentHandles;
		TArray<FModumateObjectInstance*> CachedConnectedMOIs;
		TWeakObjectPtr<ALineActor> LineActor;
		FColor HoverColor;
		float HoverThickness;
		FMiterData CachedMiterData;
	};
}

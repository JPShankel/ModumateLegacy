// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "ModumateControlPointObjectBase.h"



namespace Modumate
{
	class MODUMATE_API FDynamicModumateObjectInstanceImpl : public FModumateControlPointObjectBase
	{
	protected:
		TWeakObjectPtr<ADynamicMeshActor> DynamicMeshActor;
		TWeakObjectPtr<UWorld> World;
		TArray<TWeakObjectPtr<AAdjustmentHandleActor_CPP>> AdjustmentHandles;
		bool bDrawHUDTags;

	public:

		FDynamicModumateObjectInstanceImpl(FModumateObjectInstance *moi)
			: FModumateControlPointObjectBase(moi)
			, DynamicMeshActor(nullptr)
			, World(nullptr)
			, bDrawHUDTags(true)
		{ }

		virtual ~FDynamicModumateObjectInstanceImpl() {}

		virtual AActor *CreateActor(UWorld *world, const FVector &loc, const FQuat &rot) override;
		virtual void SetMaterial(UMaterialInterface *m) override;
		virtual UMaterialInterface *GetMaterial() override;
		virtual void SetRotation(const FQuat &r) override;
		virtual FQuat GetRotation() const override;
		virtual void SetLocation(const FVector &p) override;
		virtual FVector GetLocation() const override;
		virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping = false, bool bForSelection = false) const override;
		virtual bool GetTriInternalNormalFromEdge(int32 cp1, int32 cp2, FVector &outNormal) const override;


		// Adjustment Handles
		virtual void SetupAdjustmentHandles(AEditModelPlayerController_CPP *controller) = 0;
		virtual void ClearAdjustmentHandles(AEditModelPlayerController_CPP *controller) override;
		virtual void ShowAdjustmentHandles(AEditModelPlayerController_CPP *controller, bool show) override;
		virtual void GetAdjustmentHandleActors(TArray<TWeakObjectPtr<AAdjustmentHandleActor_CPP>>& outHandleActors) override;

		virtual void SetIsDynamic(bool bIsDynamic) override;
		virtual bool GetIsDynamic() const override;
	};
}

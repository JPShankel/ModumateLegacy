// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "ModumateControlPointObjectBase.h"


class AEditModelPlayerController_CPP;
class UWorld;
class AAdjustmentHandleActor_CPP;
class ALineActor;
class UMaterialInterface;
class AActor;

namespace Modumate
{

	class FModumateObjectInstance;
	class FAdjustLineSegmentHandle;

	class MODUMATE_API FMOILineSegment : public FModumateControlPointObjectBase
	{
	private:
		TWeakObjectPtr<UWorld> World;
		TArray<TWeakObjectPtr<AAdjustmentHandleActor_CPP>> AdjustmentHandles;
		TWeakObjectPtr<UMaterialInterface> Material;

	protected:
		TWeakObjectPtr<ALineActor> LineActor;
		FColor BaseColor;

		virtual FAdjustLineSegmentHandle *MakeAdjustmentHandle(FModumateObjectInstance *handleMOI, int cp);

	public:
		FMOILineSegment(FModumateObjectInstance *moi)
			: FModumateControlPointObjectBase(moi)
			, World(nullptr)
			, Material(nullptr)
			, LineActor(nullptr)
			, BaseColor(FColor::Cyan)
		{ }

		virtual void SetMaterial(UMaterialInterface *m) override;
		virtual UMaterialInterface *GetMaterial() override;
		virtual void SetRotation(const FQuat &r) override;
		virtual FQuat GetRotation() const override;
		virtual void SetLocation(const FVector &p) override;
		virtual FVector GetLocation() const override;
		virtual void ClearAdjustmentHandles(AEditModelPlayerController_CPP *controller) override;
		virtual void ShowAdjustmentHandles(AEditModelPlayerController_CPP *controller, bool show) override;
		virtual void OnCursorHoverActor(AEditModelPlayerController_CPP *controller, bool EnableHover) override {};
		virtual void GetAdjustmentHandleActors(TArray<TWeakObjectPtr<AAdjustmentHandleActor_CPP>>& outHandleActors) override;
		void SetupAdjustmentHandles(AEditModelPlayerController_CPP *controller);
		virtual AActor *CreateActor(UWorld *world, const FVector &loc, const FQuat &rot) override;
		virtual void OnAssemblyChanged() override {};
		virtual void SetupDynamicGeometry() override;
		virtual void UpdateDynamicGeometry() override;
		virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping = false, bool bForSelection = false) const override;
		virtual bool GetTriInternalNormalFromEdge(int32 cp1, int32 cp2, FVector &outNormal) const override { return false; }
		virtual void InvertObject() override {};
		virtual void TransverseObject() override {};

		virtual TArray<FModelDimensionString> GetDimensionStrings() const override;
	};
}

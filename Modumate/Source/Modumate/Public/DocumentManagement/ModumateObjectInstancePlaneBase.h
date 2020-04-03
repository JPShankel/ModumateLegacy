// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "ModumateDynamicObjectBase.h"

namespace Modumate
{
	class MODUMATE_API FMOIPlaneImplBase : public FDynamicModumateObjectInstanceImpl
	{
	public:
		FMOIPlaneImplBase(FModumateObjectInstance *moi);

		virtual FVector GetCorner(int32 index) const override;
		virtual FQuat GetRotation() const override;
		virtual void SetRotation(const FQuat& r) override;
		virtual FVector GetLocation() const override;
		virtual FVector GetNormal() const override;
		virtual TArray<FModelDimensionString> GetDimensionStrings() const override;
		virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const override;
		void SetupAdjustmentHandles(AEditModelPlayerController_CPP *controller);
		virtual bool ShowStructureOnSelection() const override;

	protected:
		virtual float GetAlpha() const;

	protected:
		FArchitecturalMaterial MaterialData;
		FPlane CachedPlane;
		FVector CachedAxisX, CachedAxisY, CachedOrigin, CachedCenter;
	};
}

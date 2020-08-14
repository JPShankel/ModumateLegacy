// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "DocumentManagement/Objects/PlaneBase.h"


namespace Modumate
{
	class MODUMATE_API FMOISurfacePolygonImpl : public FMOIPlaneImplBase
	{
	public:
		FMOISurfacePolygonImpl(FModumateObjectInstance *moi);

		virtual void UpdateVisibilityAndCollision(bool &bOutVisible, bool &bOutCollisionEnabled) override;
		virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<TSharedPtr<FDelta>>* OutSideEffectDeltas) override;

	protected:
		float MeshPointOffset;
		TArray<FVector> CachedOffsetPoints;
		bool bInteriorPolygon;

		bool UpdateCachedGraphData();
		virtual float GetAlpha() const override;
	};
}

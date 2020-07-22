// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "DocumentManagement/Objects/PlaneBase.h"
#include "Graph/Graph3D.h"


namespace Modumate
{
	class MODUMATE_API FMOISurfacePolygonImpl : public FMOIPlaneImplBase
	{
	public:
		FMOISurfacePolygonImpl(FModumateObjectInstance *moi);

		virtual void UpdateVisibilityAndCollision(bool &bOutVisible, bool &bOutCollisionEnabled) override;
		virtual void SetupDynamicGeometry() override;

	protected:
		float MeshPointOffset;
		TArray<FVector> CachedOffsetPoints;

		void UpdateCachedGraphData();
		virtual float GetAlpha() const override;
	};
}

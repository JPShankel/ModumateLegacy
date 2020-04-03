// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "ModumateObjectInstancePlaneBase.h"
#include "Graph3D.h"


namespace Modumate
{
	class MODUMATE_API FMOIMetaPlaneImpl : public FMOIPlaneImplBase
	{
	public:
		FMOIMetaPlaneImpl(FModumateObjectInstance *moi);

		virtual void UpdateVisibilityAndCollision(bool &bOutVisible, bool &bOutCollisionEnabled) override;
		virtual void SetupDynamicGeometry() override;
		virtual void UpdateDynamicGeometry() override;
		virtual void OnSelected(bool bNewSelected) override;
		virtual void OnCursorHoverActor(AEditModelPlayerController_CPP *controller, bool bEnableHover) override;
		virtual void SetFromDataRecordAndRotation(const FMOIDataRecordV1 &dataRec, const FVector &origin, const FQuat &rotation) override;
		virtual void SetFromDataRecordAndDisplacement(const FMOIDataRecordV1 &dataRec, const FVector &displacement) override;

	protected:
		void UpdateConnectedVisuals();
		void UpdateCachedGraphData();
		virtual float GetAlpha() const override;

		TArray<FModumateObjectInstance *> TempConnectedMOIs;
		TArray<int32> LastChildIDs;
	};
}

// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CompoundMeshActor.h"
#include "CoreMinimal.h"
#include "EditModelAdjustmentHandleBase.h"
#include "ModumateArchitecturalMaterial.h"
#include "ModumateDynamicObjectBase.h"

class AEditModelPlayerController_CPP;

namespace Modumate
{
	class FModumateObjectInstance;

	class MODUMATE_API FMOICabinetImpl : public FDynamicModumateObjectInstanceImpl
	{
	public:
		FMOICabinetImpl(FModumateObjectInstance *moi);
		virtual ~FMOICabinetImpl();

		virtual FVector GetCorner(int32 index) const override;
		virtual FVector GetNormal() const override;
		virtual void UpdateVisibilityAndCollision(bool &bOutVisible, bool &bOutCollisionEnabled) override;
		virtual void SetupDynamicGeometry() override;
		virtual void UpdateDynamicGeometry() override;
		virtual void SetupAdjustmentHandles(AEditModelPlayerController_CPP *controller) override;
		virtual void ClearAdjustmentHandles(AEditModelPlayerController_CPP *controller) override;
		virtual void ShowAdjustmentHandles(AEditModelPlayerController_CPP *controller, bool show) override;
		virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping = false, bool bForSelection = false) const override;

		static FName CabinetGeometryMatName;

	protected:
		void UpdateToeKickDimensions();
		void UpdateCabinetPortal();

		bool AdjustmentHandlesVisible;
		TArray<class FSelectFrontAdjustmentHandle *> FrontSelectionHandleImpls;
		TWeakObjectPtr<ACompoundMeshActor> FrontFacePortalActor;
		FVector2D ToeKickDimensions;
	};

	class MODUMATE_API FSelectFrontAdjustmentHandle : public FEditModelAdjustmentHandleBase
	{
	public:
		FSelectFrontAdjustmentHandle(FModumateObjectInstance *moi, int32 cp);

		int32 CP;

		static const float FaceCenterHeightOffset;

		virtual bool OnBeginUse() override;
		virtual FVector GetAttachmentPoint() override;
	};
}

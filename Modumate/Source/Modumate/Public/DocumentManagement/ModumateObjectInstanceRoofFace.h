// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "UnrealClasses/CompoundMeshActor.h"
#include "CoreMinimal.h"
#include "ToolsAndAdjustments/Handles/EditModelPortalAdjustmentHandles.h"
#include "Database/ModumateArchitecturalMaterial.h"
#include "DocumentManagement/ModumateDynamicObjectBase.h"

class AEditModelPlayerController_CPP;

namespace Modumate
{
	class FModumateObjectInstance;

	class MODUMATE_API FMOIRoofFaceImpl : public FDynamicModumateObjectInstanceImpl
	{
	public:
		FMOIRoofFaceImpl(FModumateObjectInstance *moi);
		virtual ~FMOIRoofFaceImpl();

		virtual FVector GetCorner(int32 index) const override;
		virtual void SetupDynamicGeometry() override;
		virtual void UpdateDynamicGeometry() override;
		virtual void SetupAdjustmentHandles(AEditModelPlayerController_CPP *controller) override;
		virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping = false, bool bForSelection = false) const override;

	protected:
		bool bAdjustmentHandlesVisible;
		TArray<class FSetSegmentSlopeHandle *> SetSlopeHandleImpls;

		TArray<FVector> EdgePoints;
		TArray<float> EdgeSlopes;
		TArray<bool> EdgesHaveFaces;
	};

	class MODUMATE_API FSetSegmentSlopeHandle : public FEditModelAdjustmentHandleBase
	{
	public:
		FSetSegmentSlopeHandle(FModumateObjectInstance *moi, int32 cp);

		int32 CP;

		static const float FaceCenterHeightOffset;

		virtual bool OnBeginUse() override;
		virtual FVector GetAttachmentPoint() override;

	protected:
		TArray<FVector> EdgePoints;
		TArray<float> EdgeSlopes;
		TArray<bool> EdgesHaveFaces;
	};
}

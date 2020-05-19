// Copyright 2018 Modumate, Inc. All Rights Reserved.
#pragma once

#include "DocumentManagement/ModumateDynamicObjectBase.h"
#include "UnrealClasses/DynamicMeshActor.h"

class AEditModelPlayerController_CPP;

namespace Modumate
{
	class FModumateObjectInstance;

	class MODUMATE_API FMOIFinishImpl : public FDynamicModumateObjectInstanceImpl
	{
	public:
		FMOIFinishImpl(FModumateObjectInstance *moi);
		virtual ~FMOIFinishImpl();

		virtual FVector GetCorner(int32 index) const override;
		virtual FVector GetNormal() const override;
		virtual bool CleanObject(EObjectDirtyFlags DirtyFlag) override;
		virtual void SetupDynamicGeometry() override;
		virtual void UpdateDynamicGeometry() override;
		virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping = false, bool bForSelection = false) const override;
		virtual void SetupAdjustmentHandles(AEditModelPlayerController_CPP *controller) override { }

		virtual FModumateWallMount GetWallMountForSelf(int32 originIndex) const override;
		virtual void SetWallMountForSelf(const FModumateWallMount &wm) override;

	protected:
		FVector CachedNormal;
	};
}

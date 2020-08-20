// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "DocumentManagement/ModumateObjectInstance.h"

class UWorld;
class ALineActor;

namespace Modumate
{
	class MODUMATE_API FMOIEdgeImplBase : public FModumateObjectInstanceImplBase
	{
	public:
		FMOIEdgeImplBase(FModumateObjectInstance *moi);

		virtual void SetLocation(const FVector &p) override;
		virtual FVector GetLocation() const override;
		virtual FVector GetCorner(int32 index) const override;
		virtual int32 GetNumCorners() const override;
		virtual void OnCursorHoverActor(AEditModelPlayerController_CPP *controller, bool bEnableHover) override;
		virtual AActor *CreateActor(UWorld *world, const FVector &loc, const FQuat &rot) override;
		virtual void OnSelected(bool bNewSelected) override;
		virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping = false, bool bForSelection = false) const override;
		virtual bool ShowStructureOnSelection() const override { return false; }
		virtual bool UseStructureDataForCollision() const override { return true; }

	protected:
		float GetThicknessMultiplier() const;

		TWeakObjectPtr<UWorld> World;
		TArray<FModumateObjectInstance*> CachedConnectedMOIs;
		TWeakObjectPtr<ALineActor> LineActor;
		FColor HoverColor;
		float HoverThickness;
	};
}

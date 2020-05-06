// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "ModumateObjectInstance.h"

class ALineActor;

namespace Modumate
{
	class MODUMATE_API FMOIRoofPerimeterImpl : public FModumateObjectInstanceImplBase
	{
	public:
		FMOIRoofPerimeterImpl(FModumateObjectInstance *moi);

		virtual void SetLocation(const FVector &p) override;
		virtual FVector GetLocation() const override;
		virtual FQuat GetRotation() const override { return FQuat::Identity; }
		virtual FVector GetCorner(int32 index) const override;
		virtual void UpdateVisibilityAndCollision(bool &bOutVisible, bool &bOutCollisionEnabled) override;
		virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping = false, bool bForSelection = false) const override;
		virtual AActor *CreateActor(UWorld *world, const FVector &loc, const FQuat &rot) override;
		virtual bool CleanObject(EObjectDirtyFlags DirtyFlag) override;
		virtual void SetupDynamicGeometry() override;
		virtual void UpdateDynamicGeometry() override;
		virtual void OnSelected(bool bNewSelected) override;
		virtual bool ShowStructureOnSelection() const override { return false; }
	
	protected:
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<AActor> PerimeterActor;
		TMap<int32, TWeakObjectPtr<ALineActor>> LineActors;
		TArray<FModumateObjectInstance*> CachedConnectedMOIs;

		void UpdateLineActors();
	};
}

// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once

#include "ModumateObjectInstance.h"

class AMOIGroupActor_CPP;

namespace Modumate
{
	class FModumateObjectInstance;

	class MODUMATE_API FMOIGroupImpl : public FModumateObjectInstanceImplBase
	{
	private:
		TWeakObjectPtr<UWorld> World;
		FVector Location;

		// Only used temporarily inside of GetStructuralPointsAndLines
		mutable TArray<FStructurePoint> TempPoints;
		mutable TArray<FStructureLine> TempLines;

	public:
		FMOIGroupImpl(FModumateObjectInstance *moi);
		virtual ~FMOIGroupImpl() {};

		virtual void SetLocation(const FVector &p) override;

		virtual bool CleanObject(EObjectDirtyFlags DirtyFlag) override;

		virtual AActor *RestoreActor() override;
		virtual AActor *CreateActor(UWorld *world, const FVector &loc, const FQuat &rot) override;

		virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping = false, bool bForSelection = false) const override;

		static float StructuralExtentsExpansion;
	};
}
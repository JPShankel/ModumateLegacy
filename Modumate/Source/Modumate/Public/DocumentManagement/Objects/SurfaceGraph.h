// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "DocumentManagement/ModumateObjectInstance.h"

namespace Modumate
{
	class MODUMATE_API FMOISurfaceGraphImpl : public FModumateObjectInstanceImplBase
	{
	public:
		FMOISurfaceGraphImpl(FModumateObjectInstance *moi);

		virtual void SetRotation(const FQuat &r) override;
		virtual FQuat GetRotation() const override;
		virtual void SetLocation(const FVector &p) override;
		virtual FVector GetLocation() const override;
		virtual FVector GetCorner(int32 index) const override;
		virtual FVector GetNormal() const override;
		virtual void SetupDynamicGeometry() override;

	protected:
		TArray<FVector> CachedFacePoints;
		FVector CachedFaceNormal, CachedFaceAxisX, CachedFaceAxisY, CachedFaceOrigin;
	};
}

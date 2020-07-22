// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/Objects/SurfaceGraph.h"

#include "ModumateCore/ModumateObjectStatics.h"

namespace Modumate
{
	FMOISurfaceGraphImpl::FMOISurfaceGraphImpl(FModumateObjectInstance *moi)
		: FModumateObjectInstanceImplBase(moi)
		, CachedFaceNormal(ForceInitToZero)
		, CachedFaceAxisX(ForceInitToZero)
		, CachedFaceAxisY(ForceInitToZero)
		, CachedFaceOrigin(ForceInitToZero)
	{
	}

	void FMOISurfaceGraphImpl::SetRotation(const FQuat &r)
	{
	}

	FQuat FMOISurfaceGraphImpl::GetRotation() const
	{
		return FRotationMatrix::MakeFromXY(CachedFaceAxisX, CachedFaceAxisY).ToQuat();
	}

	void FMOISurfaceGraphImpl::SetLocation(const FVector &p)
	{
	}

	FVector FMOISurfaceGraphImpl::GetLocation() const
	{
		return CachedFaceOrigin;
	}

	FVector FMOISurfaceGraphImpl::GetCorner(int32 index) const
	{
		return CachedFacePoints.IsValidIndex(index) ? CachedFacePoints[index] : GetLocation();
	}

	FVector FMOISurfaceGraphImpl::GetNormal() const
	{
		return CachedFaceNormal;
	}

	void FMOISurfaceGraphImpl::SetupDynamicGeometry()
	{
		const FModumateObjectInstance *parentObj = MOI ? MOI->GetParentObject() : nullptr;
		int32 faceIndex = UModumateObjectStatics::GetParentFaceIndex(MOI);
		if (UModumateObjectStatics::GetGeometryFromFaceIndex(parentObj, faceIndex, CachedFacePoints, CachedFaceNormal, CachedFaceAxisX, CachedFaceAxisY))
		{
			CachedFaceOrigin = CachedFacePoints[0];
		}
	}
}

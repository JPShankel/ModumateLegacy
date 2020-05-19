// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "DocumentManagement/ModumateControlPointObjectBase.h"


namespace Modumate
{
	class MODUMATE_API FMOIGraphVertex : public FModumateObjectInstanceImplBase
	{
	public:
		FMOIGraphVertex(FModumateObjectInstance *moi)
			: FModumateObjectInstanceImplBase(moi)
		{ }

		virtual void SetLocation(const FVector &p) override;
		virtual FVector GetLocation() const override;
	};

	class MODUMATE_API FMOIGraphEdge : public FModumateControlPointObjectBase
	{
	public:
		FMOIGraphEdge(FModumateObjectInstance *moi)
			: FModumateControlPointObjectBase(moi)
		{ }
	};
}

// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "DocumentManagement/Objects/EdgeBase.h"

namespace Modumate
{
	class MODUMATE_API FMOISurfaceEdgeImpl : public FMOIEdgeImplBase
	{
	public:
		FMOISurfaceEdgeImpl(FModumateObjectInstance *moi);

		virtual bool CleanObject(EObjectDirtyFlags DirtyFlag) override;
		virtual void UpdateVisibilityAndCollision(bool &bOutVisible, bool &bOutCollisionEnabled) override;
	};
}

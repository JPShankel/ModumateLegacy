// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "DocumentManagement/Objects/VertexBase.h"

class AModumateVertexActor_CPP;

namespace Modumate
{
	class MODUMATE_API FMOIMetaVertexImpl : public FMOIVertexImplBase
	{
	public:
		FMOIMetaVertexImpl(FModumateObjectInstance *moi);

		virtual void UpdateVisibilityAndCollision(bool &bOutVisible, bool &bOutCollisionEnabled) override;
		virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<TSharedPtr<FDelta>>* OutSideEffectDeltas) override;
	};
}

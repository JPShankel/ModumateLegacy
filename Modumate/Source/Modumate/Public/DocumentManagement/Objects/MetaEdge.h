// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "DocumentManagement/Objects/EdgeBase.h"
#include "DocumentManagement/ModumateMiterNodeInterface.h"

namespace Modumate
{
	class MODUMATE_API FMOIMetaEdgeImpl : public FMOIEdgeImplBase, IMiterNode
	{
	public:
		FMOIMetaEdgeImpl(FModumateObjectInstance *moi);

		virtual bool CleanObject(EObjectDirtyFlags DirtyFlag) override;
		virtual void UpdateVisibilityAndCollision(bool &bOutVisible, bool &bOutCollisionEnabled) override;
		virtual const IMiterNode* GetMiterInterface() const override { return this; }

		// Begin IMiterNode interface
		virtual const FMiterData &GetMiterData() const;
		// End IMiterNode interface

	protected:
		FMiterData CachedMiterData;
	};
}

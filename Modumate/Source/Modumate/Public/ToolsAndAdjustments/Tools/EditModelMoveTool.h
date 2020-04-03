// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once

#include "EditModelSelectTool.h"

class ALineActor3D_CPP;

namespace Modumate
{
	class FMoveObjectTool : public FEditModelToolBase, public FSelectedObjectToolMixin
	{
	private:
		FVector AnchorPoint;
		TMap<int32, FTransform> StartTransforms;
		TWeakObjectPtr<ALineActor3D_CPP> PendingMoveLine;

	public:
		FMoveObjectTool(AEditModelPlayerController_CPP *pc);
		virtual ~FMoveObjectTool();

		virtual bool Activate() override;
		virtual bool Deactivate() override;
		virtual bool BeginUse() override;
		virtual bool FrameUpdate() override;
		virtual bool HandleInputNumber(double n) override;
		virtual bool EndUse() override;
		virtual bool AbortUse() override;
		virtual bool HandleControlKey(bool pressed) override;
	};
}

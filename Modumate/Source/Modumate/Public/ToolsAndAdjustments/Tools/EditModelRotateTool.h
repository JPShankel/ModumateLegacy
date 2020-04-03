// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "EditModelToolbase.h"
#include "EditModelSelectTool.h"

class ALineActor3D_CPP;
class AEditModelPlayerController_CPP;

namespace Modumate
{
	class FRotateObjectTool : public FEditModelToolBase, public FSelectedObjectToolMixin
	{
	private:
		FVector AnchorPoint, AngleAnchor;
		enum eStages
		{
			Neutral = 0,
			AnchorPlaced,
			SelectingAngle
		};

		eStages Stage;

		TWeakObjectPtr<ALineActor3D_CPP> PendingSegmentStart;
		TWeakObjectPtr<ALineActor3D_CPP> PendingSegmentEnd;
		bool bOverrideAngleWithInput = false;
		float InputAngle = 0.f;

	public:
		FRotateObjectTool(AEditModelPlayerController_CPP *pc);
		virtual ~FRotateObjectTool();
		virtual bool Activate() override;
		virtual bool Deactivate() override;
		virtual bool BeginUse() override;
		virtual bool EnterNextStage() override;
		virtual bool EndUse() override;
		virtual bool AbortUse() override;
		void ApplyRotation();
		float CalcToolAngle();
		virtual bool FrameUpdate() override;
		virtual bool HandleInputNumber(double n) override;
	};
}
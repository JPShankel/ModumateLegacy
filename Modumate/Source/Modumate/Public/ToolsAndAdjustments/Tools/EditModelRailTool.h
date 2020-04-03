// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once
#include "EditModelToolBase.h"

class ALineActor3D_CPP;
namespace Modumate
{
	class MODUMATE_API FRailTool : public FEditModelToolBase
	{
	private:
		ALineActor3D_CPP * PendingSegment;

	public:
		FRailTool(AEditModelPlayerController_CPP *controller);
		virtual ~FRailTool();
		virtual bool Activate() override;
		virtual bool BeginUse() override;
		virtual bool EnterNextStage() override;
		virtual bool FrameUpdate() override;
		virtual bool EndUse() override;
		virtual bool AbortUse() override;
		virtual bool HandleSpacebar() override;
		virtual bool HandleControlKey(bool pressed) override;
		bool HandleInputNumber(double n) override;
	};
}
// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "EditModelToolBase.h"

namespace Modumate
{
	class MODUMATE_API FDrawingTool : public FEditModelToolBase
	{
	public:
		FDrawingTool(AEditModelPlayerController_CPP *InController);
		virtual ~FDrawingTool();

		virtual bool Activate() override;
		virtual bool Deactivate() override;
		virtual bool BeginUse() override;
		virtual bool FrameUpdate() override;

		virtual bool EnterNextStage() override;

		virtual bool EndUse() override;
		virtual bool AbortUse() override;

	protected:
	
		TWeakObjectPtr<AEditModelGameMode_CPP> GameMode;
	};
}

// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditModelToolBase.h"

namespace Modumate
{

	class MODUMATE_API FWandTool : public FEditModelToolBase
	{
	public:
		FWandTool(AEditModelPlayerController_CPP *pc);
		virtual ~FWandTool();
		virtual bool Activate() override;
		virtual bool Deactivate() override;
		virtual bool BeginUse() override;
		virtual bool HandleMouseUp() override;
		virtual bool FrameUpdate() override;
		virtual bool EndUse() override;
		virtual bool ShowSnapCursorAffordances() override { return false; }
	};
}
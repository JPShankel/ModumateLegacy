// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once

#include "EditModelToolBase.h"
#include "EditModelPlayerState_CPP.h"
#include "DynamicMeshActor.h"

class AEditModelGameMode_CPP;
class AEditModelGameState_CPP;
class ALineActor3D_CPP;

namespace Modumate
{
	class MODUMATE_API FCreateSimilarTool : public FEditModelToolBase
	{
	public:
		FCreateSimilarTool(AEditModelPlayerController_CPP *InController);
		virtual ~FCreateSimilarTool();

	public:
		virtual bool Activate() override;
		virtual bool Deactivate() override;

		virtual bool BeginUse() override;
		virtual bool HandleMouseUp() override;
		virtual bool EnterNextStage() override;
		virtual bool FrameUpdate() override;
		virtual bool EndUse() override;
		virtual bool AbortUse() override;

		bool GetCreateSimilarActor(bool bUseMouseHoverObject);
	};
}

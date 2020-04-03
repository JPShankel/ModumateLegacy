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
	class MODUMATE_API FCutPlaneTool : public FEditModelToolBase
	{
	public:
		FCutPlaneTool(AEditModelPlayerController_CPP *InController);
		virtual ~FCutPlaneTool();

		virtual bool Activate() override;
		virtual bool Deactivate() override;
		virtual bool BeginUse() override;
		virtual bool FrameUpdate() override;

		virtual bool EnterNextStage() override;

		virtual bool EndUse() override;
		virtual bool AbortUse() override;

	protected:
		TWeakObjectPtr<AEditModelGameMode_CPP> GameMode;

		TWeakObjectPtr<ALineActor3D_CPP> PendingSegment;
		TWeakObjectPtr<ADynamicMeshActor> PendingPlane;
		TArray<FVector> PendingPlanePoints;
		FArchitecturalMaterial PendingPlaneMaterial;

		FVector Origin;
		FVector Normal;
		EMouseMode OriginalMouseMode;

		float DefaultPlaneDimension = 100.0f;
	};
}

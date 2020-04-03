// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "EditModelToolBase.h"
#include "EditModelPlayerState_CPP.h"
#include "DynamicMeshActor.h"

class AEditModelGameMode_CPP;
class AEditModelGameState_CPP;
class ALineActor3D_CPP;

namespace Modumate
{
	class MODUMATE_API FScopeBoxTool : public FEditModelToolBase
	{
	public:
		FScopeBoxTool(AEditModelPlayerController_CPP *InController);
		virtual ~FScopeBoxTool();

		virtual bool Activate() override;
		virtual bool Deactivate() override;
		virtual bool BeginUse() override;
		virtual bool FrameUpdate() override;

		virtual bool EnterNextStage() override;

		virtual bool EndUse() override;
		virtual bool AbortUse() override;

	protected:
		enum EState
		{
			SeekingCutPlane,
			BasePending,
			NormalPending
		};

		bool ResetState();

		TWeakObjectPtr<AEditModelGameMode_CPP> GameMode;
		TWeakObjectPtr<ADynamicMeshActor> PendingBox;
		TWeakObjectPtr<ALineActor3D_CPP> PendingSegment;
		FArchitecturalMaterial PendingBoxMaterial;

		TArray<FVector> PendingBoxBasePoints;

		FVector Origin;
		FVector Normal;
		float Extrusion;
		EMouseMode OriginalMouseMode;

		EState CurrentState;
	};
}

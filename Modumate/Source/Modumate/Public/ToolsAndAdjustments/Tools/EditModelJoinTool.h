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
	class MODUMATE_API FJoinTool : public FEditModelToolBase
	{
	public:
		FJoinTool(AEditModelPlayerController_CPP *InController);
		virtual ~FJoinTool();

	public:
		virtual bool Activate() override;
		virtual bool Deactivate() override;

		virtual bool BeginUse() override;
		virtual bool HandleMouseUp() override;
		virtual bool EnterNextStage() override;
		virtual bool FrameUpdate() override;
		virtual bool EndUse() override;
		virtual bool AbortUse() override;

	private:
		bool UpdateSelectedPlanes();
		
	private:

		EObjectType ActiveObjectType = EObjectType::OTUnknown;

		FPlane ActivePlane;
		TPair<FVector, FVector> ActiveEdge;

		TSet<int32> PendingObjectIDs;
		// Objects that are selectable for join given the starting object
		TSet<int32> FrontierObjectIDs;
	};
}

// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"
#include "EditModelToolBase.h"

class ALineActor3D_CPP;
namespace Modumate
{
	class MODUMATE_API FCabinetTool : public FEditModelToolBase
	{
	private:
		enum EState
		{
			Neutral = 0,
			NewSegmentPending,
			SetHeight
		};

		EState State;

		TWeakObjectPtr<ALineActor3D_CPP> PendingSegment;

		TArray<ALineActor3D_CPP*> BaseSegs, TopSegs, ConnectSegs;
		FPlane CabinetPlane = FPlane(ForceInitToZero);

		FVector LastPendingSegmentLoc = FVector::ZeroVector;
		bool LastPendingSegmentLocValid = false;

	public:
		virtual ~FCabinetTool();
		FCabinetTool(AEditModelPlayerController_CPP *pc);

		virtual bool Activate() override;
		virtual bool BeginUse() override;
		virtual bool FrameUpdate() override;
		void HandleClick(const FVector &p);
		virtual bool HandleInputNumber(double n) override;
		virtual bool AbortUse() override;
		virtual bool EndUse() override;
		virtual bool EnterNextStage() override;

		void BeginSetHeightMode(const TArray<FVector> &basePoly);
	};
}
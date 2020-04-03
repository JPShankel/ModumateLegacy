// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once
#include "EditModelToolBase.h"

class ALineActor3D_CPP;

namespace Modumate
{
	class MODUMATE_API FCountertopTool : public FEditModelToolBase
	{
	private:
		enum EState
		{
			Neutral = 0,
			NewSegmentPending,
		};
		EState State;

		TWeakObjectPtr<ALineActor3D_CPP> PendingSegment;
		bool Inverted = true;
		FVector AnchorPointDegree;

	public:

		FCountertopTool(AEditModelPlayerController_CPP *controller);
		virtual ~FCountertopTool();
		virtual bool HandleInputNumber(double n) override;
		virtual bool Activate() override;
		virtual bool Deactivate() override;
		virtual bool BeginUse() override;
		virtual bool EnterNextStage() override;
		void HandleClick(const FVector &p);
		virtual bool FrameUpdate() override;
		virtual bool EndUse() override;
		virtual bool AbortUse() override;
		virtual bool HandleSpacebar() override;
		void SegmentsConformInvert();
	};
}
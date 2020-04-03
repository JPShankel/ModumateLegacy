// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once

#include "EditModelToolBase.h"
#include "Engine/Engine.h"
#include "ModumateObjectAssembly.h"
#include "ModumateObjectStatics.h"
#include "EditModelPlayerState_CPP.h"

class ADynamicMeshActor;

namespace Modumate
{
	class MODUMATE_API FTrimTool : public FEditModelToolBase
	{
	private:
		bool bInverted;
		class FModumateObjectInstance *CurrentTarget;
		TArray<class FModumateObjectInstance *> CurrentTargetChildren;
		TWeakObjectPtr<AActor> CurrentHitActor;
		int32 CurrentStartIndex, CurrentEndIndex, CurrentMountIndex;
		float CurrentStartAlongEdge, CurrentEndAlongEdge;
		bool bCurrentLengthsArePCT;
		FVector CurrentEdgeStart, CurrentEdgeEnd;
		ETrimMiterOptions MiterOptionStart, MiterOptionEnd;
		EMouseMode OriginalMouseMode;
		FModumateObjectAssembly TrimAssembly;
		TWeakObjectPtr<ADynamicMeshActor> PendingTrimActor;

		void ResetTarget();
		bool HasValidTarget() const;
		void OnAssemblySet();
		bool ConstrainTargetToSolidEdge(float targetPosAlongEdge, int32 startIndex,int32 endIndex);

	public:

		FTrimTool(AEditModelPlayerController_CPP *controller);
		virtual ~FTrimTool();
		virtual bool HandleInputNumber(double n) override;
		virtual bool Activate() override;
		virtual bool Deactivate() override;
		virtual bool BeginUse() override;
		virtual bool FrameUpdate() override;
		virtual bool EndUse() override;
		virtual bool AbortUse() override;
		virtual bool HandleSpacebar() override;
		virtual void SetAssembly(const FShoppingItem &key) override;

		FColor AffordanceLineColor = FColor::Orange;
		float AffordanceLineInterval = 4.0f;
	};
}
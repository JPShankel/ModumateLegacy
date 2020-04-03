// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once

#include "EditModelToolbase.h"
#include "EditModelSelectTool.h"
#include "EditModelGameState_CPP.h"
#include "EditModelPlayerController_CPP.h"

namespace Modumate
{
	class FFinishTool : public FEditModelToolBase, public FSelectedObjectToolMixin
	{
	private:
		class FModumateObjectInstance *LastValidTarget = nullptr;
		TWeakObjectPtr<AActor> LastValidHitActor = nullptr;
		FVector LastValidHitLocation = FVector::ZeroVector;
		FVector LastValidHitNormal = FVector::ZeroVector;
		int32 LastValidFaceIndex = INDEX_NONE;
		TArray<int32> LastCornerIndices;
		EMouseMode OriginalMouseMode = EMouseMode::Object;
		FModumateObjectAssembly FinishAssembly = FModumateObjectAssembly();

	public:
		FFinishTool(AEditModelPlayerController_CPP *pc);
		virtual ~FFinishTool();
		virtual bool Activate() override;
		virtual bool Deactivate() override;
		virtual bool BeginUse() override;
		virtual bool EnterNextStage() override;
		virtual bool EndUse() override;
		virtual bool AbortUse() override;
		virtual bool FrameUpdate() override;

		FColor AffordanceLineColor = FColor::Orange;
		float AffordanceLineThickness = 4.0f;
		float AffordanceLineInterval = 8.0f;
	};
}

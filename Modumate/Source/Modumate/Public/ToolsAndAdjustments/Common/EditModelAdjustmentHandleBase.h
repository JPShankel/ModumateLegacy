// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "ToolsAndAdjustments/Interface/IModumateAdjustmentHandle.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "CoreMinimal.h"

namespace Modumate
{
	class MODUMATE_API FEditModelAdjustmentHandleBase : public IAdjustmentHandleImpl
	{
	public:
		FEditModelAdjustmentHandleBase(FModumateObjectInstance *moi);

		FModumateObjectInstance * MOI;
		EMouseMode OriginalMouseMode;
		TWeakObjectPtr<AEditModelPlayerController_CPP> Controller;
		bool bIsInUse = false;

		virtual bool OnBeginUse() override;
		virtual bool OnUpdateUse() override;
		virtual bool OnEndUse() override;
		virtual bool OnAbortUse() override;
		virtual bool IsInUse() const { return bIsInUse; }
		virtual bool HandleInputNumber(float number) override { return false; }
		virtual bool HandleSpacebar() override { return false; }
		virtual bool GetOverrideHandleRotation(FQuat &OutRotation) override { return false; }

		const static FName StateRequestTag;

	protected:
		void UpdateTargetGeometry(bool bIncremental = true);

	private:
		bool OnEndOrAbort();
	};

	class MODUMATE_API FAdjustLineSegmentHandle : public FEditModelAdjustmentHandleBase
	{
	protected:
		FVector OriginalP;
		int CP;

	public:
		FAdjustLineSegmentHandle(FModumateObjectInstance *moi, int cp);

		virtual bool OnBeginUse() override;
		virtual bool OnUpdateUse() override;
		virtual bool OnAbortUse() override;
		virtual bool OnEndUse();
		virtual FVector GetAttachmentPoint() override;
	};
}

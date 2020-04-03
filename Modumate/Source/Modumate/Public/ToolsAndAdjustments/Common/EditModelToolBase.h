// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "IModumateEditorTool.h"
#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "ModumateShoppingItem.h"

/**
 *
 */

class AEditModelPlayerController_CPP;

namespace Modumate
{

	class MODUMATE_API FEditModelToolBase : public IModumateEditorTool
	{
	protected:
		bool InUse;
		bool Active;
		FShoppingItem Assembly;
		TWeakObjectPtr<AEditModelPlayerController_CPP> Controller;
		EAxisConstraint AxisConstraint;
		EToolCreateObjectMode CreateObjectMode;

	public:

		FEditModelToolBase(AEditModelPlayerController_CPP *pc);

		virtual ~FEditModelToolBase() {}

		virtual bool Activate() override;
		virtual bool HandleInputNumber(double n) override;
		virtual bool Deactivate() override;
		virtual bool IsInUse() const override { return InUse; }
		virtual bool IsActive() const override { return Active; }
		virtual bool BeginUse() override;
		virtual bool EnterNextStage() override;
		virtual bool ScrollToolOption(int32 dir) override;
		virtual bool FrameUpdate() override;
		virtual bool EndUse() override;
		virtual bool AbortUse() override;
		virtual bool HandleSpacebar() override { return true; }
		virtual bool HandleControlKey(bool pressed) override { return true; }
		virtual bool HandleMouseUp() override { return true; }
		virtual bool ShowSnapCursorAffordances() { return true; }

		virtual const FShoppingItem &GetAssembly() const override { return Assembly; }
		virtual void SetAssembly(const FShoppingItem &key) override { Assembly = key; }
		virtual void SetAxisConstraint(EAxisConstraint InAxisConstraint) override { AxisConstraint = InAxisConstraint; }
		virtual void SetCreateObjectMode(EToolCreateObjectMode InCreateObjectMode) override { CreateObjectMode = InCreateObjectMode; }
	};
}

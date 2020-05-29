// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

class AAdjustmentHandleActor_CPP;

namespace Modumate {
	class FModumateObjectInstance;

	class MODUMATE_API IAdjustmentHandleImpl
	{
	public:
		virtual ~IAdjustmentHandleImpl() {}
		virtual bool OnBeginUse() = 0;
		virtual bool OnUpdateUse() = 0;
		virtual bool OnEndUse() = 0;
		virtual bool OnAbortUse() = 0;
		virtual bool IsInUse() const = 0;
		virtual bool HandleInputNumber(float number) = 0;
		virtual FVector GetAttachmentPoint() = 0;
		virtual bool GetOverrideHandleRotation(FQuat &OutRotation) = 0;
		TWeakObjectPtr<AAdjustmentHandleActor_CPP> Handle;
		virtual bool HandleInvert() = 0;
	};
}

// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Common/ModumateInterpBuffer.h"


FModumateInterpBuffer::FModumateInterpBuffer()
	: TransformBufferSize(6)
	, TransformInterpDelay(0.25)
{ }

FModumateInterpBuffer::FModumateInterpBuffer(int32 BufferSize, float InterpDelay)
{
	TransformBufferSize = BufferSize;
	TransformInterpDelay = InterpDelay;
}

bool FModumateInterpBuffer::GetBlendedTransform(float CurrentTime, FTransform& OutTransform)
{
	// Use buffered events to smooth the replicated transform,
	// then interpolate some of the buffered events at a target delay from the current time, to avoid jittering.
	int32 numBufferedCamTransforms = TransformBuffer.Num();
	if (numBufferedCamTransforms > 0 && TransformBufferSize > 1)
	{
		float targetTime = CurrentTime - TransformInterpDelay;

		float interpAlpha = 0.0f;
		FTransform olderTransform, newerTransform;
		for (int32 i = 0; i < numBufferedCamTransforms; ++i)
		{
			auto& bufferedEvent = TransformBuffer[i];
			if (targetTime >= bufferedEvent.Key)
			{
				auto& newerEvent = TransformBuffer[(i > 0) ? i - 1 : i];

				olderTransform = bufferedEvent.Value;
				newerTransform = newerEvent.Value;
				interpAlpha = FMath::GetRangePct(bufferedEvent.Key, newerEvent.Key, targetTime);
				break;
			}
		}

		FTransform blendedCamTransform;
		blendedCamTransform.LerpTranslationScale3D(olderTransform, newerTransform, ScalarRegister(interpAlpha));
		blendedCamTransform.SetRotation(FQuat::Slerp(olderTransform.GetRotation(), newerTransform.GetRotation(), interpAlpha));
		OutTransform = blendedCamTransform;
		return true;
	}
	OutTransform = FTransform::Identity;
	return false;
}

void FModumateInterpBuffer::AddToTransformBuffer(float CurrentTime, const FTransform& InTransform)
{
	TransformBuffer.Insert(TPair<float, FTransform>(CurrentTime, InTransform), 0);
	while (TransformBuffer.Num() > TransformBufferSize)
	{
		TransformBuffer.Pop(false);
	}
}

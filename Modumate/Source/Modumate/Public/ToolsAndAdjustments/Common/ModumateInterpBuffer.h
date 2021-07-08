// Copyright 2021 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "ModumateInterpBuffer.generated.h"

USTRUCT(BlueprintType)
struct MODUMATE_API FModumateInterpBuffer
{
	GENERATED_BODY()

	FModumateInterpBuffer();
	FModumateInterpBuffer(int32 BufferSize, float InterpDelay);

	// The number of buffer to use for smoothing
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Multiplayer")
	int32 TransformBufferSize;

	// The amount of time, in seconds, to look backwards for interpolating
	// Should be smaller than the maximum expected buffer duration (TransformBufferSize / NetUpdateFrequency)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Multiplayer")
	float TransformInterpDelay;

	bool GetBlendedTransform(float CurrentTime, FTransform& OutTransform);
	void AddToTransformBuffer(float CurrentTime, const FTransform& InTransform);

private:

	TArray<TPair<float, FTransform>> TransformBuffer;
};

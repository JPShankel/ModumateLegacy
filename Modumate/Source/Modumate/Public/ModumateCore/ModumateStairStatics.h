#pragma once

#include "CoreMinimal.h"

namespace Modumate
{
	struct FStairStatics
	{
		static bool MakeLinearRunPolysFromBox(
			const FVector &RunDir, const FVector &WidthDir,
			float StepRun, float StepRise, int32 NumTreads, float Width,
			bool bMakeRisers, bool bStartingRiser, bool bEndingRiser,
			TArray<TArray<FVector>> &OutTreadPolys, TArray<TArray<FVector>> &OutRiserPolys);

		static bool CalculateLinearRunPolysFromPlane(
			const TArray<FVector> &RunPlanePoints, float GoalRunDepth, bool bMakeRisers, bool bStartingRiser, bool bEndingRiser,
			float &OutStepRun, float &OutStepRise, FVector &OutRunDir, FVector &OutStairOrigin,
			TArray<TArray<FVector>> &OutTreadPolys, TArray<TArray<FVector>> &OutRiserPolys);
	};
}

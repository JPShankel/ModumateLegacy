#include "ModumateCore/ModumateStairStatics.h"

#include "Algo/Transform.h"
#include "ModumateCore/ModumateGeometryStatics.h"

namespace Modumate
{
	bool FStairStatics::MakeLinearRunPolysFromBox(
		const FVector &RunDir, const FVector &WidthDir,
		float StepRun, float StepRise, int32 NumTreads, float Width,
		bool bMakeRisers, bool bStartingRiser, bool bEndingRiser,
		TArray<TArray<FVector>> &OutTreadPolys, TArray<TArray<FVector>> &OutRiserPolys)
	{
		OutTreadPolys.Reset();
		OutRiserPolys.Reset();

		if (!RunDir.IsNormalized() || !WidthDir.IsNormalized() || FMath::IsNearlyZero(StepRun) ||
			FMath::IsNearlyZero(StepRise) || (NumTreads == 0) || FMath::IsNearlyZero(Width))
		{
			return false;
		}

		FVector stepMinWidthOffset(ForceInitToZero);
		FVector stepMaxWidthOffset = Width * WidthDir;

		// Make the polygons for each tread and riser
		float curStepMinRun = 0.0f;
		float curStepMinRise = (bStartingRiser ? 0.0f : -StepRise);
		for (int32 treadIdx = 0; treadIdx <= NumTreads; ++treadIdx)
		{
			FVector stepMinRunOffset = curStepMinRun * RunDir;
			FVector stepMaxRunOffset = (curStepMinRun + StepRun) * RunDir;
			FVector stepMinRiseOffset = curStepMinRise * FVector::UpVector;
			FVector stepMaxRiseOffset = (curStepMinRise + StepRise) * FVector::UpVector;

			// Create the step riser
			if (((treadIdx == 0) && bStartingRiser) ||
				((treadIdx == NumTreads) && bEndingRiser) ||
				((treadIdx > 0) && (treadIdx < NumTreads)))
			{
				TArray<FVector> &riserPoly = OutRiserPolys.AddDefaulted_GetRef();

				riserPoly.Add(stepMinRiseOffset + stepMinWidthOffset + stepMinRunOffset);
				riserPoly.Add(stepMinRiseOffset + stepMaxWidthOffset + stepMinRunOffset);
				riserPoly.Add(stepMaxRiseOffset + stepMaxWidthOffset + stepMinRunOffset);
				riserPoly.Add(stepMaxRiseOffset + stepMinWidthOffset + stepMinRunOffset);
			}

			// Create the step tread
			if (treadIdx < NumTreads)
			{
				TArray<FVector> &treadPoly = OutTreadPolys.AddDefaulted_GetRef();

				treadPoly.Add(stepMaxRiseOffset + stepMinWidthOffset + stepMinRunOffset);
				treadPoly.Add(stepMaxRiseOffset + stepMaxWidthOffset + stepMinRunOffset);
				treadPoly.Add(stepMaxRiseOffset + stepMaxWidthOffset + stepMaxRunOffset);
				treadPoly.Add(stepMaxRiseOffset + stepMinWidthOffset + stepMaxRunOffset);
			}

			curStepMinRun += StepRun;
			curStepMinRise += StepRise;
		}

		return true;
	}

	bool FStairStatics::CalculateLinearRunPolysFromPlane(
		const TArray<FVector> &RunPlanePoints, float GoalRunDepth, bool bMakeRisers, bool bStartingRiser, bool bEndingRiser,
		float &OutStepRun, float &OutStepRise, FVector &OutRunDir, FVector &OutStairOrigin,
		TArray<TArray<FVector>> &OutTreadPolys, TArray<TArray<FVector>> &OutRiserPolys)
	{
		OutStepRun = 0.0f;
		OutStepRise = 0.0f;
		OutRunDir = FVector::ZeroVector;
		OutTreadPolys.Reset();
		OutRiserPolys.Reset();

		if (RunPlanePoints.Num() < 3)
		{
			return false;
		}

		FPlane runPlane;
		if (!UModumateGeometryStatics::GetPlaneFromPoints(RunPlanePoints, runPlane))
		{
			return false;
		}

		// We can't create stairs that are completely vertical or horizontal.
		FVector runPlaneNormal(runPlane);
		if (FVector::Orthogonal(runPlaneNormal, FVector::UpVector) ||
			FVector::Parallel(runPlaneNormal, FVector::UpVector))
		{
			return false;
		}

		// Ensure that our stair coordinate space has its normal and Y (run dir) axes pointing towards the world up.
		if ((runPlaneNormal | FVector::UpVector) < 0.0f)
		{
			runPlaneNormal *= -1.0f;
		}

		FVector runPlaneX, runPlaneY;
		UModumateGeometryStatics::FindBasisVectors(runPlaneX, runPlaneY, runPlaneNormal);

		if ((runPlaneY | FVector::UpVector) < 0.0f)
		{
			runPlaneX *= -1.0f;
			runPlaneY *= -1.0f;
		}

		OutRunDir = FVector::VectorPlaneProject(runPlaneY, FVector::UpVector).GetSafeNormal();
		int32 numPoints = RunPlanePoints.Num();

		// Find the minimum and maximum points of the staircase plane,
		// to find the total number of steps to create.
		int32 minZPointIdx = INDEX_NONE, maxZPointIdx = INDEX_NONE;
		int32 minXPointIdx = INDEX_NONE, maxXPointIdx = INDEX_NONE;

		for (int32 pointIdx = 0; pointIdx < numPoints; ++pointIdx)
		{
			const FVector &point = RunPlanePoints[pointIdx];

			if ((minZPointIdx == INDEX_NONE) || (point.Z < RunPlanePoints[minZPointIdx].Z))
			{
				minZPointIdx = pointIdx;
			}
			if ((maxZPointIdx == INDEX_NONE) || (point.Z > RunPlanePoints[maxZPointIdx].Z))
			{
				maxZPointIdx = pointIdx;
			}

			if ((minXPointIdx == INDEX_NONE) ||
				((point | runPlaneX) < (RunPlanePoints[minXPointIdx] | runPlaneX)))
			{
				minXPointIdx = pointIdx;
			}
			if ((maxXPointIdx == INDEX_NONE) ||
				((point | runPlaneX) > (RunPlanePoints[maxXPointIdx] | runPlaneX)))
			{
				maxXPointIdx = pointIdx;
			}
		}

		if ((minZPointIdx == INDEX_NONE) || (maxZPointIdx == INDEX_NONE) ||
			(minXPointIdx == INDEX_NONE) || (maxXPointIdx == INDEX_NONE))
		{
			return false;
		}

		// Find the delta that maximally describes the extents of the staircase rise and run
		const FVector &minZPoint = RunPlanePoints[minZPointIdx];
		const FVector &maxZPoint = RunPlanePoints[maxZPointIdx];
		FVector maxPointDelta = maxZPoint - minZPoint;
		maxPointDelta -= maxPointDelta.ProjectOnToNormal(runPlaneX);
		float maxDeltaRise = maxPointDelta.Z;
		float maxDeltaRun = maxPointDelta | OutRunDir;

		int32 numTreads = FMath::FloorToInt(maxDeltaRun / GoalRunDepth);
		OutStepRun = maxDeltaRun / numTreads;
		int32 numRisers = numTreads + (bStartingRiser ? 1 : 0) + (bEndingRiser ? 0 : -1);
		OutStepRise = maxDeltaRise / numRisers;

		const FVector &minXPoint = RunPlanePoints[minXPointIdx];
		const FVector &maxXPoint = RunPlanePoints[maxXPointIdx];
		OutStairOrigin =
			(OutRunDir * (minZPoint | OutRunDir)) +
			(runPlaneX * (minXPoint | runPlaneX)) +
			(FVector::UpVector * minZPoint.Z);
		float stairWidth = (maxXPoint - minXPoint) | runPlaneX;

		return FStairStatics::MakeLinearRunPolysFromBox(
			OutRunDir, runPlaneX, OutStepRun, OutStepRise, numTreads, stairWidth,
			bMakeRisers, bStartingRiser, bEndingRiser, OutTreadPolys, OutRiserPolys
		);
	}
}

// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "Database/ModumateSimpleMesh.h"

#include "Misc/FeedbackContext.h"
#include "Misc/Paths.h"


void FSimplePolygon::Reset()
{
	Points.Reset();
	Triangles.Reset();
	Extents.Init();
}

void FSimplePolygon::UpdateExtents()
{
	Extents = FBox2D(Points);
}

bool FSimplePolygon::ValidateSimple(FFeedbackContext* InWarn) const
{
	int32 numPoints = Points.Num();
	for (int32 segIdxAStart = 0; segIdxAStart < numPoints; ++segIdxAStart)
	{
		int32 segIdxAEnd = (segIdxAStart + 1) % numPoints;
		const FVector2D &segAStart = Points[segIdxAStart];
		const FVector2D &segAEnd = Points[segIdxAEnd];

		for (int32 segIdxBStart = 0; segIdxBStart < numPoints; ++segIdxBStart)
		{
			int32 segIdxBEnd = (segIdxBStart + 1) % numPoints;
			const FVector2D &segBStart = Points[segIdxBStart];
			const FVector2D &segBEnd = Points[segIdxBEnd];
			FVector intersectionPoint;

			if ((segIdxAStart != segIdxBStart) &&
				!segAStart.Equals(segBStart) && !segAStart.Equals(segBEnd) &&
				!segAEnd.Equals(segBStart) && !segAEnd.Equals(segBEnd))
			{
				FVector segAStart3D(segAStart, 0.0f), segAEnd3D(segAEnd, 0.0f);
				FVector segBStart3D(segBStart, 0.0f), segBEnd3D(segBEnd, 0.0f);

				if (FMath::SegmentIntersection2D(segAStart3D, segAEnd3D, segBStart3D, segBEnd3D, intersectionPoint))
				{
					if (InWarn)
					{
						InWarn->Logf(ELogVerbosity::Error, TEXT("Line segments (%s, %s) and (%s, %s) intersect; invalid simple polygon!"),
							*segAStart.ToString(), *segAEnd.ToString(), *segBStart.ToString(), *segBEnd.ToString());
					}
					return false;
				}
			}
		}
	}

	return true;
}

bool USimpleMeshData::SetSourcePath(const FString &FullFilePath)
{
	FString convertedRelPath = FullFilePath;

	SourceRelPath.Empty();
	SourceAbsPath.Empty();

	if (FPaths::MakePathRelativeTo(convertedRelPath, *FPaths::ProjectContentDir()))
	{
		SourceRelPath = convertedRelPath;
		return true;
	}
	else if (!FPaths::IsRelative(FullFilePath) && FPaths::FileExists(FullFilePath))
	{
		SourceAbsPath = FullFilePath;
		return true;
	}

	return false;
}

FString USimpleMeshData::GetSourceFullPath() const
{
	if (!SourceRelPath.IsEmpty())
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir(), SourceRelPath);
	}
	else if (!SourceAbsPath.IsEmpty())
	{
		return SourceAbsPath;
	}
	else
	{
		return FString();
	}
}

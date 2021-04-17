// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "Database/ModumateSimpleMesh.h"

#include "Misc/FeedbackContext.h"
#include "Misc/Paths.h"
#include "ModumateCore/ModumateGeometryStatics.h"


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
	// Use a smaller dot epsilon for these polygons, since they're being validated directly from parsed data rather than from geometry calculations.
	static constexpr float polyDotEpsilon = 0.01f;
	return UModumateGeometryStatics::IsPolygon2DValid(Points, InWarn, KINDA_SMALL_NUMBER, polyDotEpsilon);
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

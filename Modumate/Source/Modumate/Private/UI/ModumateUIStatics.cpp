// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/ModumateUIStatics.h"

FString UModumateUIStatics::GetEllipsizeString(const FString& InString, const int32 StringLength)
{
	if (InString.Len() <= StringLength)
	{
		return InString;
	}
	return InString.Left(StringLength) + FString(TEXT("..."));
}

FLinearColor UModumateUIStatics::GetTextColorFromBackgroundColor(const FLinearColor& BackgroundColor)
{
	// Decide whether the text for the player name should be black or white, based on the luminance of the background,
	// for contrast purposes, based on standard accessibility thresholds.
	static const float whiteTextThresh = 0.66667f;
	float playerColorLuminance = BackgroundColor.ComputeLuminance();
	return (playerColorLuminance > whiteTextThresh) ? FLinearColor::Black : FLinearColor::White;
}

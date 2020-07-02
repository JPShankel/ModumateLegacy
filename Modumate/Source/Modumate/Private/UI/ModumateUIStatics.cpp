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

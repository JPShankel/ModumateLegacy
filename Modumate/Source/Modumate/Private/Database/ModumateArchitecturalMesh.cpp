// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "Database/ModumateArchitecturalMesh.h"

void FArchitecturalMesh::ReadNamedDimensions(const FString& InNamedDimensions)
{
	// Named dimensions are of the format: ("NamedDim1",5),("NamedDim2",10)
	if (!InNamedDimensions.IsEmpty())
	{
		TArray<FString> params;
		InNamedDimensions.ParseIntoArray(params,TEXT(","));

		if (ensureAlways((params.Num() % 2) == 0))
		{
			// TODO: ini file reader
			for (int32 i = 0; i < params.Num(); i += 2)
			{
				params[i].RemoveFromStart(TEXT("("));
				params[i] = params[i].TrimQuotes();
				params[i + 1].RemoveFromEnd(TEXT(")"));
				float inches = FCString::Atof(*params[i + 1]);
				NamedDimensions.Add(params[i], FModumateUnitValue(inches,EModumateUnitType::WorldInches));
			}
		}
	}
}

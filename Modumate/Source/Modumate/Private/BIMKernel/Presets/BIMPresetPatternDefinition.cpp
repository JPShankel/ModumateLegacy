// Copyright 2022 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/Presets/BIMPresetPatternDefinition.h"

void FBIMPatternCoordinate::FromCSVCells(const TArray<FString>& Cells)
{
	if (Cells.Num() > 0)
	{
		X = FCString::Atoi(*Cells[0]);
		if (Cells.Num() > 1)
		{
			Y = FCString::Atoi(*Cells[1]);
			if (Cells.Num() > 2)
			{
				Z = FCString::Atoi(*Cells[2]);
			}
		}
	}
}

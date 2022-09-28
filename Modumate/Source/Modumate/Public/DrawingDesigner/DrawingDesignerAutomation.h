// Copyright 2022 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Drafting/ModumateDimensions.h"
#include "DrawingDesignerAutomation.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(ModumateDrawingAutomation, Log, All);

USTRUCT()
struct MODUMATE_API  FDrawingDesignerAutomation
{
	GENERATED_BODY()

	FDrawingDesignerAutomation() = default;
	FDrawingDesignerAutomation(UModumateDocument * Document);
	TArray<FModumateDimension> GenerateDimensions(int32 CutPlaneID) const;


private:
	UModumateDocument * Document;
};
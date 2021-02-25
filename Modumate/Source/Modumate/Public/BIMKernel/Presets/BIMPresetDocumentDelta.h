// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "DocumentManagement/DocumentDelta.h"
#include "BIMKernel/Presets/BIMPresetInstance.h"
#include "BIMPresetDocumentDelta.generated.h"

USTRUCT()
struct MODUMATE_API FBIMPresetDelta : public FDocumentDelta
{
	GENERATED_BODY()

	UPROPERTY()
	FBIMPresetInstance OldState;
	
	UPROPERTY()
	FBIMPresetInstance NewState;

	virtual bool ApplyTo(UModumateDocument* Doc, UWorld* World) const override;
	virtual TSharedPtr<FDocumentDelta> MakeInverse() const override;
	virtual FStructDataWrapper SerializeStruct() override;
};
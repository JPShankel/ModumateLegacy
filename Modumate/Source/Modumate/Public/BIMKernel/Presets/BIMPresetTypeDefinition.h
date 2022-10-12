// Copyright 2020 Modumate, Inc.All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "BIMKernel/Core/BIMProperties.h"
#include "BIMKernel/Core/BIMEnums.h"
#include "BIMKernel/Presets/BIMPresetEditorForm.h"

#include "BIMPresetTypeDefinition.generated.h"


/*
A pin set defines a minimum and maximum number of attached children (each child is a pin), which lists define legal
children for this pin (ie a list of colors or materials available for a given object) and pointers to node instances
attached to this pin set.

A node definition specifies zero or more pin sets for child attachment
*/

UENUM()
enum class EBIMPinTarget : uint8
{
	None = 0,
	Default,
	Tread,
	Riser,
	Cabinet
};

USTRUCT()
struct MODUMATE_API FBIMPresetNodePinSet
{
	GENERATED_BODY()
	
	UPROPERTY()
	FName SetName;
	
	UPROPERTY()
	int32 MinCount;
	
	UPROPERTY()
	int32 MaxCount;
	
	UPROPERTY()
	EBIMPinTarget PinTarget = EBIMPinTarget::Default;

	FBIMPresetNodePinSet(FName InSetName, int32 InMinCount, int32 InMaxCount);
	FBIMPresetNodePinSet();

	bool operator==(const FBIMPresetNodePinSet& RHS) const;
	bool operator!=(const FBIMPresetNodePinSet& RHS) const;
};

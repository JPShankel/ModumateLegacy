// Copyright 2020 Modumate, Inc.All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "BIMKernel/BIMProperties.h"
#include "BIMKernel/BIMEnums.h"

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

	FBIMPresetNodePinSet(FName InSetName, int32 InMinCount, int32 InMaxCount) :
		SetName(InSetName), MinCount(InMinCount), MaxCount(InMaxCount)
	{}

	FBIMPresetNodePinSet() {};
};

/*
Crafting tree nodes are defined in the DDL 2.0 script, which specifies which properties nodes of a given type carry
and what pins it supports for child nodes

Some properties are hidden, so visible/editable properties are specified in the FormItemToProperty map
TODO: form visibility and editability of properties to be specified in presets, not as part of the fundamental type
*/
USTRUCT()
struct MODUMATE_API FBIMPresetTypeDefinition
{
	GENERATED_BODY()

	UPROPERTY()
	FName TypeName;

	/*
	Motivating use case for Scope is so adjective nodes (ie color, dimension, material) will understand what noun
	(ie module, layer, gap) they modify. Adjective nodes with properties in the 'Node' scope have those properties
	rebound to the scope of the parent pin to which they are attached
	*/
	
	UPROPERTY()
	EBIMValueScope Scope = EBIMValueScope::None;
	
	UPROPERTY()
	TMap<FString, FName> FormItemToProperty;

	UPROPERTY()
	TArray<FBIMPresetNodePinSet> PinSets;

	FBIMPropertySheet Properties;
};
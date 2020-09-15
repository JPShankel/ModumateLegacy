// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BIMKernel/BIMEnums.h"
#include "BIMUProperties.generated.h"

/*
UBIMProperty objects are sheets of UProperties associated with the presets defined in the DDL tables
Each node type will have a corresponding UObject to hold its properties.
Helper functions in the base class allow properties to be accessed by name and type
*/

UCLASS()
class MODUMATE_API UBIMPropertyBase : public UObject
{
	GENERATED_BODY()

public:

	// TODO: privatize and provide friend access to a reader
	// No one else should be setting properties by name
	ECraftingResult SetFloatProperty(const FName &Property, float Value);
	ECraftingResult SetStringProperty(const FName &Property, const FString& Value);

	UPROPERTY()
	FString Name;
};

const static float MOD_DIM_NONE = NAN;

UCLASS()
class MODUMATE_API UBIMPropertiesDIM : public UBIMPropertyBase
{
	GENERATED_BODY()

public:
	UPROPERTY()
	float Thickness = MOD_DIM_NONE;

	UPROPERTY()
	float Width = MOD_DIM_NONE;

	UPROPERTY()
	float Depth = MOD_DIM_NONE;

	UPROPERTY()
	float Height = MOD_DIM_NONE;

	UPROPERTY()
	float Length = MOD_DIM_NONE;

	UPROPERTY()
	float WebThickness = MOD_DIM_NONE;

	UPROPERTY()
	float FlangeThickness = MOD_DIM_NONE;
};

UCLASS()
class MODUMATE_API UBIMPropertiesSlot : public UBIMPropertyBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FName ID;

	UPROPERTY()
	FName SupportedNCPs;

	UPROPERTY()
	FString LocationX;

	UPROPERTY()
	FString LocationY;

	UPROPERTY()
	FString LocationZ;

	UPROPERTY()
	FString SizeX;

	UPROPERTY()
	FString SizeY;

	UPROPERTY()
	FString SizeZ;

	UPROPERTY()
	FString RotationX;

	UPROPERTY()
	FString RotationY;

	UPROPERTY()
	FString RotationZ;

	UPROPERTY()
	bool FlipX = false;

	UPROPERTY()
	bool FlipY = false;

	UPROPERTY()
	bool FlipZ = false;
};
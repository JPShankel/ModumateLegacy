#pragma once

#include "CustomDataWebConvertable.h"
#include "BIMSlot.generated.h"


USTRUCT()
struct FBIMSlot : public FCustomDataWebConvertable
{
	GENERATED_BODY()

	UPROPERTY()
	FString ID;

	UPROPERTY()
	FString SupportedNCPs;

	UPROPERTY()
	FString RequiredInputParamaters;

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
	bool FlipX;

	UPROPERTY()
	bool FlipY;

	UPROPERTY()
	bool FlipZ;

	virtual FString GetPropertyPrefix() const override
	{
		return GetEnumValueString(EPresetPropertyMatrixNames::Slot);
	};

	friend bool operator==(const FBIMSlot& Lhs, const FBIMSlot& RHS)
	{
		return Lhs.ID == RHS.ID
			&& Lhs.SupportedNCPs == RHS.SupportedNCPs
			&& Lhs.RequiredInputParamaters == RHS.RequiredInputParamaters
			&& Lhs.LocationX == RHS.LocationX
			&& Lhs.LocationY == RHS.LocationY
			&& Lhs.LocationZ == RHS.LocationZ
			&& Lhs.SizeX == RHS.SizeX
			&& Lhs.SizeY == RHS.SizeY
			&& Lhs.SizeZ == RHS.SizeZ
			&& Lhs.RotationX == RHS.RotationX
			&& Lhs.RotationY == RHS.RotationY
			&& Lhs.RotationZ == RHS.RotationZ
			&& Lhs.FlipX == RHS.FlipX
			&& Lhs.FlipY == RHS.FlipY
			&& Lhs.FlipZ == RHS.FlipZ;
	}

	friend bool operator!=(const FBIMSlot& Lhs, const FBIMSlot& RHS)
	{
		return !(Lhs == RHS);
	}
	
protected:
	virtual UStruct* VirtualizedStaticStruct() override;
};
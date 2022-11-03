#pragma once
#include "CustomDataWebConvertable.h"
#include "BIMMesh.generated.h"

USTRUCT()
struct FBIMMesh : public FCustomDataWebConvertable 
{
	GENERATED_BODY()
	
	UPROPERTY()
	FString AssetPath;

	UPROPERTY()
	float NativeSizeX;

	UPROPERTY()
	float NativeSizeY;

	UPROPERTY()
	float NativeSizeZ;

	UPROPERTY()
	float SliceX1;

	UPROPERTY()
	float SliceX2;

	UPROPERTY()
	float SliceY1;

	UPROPERTY()
	float SliceY2;

	UPROPERTY()
	float SliceZ1;
	
	UPROPERTY()
	float SliceZ2;

	UPROPERTY()
	FString Tangent;

	UPROPERTY()
	FString Normal;

	UPROPERTY()
	FString MaterialKey;

	virtual FString GetPropertyPrefix() const override
	{
		return GetEnumValueString(EPresetPropertyMatrixNames::Mesh);
	}

	friend bool operator==(const FBIMMesh& Lhs, const FBIMMesh& RHS)
	{
		return Lhs.AssetPath == RHS.AssetPath
			&& Lhs.NativeSizeX == RHS.NativeSizeX
			&& Lhs.NativeSizeY == RHS.NativeSizeY
			&& Lhs.NativeSizeZ == RHS.NativeSizeZ
			&& Lhs.SliceX1 == RHS.SliceX1
			&& Lhs.SliceX2 == RHS.SliceX2
			&& Lhs.SliceY1 == RHS.SliceY1
			&& Lhs.SliceY2 == RHS.SliceY2
			&& Lhs.SliceZ1 == RHS.SliceZ1
			&& Lhs.SliceZ2 == RHS.SliceZ2
			&& Lhs.Tangent == RHS.Tangent
			&& Lhs.Normal == RHS.Normal
			&& Lhs.MaterialKey == RHS.MaterialKey;
	}

	friend bool operator!=(const FBIMMesh& Lhs, const FBIMMesh& RHS)
	{
		return !(Lhs == RHS);
	}

protected:
	virtual UStruct* VirtualizedStaticStruct() override;
};

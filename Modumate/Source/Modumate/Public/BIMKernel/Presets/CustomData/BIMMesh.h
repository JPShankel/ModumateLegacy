#pragma once

#include "BIMMesh.generated.h"

USTRUCT()
struct FBIMMesh
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
};

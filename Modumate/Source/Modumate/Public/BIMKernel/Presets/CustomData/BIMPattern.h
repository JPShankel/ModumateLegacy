#pragma once
#include "CustomDataWebConvertable.h"
#include "BIMPattern.generated.h"

// also known as 2019 patterns in the legacy csvs
USTRUCT()
struct FBIMPatternConfig : public FCustomDataWebConvertable
{
	GENERATED_BODY()
	
	UPROPERTY()
	FString CraftingIconAssetFilePath;

	UPROPERTY()
	float ModuleCount;

	UPROPERTY()
	FString Extents;
	
	UPROPERTY()
    FString Thickness;

	UPROPERTY()
	TArray<FString> ModuleDimensions;
	
	virtual FString GetPropertyPrefix() const override
	{
		return GetEnumValueString(EPresetPropertyMatrixNames::Pattern);
	};

	friend bool operator==(const FBIMPatternConfig& Lhs, const FBIMPatternConfig& RHS)
	{
		return Lhs.CraftingIconAssetFilePath == RHS.CraftingIconAssetFilePath
			&& Lhs.ModuleCount == RHS.ModuleCount
			&& Lhs.Extents == RHS.Extents
			&& Lhs.Thickness == RHS.Thickness
			&& Lhs.ModuleDimensions == RHS.ModuleDimensions;
	}

	friend bool operator!=(const FBIMPatternConfig& Lhs, const FBIMPatternConfig& RHS)
	{
		return !(Lhs == RHS);
	}
	
protected:
	virtual UStruct* VirtualizedStaticStruct() override;
};

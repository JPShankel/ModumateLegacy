#pragma once
#include "CustomDataWebConvertable.h"
#include "BIMRawMaterial.generated.h"

USTRUCT()
struct FBIMRawMaterial : public FCustomDataWebConvertable
{
	GENERATED_BODY()
	
	// Path to the Unreal material asset
	UPROPERTY()
	FString AssetPath;

	// Doesn't seem to be used currently.
	UPROPERTY()
	float TextureTilingSize;

	virtual FString GetPropertyPrefix() const override
	{
		return GetEnumValueString(EPresetPropertyMatrixNames::RawMaterial);
	};

	friend bool operator==(const FBIMRawMaterial& Lhs, const FBIMRawMaterial& RHS)
	{
		return Lhs.AssetPath == RHS.AssetPath
			&& Lhs.TextureTilingSize == RHS.TextureTilingSize;
	}

	friend bool operator!=(const FBIMRawMaterial& Lhs, const FBIMRawMaterial& RHS)
	{
		return !(Lhs == RHS);
	}
	
protected:
	virtual UStruct* VirtualizedStaticStruct() override;
};

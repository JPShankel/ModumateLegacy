#pragma once
#include "CustomDataWebConvertable.h"
#include "BIMProfile.generated.h"

USTRUCT()
struct FBIMProfile : public FCustomDataWebConvertable
{
	GENERATED_BODY()
	
	UPROPERTY()
	FString AssetPath;

	virtual FString GetPropertyPrefix() const override
	{
		return GetEnumValueString(EPresetPropertyMatrixNames::Profile);
	};

	friend bool operator==(const FBIMProfile& Lhs, const FBIMProfile& RHS)
	{
		return Lhs.AssetPath == RHS.AssetPath;
	}

	friend bool operator!=(const FBIMProfile& Lhs, const FBIMProfile& RHS)
	{
		return !(Lhs == RHS);
	}
	
protected:
	virtual UStruct* VirtualizedStaticStruct() override;
};

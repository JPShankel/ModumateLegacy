#pragma once
#include "CustomDataWebConvertable.h"
#include "BIMPart.generated.h"


USTRUCT()
struct FBIMPartConfig : public FCustomDataWebConvertable
{
	GENERATED_BODY()

	UPROPERTY()
	FString Normal;

	UPROPERTY()
	FString Tangent;
	
	UPROPERTY()
	bool Zalign;

	virtual FString GetPropertyPrefix() const override
	{
		return GetEnumValueString(EPresetPropertyMatrixNames::Part);
	};

	friend bool operator==(const FBIMPartConfig& Lhs, const FBIMPartConfig& RHS)
	{
		return Lhs.Normal == RHS.Normal
			&& Lhs.Tangent == RHS.Tangent
			&& Lhs.Zalign == RHS.Zalign;
	}

	friend bool operator!=(const FBIMPartConfig& Lhs, const FBIMPartConfig& RHS)
	{
		return !(Lhs == RHS);
	}

protected:
	virtual UStruct* VirtualizedStaticStruct() override;
};
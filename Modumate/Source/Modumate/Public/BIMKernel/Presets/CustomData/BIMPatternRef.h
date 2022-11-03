#pragma once
#include "CustomDataWebConvertable.h"
#include "BIMPatternRef.generated.h"

USTRUCT()
struct FBIMPatternRef : public FCustomDataWebConvertable
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid Source;

	virtual FString GetPropertyPrefix() const override
	{
		return GetEnumValueString(EPresetPropertyMatrixNames::PatternRef);
	};

	friend bool operator==(const FBIMPatternRef& Lhs, const FBIMPatternRef& RHS)
	{
		return Lhs.Source == RHS.Source;
	}

	friend bool operator!=(const FBIMPatternRef& Lhs, const FBIMPatternRef& RHS)
	{
		return !(Lhs == RHS);
	}

protected:
    virtual UStruct* VirtualizedStaticStruct() override;
};
#pragma once
#include "CustomDataWebConvertable.h"
#include "BIMProfileRef.generated.h"

USTRUCT()
struct FBIMProfileRef : public FCustomDataWebConvertable
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid Source;

	virtual FString GetPropertyPrefix() const override
	{
		return GetEnumValueString(EPresetPropertyMatrixNames::ProfileRef);
	};

	friend bool operator==(const FBIMProfileRef& Lhs, const FBIMProfileRef& RHS)
	{
		return Lhs.Source == RHS.Source;
	}

	friend bool operator!=(const FBIMProfileRef& Lhs, const FBIMProfileRef& RHS)
	{
		return !(Lhs == RHS);
	}
	
protected:
	virtual UStruct* VirtualizedStaticStruct() override;
};
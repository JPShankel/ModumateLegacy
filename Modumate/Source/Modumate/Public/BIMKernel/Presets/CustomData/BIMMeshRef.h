#pragma once
#include "CustomDataWebConvertable.h"
#include "BIMMeshRef.generated.h"

USTRUCT()
struct FBIMMeshRef : public FCustomDataWebConvertable
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid Source;

	virtual FString GetPropertyPrefix() const override
	{
		return GetEnumValueString(EPresetPropertyMatrixNames::MeshRef);
	};

	friend bool operator==(const FBIMMeshRef& Lhs, const FBIMMeshRef& RHS)
	{
		return Lhs.Source == RHS.Source;
	}

	friend bool operator!=(const FBIMMeshRef& Lhs, const FBIMMeshRef& RHS)
	{
		return !(Lhs == RHS);
	}

protected:
	virtual UStruct* VirtualizedStaticStruct() override;
};
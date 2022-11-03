#pragma once
#include "CustomDataWebConvertable.h"
#include "BIMSlotConfig.generated.h"


USTRUCT()
struct FBIMSlotConfig : public FCustomDataWebConvertable
{
	GENERATED_BODY()

	UPROPERTY()
	FString ConceptualSizeY;

	virtual FString GetPropertyPrefix() const override
	{
		return GetEnumValueString(EPresetPropertyMatrixNames::SlotConfig);
	};

	friend bool operator==(const FBIMSlotConfig& Lhs, const FBIMSlotConfig& RHS)
	{
		return Lhs.ConceptualSizeY == RHS.ConceptualSizeY;
	}

	friend bool operator!=(const FBIMSlotConfig& Lhs, const FBIMSlotConfig& RHS)
	{
		return !(Lhs == RHS);
	}
	
protected:
	virtual UStruct* VirtualizedStaticStruct() override;
};
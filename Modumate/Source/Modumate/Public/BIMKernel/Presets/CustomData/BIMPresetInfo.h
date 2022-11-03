#pragma once
#include "CustomDataWebConvertable.h"
#include "BIMPresetInfo.generated.h"


USTRUCT()
struct FBIMPresetInfo : public FCustomDataWebConvertable
{
	GENERATED_BODY()

	UPROPERTY()
	FString Comments;

	UPROPERTY()
	FString Mark;

	virtual FString GetPropertyPrefix() const override
	{
		return GetEnumValueString(EPresetPropertyMatrixNames::Preset);
	};

	friend bool operator==(const FBIMPresetInfo& Lhs, const FBIMPresetInfo& RHS)
	{
		return Lhs.Comments == RHS.Comments
			&& Lhs.Mark == RHS.Mark;
	}

	friend bool operator!=(const FBIMPresetInfo& Lhs, const FBIMPresetInfo& RHS)
	{
		return !(Lhs == RHS);
	}

protected:
	virtual UStruct* VirtualizedStaticStruct() override;
};
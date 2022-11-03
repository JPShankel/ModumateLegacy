#pragma once
#include "CustomDataWebConvertable.h"
#include "BIMIESProfile.generated.h"


USTRUCT()
struct FBIMIESProfile : public FCustomDataWebConvertable
{
	GENERATED_BODY()

	UPROPERTY()
	FString AssetPath;

	UPROPERTY()
	FString CraftingIconAssetFilePath;

	virtual FString GetPropertyPrefix() const override
	{
		return GetEnumValueString(EPresetPropertyMatrixNames::IESProfile);
	}

	bool operator==(const FBIMIESProfile& Other) const;
	
protected:
	virtual UStruct* VirtualizedStaticStruct() override;
};
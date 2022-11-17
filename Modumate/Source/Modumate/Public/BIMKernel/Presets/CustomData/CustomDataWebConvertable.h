#pragma once
#include "BIMKernel/Core/BIMEnums.h"
#include "ModumateCore/EnumHelpers.h"
#include "BIMKernel/Core/BIMProperties.h"
#include "CustomDataWebConvertable.generated.h"

struct FBIMWebPreset;

USTRUCT()
struct FCustomDataWebConvertable
{
	GENERATED_BODY()
	virtual ~FCustomDataWebConvertable() = default;
	
	virtual void ConvertToWebPreset(FBIMWebPreset& OutPreset);
	virtual void ConvertFromWebPreset(const FBIMWebPreset& InPreset);
	
	virtual FString GetPropertyPrefix() const
	{
		ensureAlwaysMsgf(false, TEXT("Must override GetPropertyPrefix()"));
		return TEXT("UNKNOWN");
	};
	
	virtual UStruct* VirtualizedStaticStruct();

private:
	bool SerializeByPropertyName(void* Container, FProperty* Property, TMap<FString, FBIMWebPresetProperty>& OutProperties) const;
	bool DeserializeByPropertyName(void* Container, const TMap<FString, FBIMWebPresetProperty> InProperties, FProperty* Property);

};

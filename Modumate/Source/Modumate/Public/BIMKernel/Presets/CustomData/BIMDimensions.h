#pragma once
#include "CustomDataWebConvertable.h"
#include "BIMDimensions.generated.h"


USTRUCT()
struct FBIMDimensions : public FCustomDataWebConvertable
{
	GENERATED_BODY()


private:
	/**
	 * This map is every dimension that is custom set and will be used instead of the default
	 * value from the NamedDimension table
	 *
	 * All dimensions are stored in Centimeters
	 */
	UPROPERTY()
	TMap<FName, float> CustomMap;

protected:
	virtual UStruct* VirtualizedStaticStruct() override;

public:

	bool bHasDefaults = false;
	
	bool HasCustomDimension(const FName& InDimensionName) const;
	void SetCustomDimension(const FName& InName, const float& OutValue);
	
	bool TryGetDimension(const FName& InName, float& OutValue) const;
	
	void ForEachDimension(const TFunction<void(const FName& Name, float Value)>& InFunc) const;
	void ForEachCustomDimension(const TFunction<void(const FName& Name, float Value)>& InFunc) const;

	virtual void ConvertToWebPreset(FBIMWebPreset& OutPreset) override;
	virtual void ConvertFromWebPreset(const FBIMWebPreset& InPreset) override;
	
	virtual FString GetPropertyPrefix() const override
	{
		return GetEnumValueString(EPresetPropertyMatrixNames::Dimensions);
	};

	bool operator==(const FBIMDimensions& Other);
};
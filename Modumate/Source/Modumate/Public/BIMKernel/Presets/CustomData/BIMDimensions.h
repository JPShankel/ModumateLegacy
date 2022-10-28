#pragma once
#include "BIMKernel/Core/BIMEnums.h"

#include "BIMDimensions.generated.h"


USTRUCT()
struct FBIMDimensions
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

public:

	/**
	 *	KLUDGE: Maybe this should be dynamic
	 *	This boolean is used to determine if we are to use the default values from named dimensions
	 *	if no custom value is present
	 */
	UPROPERTY()
	bool bHasDefaults = false;
	
	bool HasCustomDimension(const FName& InDimensionName) const;
	void SetCustomDimension(const FName& InName, const float& OutValue);
	
	bool TryGetDimension(const FName& InName, float& OutValue) const;
	
	void ForEachDimension(const TFunction<void(const FName& Name, float Value)>& InFunc) const;
	void ForEachCustomDimension(const TFunction<void(const FName& Name, float Value)>& InFunc) const;
};
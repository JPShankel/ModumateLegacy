#include "BIMKernel/Presets/CustomData/BIMDimensions.h"

#include "BIMKernel/AssemblySpec/BIMPartSlotSpec.h"
#include "BIMKernel/Presets/BIMPresetInstanceFactory.h"
#include "BIMKernel/Presets/BIMWebPreset.h"


UStruct* FBIMDimensions::VirtualizedStaticStruct()
{
	return FBIMDimensions::StaticStruct();
}

bool FBIMDimensions::HasCustomDimension(const FName& InDimensionName) const
{
	if (CustomMap.Contains(InDimensionName))
	{
		return true;
	}

	return false;
}
void FBIMDimensions::SetCustomDimension(const FName& InName, const float& OutValue)
{
	CustomMap.Add(InName, OutValue);
}

bool FBIMDimensions::TryGetDimension(const FName& InName, float& OutValue) const
{
	if (CustomMap.Contains(InName))
	{
		OutValue = *CustomMap.Find(InName);
		return true;
	}
	
	if (bHasDefaults && FBIMPartSlotSpec::NamedDimensionMap.Contains(InName.ToString()))
	{
		OutValue = FBIMPartSlotSpec::NamedDimensionMap.Find(InName.ToString())->DefaultValue.AsWorldCentimeters();
		return true;
	}

	return false;
}

void FBIMDimensions::ForEachDimension(const TFunction<void(const FName& Name, float Value)>& InFunc) const
{
	if (bHasDefaults)
	{
		for (auto& kvp : FBIMPartSlotSpec::NamedDimensionMap)
		{
			FName key = FName(kvp.Key);
			float val;
			TryGetDimension(key, val);
			InFunc(key, val);
		}
	}
	else
	{
		for (auto& kvp : CustomMap)
		{
			InFunc(kvp.Key, kvp.Value);
		}
	}
}

void FBIMDimensions::ForEachCustomDimension(const TFunction<void(const FName& Name, float Value)>& InFunc) const
{
	for (auto& kvp : CustomMap)
	{
		InFunc(kvp.Key, kvp.Value);
	}
}
const FString DimensionsKey = TEXT("Dimension");
const FString ValuesKey = TEXT("Value");

void FBIMDimensions::ConvertToWebPreset(FBIMWebPreset& OutPreset)
{
	const FString PropertyKey = GetEnumValueString(EPresetPropertyMatrixNames::Dimensions) + TEXT(".");
	const FString DimensionsEntry = PropertyKey + DimensionsKey;
	const FString ValuesEntry = PropertyKey + ValuesKey;
	

	if(!OutPreset.properties.Contains(DimensionsEntry))
		OutPreset.properties.Add(DimensionsEntry, {});

	if(!OutPreset.properties.Contains(ValuesEntry))
		OutPreset.properties.Add(ValuesEntry, {});
	
	TArray<FString>& dimensions = OutPreset.properties[DimensionsEntry].value;
	TArray<FString>& values = OutPreset.properties[ValuesEntry].value;
	
	for(auto& kvp : CustomMap)
	{
		dimensions.Add(kvp.Key.ToString());
		values.Add(FString::SanitizeFloat(kvp.Value));
	}
}

void FBIMDimensions::ConvertFromWebPreset(const FBIMWebPreset& InPreset)
{
	const FString PropertyKey = GetEnumValueString(EPresetPropertyMatrixNames::Dimensions) + TEXT(".");
	const FString DimensionsEntry = PropertyKey + DimensionsKey;
	const FString ValuesEntry = PropertyKey + ValuesKey;
	
	TArray<FString> dimensions = InPreset.properties[DimensionsEntry].value;
	TArray<FString> values = InPreset.properties[ValuesEntry].value;
	
	// Dimensions and values are a key-to-value pair and need to be the same size
	if (ensure(dimensions.Num() == values.Num())) {
		for (int i = 0; i < dimensions.Num(); i++)
		{
			if (!dimensions[i].IsEmpty())
			{
				float value = FCString::Atof(*values[i]);
				SetCustomDimension(*dimensions[i], value);
			}
		}
	}
}

bool FBIMDimensions::operator==(const FBIMDimensions& Other)
{
	return CustomMap.OrderIndependentCompareEqual(Other.CustomMap);
}

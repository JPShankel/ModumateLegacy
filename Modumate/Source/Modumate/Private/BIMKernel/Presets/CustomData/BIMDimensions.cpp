#include "BIMKernel/Presets/CustomData/BIMDimensions.h"

#include "BIMKernel/AssemblySpec/BIMPartSlotSpec.h"




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
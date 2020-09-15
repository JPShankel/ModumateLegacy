// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/BIMUProperties.h"

template<class P, class T>
static ECraftingResult TrySetBIMUProperty(UBIMPropertyBase* PropertySheet, const FName& PropertyName, const T& Value)
{
	FProperty* property = PropertySheet->GetClass()->FindPropertyByName(PropertyName);
	if (property == nullptr)
	{
		return ECraftingResult::Error;
	}
	void* propAddress = property->ContainerPtrToValuePtr<void>(PropertySheet);

	P* propertySubclass = CastField<P>(property);
	if (propertySubclass == nullptr)
	{
		return ECraftingResult::Error;
	}

	propertySubclass->SetPropertyValue(propAddress, Value);
	return ECraftingResult::Success;
}

ECraftingResult UBIMPropertyBase::SetFloatProperty(const FName &Property, float Value)
{
	return TrySetBIMUProperty<FFloatProperty, float>(this, Property, Value);
}

ECraftingResult UBIMPropertyBase::SetStringProperty(const FName &Property, const FString& Value)
{
	return TrySetBIMUProperty<FStrProperty, FString>(this, Property, Value);
}

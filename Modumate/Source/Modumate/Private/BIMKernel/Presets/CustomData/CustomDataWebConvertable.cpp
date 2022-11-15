#include "BIMKernel/Presets/CustomData/CustomDataWebConvertable.h"
#include "BIMKernel/Presets/BIMWebPreset.h"


void FCustomDataWebConvertable::ConvertToWebPreset(FBIMWebPreset& OutPreset)
{
	for (TFieldIterator<FProperty> It(VirtualizedStaticStruct()); It; ++It)
	{
		SerializeByPropertyName(this, *It, OutPreset.properties);
	}
}

void FCustomDataWebConvertable::ConvertFromWebPreset(const FBIMWebPreset& InPreset)
{
	for (TFieldIterator<FProperty> It(VirtualizedStaticStruct()); It; ++It)
	{
		DeserializeByPropertyName(this, InPreset.properties, *It);
	}
}

bool FCustomDataWebConvertable::SerializeByPropertyName(void* Container, FProperty* Property,
                                                      TMap<FString, FBIMWebPresetProperty>& OutProperties) const
{
	bool rtn = true;
	const FString PropType = Property->GetCPPType();
	const FString VariableName = Property->GetNameCPP();
	const FString Entry = GetPropertyPrefix() + TEXT(".") + VariableName;
	
	if(PropType == TEXT("FString"))
	{
		if (const FStrProperty* ChildStringProp = CastField<FStrProperty>(Property))
		{
			const FString* string = ChildStringProp->ContainerPtrToValuePtr<FString>(Container);
			OutProperties.Add(Entry, {Entry, {*string}});
		}
	}
	else if (PropType == TEXT("float"))
	{
		if (const FFloatProperty* ChildFloatProp = CastField<FFloatProperty>(Property))
		{
			const float* numerical = ChildFloatProp->ContainerPtrToValuePtr<float>(Container);
			OutProperties.Add(Entry, {Entry, {FString::SanitizeFloat(*numerical)}});
		}
	}
	else if (PropType == TEXT("bool"))
	{
		if (FBoolProperty * ChildBoolProp = CastField<FBoolProperty>(Property))
		{
			const bool* boolean = ChildBoolProp->ContainerPtrToValuePtr<bool>(Container);
			OutProperties.Add(Entry, {Entry, {FString::SanitizeFloat(*boolean)}});
		}
	}
	else if(PropType == TEXT("FColor"))
	{
		if (FStructProperty * ChildStructProp = CastField<FStructProperty>(Property))
		{
			FColor *color = ChildStructProp->ContainerPtrToValuePtr<FColor>(Container);
			OutProperties.Add(Entry, {Entry, {color->ToHex()}});
		}
	}
	else if(PropType == TEXT("FGuid"))
	{
		if (FStructProperty * ChildStructProp = CastField<FStructProperty>(Property))
		{
			FGuid *guid = ChildStructProp->ContainerPtrToValuePtr<FGuid>(Container);
			OutProperties.Add(Entry, {Entry, {guid->ToString(EGuidFormats::Digits)}});
		}
	}
	else if (PropType == TEXT("FVector"))
	{
		if (FStructProperty * ChildStructProp = CastField<FStructProperty>(Property))
		{
			FVector *vec = ChildStructProp->ContainerPtrToValuePtr<FVector>(Container);
			OutProperties.Add(Entry + TEXT("X"), {Entry, {FString::SanitizeFloat(vec->X)}});
			OutProperties.Add(Entry + TEXT("Y"), {Entry, {FString::SanitizeFloat(vec->Y)}});
			OutProperties.Add(Entry + TEXT("Z"), {Entry, {FString::SanitizeFloat(vec->Z)}});
		}
	}
	else if (PropType == TEXT("FRotator"))
	{
		if (FStructProperty * ChildStructProp = CastField<FStructProperty>(Property))
		{
			FRotator *rot = ChildStructProp->ContainerPtrToValuePtr<FRotator>(Container);
			OutProperties.Add(Entry + TEXT("X"), {Entry, {FString::SanitizeFloat(rot->Roll)}});
			OutProperties.Add(Entry + TEXT("Y"), {Entry, {FString::SanitizeFloat(rot->Pitch)}});
			OutProperties.Add(Entry + TEXT("Z"), {Entry, {FString::SanitizeFloat(rot->Yaw)}});
		}
	}
	else if (PropType == TEXT("TArray"))
	{
		if (FArrayProperty * ChildArrayProp = CastField<FArrayProperty>(Property))
		{
			TArray<FString> *array = ChildArrayProp->ContainerPtrToValuePtr<TArray<FString>>(Container);
			if(array != nullptr)
			{
				//TODO: Edge Details use arrays of structs which need special handling - JN
				OutProperties.Add(Entry, {Entry, *array});	
			}
		}
	}
	else
	{
		UE_LOG(LogCallTrace, Warning, TEXT("Could not SerializeByPropertyName on %s"), *Property->GetNameCPP())
		rtn = false;
	}


	return rtn;
}

bool FCustomDataWebConvertable::DeserializeByPropertyName(void* Container,
                                                        const TMap<FString, FBIMWebPresetProperty> InProperties, FProperty* Property)
{
	bool rtn = false;
	const FString PropType  = Property->GetCPPType();
	const FString VariableName = Property->GetNameCPP();
	const FString Entry = GetPropertyPrefix() + TEXT(".") + VariableName;

	if(InProperties.Contains(Entry))
	{
		if(PropType == TEXT("FString"))
		{
			FString propVal = InProperties[Entry].value[0];
			if (const FStrProperty* ChildStringProp = CastField<FStrProperty>(Property))
			{
				ChildStringProp->SetPropertyValue_InContainer(Container, propVal);
				rtn = true;
			}
		}
		else if (PropType == TEXT("float"))
		{
			float propVal = FCString::Atof(*InProperties[Entry].value[0]);
			if (const FFloatProperty* ChildFloatProp = CastField<FFloatProperty>(Property))
			{
				ChildFloatProp->SetPropertyValue_InContainer(Container, propVal);
				rtn = true;
			}
		}
		else if (PropType == TEXT("bool"))
		{
			bool propVal = InProperties[Entry].value[0] == TEXT("true");
			if (FBoolProperty * ChildBoolProp = CastField<FBoolProperty>(Property))
			{
				ChildBoolProp->SetPropertyValue_InContainer(Container, propVal);
				rtn = true;
			}
		}
		else if (PropType == TEXT("FColor"))
		{
			FColor propVal = FColor::FromHex(*InProperties[Entry].value[0]);
			if (FStructProperty * ChildStructProp = CastField<FStructProperty>(Property))
			{
				FColor *color = ChildStructProp->ContainerPtrToValuePtr<FColor>(Container);
				*color = propVal;
				rtn = true;
			}
		}
		else if (PropType == TEXT("FGuid"))
		{
			FGuid propVal;
			if(FGuid::Parse(*InProperties[Entry].value[0], propVal))
			{
				if (FStructProperty * ChildStructProp = CastField<FStructProperty>(Property))
				{
					FGuid *guid = ChildStructProp->ContainerPtrToValuePtr<FGuid>(Container);
					*guid = propVal;
					rtn = true;
				}
			}

		}
		else if (PropType == TEXT("TArray"))
        {
        	if (FArrayProperty * ChildArrayProp = CastField<FArrayProperty>(Property))
        	{
        		TArray<FString> *array = ChildArrayProp->ContainerPtrToValuePtr<TArray<FString>>(Container);
        		if(array != nullptr)
        		{
        			//TODO: Edge Details use arrays of structs which need special handling - JN
        			*array = InProperties[Entry].value;
        			rtn = true;
        		}
        	}
        } 
	}
	else if (PropType == TEXT("FVector"))
	{
		if (FStructProperty * ChildStructProp = CastField<FStructProperty>(Property))
		{
			FVector *vec = ChildStructProp->ContainerPtrToValuePtr<FVector>(Container);
			int count = 0;
			if (InProperties.Contains(Entry + TEXT("X")))
			{
				vec->X = FCString::Atof(*InProperties[Entry + TEXT("X")].value[0]);
				count++;
			}

			if (InProperties.Contains(Entry + TEXT("Y")))
			{
				vec->Y = FCString::Atof(*InProperties[Entry + TEXT("Y")].value[0]);
				count++;
			}

			if (InProperties.Contains(Entry + TEXT("Z")))
			{
				vec->Z = FCString::Atof(*InProperties[Entry + TEXT("Z")].value[0]);
				count++;
			}

			rtn = count == 3;
		}
	}
	else if (PropType == TEXT("FRotator"))
	{
		if (FStructProperty * ChildStructProp = CastField<FStructProperty>(Property))
		{
			FRotator *rot = ChildStructProp->ContainerPtrToValuePtr<FRotator>(Container);
			int count = 0;
			if (InProperties.Contains(Entry + TEXT("X")))
			{
				rot->Roll = FCString::Atof(*InProperties[Entry + TEXT("X")].value[0]);
				count++;
			}

			if (InProperties.Contains(Entry + TEXT("Y")))
			{
				rot->Pitch = FCString::Atof(*InProperties[Entry + TEXT("Y")].value[0]);
				count++;
			}

			if (InProperties.Contains(Entry + TEXT("Z")))
			{
				rot->Yaw = FCString::Atof(*InProperties[Entry + TEXT("Z")].value[0]);
				count++;
			}
			rtn = count == 3;
		}
	}


	if(!rtn)
	{
		UE_LOG(LogCallTrace, Warning, TEXT("Could not DeserializeByPropertyName on %s"), *Property->GetNameCPP())
	}

	return rtn;
}

UStruct* FCustomDataWebConvertable::VirtualizedStaticStruct()
{
	ensureAlwaysMsgf(false, TEXT("Must override VirtualizedStaticStruct()"));
	return FCustomDataWebConvertable::StaticStruct();
}

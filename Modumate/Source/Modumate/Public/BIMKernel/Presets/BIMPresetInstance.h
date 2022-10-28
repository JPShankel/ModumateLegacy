// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BIMPresetTypeDefinition.h"
#include "BIMKernel/Presets/PresetOrigination.h"
#include "BIMKernel/Core/BIMEnums.h"
#include "BIMKernel/Core/BIMProperties.h"
#include "BIMKernel/Core/BIMTagPath.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "ModumateCore/StructDataWrapper.h"

#include "BIMPresetInstance.generated.h"

struct FBIMWebPreset;

/*
A preset specifies a crafting node type, a set of specific values for the type's properties and specifications for which
other presets are attached to each pin

For example, a 3x4x7 inch red clay brick would be an instance of BRICK with clay, red and 3x4x7 child presets attached
*/
struct FBIMPresetCollection;
class FBIMPresetCollectionProxy;
class UModumateDocument;

USTRUCT()
struct MODUMATE_API FBIMPresetPinAttachment
{
	GENERATED_BODY()

	UPROPERTY()
	int32 ParentPinSetIndex = 0;

	UPROPERTY()
	int32 ParentPinSetPosition = 0;

	UPROPERTY()
	FGuid PresetGUID;

	UPROPERTY()
	EBIMPinTarget Target = EBIMPinTarget::Default;

	bool operator==(const FBIMPresetPinAttachment& OtherAttachment) const;
};

USTRUCT()
struct MODUMATE_API FBIMPresetPartSlot
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid PartPresetGUID;

	UPROPERTY()
	FGuid SlotPresetGUID;

	bool operator==(const FBIMPresetPartSlot& RHS) const
	{
		return RHS.PartPresetGUID == PartPresetGUID && RHS.SlotPresetGUID == SlotPresetGUID;
	}

	bool operator!=(const FBIMPresetPartSlot& RHS) const
	{
		return !(*this == RHS);
	}
};

USTRUCT()
struct MODUMATE_API FPresetCustomDataWrapper
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<FName, FStructDataWrapper> CustomDataWrapper;
};

USTRUCT()
struct MODUMATE_API FBIMPresetInstance
{
	GENERATED_BODY()
	
	UPROPERTY()
	//Use the origination field instead
	//@deprecated
	bool bEdited = false;

	UPROPERTY()
	FBIMPresetForm PresetForm_DEPRECATED;
	
	UPROPERTY()
	FBIMPropertySheet Properties_DEPRECATED;

	UPROPERTY()
	FText CategoryTitle;

	UPROPERTY()
	FText DisplayName;

	UPROPERTY()
	EBIMValueScope NodeScope = EBIMValueScope::None;
	
	UPROPERTY()
	TArray<FBIMPresetNodePinSet> PinSets;

	UPROPERTY()
	EPresetOrigination Origination;

	UPROPERTY()
	FGuid CanonicalBase;
	
	UPROPERTY()
	FGuid GUID;

	UPROPERTY()
	FGuid ParentGUID;

	UPROPERTY()
	EObjectType ObjectType = EObjectType::OTNone;

	UPROPERTY()
	FGuid SlotConfigPresetGUID;

	UPROPERTY()
	TArray<FBIMPresetPartSlot> PartSlots;

	UPROPERTY()
	TArray<FBIMPresetPinAttachment> ChildPresets;

	UPROPERTY()
	FBIMTagPath MyTagPath;

	UPROPERTY()
	EPresetMeasurementMethod MeasurementMethod = EPresetMeasurementMethod::None;

	UPROPERTY()
	EBIMAssetType AssetType = EBIMAssetType::None;

	UPROPERTY()
	TMap<FName, FStructDataWrapper> CustomDataByClassName;

	template<class T>
	bool TryGetCustomData(T& OutData) const
	{
		const FStructDataWrapper* wrapper = CustomDataByClassName.Find(T::StaticStruct()->GetFName());
		if (wrapper != nullptr)
		{
			return (*wrapper).LoadStructData(OutData,true);
		}
		return false;
	}

	template<class T>
	EBIMResult SetCustomData(const T& InData)
	{
		FStructDataWrapper structWrapper;
		if (ensureAlways(structWrapper.SaveStructData(InData, true)))
		{
			CustomDataByClassName.Add(T::StaticStruct()->GetFName(),structWrapper);
			return EBIMResult::Success;
		}
		ensureAlways(false);
		return EBIMResult::Error;
	}

	template<class T> bool HasCustomData() const
	{
		return CustomDataByClassName.Contains(T::StaticStruct()->GetFName());
	}

	bool HasCustomDataByName(FName InName) const
	{
		return CustomDataByClassName.Contains(InName);
	}

	TSharedPtr<FJsonValue> GetCustomDataValue(const FString& DataStruct, const FString& FieldName) const;

    bool TryGetCustomDataString(const FString& DataStruct, const FString& FieldName, FString& OutStr) const;
    template<class T>
    bool TryGetCustomDataString(const FString& FieldName, FString& OutStr) const
    {
    	return TryGetCustomDataString(T::StaticStruct()->GetName(), FieldName, OutStr);
    }

	bool TryGetCustomDataColor(const FString& DataStruct, const FString& FieldName, FString& OutStr) const;
	template<class T>
	bool TryGetCustomDataColor(const FString& FieldName, FString& OutStr) const
	{
		return TryGetCustomDataString(T::StaticStruct()->GetName(), FieldName, OutStr);
	}

    bool TrySetCustomDataString(const FString& DataStruct, const FString& FieldName, const FString& InStr);
    template<class T>
    bool TrySetCustomDataString(const FString& FieldName, const FString& InStr)
    {
    	return TrySetCustomDataString(T::StaticStruct()->GetName(), FieldName, InStr);
    }

    bool TryGetCustomDataNumber(const FString& DataStruct, const FString& FieldName, float& OutNum) const;
    template<class T>
    bool TryGetCustomDataNumber(const FString& FieldName, float& OutNum) const
    {
    	return TryGetCustomDataNumber(T::StaticStruct()->GetName(), FieldName, OutNum);
    }

    bool TrySetCustomDataNumber(const FString& DataStruct, const FString& FieldName, const float InNum);
    template<class T>
    bool TrySetCustomDataNumber(const FString& FieldName, const float InNum)
    {
    	return TrySetCustomDataNumber(T::StaticStruct()->GetName(), FieldName, InNum);
    }

	bool TrySetCustomDataColor(const FString& DataStruct, const FString& FieldName, const FString InHex);
    template<class T>
    bool TrySetCustomDataColor(const FString& FieldName, const FString InHex)
    {
    	return TrySetCustomDataNumber(T::StaticStruct()->GetName(), FieldName, InHex);
    }

	EBIMResult GetForm(const UModumateDocument* InDocument,FBIMPresetForm& OutForm) const;
	void TryGetCustomDimensionFormElements(FBIMPresetForm& OutForm) const;
	EBIMResult UpdateFormElements(const UModumateDocument* InDocument, FBIMPresetForm& RefForm) const;
	EBIMResult MakeDeltaForFormElement(const FBIMPresetFormElement& FormElement, FBIMPresetEditorDelta& OutDelta) const;
	EBIMResult ApplyDelta(const UModumateDocument* InDocument,const FBIMPresetEditorDelta& Delta);
	EBIMResult HandleMaterialBindingDelta(const FBIMPresetEditorDelta& Delta);
	EBIMResult MakeMaterialBindingDelta(const FBIMPresetFormElement& FormElement, FBIMPresetEditorDelta& OutDelta) const;

	// Sort child nodes by PinSetIndex and PinSetPosition so serialization will be consistent
	EBIMResult SortChildPresets();
	EBIMResult AddChildPreset(const FGuid& ChildPresetGUID, int32 PinSetIndex, int32 PinSetPosition);
	EBIMResult RemoveChildPreset(int32 PinSetIndex, int32 PinSetPosition);
	EBIMResult SetPartPreset(const FGuid& SlotPresetGUID, const FGuid& PartPresetGUID);
	
	EBIMResult UpgradeData(FBIMPresetCollection& PresetCollection, int32 InDocVersion);
	bool ReplaceImmediateChildGuid(FGuid& OldGuid, FGuid& NewGuid);

	bool HasPin(int32 PinSetIndex, int32 PinSetPosition) const;
	bool HasOpenPin() const;

	EBIMResult SetMaterialChannelsForMesh(const FBIMPresetCollection& PresetCollection, const FGuid& InMeshGuid);

	bool CheckChildrenForErrors() const;
	bool EnsurePresetIsValidForUse() const;

	EBIMResult GetModularDimensions(FVector& OutDimensions, float& OutBevelWidth,float& OutThickness) const;
	EBIMResult ToWebPreset(FBIMWebPreset& OutPreset, UWorld* World) const;
	EBIMResult FromWebPreset(const FBIMWebPreset& InPreset, UWorld* World);

	void ConvertWebPropertiesToCustomData(const FBIMWebPreset& InPreset, UWorld* World);
	void ConvertCustomDataToWebProperties(FBIMWebPreset& OutPreset, UWorld* World) const;
	void ConvertWebPropertiesFromChildPresets(FBIMWebPreset& OutPreset) const;

	// TODO: deprecate this and change all custom data to inherit from a class that forces a ToJSON() and FromJSON() method
template<class T>
void PopulateCustomDataFromProperties(TMap<FString, TArray<FString>>& InProperties, T& OutCustomData)
{
	for (TFieldIterator<FProperty> It(OutCustomData.StaticStruct()); It; ++It)
	{
		FProperty* Property = *It;
		auto type = Property->GetCPPType();
		FString VariableName = Property->GetNameCPP();

		// if the property is not received from the web, ignore it.
		if (!InProperties.Contains(VariableName))
		{
			continue;
		}

		if (type == TEXT("FString"))
		{
				FString propVal = InProperties[VariableName][0];
			if (!propVal.IsEmpty())
			{
				if (FStrProperty * ChildStringProp = CastField<FStrProperty>(Property))
				{
					ChildStringProp->SetPropertyValue_InContainer(&OutCustomData, propVal);
				}
			}	
		} else if (type == TEXT("float"))
		{
			float propVal = FCString::Atof(*InProperties[VariableName][0]);
			if (FFloatProperty * ChildFloatProp = CastField<FFloatProperty>(Property))
			{
				ChildFloatProp->SetPropertyValue_InContainer(&OutCustomData, propVal);
			}
		} else if (type == TEXT("bool"))
		{
			bool propVal = InProperties[VariableName][0] == TEXT("true");
			if (FBoolProperty * ChildBoolProp = CastField<FBoolProperty>(Property))
			{
				ChildBoolProp->SetPropertyValue_InContainer(&OutCustomData, propVal);
			}
		} else if (type == TEXT("FColor"))
		{
			FColor propVal = FColor::FromHex(*InProperties[VariableName][0]);
			if (FStructProperty * ChildStructProp = CastField<FStructProperty>(Property))
			{
				FColor *color = ChildStructProp->ContainerPtrToValuePtr<FColor>(&OutCustomData);
				*color = propVal;
			}
		} else if (type == TEXT("FGuid"))
		{
			FGuid propVal;
			FGuid::Parse(*InProperties[VariableName][0], propVal);
			if (FStructProperty * ChildStructProp = CastField<FStructProperty>(Property))
			{
				FGuid *guid = ChildStructProp->ContainerPtrToValuePtr<FGuid>(&OutCustomData);
				*guid = propVal;
			}
		} else if (type == TEXT("FVector"))
		{
			if (FStructProperty * ChildStructProp = CastField<FStructProperty>(Property))
			{
				FVector *vec = ChildStructProp->ContainerPtrToValuePtr<FVector>(&OutCustomData);
				if (InProperties.Contains(VariableName + TEXT("X")))
				{
					vec->X = FCString::Atof(*InProperties[VariableName + TEXT("X")][0]);	
				}

				if (InProperties.Contains(VariableName + TEXT("Y")))
				{
					vec->Y = FCString::Atof(*InProperties[VariableName + TEXT("Y")][0]);	
				}

				if (InProperties.Contains(VariableName + TEXT("Z")))
				{
					vec->Z = FCString::Atof(*InProperties[VariableName + TEXT("Z")][0]);	
				}
			}
		} else if (type == TEXT("FRotator"))
		{
			if (FStructProperty * ChildStructProp = CastField<FStructProperty>(Property))
			{
				FRotator *rot = ChildStructProp->ContainerPtrToValuePtr<FRotator>(&OutCustomData);
				if (InProperties.Contains(VariableName + TEXT("X")))
				{
					rot->Roll = FCString::Atof(*InProperties[VariableName + TEXT("X")][0]);	
				}

				if (InProperties.Contains(VariableName + TEXT("Y")))
				{
					rot->Pitch = FCString::Atof(*InProperties[VariableName + TEXT("Y")][0]);	
				}

				if (InProperties.Contains(VariableName + TEXT("Z")))
				{
					rot->Yaw = FCString::Atof(*InProperties[VariableName + TEXT("Z")][0]);	
				}
			}
		} else if (type == TEXT("TArray"))
		{
			if (FArrayProperty * ChildArrayProp = CastField<FArrayProperty>(Property))
			{
				if (InProperties.Contains(VariableName))
				{
					 TArray<FString> *array = ChildArrayProp->ContainerPtrToValuePtr<TArray<FString>>(&OutCustomData);
					*array = InProperties[VariableName];
				}
			}
		} else
		{
			UE_LOG(LogTemp, Warning, TEXT("Count not find custom data type [%s] for property [%s]"), *type, *VariableName);
		}
	}
}

	
	bool operator==(const FBIMPresetInstance& RHS) const;
	bool operator!=(const FBIMPresetInstance& RHS) const;
};

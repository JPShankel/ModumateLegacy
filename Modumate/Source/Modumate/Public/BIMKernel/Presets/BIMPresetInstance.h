// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BIMKernel/Core/BIMEnums.h"
#include "BIMKernel/Core/BIMProperties.h"
#include "BIMKernel/Core/BIMTagPath.h"
#include "BIMKernel/Core/BIMKey.h"
#include "BIMKernel/Presets/BIMPresetTypeDefinition.h"
#include "BIMKernel/Presets/BIMPresetMaterialBinding.h"
#include "BIMKernel/Presets/BIMPresetNCPTaxonomy.h"
#include "ModumateCore/StructDataWrapper.h"

#include "BIMPresetInstance.generated.h"

class FModumateDatabase;
struct FBIMKey;

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
	FBIMKey PresetID;

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
	FBIMKey PartPreset;

	UPROPERTY()
	FBIMKey SlotPreset;

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
struct MODUMATE_API FBIMWebPreset
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid presetID;

	UPROPERTY()
	FString name;

	UPROPERTY()
	FBIMTagPath tagPath;

	UPROPERTY()
	FString typeMark;
};

USTRUCT()
struct MODUMATE_API FBIMPresetInstance
{
	GENERATED_BODY()

	// TODO: roll fields below into type definition and make it the top level UPROPERTY
	UPROPERTY()
	FBIMPresetTypeDefinition TypeDefinition;

	UPROPERTY()
	bool bEdited = false;

	//TODO: Debug data must be UPROPERTY() to serialize in cache, cannot be conditionally compiled out
	//To be deprecated with move to SQL
	UPROPERTY()
	FString DEBUG_SourceFile;

	UPROPERTY()
	int32 DEBUG_SourceRow = 0;

	UPROPERTY()
	FBIMPresetForm PresetForm;

	UPROPERTY()
	FBIMPropertySheet Properties;

	UPROPERTY()
	FText CategoryTitle;

	UPROPERTY()
	FText DisplayName;

	UPROPERTY()
	EBIMValueScope NodeScope = EBIMValueScope::None;

	UPROPERTY()
	FName NodeType;

	UPROPERTY()
	FBIMKey PresetID;

	UPROPERTY()
	FGuid GUID;

	UPROPERTY()
	FBIMKey SlotConfigPresetID;

	UPROPERTY()
	FGuid SlotConfigPresetGUID;

	UPROPERTY()
	EObjectType ObjectType = EObjectType::OTNone;

	UPROPERTY()
	EConfiguratorNodeIconType IconType = EConfiguratorNodeIconType::None;

	UPROPERTY()
	EConfiguratorNodeIconOrientation Orientation = EConfiguratorNodeIconOrientation::Inherited;

	UPROPERTY()
	TArray<FBIMPresetPartSlot> PartSlots;

	UPROPERTY()
	TArray<FBIMPresetPinAttachment> ChildPresets;

	UPROPERTY()
	TArray<FBIMTagPath> ParentTagPaths;

	UPROPERTY()
	FBIMTagPath MyTagPath;

	UPROPERTY()
	EPresetMeasurementMethod MeasurementMethod = EPresetMeasurementMethod::None;

	UPROPERTY()
	EBIMAssetType AssetType = EBIMAssetType::None;

	UPROPERTY()
	FStructDataWrapper CustomData_DEPRECATED;

	UPROPERTY()
	TMap<FString, FName> FormItemToProperty_DEPRECATED;

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

	bool HasProperty(const FBIMNameType& Name) const;

	template<class T>
	T GetProperty(const FBIMNameType& Name) const
	{
		return Properties.GetProperty<T>(NodeScope, Name);
	}
	
	template<class T>
	T GetScopedProperty(const EBIMValueScope& Scope, const FBIMNameType& Name) const
	{
		return Properties.GetProperty<T>(Scope, Name);
	}

	template<class T>
	void SetScopedProperty(const EBIMValueScope& Scope, const FBIMNameType& Name, const T& Value)
	{
		Properties.SetProperty(Scope, Name, Value);
	}

	void SetProperties(const FBIMPropertySheet& InProperties);

	template <class T>
	bool TryGetProperty(const FBIMNameType& Name,T& OutT) const
	{
		return Properties.TryGetProperty(NodeScope, Name, OutT);
	}

	EBIMResult GetForm(const UModumateDocument* InDocument,FBIMPresetForm& OutForm) const;
	EBIMResult UpdateFormElements(const UModumateDocument* InDocument, FBIMPresetForm& RefForm) const;
	EBIMResult MakeDeltaForFormElement(const FBIMPresetFormElement& FormElement, FBIMPresetEditorDelta& OutDelta) const;
	EBIMResult ApplyDelta(const UModumateDocument* InDocument,const FModumateDatabase& InDB, const FBIMPresetEditorDelta& Delta);

	EBIMResult HandleMaterialBindingDelta(const FBIMPresetEditorDelta& Delta);
	EBIMResult HandleLayerPriorityGroupDelta(const FBIMPresetEditorDelta& Delta);
	EBIMResult HandleLayerPriorityValueDelta(const FBIMPresetEditorDelta& Delta);
	EBIMResult MakeMaterialBindingDelta(const FBIMPresetFormElement& FormElement, FBIMPresetEditorDelta& OutDelta) const;

	// Sort child nodes by PinSetIndex and PinSetPosition so serialization will be consistent
	EBIMResult SortChildPresets();

	EBIMResult AddChildPreset(const FGuid& ChildPresetGUID, int32 PinSetIndex, int32 PinSetPosition);
	EBIMResult RemoveChildPreset(int32 PinSetIndex, int32 PinSetPosition);

	EBIMResult SetPartPreset(const FGuid& SlotPresetGUID, const FGuid& PartPresetGUID);

	bool HasPin(int32 PinSetIndex, int32 PinSetPosition) const;
	bool HasOpenPin() const;

	EBIMResult SetMaterialChannelsForMesh(const FModumateDatabase& InDB, const FGuid& InMeshGuid);

	bool ValidatePreset() const;

	EBIMResult GetModularDimensions(FVector& OutDimensions, float& OutBevelWidth,float& OutThickness) const;

	EBIMResult UpgradeData(const FModumateDatabase& InDB, const FBIMPresetCollectionProxy& PresetCollection, int32 InDocVersion);

	EBIMResult ToWebPreset(FBIMWebPreset& OutPreset) const;

	bool operator==(const FBIMPresetInstance& RHS) const;
	bool operator!=(const FBIMPresetInstance& RHS) const;
};

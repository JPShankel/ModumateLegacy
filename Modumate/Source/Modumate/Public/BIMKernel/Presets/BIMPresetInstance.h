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
struct MODUMATE_API FPresetCustomDataWrapper
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<FName, FStructDataWrapper> CustomDataWrapper;
};

UENUM()
enum class EBIMWebPresetPropertyType : uint8
{
	none = 0,
	string,
	number,
	boolean,
	color,
};

USTRUCT()
struct MODUMATE_API FBIMWebPresetProperty
{
	GENERATED_BODY()

	UPROPERTY()
	FString key;

	UPROPERTY()
	FString name;
	
	UPROPERTY()
	EBIMWebPresetPropertyType type = EBIMWebPresetPropertyType::string;
	
	UPROPERTY()
	TArray<FString> value;
	
	UPROPERTY()
	bool isEditable = false;
	
	UPROPERTY()
	bool isVisibleInEditor = false;
};

USTRUCT()
struct MODUMATE_API FBIMWebPreset
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid guid;

	UPROPERTY()
	TArray<FGuid> childPresets;

	UPROPERTY()
	FString name;
	
	UPROPERTY()
	FBIMTagPath tagPath;

	// deprecated
	UPROPERTY()
	FString customDataJSON;

	// deprecated
	UPROPERTY()
	FString typeMark;
	
	UPROPERTY()
	TMap<FString, FBIMWebPresetProperty> properties;
};

USTRUCT()
struct MODUMATE_API FWebQuantity
{
	GENERATED_BODY()

	UPROPERTY()
	float Count = 0.0f;

	UPROPERTY()
	float Linear = 0.0f;

	UPROPERTY()
	float Area = 0.0f;

	UPROPERTY()
	float Volume = 0.0f;

	UPROPERTY()
	float MaterialCost = 0.0f;

	UPROPERTY()
	float LaborCost = 0.0f;

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
	FGuid ParentGUID;

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
	EBIMResult ApplyDelta(const UModumateDocument* InDocument,const FBIMPresetEditorDelta& Delta);

	EBIMResult HandleMaterialBindingDelta(const FBIMPresetEditorDelta& Delta);
	EBIMResult HandleLayerPriorityGroupDelta(const FBIMPresetEditorDelta& Delta);
	EBIMResult HandleLayerPriorityValueDelta(const FBIMPresetEditorDelta& Delta);
	EBIMResult MakeMaterialBindingDelta(const FBIMPresetFormElement& FormElement, FBIMPresetEditorDelta& OutDelta) const;
	EBIMResult HandleConstructionCostLaborDelta(const FBIMPresetEditorDelta& Delta);
	EBIMResult HandleConstructionCostMaterialDelta(const FBIMPresetEditorDelta& Delta);
	EBIMResult HandleLightColorDelta(const FBIMPresetEditorDelta& Delta);
	EBIMResult HandleLightIntensityDelta(const FBIMPresetEditorDelta& Delta);
	EBIMResult HandleLightRadiusDelta(const FBIMPresetEditorDelta& Delta);
	EBIMResult HandleLightProfileDelta(const FBIMPresetEditorDelta& Delta);
	EBIMResult HandleLightIsSpotDelta(const FBIMPresetEditorDelta& Delta);

	// Sort child nodes by PinSetIndex and PinSetPosition so serialization will be consistent
	EBIMResult SortChildPresets();

	EBIMResult AddChildPreset(const FGuid& ChildPresetGUID, int32 PinSetIndex, int32 PinSetPosition);
	EBIMResult RemoveChildPreset(int32 PinSetIndex, int32 PinSetPosition);

	EBIMResult SetPartPreset(const FGuid& SlotPresetGUID, const FGuid& PartPresetGUID);

	bool HasPin(int32 PinSetIndex, int32 PinSetPosition) const;
	bool HasOpenPin() const;

	EBIMResult SetMaterialChannelsForMesh(const FBIMPresetCollection& PresetCollection, const FGuid& InMeshGuid);

	bool ValidatePreset() const;

	EBIMResult GetModularDimensions(FVector& OutDimensions, float& OutBevelWidth,float& OutThickness) const;

	EBIMResult UpgradeData(const FBIMPresetCollectionProxy& PresetCollection, int32 InDocVersion);

	EBIMResult ToWebPreset(FBIMWebPreset& OutPreset, UWorld* World) const;
	EBIMResult FromWebPreset(const FBIMWebPreset& InPreset, UWorld* World);

	bool operator==(const FBIMPresetInstance& RHS) const;
	bool operator!=(const FBIMPresetInstance& RHS) const;
};

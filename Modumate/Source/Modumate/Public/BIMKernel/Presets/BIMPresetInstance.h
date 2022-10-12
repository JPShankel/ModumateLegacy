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

UENUM()
enum class EPresetPropertyMatrixNames : uint8
{
	IESLight,
	ConstructionCost,
	MiterPriority,
	PatternRef,
	ProfileRef,
	MeshRef,
	Material,
	Dimensions,
	Slots,
	InputPins,
	EdgeDetail,
	Error = 255
};

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

	TSharedPtr<FJsonValue> GetCustomDataValue(const FString& DataStruct, const FString& FieldName) const;

	bool TryGetCustomDataString(const FString& DataStruct, const FString& FieldName, FString& OutStr) const;
	template<class T>
	bool TryGetCustomDataString(const FString& FieldName, FString& OutStr) const
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
	EBIMResult UpgradeData(FBIMPresetCollection& PresetCollection, int32 InDocVersion);
	
	bool ReplaceImmediateChildGuid(FGuid& OldGuid, FGuid& NewGuid);

	bool HasPin(int32 PinSetIndex, int32 PinSetPosition) const;
	bool HasOpenPin() const;

	EBIMResult SetMaterialChannelsForMesh(const FBIMPresetCollection& PresetCollection, const FGuid& InMeshGuid);

	bool ValidatePreset() const;

	EBIMResult GetModularDimensions(FVector& OutDimensions, float& OutBevelWidth,float& OutThickness) const;
	EBIMResult ToWebPreset(FBIMWebPreset& OutPreset, UWorld* World) const;
	EBIMResult FromWebPreset(const FBIMWebPreset& InPreset, UWorld* World);

	/**
	 * For Canonical Presets, this method copies the preset in to a 
     * VanillaDerived Copy w/ the provided GUID. It is up to the caller
     * to add it to the BIM Collection and VDP tables.
     * TODO: Clean up the Collection<>Preset interface -JN
	 */
	FBIMPresetInstance Derive(const FGuid& NewGUID) const;

	void ConvertWebPropertiesToCustomData(const FBIMWebPreset& InPreset, UWorld* World);
	void ConvertCustomDataToWebProperties(FBIMWebPreset& OutPreset, UWorld* World) const;
	void ConvertWebPropertiesFromChildPresets(FBIMWebPreset& OutPreset) const;
	
	bool operator==(const FBIMPresetInstance& RHS) const;
	bool operator!=(const FBIMPresetInstance& RHS) const;
};

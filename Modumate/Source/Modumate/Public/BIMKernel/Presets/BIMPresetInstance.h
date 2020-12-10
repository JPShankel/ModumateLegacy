// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BIMKernel/Core/BIMEnums.h"
#include "BIMKernel/Core/BIMProperties.h"
#include "BIMKernel/Core/BIMTagPath.h"
#include "BIMKernel/Core/BIMKey.h"
#include "BIMKernel/Presets/BIMPresetTypeDefinition.h"

#include "BIMPresetInstance.generated.h"

class FModumateDatabase;
struct FBIMKey;

/*
A preset specifies a crafting node type, a set of specific values for the type's properties and specifications for which
other presets are attached to each pin

For example, a 3x4x7 inch red clay brick would be an instance of BRICK with clay, red and 3x4x7 child presets attached
*/
struct FBIMPresetCollection;

USTRUCT()
struct MODUMATE_API FBIMPresetPinAttachment
{
	GENERATED_BODY()

	UPROPERTY()
	int32 ParentPinSetIndex;

	UPROPERTY()
	int32 ParentPinSetPosition;

	UPROPERTY()
	FBIMKey PresetID;

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

	bool operator==(const FBIMPresetPartSlot& RHS) const
	{
		return RHS.PartPreset == PartPreset && RHS.SlotPreset == SlotPreset;
	}

	bool operator!=(const FBIMPresetPartSlot& RHS) const
	{
		return !(*this == RHS);
	}
};

USTRUCT()
struct MODUMATE_API FBIMPresetMaterialChannelBinding
{
	GENERATED_BODY()

	UPROPERTY()
	FString Channel;

	UPROPERTY()
	FBIMKey InnerMaterial;

	UPROPERTY()
	FBIMKey SurfaceMaterial;

	UPROPERTY()
	FString ColorHexValue;
};

USTRUCT()
struct MODUMATE_API FBIMPresetInstance
{
	GENERATED_BODY()

private:

	//for direct access to properties
	friend class FBIMPresetEditor;
	friend class FBIMAssemblySpec;
	friend class FBIMPresetEditorNode;

public:

	// TODO: roll fields below into type definition and make it the top level UPROPERTY
	FBIMPresetTypeDefinition TypeDefinition;

	bool ReadOnly = true;

	UPROPERTY()
	FBIMPropertySheet Properties;

	UPROPERTY()
	TMap<FString, FName> FormItemToProperty;

	UPROPERTY()
	FText CategoryTitle = FText::FromString(TEXT("Unknown Category"));

	UPROPERTY()
	FText DisplayName = FText::FromString(TEXT("Unknown Preset"));

	UPROPERTY()
	EBIMValueScope NodeScope;

	UPROPERTY()
	FName NodeType;

	UPROPERTY()
	FBIMKey PresetID;

	UPROPERTY()
	FGuid GUID;

	UPROPERTY()
	FBIMKey SlotConfigPresetID;

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
	TArray<FBIMPresetMaterialChannelBinding> MaterialChannelBindings;

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
	bool TryGetProperty(const FBIMNameType& Name, T& OutT) const
	{
		return Properties.TryGetProperty(NodeScope, Name, OutT);
	}

	// Sort child nodes by PinSetIndex and PinSetPosition so serialization will be consistent
	EBIMResult SortChildPresets();

	EBIMResult AddChildPreset(const FBIMKey& ChildPresetID, int32 PinSetIndex, int32 PinSetPosition);
	EBIMResult RemoveChildPreset(int32 PinSetIndex, int32 PinSetPosition);
	EBIMResult SetPartPreset(const FBIMKey& SlotPreset, const FBIMKey& PartPreset);
	bool HasPin(int32 PinSetIndex, int32 PinSetPosition) const;

	bool ValidatePreset() const;

	bool Matches(const FBIMPresetInstance& OtherPreset) const;
};

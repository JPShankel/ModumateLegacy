// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BIMKernel/BIMEnums.h"
#include "BIMKernel/BIMProperties.h"
#include "BIMKernel/BIMTagPath.h"
#include "BIMKernel/BIMKey.h"
#include "BIMKernel/BIMSerialization.h"
#include "BIMKernel/BIMAssemblySpec.h"
#include "BIMKernel/BIMPresetTypeDefinition.h"

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
	FName PinChannel;

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
	FBIMKey SlotName;

	UPROPERTY()
	FBIMKey ID;

	UPROPERTY()
	FBIMKey ParentID;
};

USTRUCT()
struct MODUMATE_API FBIMPresetInstance
{
	GENERATED_BODY()

private:
	FBIMPropertySheet Properties;

	//for direct access to properties
	friend class FBIMCraftingTreeNodePool;
	friend class FBIMAssemblySpec;

public:

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

	TArray<FBIMTagPath> ParentTagPaths;
	FBIMTagPath MyTagPath;

	bool HasProperty(const FBIMNameType& Name) const;
	Modumate::FModumateCommandParameter GetProperty(const FBIMNameType& Name) const;
	
	Modumate::FModumateCommandParameter GetScopedProperty(const EBIMValueScope& Scope, const FBIMNameType& Name) const;
	void SetScopedProperty(const EBIMValueScope& Scope, const FBIMNameType& Name, const Modumate::FModumateCommandParameter& V);

	void SetProperties(const FBIMPropertySheet& InProperties);

	template <class T>
	bool TryGetProperty(const FBIMNameType& Name, T& OutT) const
	{
		if (HasProperty(Name))
		{
			OutT = GetProperty(Name);
			return true;
		}
		return false;
	}

	// Sort child nodes by PinSetIndex and PinSetPosition so serialization will be consistent
	EBIMResult SortChildNodes();

	FString GetDisplayName() const;

	bool Matches(const FBIMPresetInstance& OtherPreset) const;
	bool SupportsChild(const FBIMPresetInstance& CandidateChild) const;

	EBIMResult ToDataRecord(FCraftingPresetRecord &OutRecord) const;
	EBIMResult FromDataRecord(const FBIMPresetCollection &PresetCollection, const FCraftingPresetRecord &Records);
};
// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BIMKernel/BIMEnums.h"
#include "BIMKernel/BIMProperties.h"
#include "BIMKernel/BIMTagPath.h"
#include "BIMKernel/BIMUProperties.h"
#include "BIMKernel/BIMKey.h"
#include "BIMKernel/BIMSerialization.h"

/*
A pin set defines a minimum and maximum number of attached children (each child is a pin), which lists define legal
children for this pin (ie a list of colors or materials available for a given object) and pointers to node instances
attached to this pin set.

A node definition specifies zero or more pin sets for child attachment
*/

struct MODUMATE_API FChildAttachmentType
{
	FName SetName;
	int32 MinCount, MaxCount;

	FChildAttachmentType(FName InSetName, int32 InMinCount, int32 InMaxCount) :
		SetName(InSetName), MinCount(InMinCount), MaxCount(InMaxCount)
	{}

	FChildAttachmentType() {};
};

/*
Crafting tree nodes are defined in the DDL 2.0 script, which specifies which properties nodes of a given type carry
and what pins it supports for child nodes

Some properties are hidden, so visible/editable properties are specified in the FormItemToProperty map
TODO: form visibility and editability of properties to be specified in presets, not as part of the fundamental type
*/
class MODUMATE_API FBIMPresetNodeType
{
public:
	FName TypeName;

	/*
	Motivating use case for Scope is so adjective nodes (ie color, dimension, material) will understand what noun
	(ie module, layer, gap) they modify. Adjective nodes with properties in the 'Node' scope have those properties
	rebound to the scope of the parent pin to which they are attached
	*/
	EBIMValueScope Scope = EBIMValueScope::None;

	FBIMPropertySheet Properties;
	TMap<FString, FBIMNameType> FormItemToProperty;
	TArray<FChildAttachmentType> ChildAttachments;
};

/*
A preset specifies a crafting node type, a set of specific values for the type's properties and specifications for which
other presets are attached to each pin

For example, a 3x4x7 inch red clay brick would be an instance of BRICK with clay, red and 3x4x7 child presets attached
*/
class FBIMPresetCollection;

class MODUMATE_API FBIMPreset
{

private:
	FBIMPropertySheet Properties;

	//for direct access to properties
	friend class FBIMCraftingTreeNodePool;
	friend class FBIMAssemblySpec;

public:

	struct FChildAttachment
	{
		int32 ParentPinSetIndex, ParentPinSetPosition;
		FBIMKey PresetID;
		bool operator==(const FChildAttachment& OtherAttachment) const;
	};

	struct FPartSlot
	{
		FBIMKey PartPreset,SlotName,ID,ParentID;
	};

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

	FString CategoryTitle = TEXT("Unknown Category");

	EBIMValueScope NodeScope;
	FName NodeType;
	FBIMKey PresetID, SlotConfigPresetID;
	TArray<FChildAttachment> ChildPresets;
	TArray<FBIMTagPath> ParentTagPaths;
	TArray<FPartSlot> PartSlots;
	FBIMTagPath MyTagPath;

	// Sort child nodes by PinSetIndex and PinSetPosition so serialization will be consistent
	ECraftingResult SortChildNodes();

	FString GetDisplayName() const;

	bool Matches(const FBIMPreset& OtherPreset) const;
	bool SupportsChild(const FBIMPreset& CandidateChild) const;

	EObjectType ObjectType = EObjectType::OTNone;
	EConfiguratorNodeIconType IconType = EConfiguratorNodeIconType::None;
	EConfiguratorNodeIconOrientation Orientation = EConfiguratorNodeIconOrientation::Inherited;

	ECraftingResult ToDataRecord(FCraftingPresetRecord &OutRecord) const;
	ECraftingResult FromDataRecord(const FBIMPresetCollection &PresetCollection, const FCraftingPresetRecord &Records);
};

class MODUMATE_API FBIMPresetCollection
{
public:

	TMap<FName, FBIMPresetNodeType> NodeDescriptors;
	TMap<FBIMKey, FBIMPreset> Presets;

	EObjectType GetPresetObjectType(const FBIMKey &PresetID) const;


	ECraftingResult ToDataRecords(TArray<FCraftingPresetRecord> &OutRecords) const;
	ECraftingResult FromDataRecords(const TArray<FCraftingPresetRecord> &Record);

	ECraftingResult GetDependentPresets(const FBIMKey &PresetID, TSet<FBIMKey> &OutPresets) const;

	ECraftingResult GetPropertyFormForPreset(const FBIMKey &PresetID, TMap<FString, FBIMNameType> &OutForm) const;

	ECraftingResult LoadCSVManifest(const FString& ManifestPath, const FString& ManifestFile, TArray<FBIMKey>& OutStarters, TArray<FString>& OutMessages);
};

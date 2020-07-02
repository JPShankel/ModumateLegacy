// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BIMKernel/BIMEnums.h"
#include "BIMKernel/BIMProperties.h"
#include "BIMKernel/BIMTagPath.h"
#include "BIMKernel/BIMSerialization.h"

namespace Modumate { namespace BIM {

	/*
	A pin set defines a minimum and maximum number of attached children (each child is a pin), which lists define legal
	children for this pin (ie a list of colors or materials available for a given object) and pointers to node instances
	attached to this pin set.

	A node definition specifies zero or more pin sets for child attachment
	*/
	class MODUMATE_API FCraftingTreeNodeInstance;

	typedef TSharedPtr<FCraftingTreeNodeInstance> FCraftingTreeNodeInstanceSharedPtr;
	typedef TWeakPtr<FCraftingTreeNodeInstance> FCraftingTreeNodeInstanceWeakPtr;

	typedef TSharedPtr<const FCraftingTreeNodeInstance> FCraftingTreeNodeInstanceSharedPtrConst;
	typedef TWeakPtr<const FCraftingTreeNodeInstance> FCraftingTreeNodeInstanceWeakPtrConst;

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
	class MODUMATE_API FCraftingTreeNodeType
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
		TMap<FString, FNameType> FormItemToProperty;
		TArray<FChildAttachmentType> ChildAttachments;
	};

/*
A preset specifies a crafting node type, a set of specific values for the type's properties and specifications for which
other presets are attached to each pin

For example, a 3x4x7 inch red clay brick would be an instance of BRICK with clay, red and 3x4x7 child presets attached
*/
	class MODUMATE_API FCraftingPresetCollection;

	struct MODUMATE_API FChildAttachmentPreset
	{
		int32 ParentPinSetIndex, ParentPinSetPosition;
		FName PresetID;
		bool operator==(const FChildAttachmentPreset &OtherAttachment) const;
	};

	class MODUMATE_API FCraftingTreeNodePreset
	{
	public:

		FName NodeType, PresetID;
		FBIMPropertySheet Properties;
		TArray<FChildAttachmentPreset> ChildPresets;
		TArray<FTagPath> ParentTagPaths;
		FTagPath MyTagPath;

		// Sort child nodes by PinSetIndex and PinSetPosition so serialization will be consistent
		ECraftingResult SortChildNodes();

		FString GetDisplayName() const;

		bool Matches(const FCraftingTreeNodePreset &OtherPreset) const;

		EObjectType ObjectType = EObjectType::OTNone;
		EConfiguratorNodeIconType IconType = EConfiguratorNodeIconType::None;
		EConfiguratorNodeIconOrientation Orientation = EConfiguratorNodeIconOrientation::Inherited;
		bool CanFlipOrientation = false;

		ECraftingResult ToDataRecord(FCraftingPresetRecord &OutRecord) const;
		ECraftingResult FromDataRecord(const FCraftingPresetCollection &PresetCollection, const FCraftingPresetRecord &Records);

		ECraftingResult ToParameterSet(FModumateFunctionParameterSet &OutParameterSet) const;
		ECraftingResult FromParameterSet(const FCraftingPresetCollection &PresetCollection, const FModumateFunctionParameterSet &ParameterSet);
	};

	class MODUMATE_API FCraftingPresetCollection
	{
	public:
		TMap<FName, FCraftingTreeNodeType> NodeDescriptors;
		TMap<FName, FCraftingTreeNodePreset> Presets;

		EObjectType GetPresetObjectType(const FName &PresetID) const;

		ECraftingResult ToDataRecords(TArray<FCraftingPresetRecord> &OutRecords) const;
		ECraftingResult FromDataRecords(const TArray<FCraftingPresetRecord> &Record);

		ECraftingResult GetDependentPresets(const FName &PresetID, TSet<FName> &OutPresets) const;
		ECraftingResult GetInstanceDataAsPreset(const FCraftingTreeNodeInstanceSharedPtrConst &Instance, FCraftingTreeNodePreset &OutPreset) const;

		ECraftingResult LoadCSVManifest(const FString &ManifestPath, const FString &ManifestFile, TArray<FString> &OutMessages);
	};
} }
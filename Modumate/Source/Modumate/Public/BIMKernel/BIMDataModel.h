// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Database/ModumateObjectEnums.h"
#include "BIMKernel/BIMSerialization.h"
#include "BIMKernel/BIMProperties.h"
#include "BIMDataModel.generated.h"

struct MODUMATE_API FCraftingPresetRecord;

UENUM(BlueprintType)
enum class EConfiguratorNodeIconType : uint8
{
	None = 0,
	LayeredAssembly,
	Stair,
	OpeningSystem,
	FFEConfig,
	ExtrudedProfile,
	LayerVertical,
	LayerHorizontal,
	Layer,
	Module,
	Gap2D,
	StaticMesh,
	Profile,
	SawtoothStringer,
	StairNosing,
	GraphConfig2D,
	Material,
	SrfTreatment,
	Color,
	Dimension,
	Pattern,
};

UENUM(BlueprintType)
enum class EConfiguratorNodeIconOrientation : uint8
{
	None = 0,
	Horizontal,
	Vertical,
	Inherited
};

UENUM(BlueprintType)
enum class ECraftingResult : uint8
{
	None = 0,
	Success,
	Error
};

UENUM(BlueprintType)
enum class ELayerFunction : uint8
{
	None,
	Void,
	Insulation,
	Structure,
	Substrate,
	Membrane,
	// Items below are for finishes, for now
	Adhesive,
	Underlayment,
	Finish,
	Abstract
};

UENUM(BlueprintType)
enum class ELayerFormat : uint8
{
	None,
	Block,
	Board,
	Brick,
	Channel,
	Deck,
	Joist,
	Masonry,
	Mass,
	Panel,
	Plank,
	Roll,
	Spread,
	Stud,
	Sheet,
	Shingle,
	Tile
};

USTRUCT()
struct FBIMScriptRecord
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	FString FilePath;

	UPROPERTY()
	bool IsCSV = true;
};

namespace Modumate { namespace BIM {
	
	/*
	DDL 2.0 - crafting trees are acyclical connected graphs of crafting nodes built in the crafting UI

	Each node instance has a pointer to its parent and a set of pins defining which children it can support

	Every node instance must be built from a preset which defines the data type for the node (defined in the DDL script),
	the properties carried by the node (width, height, color, name, etc) and which other presets are present in the child pin sets

	As with DDL 1.0, crafting trees are walked to produce a FModumateAssemblyPropertySpec, which are used to create runtime assemblies attached to MOIs

	*/

	class MODUMATE_API FCraftingTreeNodeInstance;

	typedef TSharedPtr<FCraftingTreeNodeInstance> FCraftingTreeNodeInstanceSharedPtr;
	typedef TWeakPtr<FCraftingTreeNodeInstance> FCraftingTreeNodeInstanceWeakPtr;

	typedef TSharedPtr<const FCraftingTreeNodeInstance> FCraftingTreeNodeInstanceSharedPtrConst;
	typedef TWeakPtr<const FCraftingTreeNodeInstance> FCraftingTreeNodeInstanceWeakPtrConst;

	class MODUMATE_API FCraftingPresetTag
	{
	private:
		FName Scope, Tag;
		FString QualifiedString;

	public:
		FCraftingPresetTag() {};
		FCraftingPresetTag(const FName &InScope, const FName &InTag);
		FCraftingPresetTag(const FString &InQualifiedString);

		FName GetScope() const { return Scope; }
		FName GetTag() const { return Tag; }
		const FString &GetQualifiedString() const { return QualifiedString; }

		bool operator==(const FCraftingPresetTag &Other) const { return Scope == Other.Scope && Tag == Other.Tag; }
	};

	/*
	A pin set defines a minimum and maximum number of attached children (each child is a pin), which lists define legal
	children for this pin (ie a list of colors or materials available for a given object) and pointers to node instances
	attached to this pin set.

	A node definition specifies zero or more pin sets for child attachment
	*/

	// Represents a preset selected by search criteria (node type & tags)
	// TODO: for tag path searches, replace single SelectionSearchTags (treated as an OR set) with a sequence of ANDed ORs
	class MODUMATE_API FCraftingPresetSearchSelection
	{
	public:
		FName SelectedPresetID, NodeType;
		TArray<FCraftingPresetTag> SelectionSearchTags;
	};

	class MODUMATE_API FCraftingTreeNodePinSet
	{
	public:
		FName SetName;
		int32 MinCount, MaxCount;

		FCraftingPresetSearchSelection EligiblePresetSearch;

		/*
		Motivating use case for Scope is so adjective nodes (ie color, dimension, material) will understand what noun
		(ie module, layer, gap) they modify. Adjective nodes with properties in the 'Node' scope have those properties
		rebound to the scope of the parent pin to which they are attached
		*/
		EBIMValueScope Scope = EBIMValueScope::None;

		TArray<FCraftingTreeNodeInstanceWeakPtr> AttachedObjects;
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
		EObjectType ObjectType = EObjectType::OTNone;

		EConfiguratorNodeIconType IconType = EConfiguratorNodeIconType::None;
		EConfiguratorNodeIconOrientation Orientation = EConfiguratorNodeIconOrientation::Inherited;

		bool CanFlipOrientation = false;

		FBIMPropertySheet Properties;
		TMap<FString, FNameType> FormItemToProperty;
		TArray<FCraftingTreeNodePinSet> InputPins;
	};

	/*
	A preset specifies a crafting node type, a set of specific values for the type's properties and specifications for which
	other presets are attached to each pin

	For example, a 3x4x7 inch red clay brick would be an instance of BRICK with clay, red and 3x4x7 child presets attached
	*/
	class MODUMATE_API FCraftingPresetCollection;

	class MODUMATE_API FCraftingTreeNodePreset
	{
	public:
		struct FPinSpec
		{
			int32 PinSetIndex, PinSetPosition;

			//A pin has zero or more ReadOnly presets terminated by a single writeable preset
			//TODO: preset sequences to be replaced by tag paths
			TArray<FCraftingPresetSearchSelection> PresetSequence;

			//TODO: deprecate in favor of search criteria in PresetSequence[0] (of which there will always be one)
			TArray<FCraftingPresetTag> PinSpecSearchTags;

			EBIMValueScope Scope = EBIMValueScope::None;
		};

		bool IsReadOnly = false;

		FName NodeType, PresetID;
		FBIMPropertySheet Properties;
		TArray<FPinSpec> ChildNodes;

		// Sort child nodes by PinSetIndex and PinSetPosition so serialization will be consistent
		ECraftingResult SortChildNodes();

		// Tags grouped by scope
		TMap<FName, TArray<FCraftingPresetTag>> Tags;

		FString GetDisplayName() const;

		bool Matches(const FCraftingTreeNodePreset &OtherPreset) const;

		bool HasTag(const FCraftingPresetTag &Tag) const;

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

		TMap<FName, TArray<FName>> Lists;

		//TODO: remove when lists are factored out of table read
		TMap<FName, TArray<FCraftingPresetTag>> ListTags;

		EObjectType GetPresetObjectType(const FName &PresetID) const;

		ECraftingResult SearchPresets(const FName &NodeType, const TArray<FCraftingPresetTag> &IncludedTags, const TArray<FCraftingPresetTag> &ExcludedTags, TArray<FName> &OutPresets) const;
		ECraftingResult SearchPresets(const FCraftingPresetSearchSelection &FieldValue, TArray<FName> &OutPresets) const;

		ECraftingResult ToDataRecords(TArray<FCraftingPresetRecord> &OutRecords) const;
		ECraftingResult FromDataRecords(const TArray<FCraftingPresetRecord> &Record);

		ECraftingResult GetDependentPresets(const FName &PresetID, TSet<FName> &OutPresets) const;

		ECraftingResult ReadDataTable(UDataTable *DataTable, TArray<FString> &OutMessages);
		ECraftingResult ParseScriptFile(const FString &FilePath, TArray<FString> &OutMessages);

		ECraftingResult GetInstanceDataAsPreset(const FCraftingTreeNodeInstanceSharedPtrConst &Instance, FCraftingTreeNodePreset &OutPreset) const;
	};
} }
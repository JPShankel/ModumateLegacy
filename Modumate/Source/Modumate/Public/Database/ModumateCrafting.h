// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"

#include "ModumateCore/ModumateUnits.h"
#include "ModumateCore/ModumateConsoleCommand.h"
#include "Database/ModumateArchitecturalMaterial.h"
#include "Database/ModumateObjectEnums.h"
#include "Database/ModumateSimpleMesh.h"
#include "Database/ModumateBIMSchema.h"
#include "ModumateCrafting.generated.h"

namespace Modumate
{
	class ModumateObjectDatabase;

	struct MODUMATE_API FCraftingOptionBase
	{
		FName Key;
		FText DisplayName;

		TWeakObjectPtr<UTexture2D> Icon;
		TWeakObjectPtr<UMaterialInterface> EngineMaterial;
		FCustomColor CustomColor;
		FSimpleMeshRef ProfileMesh;

		Modumate::BIM::FBIMPropertySheet CraftingParameters;

		FName UniqueKey() const { return Key; }
	};

	namespace CraftingParameters
	{
		extern const FString ThicknessValue;
		extern const FString ThicknessUnits;
		extern const FString MaterialColorMaterial;
		extern const FString MaterialColorColor;
		extern const FString DimensionLength;
		extern const FString DimensionWidth;
		extern const FString DimensionHeight;
		extern const FString DimensionDepth;
		extern const FString DimensionThickness;
		extern const FString DimensionBevelWidth;
		extern const FString PatternModuleCount;
		extern const FString PatternExtents;
		extern const FString PatternThickness;
		extern const FString PatternGap;
		extern const FString PatternName;
		extern const FString PatternModuleDimensions;
		extern const FString TrimProfileNativeSizeX;
		extern const FString TrimProfileNativeSizeY;
	}

	struct MODUMATE_API FCraftingSubcategoryData : public FCraftingOptionBase
	{
		FString SheetName;
		FString IDCodeLine1;
		ELayerFunction LayerFunction;
		ELayerFormat LayerFormat;
	};

	template<class T>
	struct MODUMATE_API TCraftingOptionSet
	{
		typedef T OptionType;
		FName Key;
		TArray<T> Options;
		FName UniqueKey() const { return Key; }
	};

	typedef TCraftingOptionSet<FCraftingOptionBase> FCraftingOptionSet;

	struct MODUMATE_API FCraftingPortalPartOption : public FCraftingOptionBase
	{
		TSet<FName> ConfigurationWhitelist;
		TMap<EPortalSlotType, FName> PartsBySlotType;
		static const FString TableName;
	};

	typedef TCraftingOptionSet<FCraftingPortalPartOption> FCraftingPortalPartOptionSet;

	struct MODUMATE_API FPortalAssemblyConfigurationSlot
	{
		EPortalSlotType Type = EPortalSlotType::None;
		FString LocationX, LocationY, LocationZ;
		FString SizeX, SizeY, SizeZ;
		float RotateX = 0, RotateY = 0, RotateZ = 0;
		bool FlipX = false, FlipY = false, FlipZ = false;
	};

	struct MODUMATE_API FPortalReferencePlane
	{
		Modumate::Units::FUnitValue FixedValue = Modumate::Units::FUnitValue::WorldCentimeters(0);
		FString ValueExpression = FString();
		EAxis::Type Axis = EAxis::None;
		int32 Index = 0;
		FName Name;
	};

	struct MODUMATE_API FPortalConfigDimensionSet
	{
		FName Key;
		FText DisplayName;
		TMap<FName, Modumate::Units::FUnitValue> DimensionMap;
	};

	struct MODUMATE_API FPortalAssemblyConfigurationOption : public FCraftingOptionBase
	{
		EPortalFunction PortalFunction = EPortalFunction::None;
		TArray<FPortalReferencePlane> ReferencePlanes[3];
		TArray<FPortalAssemblyConfigurationSlot> Slots;
		TArray<FPortalConfigDimensionSet> SupportedWidths;
		TArray<FPortalConfigDimensionSet> SupportedHeights;

		static const FString TableName;

		bool IsValid() const;
	};

	typedef TCraftingOptionSet<FPortalAssemblyConfigurationOption> FPortalAssemblyConfigurationOptionSet;

}

struct FCustomAssemblyCraftingNodeRecord;
struct FCraftingPresetRecord;
// DDL 2.0
namespace Modumate {

	namespace BIM {

		struct MODUMATE_API FModumateAssemblyPropertySpec
		{
			EObjectType ObjectType = EObjectType::OTNone;
			FName RootPreset;
			FBIMPropertySheet RootProperties;
			TArray<FBIMPropertySheet> LayerProperties;
		};

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
			TMap<FName,TArray<FCraftingPresetTag>> Tags;

			FString GetDisplayName() const;

			bool Matches(const FCraftingTreeNodePreset &OtherPreset) const;

			bool HasTag(const FCraftingPresetTag &Tag) const;

			ECraftingResult ToDataRecord(FCraftingPresetRecord &OutRecord) const;
			ECraftingResult FromDataRecord(const FCraftingPresetCollection &PresetCollection,const FCraftingPresetRecord &Records);

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

		/*
		Crafting trees are acyclical networks of crafting node instances
		They have their own input pin sets and properties, both of which are inherited from the preset used to create the node
		If a node's properties or child configuration are inconsistent with the base preset, the preset is considered 'dirty' and must be updated or branched
		*/
		class MODUMATE_API FCraftingTreeNodeInstance : public TSharedFromThis<FCraftingTreeNodeInstance>
		{
			friend class MODUMATE_API FCraftingTreeNodeInstancePool;

#if WITH_EDITOR // for debugging new/delete balance
			static int32 InstanceCount;
#endif

		private:
			// Node instances are managed solely by the object node set and should not be passed around by value or constructed by anyone else
			FCraftingTreeNodeInstance() = delete;
			FCraftingTreeNodeInstance(const FCraftingTreeNodeInstance &rhs) = delete;
			FCraftingTreeNodeInstance &operator=(const FCraftingTreeNodeInstance &rhs) = delete;
			FCraftingTreeNodeInstance(int32 instanceID);


			int32 InstanceID;

			ECraftingResult GatherAllChildNodes(TArray<FCraftingTreeNodeInstanceSharedPtr> &OutChildren);

		public:
			~FCraftingTreeNodeInstance() 
			{
#if WITH_EDITOR // for debugging new/delete balance
				--InstanceCount; 
#endif
			};

			FName PresetID;
			FCraftingTreeNodeInstanceWeakPtr ParentInstance;

			// TODO: privatize and report 'preset dirty' state
			TArray<FCraftingTreeNodePinSet> InputPins;
			FBIMPropertySheet InstanceProperties;

			// May be fixed in type definition, inherited from parent or switchable
			EConfiguratorNodeIconOrientation CurrentOrientation;

			bool CanFlipOrientation = false;

			int32 GetInstanceID() const;

			ECraftingNodePresetStatus GetPresetStatus(const FCraftingPresetCollection &PresetCollection) const;

			ECraftingResult ToDataRecord(FCustomAssemblyCraftingNodeRecord &OutRecord) const;
			ECraftingResult FromDataRecord(FCraftingTreeNodeInstancePool &InstancePool, const FCraftingPresetCollection &PresetCollection,const FCustomAssemblyCraftingNodeRecord &DataRecord, bool RecursePresets);

			bool CanRemoveChild(const FCraftingTreeNodeInstanceSharedPtrConst &Child) const;

			ECraftingResult DetachSelfFromParent();

			ECraftingResult AttachChild(const FCraftingPresetCollection &PresetCollection, const FCraftingTreeNodeInstanceSharedPtr &Child);
			ECraftingResult FindChild(int32 ChildID, int32 &OutPinSetIndex, int32 &OutPinSetPosition);
			ECraftingResult FindChildOrder(int32 ChildID, int32 &Order);
		};

		/*
		A Crafting Tree Node Set contains all the context information for node crafting.
		Node descriptors, preset definitions and preset lists are read in via ParseScriptFile
		The crafting widget and preset manager use this node set to implement the crafting interface

		TODO: this functionality to be merged into the crafting widget which is the only client of node instances
		*/

		class MODUMATE_API FCraftingTreeNodeInstancePool
		{
		private:
			int32 NextInstanceID = 1;
			TMap<int32, FCraftingTreeNodeInstanceWeakPtr> InstanceMap;
			TArray<FCraftingTreeNodeInstanceSharedPtr> InstancePool;

			// Does not create a fully resolved node, used in FromDataRecord only
			FCraftingTreeNodeInstanceSharedPtr CreateNodeInstanceFromDataRecord(const FCraftingPresetCollection &PresetCollection,const FCustomAssemblyCraftingNodeRecord &DataRecord, bool RecursePresets);

		public:

			const TArray<FCraftingTreeNodeInstanceSharedPtr> &GetInstancePool() const { return InstancePool; }

			ECraftingResult ResetInstances();

			FCraftingTreeNodeInstanceSharedPtr CreateNodeInstanceFromPreset(const FCraftingPresetCollection &PresetCollection, int32 ParentID, const FName &PresetID, bool CreateDefaultReadOnlyChildren);
			ECraftingResult SetNewPresetForNode(const FCraftingPresetCollection &PresetCollection, int32 InstanceID, const FName &PresetID);
			
			ECraftingResult DestroyNodeInstance(const FCraftingTreeNodeInstanceSharedPtr &Instance, TArray<int32> &OutDestroyed);
			ECraftingResult DestroyNodeInstance(int32 InstanceID, TArray<int32> &OutDestroyed);

			ECraftingResult PresetToSpec(const FName &PresetID, const FCraftingPresetCollection &PresetCollection, FModumateAssemblyPropertySpec &OutPropertySpec) const;

			const FCraftingTreeNodeInstanceSharedPtr InstanceFromID(int32 InstanceID) const;
			FCraftingTreeNodeInstanceSharedPtr InstanceFromID(int32 InstanceID);
			FCraftingTreeNodeInstanceSharedPtr FindInstanceByPredicate(const std::function<bool(const FCraftingTreeNodeInstanceSharedPtr &Instance)> &Predicate);

			bool FromDataRecord(const FCraftingPresetCollection &PresetCollection, const TArray<FCustomAssemblyCraftingNodeRecord> &InDataRecords, bool RecursePresets);
			bool ToDataRecord(TArray<FCustomAssemblyCraftingNodeRecord> &OutDataRecords) const;

			bool ValidatePool() const;
		};
	}
} 

/*
	These USTRUCTs are used to share node tree data with the widget-based UI
*/

USTRUCT(BlueprintType)
struct MODUMATE_API FCraftingNodeFormItem
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	EDecisionType ItemType;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	FText DisplayLabel;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	FString ItemValue;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	TArray<FString> ListItems;

	int32 NodeInstance;
	FString PropertyBindingQN;
};

USTRUCT(BlueprintType)
struct MODUMATE_API FCraftingNode
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	FText Label;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	FText ParentPinLabel;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	int32 InstanceID;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	int32 ParentInstanceID;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	FName PresetID;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	ECraftingNodePresetStatus PresetStatus;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	bool CanSwapPreset;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	bool CanAddToProject;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	bool IsEmbeddedInParent = false;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	EConfiguratorNodeIconType NodeIconType;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	EConfiguratorNodeIconOrientation NodeIconOrientation;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	bool CanSwitchOrientation;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	TArray<int32> EmbeddedInstanceIDs;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	int32 ChildOrder;

	static FCraftingNode FromPreset(const Modumate::BIM::FCraftingTreeNodePreset &Preset);
};

USTRUCT(BlueprintType)
struct MODUMATE_API FCraftingNodePinGroup
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	int32 MinPins;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	int32 MaxPins;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	TArray<int32> AttachedChildren;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	TArray<FName> SupportedTypePins;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	FName GroupName;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	FName NodeType;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	TArray<FString> Tags;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	int32 OwnerInstance;
};

/*
Static support functions for common node widget UI operations
Custom behaviors are in the widget classes themsleves (ModumateCraftingWidget, ModumateDrawingSetWidget)
*/
class MODUMATE_API UModumateCraftingNodeWidgetStatics : public UBlueprintFunctionLibrary
{
private:
	static ECraftingResult CraftingNodeFromInstance(const Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances, int32 InstanceID, const Modumate::BIM::FCraftingPresetCollection &PresetCollection, FCraftingNode &OutNode);

public:

	static ECraftingResult CreateNewNodeInstanceFromPreset(Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances,const Modumate::BIM::FCraftingPresetCollection &PresetCollection, int32 ParentID, const FName &PresetID, FCraftingNode &OutNode);
	static ECraftingResult GetInstantiatedNodes(Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances, const Modumate::BIM::FCraftingPresetCollection &PresetCollection, TArray<FCraftingNode> &OutNodes);
	static ECraftingResult GetPinAttachmentToParentForNode(Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances, int32 InstanceID, int32 &OutPinGroupIndex, int32 &OutPinIndex);
	static ECraftingResult GetFormItemsForCraftingNode(Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances, const Modumate::BIM::FCraftingPresetCollection &PresetCollection,int32 InstanceID, TArray<FCraftingNodeFormItem> &OutForm);
	static ECraftingResult GetFormItemsForPreset(const Modumate::BIM::FCraftingPresetCollection &PresetCollection, const FName &PresetID, TArray<FCraftingNodeFormItem> &OutForm);
	static ECraftingResult SetValueForFormItem(Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances, const FCraftingNodeFormItem &FormItem, const FString &Value);
	static ECraftingResult SetValueForPreset(const FName &PresetID, const FString &Value);
	static ECraftingResult RemoveNodeInstance(Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances, int32 ParentID, int32 InstanceID);
	static ECraftingResult GetPinGroupsForNode(Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances, const Modumate::BIM::FCraftingPresetCollection &PresetCollection, int32 NodeID, TArray<FCraftingNodePinGroup> &OutPins);
	static ECraftingResult DragMovePinChild(Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances, int32 InstanceID, const FName &PinGroup, int32 From, int32 To);
	static ECraftingResult GetLayerIDFromNodeInstanceID(const Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances, const Modumate::BIM::FCraftingPresetCollection &PresetCollection, int32 InstanceID, int32 &OutLayerID, int32 &NumberOfLayers);
	static ECraftingResult GetPropertyTipsByIconType(const Modumate::ModumateObjectDatabase &InDB, EConfiguratorNodeIconType IconType, const Modumate::BIM::FModumateAssemblyPropertySpec &PresetProperties, TArray<FString> &OutTips);
	static ECraftingResult GetEligiblePresetsForSwap(Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances, const Modumate::BIM::FCraftingPresetCollection &PresetCollection, int32 InstanceID, TArray<FCraftingNode> &OutPresets);
};







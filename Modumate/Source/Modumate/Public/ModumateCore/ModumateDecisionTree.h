// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include <functional>

#include "ModumateDecisionTree.generated.h"


class UDataTable;

UENUM(BlueprintType)
enum class EDecisionSelectStyle : uint8
{
	None,
	Icons,
	Dropdown,
	FixedList,
	HorizontalWheel,
	VerticalWheel,
	Slider
};

namespace Modumate
{
	template<class NodeType>
	class MODUMATE_API TDecisionTree;


	template<class NodeType>
	class MODUMATE_API TDecisionTreeNode
	{
		friend class TDecisionTree<NodeType>;

	private:
		
		// non-const version const-casts the const version, so disambiguate naming
		const TDecisionTreeNode<NodeType> *FindNodeConstLambda(std::function<bool(const NodeType &n)> isIt) const;
		TDecisionTreeNode<NodeType> *FindNodeLambdaNonConst(std::function<bool(const NodeType &n)> isIt);

		const TDecisionTreeNode<NodeType> *GetSelectedSubfieldConst() const;

		static TDecisionTreeNode<NodeType> FromDataTable(UDataTable *data);

	public:

		NodeType Data;
		TArray<TDecisionTreeNode<NodeType>> Subfields;

		TDecisionTreeNode<NodeType> *FindNodeByGUID(const FName &guid);
		const TDecisionTreeNode<NodeType> *FindNodeByGUID(const FName &guid) const;

		const TDecisionTreeNode<NodeType> *FindNode(std::function<bool(const NodeType &n)> isIt) const;
		TDecisionTreeNode<NodeType> *FindNode(std::function<bool(const NodeType &n)> isIt);

		TArray<NodeType> GetOpenNodes() const;

		void SetStringValueByGUID(const FName &guid, const FString &value);
		void SetNameValueByGUID(const FName &guid, const FName &value);

		bool ValidateData();

		void WalkDepthFirst(std::function<bool(TDecisionTreeNode<NodeType> &tr)> fn);
		void WalkDepthFirst(std::function<bool(const TDecisionTreeNode<NodeType> &tr)> fn) const;

		void GetTrace(std::function<void(const TDecisionTreeNode<NodeType> &)> fn) const;

		TDecisionTreeNode<NodeType> *GetSelectedSubfield();
		const TDecisionTreeNode<NodeType> *GetSelectedSubfield() const;
		bool SetSelectedSubfield(int32 i);

		void Reset();
	};

	template<class NodeType>
	class MODUMATE_API TDecisionTree
	{
	public:
		TDecisionTreeNode<NodeType> RootNode;

		// Dynamic lists have zero or more children whose form is defined by the child of the dynamic list node in the tree data
		TMap<FName, TDecisionTreeNode<NodeType>> DynamicListElementTypes;
		typedef TArray<TDecisionTreeNode<NodeType>> FDecisionNodeArray;
		TMap<FName, FDecisionNodeArray> DynamicListElementInstances;

		static TDecisionTree<NodeType> FromDataTable(UDataTable *data);

		TDecisionTreeNode<NodeType> *FindDynamicListInstanceNode(const FName &listGUID, int32 index, const FName &nodeGUID);
		const TDecisionTreeNode<NodeType> *FindDynamicListInstanceNode(const FName &listGUID, int32 index, const FName &nodeGUID) const;

		TDecisionTreeNode<NodeType> *FindDynamicListTypeNode(const FName &listGUID, const FName &nodeGUID);
		const TDecisionTreeNode<NodeType> *FindDynamicListTypeNode(const FName &listGUID, const FName &nodeGUID) const;

		TDecisionTreeNode<NodeType> *FindNodeByGUID(const FName &guid);
		const TDecisionTreeNode<NodeType> *FindNodeByGUID(const FName &guid) const;

		void SetStringValueByGUID(const FName &guid, const FString &value);
		void SetNameValueByGUID(const FName &guid, const FName &value);

		void SetDynamicListElementStringValueByGUID(const FName &dlist, int32 index, const FName &guid, const FString &value);
		void SetDynamicListElementNameValueByGUID(const FName &dlist, int32 index, const FName &guid, const FName &value);

		void ProcessDynamicLists();

		void Reset();
		const TDecisionTreeNode<NodeType> *MakeDynamicListElementAt(const FName &dlistGUID,int32 i);
	};

	template<class T, class O, class OS>
	void ReadOptionSet(
		UDataTable *data,
		std::function<bool(const T &row, O &ot)> readOptionData,
		std::function<void(const OS &o)> addOptionSet
	);

}
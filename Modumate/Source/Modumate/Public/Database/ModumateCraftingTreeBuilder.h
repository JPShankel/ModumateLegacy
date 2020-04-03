// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModumateBIMSchema.h"
#include "ModumateCrafting.h"

namespace Modumate
{
	class ModumateObjectDatabase;
	class MODUMATE_API FCraftingTreeBuilder
	{
	public:

		enum State {
			Neutral = 0,
			ReadingForm,
			ReadingList
		};

		struct FFormElement
		{
			EDecisionSelectStyle SelectionStyle;
			FText Name;
			TArray<FString> Targets;
			BIM::EValueType Type;
		};

		struct FForm
		{
			bool Collapsible = false;
			bool HasPreset = false;
			FString InternalName, Scope;
			FText VisibleLabel;
			TArray<FFormElement> Elements;
		};

		FForm CurrentForm;

		struct FValueBinding
		{
			FString Variable, Value;
		};

		struct FListElement
		{
			TArray<FValueBinding> Bindings;
		};

		struct FList
		{
			FString Name, ElementType;
			TArray<FListElement> Elements;
		};

		FList CurrentList;

		TMap<FString, FForm> Forms;
		TMap<FString, FList> Lists;

		FCraftingTreeBuilder() {};
		bool ParseDDL(const FString &filepath, TArray<FString> &output);

		bool BuildDecisionTree(const ModumateObjectDatabase &db, const FString &rootName, TDecisionTree<FCraftingItem> &outTree, TArray<FString> &output);

		bool RecurseBuildFormNode(const ModumateObjectDatabase &db, const FForm &form, TDecisionTreeNode<FCraftingItem> &node, TArray<FString> &errors);
		bool RecurseBuildListItemNode(const ModumateObjectDatabase &db, const FListElement &listElem, const FForm &elemType, TDecisionTreeNode<FCraftingItem> &node, TArray<FString> &errors);
		bool FillTableSelect(const ModumateObjectDatabase &db, TDecisionTreeNode<FCraftingItem> &node, TArray<FString> &errors);

		void BindUserString(TDecisionTreeNode<FCraftingItem> &node, const FFormElement &el) const;
		void BindFixedValue(TDecisionTreeNode<FCraftingItem> &node, const BIM::FValueSpec &b) const;
	};
}

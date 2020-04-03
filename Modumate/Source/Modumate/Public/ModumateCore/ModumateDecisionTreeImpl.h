// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "ModumateDecisionTree.h"
#include "ModumateDataTables.h"


namespace Modumate
{

	/*
	Node Finders
	*/
	template<class NodeType>
	const TDecisionTreeNode<NodeType> *TDecisionTreeNode<NodeType>::FindNodeConstLambda(std::function<bool(const NodeType &n)> isIt) const
	{
		if (isIt(Data))
		{
			return this;
		}
		TArray<const TDecisionTreeNode<NodeType>*> nodeStack;
		nodeStack.Push(this);
		while (nodeStack.Num() > 0)
		{
			const TDecisionTreeNode<NodeType> *node = nodeStack.Pop();
			if (isIt(node->Data))
			{
				return node;
			}
			for (auto &n : node->Subfields)
			{
				nodeStack.Push(&n);
			}
		}
		return nullptr;
	}

	template<class NodeType>
	TDecisionTreeNode<NodeType> *TDecisionTreeNode<NodeType>::FindNodeLambdaNonConst(std::function<bool(const NodeType &n)> isIt)
	{
		return const_cast<TDecisionTreeNode<NodeType> *>(FindNodeConstLambda(isIt));
	}

	template<class NodeType>
	const TDecisionTreeNode<NodeType> *TDecisionTreeNode<NodeType>::FindNode(std::function<bool(const NodeType &n)> isIt) const
	{
		return FindNodeConstLambda(isIt);
	}

	template<class NodeType>
	TDecisionTreeNode<NodeType> *TDecisionTreeNode<NodeType>::FindNode(std::function<bool(const NodeType &n)> isIt)
	{
		return FindNodeLambdaNonConst(isIt);
	}


	template<class NodeType>
	TDecisionTreeNode<NodeType> *TDecisionTreeNode<NodeType>::FindNodeByGUID(const FName &guid)
	{
		return const_cast<TDecisionTreeNode<NodeType> *>(FindNodeConstLambda([guid](const NodeType &n) {return n.GUID == guid; }));
	}

	template<class NodeType>
	const TDecisionTreeNode<NodeType> *TDecisionTreeNode<NodeType>::FindNodeByGUID(const FName &guid) const
	{
		return FindNodeConstLambda([guid](const NodeType &n) {return n.GUID == guid; });
	}


	/*
	Data Readers
	*/

	template<class T, class O, class OS>
	void ReadOptionSet(
		UDataTable *data,
		std::function<bool(const T &row, O &ot)> readOptionData,
		std::function<void(const OS &o)> addOptionSet
	)
	{
		if (!ensureAlways(data))
		{
			return;
		}

		OS optionSet;
		TArray<FString> supportedSubcategories;

		data->ForeachRow<T>(TEXT("OPTIONSET"),
			[&optionSet, &supportedSubcategories, readOptionData, addOptionSet](const FName &Key, const T &row)
		{
			if (row.SupportedSubcategories.Num() > 0)
			{
				if (optionSet.Options.Num() > 0)
				{
					for (auto &sc : supportedSubcategories)
					{
						optionSet.Key = *sc;
						addOptionSet(optionSet);
					}
				}
				optionSet.Options.Empty();
				supportedSubcategories = row.SupportedSubcategories;
			}

			O option;
			option.Key = Key;
			if (readOptionData(row, option))
			{
				optionSet.Options.Add(option);
			}
		});

		if (optionSet.Options.Num() > 0)
		{
			for (auto &sc : supportedSubcategories)
			{
				optionSet.Key = *sc;
				addOptionSet(optionSet);
			}
		}
	}

	template<class NodeType>
	TDecisionTree<NodeType> TDecisionTree<NodeType>::FromDataTable(UDataTable *data)
	{
		TDecisionTree<NodeType> ret;
		ret.RootNode = TDecisionTreeNode<NodeType>::FromDataTable(data);
		ret.ProcessDynamicLists();
		return ret;
	}

#define DTN_SETROWVAL(a) if (data.C##a != TEXT("")) { row[a] = data.C##a; keys.Add(a); }

	template<class NodeType>
	TDecisionTreeNode<NodeType> TDecisionTreeNode<NodeType>::FromDataTable(UDataTable *data)
	{
		ensureAlways(data != nullptr);
		if (data == nullptr)
		{
			return TDecisionTreeNode<NodeType>();
		}

		auto typeFromString = [](const FString &s)
		{
			if (s == TEXT("Form")) return EDecisionType::Form;
			if (s == TEXT("TableSelect")) return EDecisionType::TableSelect;
			if (s == TEXT("Select")) return EDecisionType::Select;
			if (s == TEXT("YesNo")) return EDecisionType::YesNo;
			if (s == TEXT("DynamicList")) return EDecisionType::DynamicList;
			if (s == TEXT("Private")) return EDecisionType::Private;
			if (s == TEXT("String")) return EDecisionType::String;
			if (s == TEXT("Dimension")) return EDecisionType::Dimension;
			if (s == TEXT("Derived")) return EDecisionType::Derived;
			return EDecisionType::None;
		};

		const FName rootGUID = TEXT("ROOT");

		TDecisionTreeNode<NodeType> ret;
		ret.Data.Label = FText::FromName(rootGUID);
		ret.Data.Type = EDecisionType::Form;
		ret.Data.Value.Empty();
		ret.Data.GUID = rootGUID;

		int32 lastLevel = 0;
		FName lastNodeAtLevel[9];
		lastNodeAtLevel[0] = ret.Data.GUID;

		TDecisionTreeNode<NodeType> *parentNode = &ret;
		TMap<FName, TDecisionTreeNode<NodeType> *> nodeMap;
		nodeMap.Add(parentNode->Data.GUID, parentNode);

		data->ForeachRow<FDecisionTreeDataTable>(TEXT("FDecisionTreeDataTable"),
			[&ret,rootGUID,&lastNodeAtLevel,&nodeMap,&lastLevel,&parentNode,typeFromString](
				const FName &Key,
				const FDecisionTreeDataTable &data)
		{
			FString row[10];
			TArray<int32> keys;

			// The keys for a new node are the columns of the spreadsheet that have entries
			DTN_SETROWVAL(0);
			DTN_SETROWVAL(1);
			DTN_SETROWVAL(2);
			DTN_SETROWVAL(3);
			DTN_SETROWVAL(4);
			DTN_SETROWVAL(5);
			DTN_SETROWVAL(6);
			DTN_SETROWVAL(7);
			DTN_SETROWVAL(8);
			DTN_SETROWVAL(9);

			// Skip empty rows
			if (keys.Num() == 0)
			{
				return;
			}

			// Five is right out...
			ensureAlways(keys.Num() <= 3);

			TDecisionTreeNode<NodeType> newNode;

			const FString &label = row[keys[0]];
			newNode.Data.Label = FText::FromString(label);
			newNode.Data.Key = label;

			newNode.Data.GUID = Key;
			newNode.Data.Value.Empty();

			// If a row only has one entry, it's a simple terminal value (example: data used by TableSelect or Private)
			if (keys.Num() == 1)
			{
				newNode.Data.Type = EDecisionType::Terminal;
			}
			else
			{
				// The first entry is the label, the second entry is the data type
				FString *type = &row[keys[1]];
				ensureAlways(type != nullptr);
				newNode.Data.Type = typeFromString(*type);

				// If we have three entries, the third is a 'style' setting
				if (keys.Num() >= 3)
				{
					if (newNode.Data.Type == EDecisionType::Select ||
						newNode.Data.Type == EDecisionType::DynamicList ||
						newNode.Data.Type == EDecisionType::TableSelect)
					{
						FString *selectStyle = &row[keys[2]];
						if (ensureAlways(selectStyle != nullptr))
						{
							if (selectStyle->Equals(TEXT("Icons")))
							{
								newNode.Data.SelectStyle = EDecisionSelectStyle::Icons;
							}
							else if (ensureAlways(selectStyle->Equals(TEXT("Dropdown"))))
							{
								newNode.Data.SelectStyle = EDecisionSelectStyle::Dropdown;
							}
						}
					}
					else if (newNode.Data.Type == EDecisionType::Form)
					{
						FString *formStyle = &row[keys[2]];
						if (ensureAlways(formStyle != nullptr && formStyle->Equals(TEXT("Preset"))))
						{
							newNode.Data.HasPreset = true;
						}
					}
				}
			}

			// A new row should either be at the same level as the previous (a sibling), one greater (a child) or some number less (a new aunt or uncle)
			ensureAlways(keys[0] == lastLevel || keys[0] == lastLevel + 1 || keys[0] < lastLevel);
			lastLevel = keys[0];

			// We are now the most recent 'parent' at this level
			lastNodeAtLevel[keys[0]] = Key;

			// If we are not at level 0 (making our parent ROOT), look up our parent, who is the last node encountered at the level one before us
			if (keys[0] > 0)
			{
				newNode.Data.ParentGUID = lastNodeAtLevel[keys[0] - 1];
				if (parentNode->Data.GUID != newNode.Data.ParentGUID)
				{
					parentNode = nodeMap[newNode.Data.ParentGUID];
				}
			}
			// If we are at level 0, reparent to the root of the tree
			else
			{
				newNode.Data.ParentGUID = rootGUID;
				parentNode = &ret;
			}
			if (ensureAlways(parentNode != nullptr))
			{
				// The Subfields.Add operation can invalidate pointers stored in nodeMap
				// So we eliminate all our own children from the map before extending the nodeMap
				// If we end up needing our previous children to be in the map (we shouldn't) then we
				// can re-add parentNode->Subfields to the nodeMap after the add

				for (auto &sf : parentNode->Subfields)
				{
					nodeMap.Remove(sf.Data.GUID);
				}
				parentNode->Subfields.Add(newNode);
				nodeMap.Add(newNode.Data.GUID, &parentNode->Subfields.Last());
			}
		});
		return ret;
	}

	template<class NodeType>
	void TDecisionTree<NodeType>::ProcessDynamicLists()
	{
		// Collect GUIDs because modifying node subfields can move other nodes in memory
		TArray<FName> dynamicGUIDs;
		RootNode.WalkDepthFirst([&dynamicGUIDs](TDecisionTreeNode<NodeType> &tr)
		{
			if (tr.Data.Type == EDecisionType::DynamicList)
			{
				dynamicGUIDs.Add(tr.Data.GUID);
			}
			return true;
		});

		for (auto &dg : dynamicGUIDs)
		{
			TDecisionTreeNode<NodeType> *node = RootNode.FindNodeByGUID(dg);
			if (ensureAlways(node != nullptr))
			{
				// If I only have one child, that's my template
				if (node->Subfields.Num() == 1)
				{
					TDecisionTreeNode<NodeType> selectNode = *node;
					selectNode.Data.Type = EDecisionType::Form;
					selectNode.Reset();
					DynamicListElementTypes.Add(node->Data.GUID, selectNode);
				}
				// Otherwise I have a group of possibilities that should be organized under a single select
				// Note: the synthetic select node will have the same GUID as the original dynamic list node
				// This should not be a problem because child instances and templates are stored outside of the main tree
				else
				{
					TDecisionTreeNode<NodeType> selectNode = *node;
					selectNode.Data.Type = EDecisionType::Select;
					selectNode.Reset();
					DynamicListElementTypes.Add(node->Data.GUID, selectNode);
				}

				node->Subfields.Empty();
			}
		}
	}

	/*
	Field Manipulators
	*/

	template<class NodeType>
	void TDecisionTreeNode<NodeType>::SetStringValueByGUID(const FName &guid, const FString &value)
	{
		TDecisionTreeNode<NodeType> *node = FindNodeByGUID(guid);
		if (node != nullptr)
		{
			node->Data.Value = value;
			node->Data.UpdateMetaValue();
			if (node->Data.Type == EDecisionType::Select)
			{
				for (auto &sf : node->Subfields)
				{
					if (sf.Data.GUID.IsEqual(*value))
					{
						node->Data.AssimilateSubfieldValues(sf.Data);
						return;
					}
				}
				ensureAlways(false);
			}
		}
	}

	template<class NodeType>
	void TDecisionTreeNode<NodeType>::SetNameValueByGUID(const FName &guid, const FName &value)
	{
		TDecisionTreeNode<NodeType> *node = FindNodeByGUID(guid);
		if (node != nullptr)
		{
			node->Data.Value = value.IsNone() ? TEXT("") : value.ToString();
			node->Data.UpdateMetaValue();
			if (node->Data.Type == EDecisionType::Select)
			{
				for (auto &sf : node->Subfields)
				{
					if (value.IsEqual(sf.Data.GUID))
					{
						node->Data.AssimilateSubfieldValues(sf.Data);
						return;
					}
				}
				ensureAlways(false);
			}
		}
	}

	template<class NodeType>
	bool TDecisionTreeNode<NodeType>::SetSelectedSubfield(int32 i)
	{
		if (Data.Type != EDecisionType::Select && Data.Type != EDecisionType::Private)
		{
			return false;
		}
		Data.AssimilateSubfieldValues(Subfields[i].Data);
		return true;
	}

	template<class NodeType>
	const TDecisionTreeNode<NodeType> *TDecisionTreeNode<NodeType>::GetSelectedSubfieldConst() const
	{
		if (Data.Type != EDecisionType::Select && Data.Type != EDecisionType::Private)
		{
			return nullptr;
		}
		for (auto &sf : Subfields)
		{
			if (sf.Data.GUID == Data.SelectedSubnodeGUID)
			{
				return &sf;
			}
		}
		return nullptr;
	}

	template<class NodeType>
	TDecisionTreeNode<NodeType> *TDecisionTreeNode<NodeType>::GetSelectedSubfield()
	{
		return const_cast<TDecisionTreeNode<NodeType>*>(GetSelectedSubfieldConst());
	}

	template<class NodeType>
	const TDecisionTreeNode<NodeType> *TDecisionTreeNode<NodeType>::GetSelectedSubfield() const
	{
		return GetSelectedSubfieldConst();
	}

	/*
	Tree Walkers
	*/

	template<class NodeType>
	TArray<NodeType> TDecisionTreeNode<NodeType>::GetOpenNodes() const
	{
		TArray<const TDecisionTreeNode<NodeType>*> openStack;
		openStack.Push(this);

		TArray<NodeType> returnList;

		while (openStack.Num() > 0)
		{
			const TDecisionTreeNode<NodeType> *pTree = openStack.Pop();
			const TDecisionTreeNode<NodeType> *pParent = FindNodeByGUID(pTree->Data.ParentGUID);

			returnList.Add(pTree->Data);

			// If we are the child of a SELECT group, only open our child nodes if we are selected
			if (pParent != nullptr &&
				pParent->Data.Type == EDecisionType::Select &&
				pTree->Data.GUID != pParent->Data.SelectedSubnodeGUID)
			{
				continue;
			}

			for (int32 i = pTree->Subfields.Num() - 1; i >= 0; --i)
			{
				auto &sf = pTree->Subfields[i];
				if (sf.Data.Type != EDecisionType::Private)
				{
					openStack.Push(&sf);
				}
			}
		}
		return returnList;
	}

	template<class NodeType>
	void TDecisionTreeNode<NodeType>::WalkDepthFirst(std::function<bool(TDecisionTreeNode<NodeType> &tr)> fn)
	{
		TArray<TDecisionTreeNode<NodeType>*> nodeStack;
		nodeStack.Push(this);
		while (nodeStack.Num() > 0)
		{
			TDecisionTreeNode<NodeType> *node = nodeStack.Pop();
			if (!fn(*node))
			{
				return;
			}
			for (auto &sf : node->Subfields)
			{
				nodeStack.Push(&sf);
			}
		}
	}

	template<class NodeType>
	void TDecisionTreeNode<NodeType>::WalkDepthFirst(std::function<bool(const TDecisionTreeNode<NodeType> &tr)> fn) const
	{
		TArray<const TDecisionTreeNode<NodeType>*> nodeStack;
		nodeStack.Push(this);
		while (nodeStack.Num() > 0)
		{
			const TDecisionTreeNode<NodeType> *node = nodeStack.Pop();
			if (!fn(*node))
			{
				return;
			}
			for (auto &sf : node->Subfields)
			{
				nodeStack.Push(&sf);
			}
		}
	}

	template <class NodeType>
	void TDecisionTreeNode<NodeType>::GetTrace(std::function<void(const TDecisionTreeNode<NodeType> &)> fn) const
	{
		TArray<const TDecisionTreeNode<NodeType>*> nodeStack;
		nodeStack.Push(this);
		while (nodeStack.Num() > 0)
		{
			const TDecisionTreeNode<NodeType> *node = nodeStack.Pop();

			if (node->Data.Type == EDecisionType::Select || node->Data.Type == EDecisionType::Private)
			{
				fn(*node);
				const TDecisionTreeNode<NodeType> *selected = node->GetSelectedSubfield();
				if (ensureAlways(selected != nullptr))
				{
					nodeStack.Push(selected);
				}
			}
			else
			{
				fn(*node);
				for (auto &sf : node->Subfields)
				{
					nodeStack.Push(&sf);
				}
			}
		}
	}

	/*
	Tree Builders
	*/

	template<class NodeType>
	const TDecisionTreeNode<NodeType> *TDecisionTree<NodeType>::MakeDynamicListElementAt(const FName &dlistGUID, int32 i)
	{
		TDecisionTreeNode<NodeType> *templ = DynamicListElementTypes.Find(dlistGUID);
		if (ensureAlways(templ != nullptr))
		{
			FDecisionNodeArray &elems = DynamicListElementInstances.FindOrAdd(dlistGUID);

			TDecisionTreeNode<NodeType> newNode = *templ;
			newNode.WalkDepthFirst([](TDecisionTreeNode<NodeType> &node)
			{
				if (node.Subfields.Num() == 0)
				{
					return true;
				}
				if (node.Data.Type == EDecisionType::Select || node.Data.Type == EDecisionType::Private)
				{
					if (ensureAlways(node.Subfields.Num() > 0))
					{
						node.SetSelectedSubfield(0);
					}
				}
				return true;
			});

			if (i != -1)
			{
				if (ensureAlways(i <= elems.Num()))
				{
					elems.Insert(newNode, i);
					return &elems[i];
				}
			}
			else
			{
				elems.Add(newNode);
				return &elems.Last();
			}
		}
		return nullptr;
	}

	template<class NodeType>
	void TDecisionTreeNode<NodeType>::Reset()
	{
		Data.Value.Empty();

		for (auto &sf : Subfields)
		{
			sf.Reset();
		}
		if (Data.Type == EDecisionType::Select || Data.Type == EDecisionType::Private)
		{
			if (ensureAlways(Subfields.Num() > 0))
			{
				SetSelectedSubfield(0);
			}
		}
	}

	template<class NodeType>
	void TDecisionTree<NodeType>::Reset()
	{
		RootNode.Reset();
		DynamicListElementInstances.Empty();
		for (auto &kvp : DynamicListElementTypes)
		{
			MakeDynamicListElementAt(kvp.Key,0);
		}
	}

	template<class NodeType>
	bool TDecisionTreeNode<NodeType>::ValidateData()
	{
		TSet<FName> guids;

		WalkDepthFirst([&guids](TDecisionTreeNode<NodeType> &tr)
		{
			ensureAlways(!guids.Contains(tr.Data.GUID));
			ensureAlways(tr.Data.ParentGUID == TEXT("") || guids.Contains(tr.Data.ParentGUID));
			guids.Add(tr.Data.GUID);
			return true;
		});
		return true;
	}

	/*
	Dynamic Lists
	*/

	template<class NodeType>
	TDecisionTreeNode<NodeType> *TDecisionTree<NodeType>::FindDynamicListInstanceNode(const FName &listGUID, int32 index, const FName &nodeGUID)
	{
		if (listGUID == NAME_None)
		{
			return FindNodeByGUID(nodeGUID);
		}

		FDecisionNodeArray *elems = DynamicListElementInstances.Find(listGUID);
		if (elems != nullptr && ensureAlways(index < elems->Num()))
		{
			return (*elems)[index].FindNodeByGUID(nodeGUID);
		}
		return nullptr;
	}

	template<class NodeType>
	const TDecisionTreeNode<NodeType> *TDecisionTree<NodeType>::FindDynamicListInstanceNode(const FName &listGUID, int32 index, const FName &nodeGUID) const
	{
		if (listGUID == NAME_None)
		{
			return FindNodeByGUID(nodeGUID);
		}

		const FDecisionNodeArray *elems = DynamicListElementInstances.Find(listGUID);
		if (elems != nullptr && ensureAlways(index < elems->Num()))
		{
			return (*elems)[index].FindNodeByGUID(nodeGUID);
		}
		return nullptr;
	}

	template<class NodeType>
	TDecisionTreeNode<NodeType> *TDecisionTree<NodeType>::FindDynamicListTypeNode(const FName &listGUID, const FName &nodeGUID)
	{
		TDecisionTreeNode<NodeType> *elems = DynamicListElementTypes.Find(listGUID);
		if (elems != nullptr)
		{
			return elems->FindNodeByGUID(nodeGUID);
		}
		return nullptr;
	}

	template<class NodeType>
	const TDecisionTreeNode<NodeType> *TDecisionTree<NodeType>::FindDynamicListTypeNode(const FName &listGUID, const FName &nodeGUID) const
	{
		const TDecisionTreeNode<NodeType> *elems = DynamicListElementTypes.Find(listGUID);
		if (elems != nullptr)
		{
			return elems->FindNodeByGUID(nodeGUID);
		}
		return nullptr;
	}

	template<class NodeType>
	TDecisionTreeNode<NodeType> *TDecisionTree<NodeType>::FindNodeByGUID(const FName &guid)
	{
		return RootNode.FindNodeByGUID(guid);
	}

	template<class NodeType>
	const TDecisionTreeNode<NodeType> *TDecisionTree<NodeType>::FindNodeByGUID(const FName &guid) const
	{
		return RootNode.FindNodeByGUID(guid);
	}

	template<class NodeType>
	void TDecisionTree<NodeType>::SetStringValueByGUID(const FName &guid, const FString &value)
	{
		return RootNode.SetStringValueByGUID(guid, value);
	}

	template<class NodeType>
	void TDecisionTree<NodeType>::SetNameValueByGUID(const FName &guid, const FName &value)
	{
		return RootNode.SetNameValueByGUID(guid, value);
	}

	template<class NodeType>
	void TDecisionTree<NodeType>::SetDynamicListElementStringValueByGUID(
		const FName &dlist,
		int32 index,
		const FName &guid,
		const FString &value)
	{
		FDecisionNodeArray *elems = DynamicListElementInstances.Find(dlist);
		if (ensureAlways(elems != nullptr) && ensureAlways(index < elems->Num()))
		{
			(*elems)[index].SetStringValueByGUID(guid,value);
		}
	}

	template<class NodeType>
	void TDecisionTree<NodeType>::SetDynamicListElementNameValueByGUID(
		const FName &dlist,
		int32 index,
		const FName &guid,
		const FName &value)
	{
		FDecisionNodeArray *elems = DynamicListElementInstances.Find(dlist);
		if (ensureAlways(elems != nullptr) && ensureAlways(index < elems->Num()))
		{
			(*elems)[index].SetNameValueByGUID(guid, value);
		}
	}

}
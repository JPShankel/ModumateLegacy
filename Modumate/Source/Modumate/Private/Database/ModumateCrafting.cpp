// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "Database/ModumateCrafting.h"
#include "Database/ModumateObjectDatabase.h"
#include "Database/ModumateArchitecturalMaterial.h"
#include "ModumateCore/ModumateScriptProcessor.h"
#include "ModumateCore/ModumateDecisionTreeImpl.h"
#include "DocumentManagement/ModumateCommands.h"
#include "Algo/Accumulate.h"
#include "Algo/Transform.h"
#include "Algo/Compare.h"

namespace Modumate {
	namespace BIM {
#if WITH_EDITOR // for debugging new/delete balance
		int32 FCraftingTreeNodeInstance::InstanceCount = 0;
#endif
		FCraftingTreeNodeInstance::FCraftingTreeNodeInstance(int32 instanceID) : InstanceID(instanceID) 
		{
#if WITH_EDITOR 
			++InstanceCount; 
#endif
		}

		int32 FCraftingTreeNodeInstance::GetInstanceID() const
		{
			return InstanceID;
		}

		ECraftingResult FCraftingTreeNodeInstance::GatherAllChildNodes(TArray<FCraftingTreeNodeInstanceSharedPtr> &OutChildren)
		{
			TArray<FCraftingTreeNodeInstanceSharedPtr> nodeStack;
			nodeStack.Push(AsShared());
			do
			{
				FCraftingTreeNodeInstanceSharedPtr currentNode = nodeStack.Pop();
				for (auto &pin : currentNode->InputPins)
				{
					for (auto &ao : pin.AttachedObjects)
					{
						if (ao != nullptr)
						{
							OutChildren.Add(ao.Pin());
							nodeStack.Push(ao.Pin());
						}
					}
				}
			} while (nodeStack.Num() > 0);

			return ECraftingResult::Success;
		}

		// The preset on a crafting node is 'dirty' if the node's properties or input pins is inconsistent with the values in its preset
		ECraftingNodePresetStatus FCraftingTreeNodeInstance::GetPresetStatus(const FCraftingPresetCollection &PresetCollection) const
		{
			const FCraftingTreeNodePreset *preset = PresetCollection.Presets.Find(PresetID);
			if (preset == nullptr)
			{
				return ECraftingNodePresetStatus::None;
			}

			// "read only" presets are top level category/subcategory pickers who don't need to save their state
			if (preset->IsReadOnly)
			{
				return ECraftingNodePresetStatus::ReadOnly;
			}

			FCraftingTreeNodePreset instanceAsPreset;
			PresetCollection.GetInstanceDataAsPreset(AsShared(), instanceAsPreset);

			if (preset->Matches(instanceAsPreset))
			{
				return ECraftingNodePresetStatus::UpToDate;
			}
			return ECraftingNodePresetStatus::Dirty;
		}

		ECraftingResult FCraftingTreeNodeInstancePool::ResetInstances()
		{
			InstancePool.Empty();
			InstanceMap.Empty();
			NextInstanceID = 1;
			if (!ValidatePool())
			{
				return ECraftingResult::Error;
			}
			return ECraftingResult::Success;
		}

		FCraftingTreeNodeInstanceSharedPtr FCraftingTreeNodeInstancePool::FindInstanceByPredicate(const std::function<bool(const FCraftingTreeNodeInstanceSharedPtr &Instance)> &Predicate)
		{
			FCraftingTreeNodeInstanceSharedPtr *ret = InstancePool.FindByPredicate(Predicate);
			if (ret != nullptr)
			{
				return *ret;
			}
			return nullptr;
		}


		const FCraftingTreeNodeInstanceSharedPtr FCraftingTreeNodeInstancePool::InstanceFromID(int32 InstanceID) const
		{
			const FCraftingTreeNodeInstanceWeakPtr *instancePtr = InstanceMap.Find(InstanceID);
			if (instancePtr != nullptr && instancePtr->IsValid())
			{
				return instancePtr->Pin();
			}
			return FCraftingTreeNodeInstanceSharedPtr();
		}

		FCraftingTreeNodeInstanceSharedPtr FCraftingTreeNodeInstancePool::InstanceFromID(int32 InstanceID)
		{
			const FCraftingTreeNodeInstanceWeakPtr *instancePtr = InstanceMap.Find(InstanceID);
			if (instancePtr != nullptr && instancePtr->IsValid())
			{
				return instancePtr->Pin();
			}
			return FCraftingTreeNodeInstanceSharedPtr();
		}

		ECraftingResult FCraftingTreeNodeInstancePool::PresetToSpec(const FName &PresetID, const FCraftingPresetCollection &PresetCollection, FModumateAssemblyPropertySpec &OutAssemblySpec) const
		{
			const BIM::FCraftingTreeNodeInstanceSharedPtr *presetNode = GetInstancePool().FindByPredicate(
				[PresetID](const BIM::FCraftingTreeNodeInstanceSharedPtr &Instance)
				{
					return Instance->PresetID == PresetID;
				}
			);

			if (!ensureAlways(presetNode != nullptr))
			{
				return ECraftingResult::Error;
			}

			OutAssemblySpec.RootPreset = PresetID;
			BIM::FBIMPropertySheet *currentSheet = &OutAssemblySpec.RootProperties;

			TArray<BIM::FCraftingTreeNodeInstanceSharedPtr> nodeStack;
			nodeStack.Push(*presetNode);

			TArray<BIM::EScope> scopeStack;
			scopeStack.Push(BIM::EScope::Assembly);

			while (!nodeStack.Num() == 0)
			{
				BIM::FCraftingTreeNodeInstanceSharedPtr inst = nodeStack.Pop();
				BIM::EScope pinScope = scopeStack.Pop();

				// Nodes that have a layer function constitute a new layer
				if (inst->InstanceProperties.HasProperty(BIM::EScope::Layer, BIM::Parameters::Function))
				{
					OutAssemblySpec.LayerProperties.AddDefaulted();
					currentSheet = &OutAssemblySpec.LayerProperties.Last();
				}

				EObjectType objectType = PresetCollection.GetPresetObjectType(inst->PresetID);
				if (objectType != EObjectType::OTNone && ensureAlways(OutAssemblySpec.ObjectType == EObjectType::OTNone))
				{
					OutAssemblySpec.ObjectType = objectType;
				}

				inst->InstanceProperties.ForEachProperty([&OutAssemblySpec, &currentSheet, &inst, pinScope, PresetID](const FString &Name, const Modumate::FModumateCommandParameter &Param)
				{
					BIM::FValueSpec vs(*Name);

					// 'Node' scope values inherit their scope from their parents, specified on the pin
					BIM::EScope nodeScope = vs.Scope == BIM::EScope::Node ? pinScope : vs.Scope;

					// Preset scope properties only apply to the root node in the tree
					if (nodeScope != BIM::EScope::Preset || inst->PresetID == PresetID)
					{
						currentSheet->SetProperty(nodeScope, vs.Name, Param);
					}
				});

				for (auto &inputPin : inst->InputPins)
				{
					for (auto &ao : inputPin.AttachedObjects)
					{
						nodeStack.Push(ao.Pin());
						scopeStack.Push(inputPin.Scope);
					}
				}
			}

			// All assembly specs must bind to an object type
			if (OutAssemblySpec.ObjectType == EObjectType::OTNone)
			{
				return ECraftingResult::Error;
			}

			return ECraftingResult::Success;
		}

		bool FCraftingTreeNodeInstance::CanRemoveChild(const FCraftingTreeNodeInstanceSharedPtrConst &Child) const
		{
			for (auto &pin : InputPins)
			{
				if (pin.AttachedObjects.Contains(Child))
				{
					if (pin.AttachedObjects.Num() <= pin.MinCount)
					{
						return false;
					}
					return true;
				}
			}
			return false;
		}

		ECraftingResult FCraftingTreeNodeInstance::FindChildOrder(int32 ChildID, int32 &Order)
		{
			int32 pinSetIndex=-1, pinSetPosition=-1;
			ECraftingResult result = FindChild(ChildID, pinSetIndex, pinSetPosition);
			if (result == ECraftingResult::Success)
			{
				Order = 0;
				for (int32 i = 0; i < pinSetIndex - 1; ++i)
				{
					Order += InputPins[i].AttachedObjects.Num();
				}
				Order += pinSetPosition;
			}
			return result;
		}

		ECraftingResult FCraftingTreeNodeInstance::FindChild(int32 ChildID, int32 &OutPinSetIndex, int32 &OutPinSetPosition)
		{
			for (int32 pinIndex = 0; pinIndex < InputPins.Num(); ++pinIndex)
			{
				FCraftingTreeNodePinSet &pinSet = InputPins[pinIndex];
				for (int32 pinPosition = 0; pinPosition < pinSet.AttachedObjects.Num(); ++pinPosition)
				{
					if (pinSet.AttachedObjects[pinPosition].Pin()->GetInstanceID() == ChildID)
					{
						OutPinSetIndex = pinIndex;
						OutPinSetPosition = pinPosition;
						return ECraftingResult::Success;
					}
				}
			}
			return ECraftingResult::Error;
		}

		ECraftingResult FCraftingTreeNodeInstance::DetachSelfFromParent()
		{
			if (ParentInstance != nullptr)
			{
				for (auto &pin : ParentInstance.Pin()->InputPins)
				{
					for (auto ao : pin.AttachedObjects)
					{
						if (ao.HasSameObject(this))
						{
							pin.AttachedObjects.Remove(ao);
							break;
						}
					}
				}
			}
			ParentInstance = nullptr;
			return ECraftingResult::Success;
		}

		ECraftingResult FCraftingTreeNodeInstance::AttachChild(const FCraftingPresetCollection &PresetCollection, const FCraftingTreeNodeInstanceSharedPtr &Child)
		{
			const FCraftingTreeNodePreset *childPreset = PresetCollection.Presets.Find(Child->PresetID);
			if (childPreset == nullptr)
			{
				return ECraftingResult::Error;
			}

			auto pinSupportsPreset = [&Child, &PresetCollection](const FCraftingTreeNodePinSet &PinSet)
			{
				TArray<FName> supportedNodes;
				PresetCollection.SearchPresets(PinSet.EligiblePresetSearch.NodeType, PinSet.EligiblePresetSearch.SelectionSearchTags, TArray<FCraftingPresetTag>(), supportedNodes);
				return (supportedNodes.Contains(Child->PresetID));
			};

			for (auto &pin : InputPins)
			{
				if ((pin.MaxCount == -1 || pin.AttachedObjects.Num() < pin.MaxCount) && pinSupportsPreset(pin))
				{
					// Should never be present, so ensure instead of AddUnique
					if (ensureAlways(!pin.AttachedObjects.Contains(Child)))
					{ 
						pin.AttachedObjects.Add(Child);
						Child->ParentInstance = AsShared();
						if (Child->CurrentOrientation == EConfiguratorNodeIconOrientation::Inherited)
						{
							Child->CurrentOrientation = CurrentOrientation;
						}
						return ECraftingResult::Success;
					}
					else
					{
						return ECraftingResult::Error;
					}
				}
			}

			return ECraftingResult::Error;
		}

		ECraftingResult FCraftingTreeNodeInstance::ToDataRecord(FCustomAssemblyCraftingNodeRecord &OutRecord) const
		{
			OutRecord.InstanceID = InstanceID;
			OutRecord.PresetID = PresetID;

			if (ParentInstance.IsValid())
			{
				FCraftingTreeNodeInstanceSharedPtr sharedParent = ParentInstance.Pin();
				OutRecord.ParentID = sharedParent->GetInstanceID();
				ensureAlways(sharedParent->FindChild(InstanceID, OutRecord.PinSetIndex, OutRecord.PinSetPosition) != ECraftingResult::Error);
			}
			else
			{
				OutRecord.PinSetIndex = 0;
				OutRecord.PinSetPosition = 0;
				OutRecord.ParentID = 0;
			}

			InstanceProperties.ToDataRecord(OutRecord.PropertyRecord);

			return ECraftingResult::Success;
		}

		ECraftingResult FCraftingTreeNodeInstance::FromDataRecord(
			FCraftingTreeNodeInstancePool &InstancePool, 
			const FCraftingPresetCollection &PresetCollection, 
			const FCustomAssemblyCraftingNodeRecord &DataRecord,
			bool RecursePresets)
		{
			const FCraftingTreeNodePreset *preset = PresetCollection.Presets.Find(DataRecord.PresetID);
			if (preset == nullptr)
			{
				return ECraftingResult::Error;
			}

			const FCraftingTreeNodeType *descriptor = PresetCollection.NodeDescriptors.Find(preset->NodeType);
			if (descriptor == nullptr)
			{
				return ECraftingResult::Error;
			}

			// DestroyNodeInstance removes grand children, so we don't need to recurse
			for (auto &pin : InputPins)
			{
				while (pin.AttachedObjects.Num() > 0)
				{
					TArray<int32> destroyedChildren;
					InstancePool.DestroyNodeInstance(pin.AttachedObjects[0].Pin()->GetInstanceID(), destroyedChildren);
				}
			}

			PresetID = DataRecord.PresetID;
			InstanceProperties.FromDataRecord(DataRecord.PropertyRecord);

			InputPins = descriptor->InputPins;
			for (auto &ip : preset->ChildNodes)
			{
				if (ensureAlways(ip.PinSetIndex < InputPins.Num() && ip.PresetSequence.Num() != 0))
				{
					// The first designated list in a sequence is assigned to the top of the pinset, not the last
					// This ensures that categories will be chosen from a list of categories, not the user preset at the end of the sequence
					FCraftingTreeNodePinSet &pinSet = InputPins[ip.PinSetIndex];
					pinSet.EligiblePresetSearch.SelectionSearchTags = ip.PinSpecSearchTags;
				}
			}

			if (!RecursePresets)
			{
				FCraftingTreeNodeInstanceSharedPtr parent = InstancePool.InstanceFromID(DataRecord.ParentID);
				if (parent.IsValid())
				{
					parent->AttachChild(PresetCollection, AsShared());
				}
			}
			else
			{
				for (auto &ip : preset->ChildNodes)
				{
					if (ensureAlways(ip.PinSetIndex < InputPins.Num()))
					{
						FCraftingTreeNodePinSet &pinSet = InputPins[ip.PinSetIndex];
						if (ip.PinSetPosition == pinSet.AttachedObjects.Num())
						{
							// Create nodes for every member of the sequence
							int32 sequenceID = DataRecord.InstanceID;
							FCraftingTreeNodeInstanceSharedPtr child;
							for (int32 i=0;i < ip.PresetSequence.Num();++i)
							{
								// Note: only create default children (if necessary) for the last item in the list, the other items are children of each other
								child = InstancePool.CreateNodeInstanceFromPreset(PresetCollection, sequenceID, ip.PresetSequence[i].SelectedPresetID, i == ip.PresetSequence.Num()-1);
								if (ensureAlways(child.IsValid()))
								{
									sequenceID = child->GetInstanceID();
								}
							}
						}
						else
						{
							InstancePool.SetNewPresetForNode(PresetCollection, pinSet.AttachedObjects[ip.PinSetPosition].Pin()->GetInstanceID(), ip.PresetSequence.Last().SelectedPresetID);
						}
					}
				}
			}
			return ECraftingResult::Success;
		}

		ECraftingResult FCraftingPresetCollection::GetInstanceDataAsPreset(const FCraftingTreeNodeInstanceSharedPtrConst &Instance, FCraftingTreeNodePreset &OutPreset) const
		{
			const FCraftingTreeNodePreset *basePreset = Presets.Find(Instance->PresetID);
			if (basePreset == nullptr)
			{
				return ECraftingResult::Error;
			}

			OutPreset.NodeType = basePreset->NodeType;
			OutPreset.Properties = Instance->InstanceProperties;
			OutPreset.PresetID = basePreset->PresetID;
			OutPreset.Tags = basePreset->Tags;

			for (int32 pinSetIndex = 0; pinSetIndex < Instance->InputPins.Num(); ++pinSetIndex)
			{
				const FCraftingTreeNodePinSet &pinSet = Instance->InputPins[pinSetIndex];
				for (int32 pinSetPosition = 0; pinSetPosition < pinSet.AttachedObjects.Num(); ++pinSetPosition)
				{
					FCraftingTreeNodePreset::FPinSpec &pinSpec = OutPreset.ChildNodes.AddDefaulted_GetRef();
					pinSpec.PinSetIndex = pinSetIndex;
					pinSpec.Scope = pinSet.Scope;
					pinSpec.PinSetPosition = pinSetPosition;
					pinSpec.PinSpecSearchTags = Instance->InputPins[pinSetIndex].EligiblePresetSearch.SelectionSearchTags;
					
					FCraftingTreeNodeInstanceSharedPtr attachedOb = pinSet.AttachedObjects[pinSetPosition].Pin();

					// If an attached object is ReadOnly, it is a category selector
					// In this case, iterate down the child list of ReadOnlies and record them all in the pin sequence
					while (attachedOb.IsValid())
					{ 
						const FCraftingTreeNodePreset *attachedPreset = Presets.Find(attachedOb->PresetID);
							
						if (ensureAlways(attachedPreset != nullptr))
						{
							FCraftingPresetSearchSelection *childValue = &pinSpec.PresetSequence.AddDefaulted_GetRef();
							childValue->NodeType = attachedPreset->NodeType;
							childValue->SelectedPresetID = attachedOb->PresetID;
								
							//We hit a writeable preset, so we're done
							if (!attachedPreset->IsReadOnly)
							{
								attachedOb = nullptr;
							}
							else
							{
								// Otherwise ensure this ReadOnly preset has only one child and iterate
								if (ensureAlways(attachedOb->InputPins.Num() == 1 && attachedOb->InputPins[0].AttachedObjects.Num() == 1))
								{
									attachedOb = attachedOb->InputPins[0].AttachedObjects[0].Pin();
								}
								else
								{
									attachedOb = nullptr;
								}
							}
						}
						else
						{
							attachedOb = nullptr;
						}
					}
				}
			}
			OutPreset.SortChildNodes();
			return ECraftingResult::Success;
		}

		ECraftingResult FCraftingPresetCollection::ToDataRecords(TArray<FCraftingPresetRecord> &OutRecords) const
		{
			for (auto &kvp : Presets)
			{
				FCraftingPresetRecord &presetRec = OutRecords.AddDefaulted_GetRef();
				kvp.Value.ToDataRecord(presetRec);
			}
			return ECraftingResult::Success;
		}

		ECraftingResult FCraftingPresetCollection::FromDataRecords(const TArray<FCraftingPresetRecord> &Records)
		{
			Presets.Empty();

			for (auto &presetRecord : Records)
			{
				FCraftingTreeNodePreset newPreset;
				if (newPreset.FromDataRecord(*this, presetRecord) == ECraftingResult::Success)
				{
					Presets.Add(newPreset.PresetID, newPreset);
				}
			}

			return ECraftingResult::Success;
		}

		FCraftingPresetTag::FCraftingPresetTag(const FString &InQualifiedString) : QualifiedString(InQualifiedString)
		{
			FString scopeStr, tagStr;
			if (ensureAlways(QualifiedString.Split(TEXT("."), &scopeStr, &tagStr)))
			{
				Scope = *scopeStr;
				Tag = *tagStr;
			}
		}

		FCraftingPresetTag::FCraftingPresetTag(const FName &InScope, const FName &InTag) : Scope(InScope), Tag(InTag) 
		{
			QualifiedString = FString::Printf(TEXT("%s.%s"), *Scope.ToString(), *Tag.ToString());
		}

		bool FCraftingTreeNodePreset::HasTag(const FCraftingPresetTag &Tag) const
		{
			const TArray<FCraftingPresetTag> *group = Tags.Find(Tag.GetScope());
			if (group != nullptr)
			{
				return group->Contains(Tag);
			}
			return false;
		}

		bool FCraftingTreeNodePreset::Matches(const FCraftingTreeNodePreset &OtherPreset) const
		{
			if (!Properties.Matches(OtherPreset.Properties))
			{
				return false;
			}

			FCraftingPresetRecord thisRecord, thatRecord;
			ToDataRecord(thisRecord);
			OtherPreset.ToDataRecord(thatRecord);

			if (thisRecord.DisplayName != thatRecord.DisplayName)
			{
				return false;
			}

			if (thisRecord.NodeType != thatRecord.NodeType)
			{
				return false;
			}

			if (thisRecord.PresetID != thatRecord.PresetID)
			{
				return false;
			}

			if (thisRecord.IsReadOnly != thatRecord.IsReadOnly)
			{
				return false;
			}

			if (thisRecord.ChildNodePinSetIndices.Num() != thatRecord.ChildNodePinSetIndices.Num())
			{
				return false;
			}

			// If we pass the indices count test above, we should always pass this one
			if (!ensureAlways(thisRecord.ChildNodePinSetPositions.Num() == thatRecord.ChildNodePinSetPositions.Num()))
			{
				return false;
			}

#define CRAFTING_ARRAY_CHECK(Type,Array) if (!Algo::CompareByPredicate(thisRecord.##Array, thatRecord.##Array,[](const Type &thisVal, const Type &thatVal){return thisVal==thatVal;})){return false;}
			
			CRAFTING_ARRAY_CHECK(int32, ChildNodePinSetIndices);
			CRAFTING_ARRAY_CHECK(int32, ChildNodePinSetPositions);
			CRAFTING_ARRAY_CHECK(FName, ChildNodePinSetPresetIDs);
			CRAFTING_ARRAY_CHECK(FName, ChildNodePinSetNodeTypes);
			CRAFTING_ARRAY_CHECK(FString, ChildNodePinSetTags);

#undef CRAFTING_ARRAY_CHECK

			return true;
		}


		ECraftingResult FCraftingTreeNodePreset::ToDataRecord(FCraftingPresetRecord &OutRecord) const
		{
			OutRecord.DisplayName = GetDisplayName();
			OutRecord.NodeType = NodeType;
			OutRecord.PresetID = PresetID;
			OutRecord.IsReadOnly = IsReadOnly;

			for (const auto &kvp : Tags)
			{
				Algo::Transform(kvp.Value, OutRecord.Tags, [](const FCraftingPresetTag &Tag) {return Tag.GetQualifiedString(); });
			}

			Properties.ToDataRecord(OutRecord.PropertyRecord);

			for (auto &childNode : ChildNodes)
			{
				for (int32 i = 0; i < childNode.PresetSequence.Num(); ++i)
				{
					for (int32 j = 0; j < childNode.PinSpecSearchTags.Num(); ++j)
					{
						OutRecord.ChildNodePinSetIndices.Add(childNode.PinSetIndex);
						OutRecord.ChildNodePinSetPositions.Add(childNode.PinSetPosition);
						OutRecord.ChildNodePinSetPresetIDs.Add(childNode.PresetSequence[i].SelectedPresetID);
						OutRecord.ChildNodePinSetTags.Add(childNode.PinSpecSearchTags[j].GetQualifiedString());
						OutRecord.ChildNodePinSetNodeTypes.Add(childNode.PresetSequence[i].NodeType);
					}
				}
			}

			return ECraftingResult::Success;
		}

		ECraftingResult FCraftingTreeNodePreset::SortChildNodes()
		{
			ChildNodes.Sort([](const FPinSpec &lhs, const FPinSpec &rhs) 
			{
				if (lhs.PinSetIndex < rhs.PinSetIndex)
				{
					return true;
				}
				if (lhs.PinSetIndex > rhs.PinSetIndex)
				{
					return false;
				}
				return lhs.PinSetPosition < rhs.PinSetPosition;
			});
			return ECraftingResult::Success;
		}

		ECraftingResult FCraftingTreeNodePreset::FromDataRecord(const FCraftingPresetCollection &PresetCollection,const FCraftingPresetRecord &Record)
		{
			NodeType = Record.NodeType;

			const FCraftingTreeNodeType *nodeType = PresetCollection.NodeDescriptors.Find(NodeType);
			// TODO: this ensure will fire if expected presets have become obsolete, resave to fix
			if (!ensureAlways(nodeType != nullptr))
			{
				return ECraftingResult::Error;
			}

			PresetID = Record.PresetID;
			IsReadOnly = Record.IsReadOnly;
			Tags.Empty();

			for (auto &tag : Record.Tags)
			{
				FCraftingPresetTag newTag(tag);
				TArray<FCraftingPresetTag> &group = Tags.FindOrAdd(newTag.GetScope());
				group.Add(newTag);
			}

			Properties.Empty();
			ChildNodes.Empty();

			Properties.FromDataRecord(Record.PropertyRecord);

			int32 numChildren = Record.ChildNodePinSetIndices.Num();
			if (ensureAlways(
				numChildren == Record.ChildNodePinSetPositions.Num() &&
				numChildren == Record.ChildNodePinSetPresetIDs.Num() &&
				numChildren == Record.ChildNodePinSetNodeTypes.Num() &&
				numChildren == Record.ChildNodePinSetTags.Num()))
			{
				FPinSpec *pinSpec = nullptr;
				int32 i = 0;
				while (i < numChildren)
				{
					// Pin specs hold sequences of child presets
					// A sequence is a list of zero or more ReadOnly presets followed by a single writable terminating preset
					// When the record contains the same set index/set position pair multiple times in a row, this defines a sequence
					pinSpec = &ChildNodes.AddDefaulted_GetRef();
					pinSpec->PinSetIndex = Record.ChildNodePinSetIndices[i];
					pinSpec->PinSetPosition = Record.ChildNodePinSetPositions[i];

					if (ensureAlways(pinSpec->PinSetIndex < nodeType->InputPins.Num()))
					{ 
						pinSpec->Scope = nodeType->InputPins[pinSpec->PinSetIndex].Scope;
					}

					FCraftingPresetSearchSelection *childSequenceValue = &pinSpec->PresetSequence.AddDefaulted_GetRef();

					childSequenceValue->NodeType = Record.ChildNodePinSetNodeTypes[i];
					childSequenceValue->SelectedPresetID = Record.ChildNodePinSetPresetIDs[i];

					while ( i < numChildren &&
						pinSpec->PinSetIndex == Record.ChildNodePinSetIndices[i] &&
						pinSpec->PinSetPosition == Record.ChildNodePinSetPositions[i])
					{
						if (childSequenceValue->SelectedPresetID != Record.ChildNodePinSetPresetIDs[i] ||
							childSequenceValue->NodeType != Record.ChildNodePinSetNodeTypes[i])
						{
							childSequenceValue = &pinSpec->PresetSequence.AddDefaulted_GetRef();
							childSequenceValue->NodeType = Record.ChildNodePinSetNodeTypes[i];
							childSequenceValue->SelectedPresetID = Record.ChildNodePinSetPresetIDs[i];
						}
						pinSpec->PinSpecSearchTags.AddUnique(FCraftingPresetTag(Record.ChildNodePinSetTags[i]));
						++i;
					}
				}
			}

			return ECraftingResult::Success;
		}

		ECraftingResult FCraftingTreeNodePreset::ToParameterSet(FModumateFunctionParameterSet &OutParameterSet) const
		{
			FCraftingPresetRecord dataRecord;
			ECraftingResult result = ToDataRecord(dataRecord);
			if (result != ECraftingResult::Success)
			{
				return result;
			}

			OutParameterSet.SetValue(Modumate::Parameters::kDisplayName, dataRecord.DisplayName);
			OutParameterSet.SetValue(Modumate::Parameters::kNodeType, dataRecord.NodeType);
			OutParameterSet.SetValue(Modumate::Parameters::kPresetKey, dataRecord.PresetID);
			OutParameterSet.SetValue(Modumate::Parameters::kReadOnly, dataRecord.IsReadOnly);
			OutParameterSet.SetValue(Modumate::Parameters::kChildNodePinSetIndices, dataRecord.ChildNodePinSetIndices);
			OutParameterSet.SetValue(Modumate::Parameters::kChildNodePinSetPositions, dataRecord.ChildNodePinSetPositions);
			OutParameterSet.SetValue(Modumate::Parameters::kChildNodePinSetPresetIDs, dataRecord.ChildNodePinSetPresetIDs);
			OutParameterSet.SetValue(Modumate::Parameters::kChildNodePinSetNodeTypes, dataRecord.ChildNodePinSetNodeTypes);
			OutParameterSet.SetValue(Modumate::Parameters::kChildNodePinSetTags, dataRecord.ChildNodePinSetTags);
			OutParameterSet.SetValue(Modumate::Parameters::kTags, dataRecord.Tags);

			TArray<FString> propertyNames;
			dataRecord.PropertyRecord.Properties.GenerateKeyArray(propertyNames);
			OutParameterSet.SetValue(Modumate::Parameters::kPropertyNames, propertyNames);

			TArray<FString> propertyValues;
			dataRecord.PropertyRecord.Properties.GenerateValueArray(propertyValues);
			OutParameterSet.SetValue(Modumate::Parameters::kPropertyValues, propertyValues);

			return ECraftingResult::Success;
		}

		ECraftingResult FCraftingTreeNodePreset::FromParameterSet(const FCraftingPresetCollection &PresetCollection, const FModumateFunctionParameterSet &ParameterSet)
		{
			FCraftingPresetRecord dataRecord;

			// Get base properties, including property bindings that set one property's value to another
			dataRecord.NodeType = ParameterSet.GetValue(Modumate::Parameters::kNodeType);
			const FCraftingTreeNodeType *nodeType = PresetCollection.NodeDescriptors.Find(dataRecord.NodeType);
			if (ensureAlways(nodeType != nullptr))
			{
				nodeType->Properties.ToDataRecord(dataRecord.PropertyRecord);
			}

			// Get local overrides of properties
			TArray<FString> propertyNames = ParameterSet.GetValue(Modumate::Parameters::kPropertyNames);
			TArray<FString> propertyValues = ParameterSet.GetValue(Modumate::Parameters::kPropertyValues);

			if (!ensureAlways(propertyNames.Num() == propertyValues.Num()))
			{
				return ECraftingResult::Error;
			}

			int32 numProps = propertyNames.Num();
			for (int32 i = 0; i < numProps; ++i)
			{
				dataRecord.PropertyRecord.Properties.Add(propertyNames[i], propertyValues[i]);
			}

			dataRecord.DisplayName = ParameterSet.GetValue(Modumate::Parameters::kDisplayName);
			dataRecord.PresetID = ParameterSet.GetValue(Modumate::Parameters::kPresetKey);
			dataRecord.IsReadOnly = ParameterSet.GetValue(Modumate::Parameters::kReadOnly);
			dataRecord.ChildNodePinSetIndices = ParameterSet.GetValue(Modumate::Parameters::kChildNodePinSetIndices);
			dataRecord.ChildNodePinSetPositions = ParameterSet.GetValue(Modumate::Parameters::kChildNodePinSetPositions);
			dataRecord.ChildNodePinSetPresetIDs = ParameterSet.GetValue(Modumate::Parameters::kChildNodePinSetPresetIDs);
			dataRecord.ChildNodePinSetNodeTypes = ParameterSet.GetValue(Modumate::Parameters::kChildNodePinSetNodeTypes);
			dataRecord.ChildNodePinSetTags = ParameterSet.GetValue(Modumate::Parameters::kChildNodePinSetTags);
			dataRecord.Tags = ParameterSet.GetValue(Modumate::Parameters::kTags);

			return FromDataRecord(PresetCollection,dataRecord);
		}

		ECraftingResult FCraftingPresetCollection::SearchPresets(
			const FName &NodeType, 
			const TArray<FCraftingPresetTag> &IncludedTags, 
			const TArray<FCraftingPresetTag> &ExcludedTags, 
			TArray<FName> &OutPresets) const
		{
			TArray<FCraftingTreeNodePreset> qualifiers;
			if (NodeType.IsNone())
			{
				for (auto &kvp : Presets)
				{
					qualifiers.Add(kvp.Value);
				}
			}
			else
			{
				for (auto &kvp : Presets)
				{
					if (kvp.Value.NodeType == NodeType)
					{
						qualifiers.Add(kvp.Value);
					}
				}
			}

			qualifiers = qualifiers.FilterByPredicate([&IncludedTags,&ExcludedTags](const FCraftingTreeNodePreset &Preset)
			{
				for (auto &excluded : ExcludedTags)
				{
					if (Preset.HasTag(excluded))
					{
						return false;
					}
				}
				if (IncludedTags.Num() == 0)
				{
					return true;
				}
				for (auto &included : IncludedTags)
				{
					if (Preset.HasTag(included))
					{
						return true;
					}
				}
				return false;
			});

			Algo::Transform(qualifiers, OutPresets, [](const FCraftingTreeNodePreset &Preset) {return Preset.PresetID; });

			return ECraftingResult::Success;
		}

		/*
		Given a preset ID, recurse through all its children and gather all other presets that this one depends on
		Note: we don't empty the container because the function is recursive
		*/
		ECraftingResult FCraftingPresetCollection::GetDependentPresets(const FName &PresetID, TSet<FName> &OutPresets) const
		{
			const FCraftingTreeNodePreset *preset = Presets.Find(PresetID);
			if (preset == nullptr)
			{
				return ECraftingResult::Error;
			}

			for (auto &childNode : preset->ChildNodes)
			{
				/* 
				Only recurse if we haven't processed this ID yet
				Even if the same preset appears multiple times in a tree, its children will always be the same
				*/
				for (auto &fieldValue : childNode.PresetSequence)
				{ 
					if (!OutPresets.Contains(fieldValue.SelectedPresetID))
					{
						OutPresets.Add(fieldValue.SelectedPresetID);
						GetDependentPresets(fieldValue.SelectedPresetID, OutPresets);
					}
				}
			}
			return ECraftingResult::Success;
		}

		FString FCraftingTreeNodePreset::GetDisplayName() const
		{
			return Properties.GetProperty(BIM::EScope::Preset, BIM::Parameters::Name);
		}

		ECraftingResult FCraftingTreeNodeInstancePool::DestroyNodeInstance(const FCraftingTreeNodeInstanceSharedPtr &Instance, TArray<int32> &OutDestroyed)
		{
			if (!ensureAlways(Instance != nullptr))
			{
				return ECraftingResult::Error;
			}
			TArray<FCraftingTreeNodeInstanceSharedPtr> childNodes;
			Instance->GatherAllChildNodes(childNodes);
			OutDestroyed.Add(Instance->GetInstanceID());
			Instance->DetachSelfFromParent();
			InstanceMap.Remove(Instance->GetInstanceID());

			InstancePool.Remove(Instance);
			for (auto &inst : childNodes)
			{
				OutDestroyed.Add(inst->GetInstanceID());
				InstanceMap.Remove(inst->GetInstanceID());
				InstancePool.Remove(inst);
			}
			ValidatePool();
			return ECraftingResult::Success;
		}

		ECraftingResult FCraftingTreeNodeInstancePool::DestroyNodeInstance(int32 InstanceID, TArray<int32> &OutDestroyed)
		{
			return DestroyNodeInstance(InstanceFromID(InstanceID), OutDestroyed);
		}


		FCraftingTreeNodeInstanceSharedPtr FCraftingTreeNodeInstancePool::CreateNodeInstanceFromDataRecord(const BIM::FCraftingPresetCollection &PresetCollection,const FCustomAssemblyCraftingNodeRecord &DataRecord, bool RecursePresets)
		{
			FCraftingTreeNodeInstanceSharedPtr instance = InstancePool.Add_GetRef(MakeShareable(new FCraftingTreeNodeInstance(DataRecord.InstanceID)));

			InstanceMap.Add(DataRecord.InstanceID, instance);
			NextInstanceID = FMath::Max(NextInstanceID+1,DataRecord.InstanceID + 1);

			instance->FromDataRecord(*this,PresetCollection,DataRecord,RecursePresets);

			return instance;
		}

		ECraftingResult FCraftingTreeNodeInstancePool::SetNewPresetForNode(
			const FCraftingPresetCollection &PresetCollection, 
			int32 InstanceID, 
			const FName &PresetID)
		{
			BIM::FCraftingTreeNodeInstanceSharedPtr inst = InstanceFromID(InstanceID);

			if (!ensureAlways(inst != nullptr))
			{
				return ECraftingResult::Error;
			}

			const BIM::FCraftingTreeNodePreset *preset = PresetCollection.Presets.Find(PresetID);

			if (!ensureAlways(preset != nullptr))
			{
				return ECraftingResult::Error;
			}

			inst->PresetID = PresetID;
			inst->InstanceProperties = preset->Properties;

			FCustomAssemblyCraftingNodeRecord dataRecord;

			ECraftingResult result = inst->ToDataRecord(dataRecord);

			if (result != ECraftingResult::Success)
			{
				return result;
			}

			return inst->FromDataRecord(*this, PresetCollection, dataRecord, true);
		}

		FCraftingTreeNodeInstanceSharedPtr FCraftingTreeNodeInstancePool::CreateNodeInstanceFromPreset(const FCraftingPresetCollection &PresetCollection,int32 ParentID, const FName &PresetID, bool CreateDefaultReadOnlyChildren)
		{
			const FCraftingTreeNodePreset *preset = PresetCollection.Presets.Find(PresetID);
			if (preset == nullptr)
			{
				return nullptr;
			}

			const FCraftingTreeNodeType *descriptor = PresetCollection.NodeDescriptors.Find(preset->NodeType);
			if (descriptor == nullptr)
			{
				return nullptr;
			}

			FCraftingTreeNodeInstanceSharedPtr instance = InstancePool.Add_GetRef(MakeShareable(new FCraftingTreeNodeInstance(NextInstanceID)));
			InstanceMap.Add(NextInstanceID, instance);
			++NextInstanceID;

			instance->PresetID = preset->PresetID;
			instance->CurrentOrientation = descriptor->Orientation;
			instance->InstanceProperties = preset->Properties;
			instance->InputPins = descriptor->InputPins;
			instance->CanFlipOrientation = descriptor->CanFlipOrientation;

			FCraftingTreeNodeInstanceSharedPtr parent = InstanceFromID(ParentID);
			if (parent != nullptr)
			{
				instance->ParentInstance = parent;
				ensureAlways(parent->AttachChild(PresetCollection, instance) != ECraftingResult::Error);
			}

			for (auto &child : preset->ChildNodes)
			{
				if (ensureAlways(child.PinSetIndex < instance->InputPins.Num()))
				{
					// The first item in the preset sequence is the one directly attached to the parent, so its list is the assigned list
					// Subsequent category children are parented to each other and each carries the list of their immediate child
					instance->InputPins[child.PinSetIndex].EligiblePresetSearch.NodeType = child.PresetSequence[0].NodeType;
					instance->InputPins[child.PinSetIndex].EligiblePresetSearch.SelectionSearchTags = child.PinSpecSearchTags;
				}
			}

			// For a new widget edit, we want to go ahead and load the default children for ReadOnly nodes (specified in the script)
			// For editing or loading an existing preset, we want to rely on the children specified in the pin sequence and not the default settings for the preset
			if (preset->IsReadOnly && !CreateDefaultReadOnlyChildren)
			{
				return instance;
			}

			for (auto &child : preset->ChildNodes)
			{
				// Only the last preset in the child list is presettable, the others are category selectors
				FCraftingTreeNodeInstanceSharedPtr sequenceParent = instance;
				for (auto &sequence : child.PresetSequence)
				{
					sequenceParent = CreateNodeInstanceFromPreset(PresetCollection, sequenceParent->GetInstanceID(), sequence.SelectedPresetID, false);
				}

				// If the sequence ends with a ReadOnly preset (which will be the case for presets defined in the DDL script),
				// then we march down the default child nodes (also defined in the DDL script) until we get to the end
				// In serialized presets (saved by the user), these values will be present in the sequence looped through above
				preset = PresetCollection.Presets.Find(sequenceParent->PresetID);
				while (preset != nullptr && preset->IsReadOnly)
				{
					if (ensureAlways(preset->ChildNodes.Num()==1))
					{
						sequenceParent = CreateNodeInstanceFromPreset(PresetCollection, sequenceParent->GetInstanceID(), preset->ChildNodes[0].PresetSequence[0].SelectedPresetID, false);
						if (ensureAlways(sequenceParent.IsValid()))
						{
							preset = PresetCollection.Presets.Find(sequenceParent->PresetID);
						}
					}
				}
			}

			ValidatePool();
			return instance;
		}

		bool FCraftingTreeNodeInstancePool::FromDataRecord(const FCraftingPresetCollection &PresetCollection, const TArray<FCustomAssemblyCraftingNodeRecord> &InDataRecords,bool RecursePresets)
		{
			ResetInstances();

			for (auto &dr : InDataRecords)
			{
				// Parents recursively create their children who might also be in the data record array already, so make sure we don't double up
				if (InstanceFromID(dr.InstanceID) == nullptr)
				{ 
					FCraftingTreeNodeInstanceSharedPtr inst = CreateNodeInstanceFromDataRecord(PresetCollection,dr,RecursePresets);
					NextInstanceID = FMath::Max(dr.InstanceID + 1, NextInstanceID);
				}
			}

			ValidatePool();
			return true;
		}

		bool FCraftingTreeNodeInstancePool::ToDataRecord(TArray<FCustomAssemblyCraftingNodeRecord> &OutDataRecords) const
		{
			for (auto &instance : InstancePool)
			{
				FCustomAssemblyCraftingNodeRecord &record = OutDataRecords.AddDefaulted_GetRef();
				instance->ToDataRecord(record);
			}
			return true;
		}

		bool FCraftingTreeNodeInstancePool::ValidatePool() const
		{
			bool ret = true;

#if WITH_EDITOR
			auto myChildrenPointToMe = [](const FCraftingTreeNodeInstanceSharedPtr &me)
			{
				bool ret = true;
				for (auto &op : me->InputPins)
				{
					for (auto &oa : op.AttachedObjects)
					{
						ret = ensureAlways(oa.Pin()->ParentInstance.HasSameObject(me.Get())) && ret;
					}
				}
				return ret;
			};

			auto myParentPointsToMe = [](const FCraftingTreeNodeInstanceSharedPtr &me)
			{
				if (me->ParentInstance == nullptr)
				{
					return true;
				}
				for (auto &op : me->ParentInstance.Pin()->InputPins)
				{
					for (auto &oa : op.AttachedObjects)
					{
						if (oa.HasSameObject(me.Get()))
						{
							return true;
						}
					}
				}
				return false;
			};

			int32 parentlessNodes = 0;

			for (auto inst : InstancePool)
			{
				ret = ensureAlways(myParentPointsToMe(inst)) && ret;
				ret = ensureAlways(myChildrenPointToMe(inst)) && ret;
				if (!inst->ParentInstance.IsValid())
				{
					++parentlessNodes;
				}
			}

			ret = ensureAlways(parentlessNodes < 2) && ret;

			ret = ensureAlways(InstancePool.Num() == InstanceMap.Num()) && ret;

			for (auto &kvp : InstanceMap)
			{
				ret = ensureAlways(InstancePool.Contains(kvp.Value.Pin())) && ret;
			}

			ensureAlways(ret);

#endif
			return ret;
		}

		EObjectType FCraftingPresetCollection::GetPresetObjectType(const FName &PresetID) const
		{
			const FCraftingTreeNodePreset *preset = Presets.Find(PresetID);
			if (preset == nullptr)
			{
				return EObjectType::OTNone;
			}
			const FCraftingTreeNodeType *nodeType = NodeDescriptors.Find(preset->NodeType);
			if (ensureAlways(nodeType != nullptr))
			{
				return nodeType->ObjectType;
			}
			return EObjectType::OTNone;
		}

		ECraftingResult FCraftingPresetCollection::ReadDataTable(UDataTable *DataTable, TArray<FString> &OutMessages)
		{
			if (!ensureAlways(DataTable))
			{
				return ECraftingResult::Error;
			}

			enum ETableReadState
			{
				Neutral,
				ReadingDefinition,
				ReadingPresets,
				ReadingLists
			};

			ETableReadState state = ETableReadState::Neutral;

			FCraftingTreeNodeType nodeType;

			TArray<FString> propertyTypes = { TEXT("PRIVATE"),TEXT("STRING"),TEXT("DIMENSION") };

			TArray<FValueSpec> propList;

			DataTable->ForeachRow<FTenColumnTable>(TEXT("FTenColumnTable"),
				[this,&state, &nodeType, &OutMessages, &propList, propertyTypes](
					const FName &Key,
					const FTenColumnTable &data)
			{
				if (state == ETableReadState::Neutral)
				{
					if (!data.C0.IsEmpty())
					{
						nodeType.TypeName = *data.C0;
						state = ETableReadState::ReadingDefinition;
					}
				}
				else if (state == ETableReadState::ReadingDefinition)
				{
					if (data.C0.IsEmpty())
					{
						return;
					}
					if (data.C0 == TEXT("ICON"))
					{
						nodeType.IconType = FindEnumValueByName<EConfiguratorNodeIconType>(TEXT("EConfiguratorNodeIconType"),*data.C1);
					}
					else
					if (propertyTypes.Contains(data.C0))
					{
						if (data.C1.IsEmpty() || data.C2.IsEmpty())
						{
							OutMessages.Add(FString::Printf(TEXT("Bad property format at %s"), *Key.ToString()));
						}
						else
						{
							FValueSpec spec(*data.C2);
							nodeType.Properties.SetProperty(spec.Scope, spec.Name, TEXT(""));
							if (data.C0 != TEXT("PRIVATE"))
							{
								nodeType.FormItemToProperty.Add(data.C1, spec.QN());
							}
						}
					}
					else if (data.C0 == TEXT("Presets"))
					{
						NodeDescriptors.Add(nodeType.TypeName, nodeType);
						state = ETableReadState::ReadingPresets;
					}
				}
				else if (state == ETableReadState::ReadingPresets)
				{
					if (data.C0 == TEXT("Lists"))
					{
						state = ETableReadState::ReadingLists;
					}
					else
					if (data.C0 == TEXT("ID"))
					{
						propList = { 
							FValueSpec(*data.C1),FValueSpec(*data.C2),FValueSpec(*data.C3),
							FValueSpec(*data.C4),FValueSpec(*data.C5),FValueSpec(*data.C6),
							FValueSpec(*data.C7),FValueSpec(*data.C8),FValueSpec(*data.C9) };

					}
					else if (data.C0.Mid(0, 2) != TEXT("--"))
					{
						if (propList.Num() == 0)
						{
							OutMessages.Add(FString::Printf(TEXT("No structure for preset at %s"), *Key.ToString()));
						}

						if (data.C0.IsEmpty())
						{
							return;
						}

						FCraftingTreeNodePreset newPreset;

						if (nodeType.TypeName.IsNone())
						{
							OutMessages.Add(FString::Printf(TEXT("Found preset for unidentified node type %s"), *Key.ToString()));
							return;
						}

						newPreset.NodeType = nodeType.TypeName;
						newPreset.PresetID = *data.C0;

						if (Presets.Contains(newPreset.PresetID))
						{
							OutMessages.Add(FString::Printf(TEXT("Preset %s redfined at %s"), *newPreset.PresetID.ToString(),*Key.ToString()));
							return;
						}

						TArray<FString> values = { data.C1,data.C2,data.C3,data.C4,data.C5,data.C6,data.C7,data.C8,data.C9 };

						for (int32 i = 0; i < values.Num(); ++i)
						{
							if (propList[i].Scope == EScope::None)
							{
								break;
							}
							if (nodeType.Properties.HasProperty(propList[i].Scope, propList[i].Name))
							{
								newPreset.Properties.SetProperty(propList[i].Scope, propList[i].Name, values[i]);
							}
							else
							{
								OutMessages.Add(FString::Printf(TEXT("Unidentified property for preset at %s"), *Key.ToString()));
							}
						}

						Presets.Add(newPreset.PresetID, newPreset);
					}
				}
				else if (state == ETableReadState::ReadingLists)
				{
					if (data.C0 == TEXT("ID"))
					{
						return;
					}

					if (data.C0.IsEmpty())
					{
						return;
					}
					
					if (Lists.Contains(*data.C0))
					{
						OutMessages.Add(FString::Printf(TEXT("List %s redefined at %s"),*data.C0, *Key.ToString()));
						return;
					}

					TArray<FString> stringMembers;
					TArray<FName> nameMembers;
					data.C2.ParseIntoArray(stringMembers, TEXT(","));
					Algo::Transform(stringMembers, nameMembers, [](const FString &Member) {return *Member; });
					Lists.Add(*data.C0, nameMembers);
				}
			});
			return ECraftingResult::Success;
		}

		ECraftingResult FCraftingPresetCollection::ParseScriptFile(const FString &FilePath, TArray<FString> &OutMessages)
		{
			FModumateScriptProcessor compiler;

			static const FString kDefine = TEXT("DEFINE");
			static const FString kEnd = TEXT("END");
			static const FString kInput = TEXT("INPUT:");
			static const FString kScope = TEXT("SCOPE:");
			static const FString kOutput = TEXT("OUTPUT:");
			static const FString kIconType = TEXT("ICON:");
			static const FString kProperties = TEXT("PROPERTIES:");
			static const FString kNodeName = TEXT("[A-Z][A-Z0-9_]+");
			static const FString kNumberMatch = TEXT("[0-9]+");
			static const FString kWhitespace = TEXT("\\s*");
			static const FString kElipsesMatch = TEXT("\\.\\.");

			static const FString kVariable = TEXT("\\w+\\.\\w+");
			static const FString kSimpleWord = TEXT("[\\w\\-]+");

			static const FString kDeclare = TEXT("DECLARE");

			enum State {
				Neutral,
				ReadingProperties,
				ReadingPins,
				ReadingDefinition,
				ReadingPreset,
				ReadingList
			};

			State state = Neutral;

			FCraftingTreeNodeType currentNode;
			FCraftingTreeNodePreset currentPreset;
			TArray<FName> currentListElements;
			FName currentListName;
			TArray<FString> currentListTags;

			//Match END
			compiler.AddRule(kEnd, [&state, &currentNode, &currentListTags, &currentPreset, &currentListName, &currentListElements, this](
				const TCHAR *originalLine,
				const FModumateScriptProcessor::TRegularExpressionMatch &match,
				int32 lineNum,
				FModumateScriptProcessor::TErrorReporter errorReporter)
			{
				if (state == ReadingDefinition || state == ReadingProperties || state == ReadingPins)
				{
					if (NodeDescriptors.Contains(currentNode.TypeName))
					{
						errorReporter(*FString::Printf(TEXT("Node %s redefined at line %d"), *currentNode.TypeName.ToString(), lineNum));
					}
					else
					{
						if (currentNode.TypeName != NAME_None)
						{
							NodeDescriptors.Add(currentNode.TypeName, currentNode);
						}
						else
						{
							errorReporter(*FString::Printf(TEXT("Unnamed NODE END at line %d"), lineNum));
						}
					}
				}
				else if (state == ReadingList)
				{
					Lists.Add(currentListName, currentListElements);
					for (auto &element : currentListElements)
					{
						FCraftingTreeNodePreset *preset = Presets.Find(element);
						if (preset == nullptr)
						{
							errorReporter(*FString::Printf(TEXT("Unidentified preset %s at %d"), *element.ToString(),lineNum));
						}
						else
						{
							for (auto &tag : currentListTags)
							{
								FCraftingPresetTag tagOb(tag);
								TArray<FCraftingPresetTag> &tagArray = preset->Tags.FindOrAdd(tagOb.GetScope());
								tagArray.Add(tagOb);
							}
						}
					}

					TArray<FCraftingPresetTag> &tags = ListTags.Add(currentListName);
					for (auto &tag : currentListTags)
					{
						FCraftingPresetTag tagOb(tag);
						tags.Add(tagOb);
					}

					currentListElements.Reset();
				}
				else if (state == ReadingPreset)
				{
					// TODO validate complete preset
					currentPreset.SortChildNodes();
					Presets.Add(currentPreset.PresetID,currentPreset);
				}
				else
				{
					errorReporter(*FString::Printf(TEXT("Unexpected END at line %d"), lineNum));
				}
				state = Neutral;
			},
				false);

			//Match INPUT:
			compiler.AddRule(kInput, [&state, &currentNode, this](
				const TCHAR *originalLine,
				const FModumateScriptProcessor::TRegularExpressionMatch &match,
				int32 lineNum,
				FModumateScriptProcessor::TErrorReporter errorReporter)
			{
				if (state != ReadingDefinition && state != ReadingProperties)
				{
					errorReporter(*FString::Printf(TEXT("Unexpected INPUT at line %d"), lineNum));
				}
				state = ReadingPins;
			});

			//Match OUTPUT: <object types> ie 'OUTPUT: OTStructureLine'
			compiler.AddRule(kOutput, [&state, &currentNode, this](
				const TCHAR *originalLine,
				const FModumateScriptProcessor::TRegularExpressionMatch &match,
				int32 lineNum,
				FModumateScriptProcessor::TErrorReporter errorReporter)
			{
				if (state != ReadingDefinition && state != ReadingProperties && state != ReadingPins)
				{
					errorReporter(*FString::Printf(TEXT("Unexpected OUTPUT at line %d"), lineNum));
				}

				TArray<FString> suffices;
				FString(match.suffix().str().c_str()).ParseIntoArray(suffices, TEXT(" "));
				for (auto &suffix : suffices)
				{
					if (!TryEnumValueByString(EObjectType, suffix, currentNode.ObjectType))
					{
						errorReporter(*FString::Printf(TEXT("Unidentified object type %s at line %d"), *suffix, lineNum));
					}
				}
				state = ReadingDefinition;
			},true);

			static const FString kList = TEXT("LIST");
			static const FString kPropertyMatch = TEXT("\\w+\\.\\w+");
			static const FString kTagListRequired = TEXT("(") + kPropertyMatch + TEXT("(,") + kPropertyMatch + TEXT(")*)");
			static const FString kListMatch = kList + kWhitespace + TEXT("(") + kSimpleWord + TEXT(")") + kWhitespace + TEXT("(\\(") + kTagListRequired + TEXT("\\))?");

			compiler.AddRule(kListMatch, [this, &state, &currentListTags, &currentListName, &currentListElements](const TCHAR *originalLine,
				const FModumateScriptProcessor::TRegularExpressionMatch &match,
				int32 lineNum,
				FModumateScriptProcessor::TErrorReporter errorReporter)
			{
				if (state != Neutral)
				{
					// No need to bail, but log the error
					errorReporter(*FString::Printf(TEXT("Unexpected LIST definition at line %d"), lineNum));
				}

				if (match.size() < 2)
				{
					errorReporter(*FString::Printf(TEXT("Malformed LIST definition at line %d"), lineNum));
					return;
				}

				currentListName = match[1].str().c_str();
				if (Lists.Contains(currentListName))
				{
					errorReporter(*FString::Printf(TEXT("List %s redefined at line %d"), *currentListName.ToString(), lineNum));
					return;
				}

				currentListTags.Empty();
				if (match.size() > 3 && match[3].matched)
				{
					FString tagStr = match[3].str().c_str();
					tagStr.ParseIntoArray(currentListTags,TEXT(","));
				}

				state = ReadingList;
			}, false);

			static const FString kListElementMatch = kSimpleWord + TEXT("(,") + kWhitespace + kSimpleWord + TEXT(")*");

			compiler.AddRule(kListElementMatch, [this, &state, &currentListElements](const TCHAR *originalLine,
				const FModumateScriptProcessor::TRegularExpressionMatch &match,
				int32 lineNum,
				FModumateScriptProcessor::TErrorReporter errorReporter)
			{
				if (state != ReadingList)
				{
					errorReporter(*FString::Printf(TEXT("Unexpected LIST element list at line %d"), lineNum));
				}
				FString elementStr = match[0].str().c_str();
				TArray<FString> elements;
				elementStr.ParseIntoArray(elements, TEXT(","));
				for (auto &element : elements)
				{
					FName elementName = *element.TrimStartAndEnd();
					FCraftingTreeNodePreset *preset = Presets.Find(elementName);
					if (preset == nullptr)
					{
						errorReporter(*FString::Printf(TEXT("Unidentified list element %s at line %d"), *element, lineNum));
					}

					currentListElements.Add(elementName);
				}
			}, false);

			static const FString kListAssignment = TEXT("(") + kSimpleWord + TEXT(")=(") + kSimpleWord + TEXT("\\.") + kSimpleWord + TEXT(")");
			compiler.AddRule(kListAssignment, [this, &state, &currentPreset](const TCHAR *originalLine,
				const FModumateScriptProcessor::TRegularExpressionMatch &match,
				int32 lineNum,
				FModumateScriptProcessor::TErrorReporter errorReporter)
			{
				if (state != ReadingPreset)
				{
					errorReporter(*FString::Printf(TEXT("Unexpected list assignment at line %d"), lineNum));
					return;
				}
				if (match.size() < 3)
				{
					errorReporter(*FString::Printf(TEXT("Badly formed variable assignment at line %d"), lineNum));
					return;
				}

				FString listDefault = match[2].str().c_str();
				TArray<FString> listDefaultSplit;
				listDefault.ParseIntoArray(listDefaultSplit, TEXT("."));

				FName nodeTarget = match[1].str().c_str();

				FName listID = *listDefaultSplit[0];
				FName listValue = *listDefaultSplit[1];

				TArray<FName> *list = Lists.Find(listID);

				if (list == nullptr)
				{
					errorReporter(*FString::Printf(TEXT("Unidentified list %s line %d"), *listID.ToString(), lineNum));
					return;
				}

				if (!list->Contains(listValue))
				{
					errorReporter(*FString::Printf(TEXT("Unidentified member %s of list %s line %d"), *listValue.ToString(), *listID.ToString(), lineNum));
					return;
				}

				FCraftingTreeNodePreset *preset = Presets.Find(listValue);

				if (preset == nullptr)
				{
					errorReporter(*FString::Printf(TEXT("Unidentified preset %s in list %s line %d"), *listValue.ToString(), *listID.ToString(), lineNum));
					return;
				}

				FCraftingTreeNodeType *nodeType = NodeDescriptors.Find(currentPreset.NodeType);
				if (nodeType == nullptr)
				{
					errorReporter(*FString::Printf(TEXT("Unidentified preset node type %s line %d"), *currentPreset.NodeType.ToString(), lineNum));
					return;
				}

				TArray<FCraftingPresetTag> *searchTags = ListTags.Find(listID);

				if (searchTags == nullptr)
				{
					errorReporter(*FString::Printf(TEXT("Attempt to assign to untagged list at %d"), lineNum));
					return;
				}

				bool foundSet = false;
				for (int32 i = 0; i < nodeType->InputPins.Num(); ++i)
				{
					if (nodeType->InputPins[i].SetName == nodeTarget)
					{
						FCraftingTreeNodePreset::FPinSpec newSpec;
						newSpec.PinSetIndex = i;
						newSpec.PinSetPosition = 0;
						newSpec.Scope = nodeType->InputPins[i].Scope;

						newSpec.PinSpecSearchTags = *searchTags;

						TArray<FString> presetPath;
						int32 presetPathIndex = 0;
						if (match.suffix().length() > 0)
						{
							FString pathStr = match.suffix().str().c_str();
							pathStr.ParseIntoArray(presetPath, TEXT(":"));
						}

						preset = Presets.Find(listValue);

						while (preset != nullptr)
						{
							FCraftingPresetSearchSelection &child = newSpec.PresetSequence.AddDefaulted_GetRef();
							child.SelectedPresetID = preset->PresetID;
							child.NodeType = preset->NodeType;

							if (preset->IsReadOnly)
							{
								if (preset->ChildNodes.Num() != 1 || preset->ChildNodes[0].PresetSequence.Num() < 1)
								{
									errorReporter(*FString::Printf(TEXT("Bad ReadOnly preset at %d"), lineNum));
									preset = nullptr;
								}
								else
								{
									if (presetPathIndex < presetPath.Num())
									{
										preset = Presets.Find(*presetPath[presetPathIndex]);
										++presetPathIndex;
									}
									else
									{
										preset = Presets.Find(preset->ChildNodes[0].PresetSequence[0].SelectedPresetID);
									}
								}
							}
							else
							{
								preset = nullptr;
							}
						} 

						for (auto &pin : currentPreset.ChildNodes)
						{
							if (pin.PinSetIndex == i && pin.PinSetPosition >= newSpec.PinSetPosition)
							{
								newSpec.PinSetPosition = pin.PinSetPosition + 1;
							}
						}
						currentPreset.ChildNodes.Add(newSpec);
						foundSet = true;
					}
				}

				if (!foundSet)
				{
					errorReporter(*FString::Printf(TEXT("Unidentified list target %s line %d"), *nodeTarget.ToString(), lineNum));
				}
			}, true);


			//Match DEFINE NODENAME
			static const FString kDefineNodePattern = kDefine + kWhitespace + TEXT("(") + kNodeName + TEXT(")");
			compiler.AddRule(kDefineNodePattern, [&state, &currentNode](
				const TCHAR *originalLine,
				const FModumateScriptProcessor::TRegularExpressionMatch &match,
				int32 lineNum,
				FModumateScriptProcessor::TErrorReporter errorReporter)
			{
				if (state != Neutral)
				{
					errorReporter(*FString::Printf(TEXT("Unexpected DEFINE at line %d"), lineNum));
				}
				if (match.size() == 2)
				{
					currentNode = FCraftingTreeNodeType();
					currentNode.TypeName = match[1].str().c_str();
				}
				else
				{
					errorReporter(*FString::Printf(TEXT("Malformed DEFINE at line %d"), lineNum));
				}

				state = ReadingDefinition;
			},
			false); 

			//Match PROPERTIES:
			compiler.AddRule(kProperties, [&state](
				const TCHAR *originalLine,
				const FModumateScriptProcessor::TRegularExpressionMatch &match,
				int32 lineNum,
				FModumateScriptProcessor::TErrorReporter errorReporter)
			{
				if (state != ReadingDefinition && state != ReadingPins)
				{
					errorReporter(*FString::Printf(TEXT("Unexpected PROPERTIES at line %d"), lineNum));
				}
				state = ReadingProperties;
			},
				false);

			//Match ICON:<iconType>
			static const FString kIconMatch = kIconType + kWhitespace + TEXT("(") + kSimpleWord + TEXT(")");
			compiler.AddRule(kIconMatch, [&state,&currentNode](
				const TCHAR *originalLine,
				const FModumateScriptProcessor::TRegularExpressionMatch &match,
				int32 lineNum,
				FModumateScriptProcessor::TErrorReporter errorReporter)
			{
				if (state != ReadingDefinition)
				{
					errorReporter(*FString::Printf(TEXT("Unexpected ICON at line %d"), lineNum));
				}
				if (match.size() > 1 && match[1].matched)
				{
					currentNode.IconType = FindEnumValueByName<EConfiguratorNodeIconType>(TEXT("EConfiguratorNodeIconType"), match[1].str().c_str());
				}
				else
				{
					errorReporter(*FString::Printf(TEXT("Badly formed ICON at line %d"), lineNum));
				}

				TArray<FString> suffices;
				FString(match.suffix().str().c_str()).ParseIntoArray(suffices, TEXT(" "));
				for (auto &suffix : suffices)
				{
					EConfiguratorNodeIconOrientation orientation;
					if (TryFindEnumValueByName<EConfiguratorNodeIconOrientation>(TEXT("EConfiguratorNodeIconOrientation"), *suffix, orientation))
					{
						currentNode.Orientation = orientation;
					}
					else if (suffix == TEXT("CanSwitch"))
					{
						currentNode.CanFlipOrientation = true;
					}
					else
					{
						errorReporter(*FString::Printf(TEXT("Unrecognized ICON directive %s at line %d"), *suffix, lineNum));
					}
				}
			},
				true);

			static const FString kQuotedString = TEXT("\"([\\w\\s\\-\\.\\,\"/:]+)\"");
			static const FString kPrimitiveTypeMatch = TEXT("(STRING|DIMENSION|PRIVATE|BOOLEAN|FIXED)");

			//Match PROPERTY definition (ie STRING("Name",Node.Name))
			static const FString kPropertyPattern = kPrimitiveTypeMatch + TEXT("\\(") + kQuotedString + TEXT("\\,(") + kPropertyMatch + TEXT(")\\)");
			compiler.AddRule(kPropertyPattern, [&state, &currentNode](
				const TCHAR *originalLine,
				const FModumateScriptProcessor::TRegularExpressionMatch &match,
				int32 lineNum,
				FModumateScriptProcessor::TErrorReporter errorReporter)
			{
				if (state != ReadingProperties)
				{
					errorReporter(*FString::Printf(TEXT("Unexpected PROPERTY spec at line %d"), lineNum));
				}
				FValueSpec vs(match[3].str().c_str());
				FString type = match[1].str().c_str();
				if (type != TEXT("PRIVATE"))
				{
					currentNode.FormItemToProperty.Add(match[2].str().c_str(), vs.QN());
				}
				currentNode.Properties.SetProperty(vs.Scope, vs.Name, TEXT(""));
			},
				false);

			// Match BIND(Scope.Property,OtherScope.OtherProperty)
			// Used to bind properties like Layer.Thickness to child values like Node.Depth
			static const FString kBindPattern = TEXT("BIND\\((") + kPropertyMatch + TEXT(")\\,(") + kPropertyMatch + TEXT(")\\)");
			compiler.AddRule(kBindPattern, [&state, &currentNode](
				const TCHAR *originalLine,
				const FModumateScriptProcessor::TRegularExpressionMatch &match,
				int32 lineNum,
				FModumateScriptProcessor::TErrorReporter errorReporter)
			{
				if (state != ReadingProperties)
				{
					errorReporter(*FString::Printf(TEXT("Unexpected BIND spec at line %d"), lineNum));
				}

				FValueSpec source(match[1].str().c_str());
				FValueSpec target(match[2].str().c_str());

				if (!currentNode.Properties.BindProperty(source.Scope,source.Name,target.Scope,target.Name))
				{
					errorReporter(*FString::Printf(TEXT("Could not bind property at %d"), lineNum));
				}

			},
			false);

			static const FString kNodePinTagMatch = TEXT("(") + kSimpleWord + TEXT(")=(") + kSimpleWord + TEXT(")\\(") + kTagListRequired + TEXT("\\)\\.(") + kSimpleWord + TEXT(")");

			compiler.AddRule(kNodePinTagMatch, [this, &state, &currentNode, &currentPreset](
				const TCHAR *originalLine,
				const FModumateScriptProcessor::TRegularExpressionMatch &match,
				int32 lineNum,
				FModumateScriptProcessor::TErrorReporter errorReporter)
			{
				if (state != ReadingPreset)
				{
					errorReporter(*FString::Printf(TEXT("Unexpected list assignment at line %d"), lineNum));
					return;
				}

				FName nodeTarget = match[1].str().c_str();

				FCraftingTreeNodeType *parentNode = NodeDescriptors.Find(currentPreset.NodeType);
				if (parentNode == nullptr)
				{
					errorReporter(*FString::Printf(TEXT("Unidentified preset node type %s line %d"), *currentPreset.NodeType.ToString(), lineNum));
					return;
				}

				FName pinNodeType = match[2].str().c_str();

				if (NodeDescriptors.Find(pinNodeType) == nullptr)
				{
					errorReporter(*FString::Printf(TEXT("Unidentified preset node type %s line %d"), *pinNodeType.ToString(), lineNum));
					return;
				}

				FString tagsStr = match[3].str().c_str();
				TArray<FString> tagStrs;
				tagsStr.ParseIntoArray(tagStrs, TEXT(","));

				FName selectedPreset = match[5].str().c_str();

				bool foundSet = false;
				for (int32 i = 0; i < parentNode->InputPins.Num(); ++i)
				{
					if (parentNode->InputPins[i].SetName == nodeTarget)
					{
						//Don't ChildNodes.AddDefaulted_GetRef because we iterate over ChildNodes below
						FCraftingTreeNodePreset::FPinSpec newSpec;
						newSpec.PinSetIndex = i;
						newSpec.PinSetPosition = 0;
						newSpec.Scope = parentNode->InputPins[i].Scope;

						FCraftingPresetSearchSelection &child = newSpec.PresetSequence.AddDefaulted_GetRef();
						child.NodeType = pinNodeType;
						child.SelectedPresetID = selectedPreset;

						for (auto &tag : tagStrs)
						{
							child.SelectionSearchTags.AddUnique(tag);
						}

						for (auto &pin : currentPreset.ChildNodes)
						{
							if (pin.PinSetIndex == i && pin.PinSetPosition >= newSpec.PinSetPosition)
							{
								newSpec.PinSetPosition = pin.PinSetPosition + 1;
							}
						}

						currentPreset.ChildNodes.Add(newSpec);

						foundSet = true;
						break;
					}
				}

				if (!foundSet)
				{
					errorReporter(*FString::Printf(TEXT("Unidentified pin tag target %s line %d"), *nodeTarget.ToString(), lineNum));
				}
			}, false);

			static const FString kRangeMatch = TEXT("\\[(") + kNumberMatch + TEXT(")") + kElipsesMatch + TEXT("(") + kNumberMatch + TEXT(")\\]");
			static const FString kOpenRangeMatch = TEXT("\\[(") + kNumberMatch + TEXT(")") + kElipsesMatch + TEXT("\\]");
			static const FString kScopeMatch = TEXT("(") + kSimpleWord + TEXT(")->");
			static const FString kNodePinMatch = kScopeMatch + TEXT("(") + kSimpleWord + TEXT(")") + TEXT("(") + kRangeMatch + TEXT("|") + kOpenRangeMatch + TEXT(")*");

			//Match pin definition (ie Layer->MODULE[1..])
			compiler.AddRule(kNodePinMatch, [this, &state, &currentNode](
				const TCHAR *originalLine,
				const FModumateScriptProcessor::TRegularExpressionMatch &match,
				int32 lineNum,
				FModumateScriptProcessor::TErrorReporter errorReporter)
			{
				if (state != ReadingPins)
				{
					errorReporter(*FString::Printf(TEXT("Unexpected INPUT spec at line %d"), lineNum));
				}
				if (match.size() < 7)
				{
					errorReporter(*FString::Printf(TEXT("Malformed INPUT spec at line %d"), lineNum));
				}
				else
				{
					FCraftingTreeNodePinSet &pinset = currentNode.InputPins.AddDefaulted_GetRef();

					if (!TryFindEnumValueByName<EBIMValueScope>(TEXT("EBIMValueScope"), match[1].str().c_str(), pinset.Scope))
					{
						errorReporter(*FString::Printf(TEXT("Unidentified scope %s at line %d"), match[1].str().c_str(), lineNum));
					}

					pinset.SetName = match[2].str().c_str();

					//fully qualified range (ie [1..3])
					if (match[5].matched)
					{
						pinset.MinCount = FCString::Atoi(match[4].str().c_str());
						pinset.MaxCount = FCString::Atoi(match[5].str().c_str());
					}
					//open-ended range (ie [1..])
					else if (match[6].matched)
					{
						pinset.MinCount = FCString::Atoi(match[6].str().c_str());
						pinset.MaxCount = -1;
					}
					//no range specified, only one pin
					else
					{
						pinset.MinCount = pinset.MaxCount = 1;
					}
				}
			},
				false);

			static const FString kTags = TEXT("TAGS=\\[");
			static const FString kTagMatch = kTags + kTagListRequired + TEXT("\\]");

			compiler.AddRule(kTagMatch, [this, &state, &currentPreset](const TCHAR *originalLine,
				const FModumateScriptProcessor::TRegularExpressionMatch &match,
				int32 lineNum,
				FModumateScriptProcessor::TErrorReporter errorReporter)
			{
				if (state != ReadingPreset)
				{
					errorReporter(*FString::Printf(TEXT("Unexpected TAGS at line %d"), lineNum));
				}
				else
				{
					FString tagStr = match[1].str().c_str();
					TArray<FString> elements;
					tagStr.ParseIntoArray(elements, TEXT(","));
					for (auto &element : elements)
					{
						TArray<FString> tag;
						element.ParseIntoArray(tag, TEXT("."));
						if (tag.Num() != 2)
						{
							errorReporter(*FString::Printf(TEXT("Badly formed tag %s at line %d"), *element, lineNum));
						}
						else
						{
							TArray<FCraftingPresetTag> &tagArray = currentPreset.Tags.FindOrAdd(*tag[0]);
							tagArray.Add(FCraftingPresetTag(*tag[0],*tag[1]));
						}
					}
				}
			}, false);

			static const FString kNodeInstanceDecl = TEXT("(") + kNodeName + TEXT(")") + kWhitespace + TEXT("(") + kSimpleWord + TEXT(")");
			static const FString kPreset = TEXT("PRESET");
			static const FString kPresetMatch = kPreset + kWhitespace + kNodeInstanceDecl;
			static const FString kReadOnlyFlag = TEXT("READONLY");

			compiler.AddRule(kPresetMatch, [this, &state, &currentPreset](const TCHAR *originalLine,
				const FModumateScriptProcessor::TRegularExpressionMatch &match,
				int32 lineNum,
				FModumateScriptProcessor::TErrorReporter errorReporter)
			{
				if (state != Neutral)
				{
					// No need to bail, but log the error
					errorReporter(*FString::Printf(TEXT("Unexpected PRESET definition at line %d"), lineNum));
				}

				if (match.size() < 3)
				{
					errorReporter(*FString::Printf(TEXT("Malformed PRESET definition at line %d"), lineNum));
					return;
				}

				state = ReadingPreset;

				currentPreset = FCraftingTreeNodePreset();

				currentPreset.PresetID = match[2].str().c_str();
				if (Presets.Contains(currentPreset.PresetID))
				{
					errorReporter(*FString::Printf(TEXT("PRESET %s redefined at line %d"), *currentPreset.PresetID.ToString(), lineNum));
					return;
				}

				currentPreset.NodeType = match[1].str().c_str();
				FCraftingTreeNodeType *baseDescriptor = NodeDescriptors.Find(currentPreset.NodeType);

				if (baseDescriptor == nullptr)
				{
					errorReporter(*FString::Printf(TEXT("Unrecognized PRESET type %s at line %d"), *currentPreset.NodeType.ToString(), lineNum));
					return;
				}

				currentPreset.Properties = baseDescriptor->Properties;

				TArray<FString> suffices;
				FString(match.suffix().str().c_str()).ParseIntoArray(suffices, TEXT(" "));
				for (auto &suffix : suffices)
				{
					if (suffix == kReadOnlyFlag)
					{
						currentPreset.IsReadOnly = true; // set to false by default in the class definition constructor
					}
				}

			}, true); // true here indicates that presets are allowed to have a suffix after the main match, used for READONLY and other flags

			static const FString kVariableAssignment = TEXT("(") + kVariable + TEXT(")=") + kQuotedString;
			compiler.AddRule(kVariableAssignment, [this, &state, &currentPreset](const TCHAR *originalLine,
				const FModumateScriptProcessor::TRegularExpressionMatch &match,
				int32 lineNum,
				FModumateScriptProcessor::TErrorReporter errorReporter)
			{
				if (state != ReadingPreset)
				{
					errorReporter(*FString::Printf(TEXT("Unexpected variable assignment at line %d"), lineNum));
					return;
				}
				if (match.size() < 3)
				{
					errorReporter(*FString::Printf(TEXT("Badly formed variable assignment at line %d"), lineNum));
				}

				FValueSpec vs(match[1].str().c_str());
				currentPreset.Properties.SetProperty(vs.Scope, vs.Name, match[2].str().c_str());

				return;
			}, false);

			ECraftingResult result = compiler.ParseFile(FilePath, [&OutMessages](const FString &msg) {OutMessages.Add(msg); }) ? ECraftingResult::Success : ECraftingResult::Error;

			/*
			Presets may be tagged after they are attached to previously defined pins, so gather them after read and update pins
			*/	

			return result;
		}
} }


namespace Modumate
{
	template class TDecisionTree<FCraftingItem>;
	template class TDecisionTreeNode<FCraftingItem>;

	namespace CraftingParameters
	{
		const FString ThicknessValue = TEXT("Thickness.Value");
		const FString ThicknessUnits = TEXT("Thickness.Units");

		const FString MaterialColorMaterial = TEXT("MaterialColor.Material");
		const FString MaterialColorColor = TEXT("MaterialColor.Color");

		const FString DimensionLength = TEXT("Dimension.Length");
		const FString DimensionWidth = TEXT("Dimension.Width");
		const FString DimensionHeight = TEXT("Dimension.Height");
		const FString DimensionDepth = TEXT("Dimension.Depth");
		const FString DimensionThickness = TEXT("Dimension.Thickness");
		const FString DimensionBevelWidth = TEXT("Dimension.BevelWidth");

		const FString PatternModuleCount = TEXT("Pattern.ModuleCount");
		const FString PatternExtents = TEXT("Pattern.Extents");
		const FString PatternThickness = TEXT("Pattern.Thickness");
		const FString PatternGap = TEXT("Pattern.Gap");
		const FString PatternName = TEXT("Pattern.Name");
		const FString PatternModuleDimensions = TEXT("Pattern.ModuleDimensions");

		const FString TrimProfileNativeSizeX = TEXT("TrimProfile.NativeSizeX");
		const FString TrimProfileNativeSizeY = TEXT("TrimProfile.NativeSizeY");
	}

	/*
	AddSelfToDecisionNode and SetSelfToPrivateNode are used by the Object Database to reconcile
	private nodes (which have a single hidden meta value) and Table Selects (which build Select
	nodes out of option sets.)
	*/

	static auto getNewNode = [](const FCraftingDecisionTreeNode &parent,
		const FName &luid,
		const FText &label,
		EDecisionType decisionType)
	{
		FCraftingDecisionTreeNode ret;
		ret.Data.GUID = *FString::Printf(TEXT("%s:%s"), *luid.ToString(), *parent.Data.GUID.ToString());
		ret.Data.ParentGUID = parent.Data.GUID;
		ret.Data.Label = label;
		ret.Data.Key = luid.ToString();
		ret.Data.Type = decisionType;
		return ret;
	};

	void FCraftingOptionBase::AddSelfToDecisionNode(const ModumateObjectDatabase *db, FCraftingDecisionTreeNode &parent) const
	{
		FCraftingDecisionTreeNode newTree = getNewNode(parent, Key, DisplayName, EDecisionType::Terminal);
		newTree.Data.Properties = CraftingParameters;
		newTree.Data.Icon = Icon;
		newTree.Data.EngineMaterial = EngineMaterial;
		newTree.Data.Color = CustomColor;
		newTree.Data.ProfileMesh = ProfileMesh;
		parent.Subfields.Add(newTree);
	}

	void FCraftingOptionBase::SetSelfToPrivateNode(FCraftingDecisionTreeNode &privateNode) const
	{
		ensureAlways(privateNode.Data.Type == EDecisionType::Private);
		privateNode.Subfields[0].Data.Properties = CraftingParameters;
		privateNode.Subfields[0].Data.Icon = Icon;
		privateNode.Subfields[0].Data.EngineMaterial = EngineMaterial;
		privateNode.Subfields[0].Data.Color = CustomColor;
		privateNode.Subfields[0].Data.ProfileMesh = ProfileMesh;
		privateNode.SetSelectedSubfield(0);
	}

	const FString FCraftingPortalPartOption::TableName = TEXT("PortalPartSets");
	void FCraftingPortalPartOption::AddSelfToDecisionNode(const ModumateObjectDatabase *db, FCraftingDecisionTreeNode &parent) const
	{
		FCraftingDecisionTreeNode newTree = getNewNode(parent, Key, DisplayName, EDecisionType::Terminal);
		newTree.Data.PortalPartOption = *this;
		parent.Subfields.Add(newTree);
	}

	const FString FPortalAssemblyConfigurationOption::TableName = TEXT("PortalConfigurations");
	void FPortalAssemblyConfigurationOption::AddSelfToDecisionNode(const ModumateObjectDatabase *db, FCraftingDecisionTreeNode &parent) const
	{
		FCraftingDecisionTreeNode newTree = getNewNode(parent, Key, DisplayName, EDecisionType::Form);
		newTree.Data.PortalConfigurationOption = *this;

		// Portal configurations have both a width and a height option set
		FCraftingDecisionTreeNode widthTree = getNewNode(newTree, "Width", FText::FromString("Width"), EDecisionType::Select);

		for (auto &sw : SupportedWidths)
		{
			FCraftingDecisionTreeNode widthOption = getNewNode(widthTree, sw.Key, sw.DisplayName, EDecisionType::Terminal);
			widthOption.Data.PortalDimension = sw;
			widthTree.Subfields.Add(widthOption);
		}

		if (widthTree.Subfields.Num() > 0)
		{
			newTree.Subfields.Add(widthTree);
		}

		FCraftingDecisionTreeNode heightTree = getNewNode(newTree, "Height", FText::FromString("Height"), EDecisionType::Select);

		for (auto &sh : SupportedHeights)
		{
			FCraftingDecisionTreeNode heightOption = getNewNode(heightTree, sh.Key, sh.DisplayName, EDecisionType::Terminal);
			heightOption.Data.PortalDimension = sh;
			heightTree.Subfields.Add(heightOption);
		}

		if (heightTree.Subfields.Num() > 0)
		{
			newTree.Subfields.Add(heightTree);
		}

		parent.Subfields.Add(newTree);
	}
	void FPortalAssemblyConfigurationOption::SetSelfToPrivateNode(FCraftingDecisionTreeNode &privateNode) const
	{
		ensureAlways(privateNode.Data.Type == EDecisionType::Private);
		privateNode.Subfields[0].Data.PortalConfigurationOption = *this;
		privateNode.SetSelectedSubfield(0);
	}

	bool FPortalAssemblyConfigurationOption::IsValid() const
	{
		return (PortalFunction != EPortalFunction::None) && !Key.IsNone() && !DisplayName.IsEmpty() &&
			(ReferencePlanes[0].Num() >= 2) && (ReferencePlanes[2].Num() >= 2) && (Slots.Num() > 0);
	}
}

using namespace Modumate;

void FCraftingItem::UpdateMetaValue()
{
	for (auto &pvb : PropertyValueBindings)
	{
		Properties.SetValue(pvb, Value);
	}
}


// When the child of a Select is chosen, its data are promoted to parent
void FCraftingItem::AssimilateSubfieldValues(const FCraftingItem &sf)
{
	LayerFunction = sf.LayerFunction;
	LayerFormat = sf.LayerFormat;

	SelectedSubnodeGUID = sf.GUID;

	// Display your selected child's name in your input field if you're a dropdown or icon select
	Value = sf.Label.ToString();

	PortalPartOption = sf.PortalPartOption;
	PortalDimension = sf.PortalDimension;
	PortalConfigurationOption = sf.PortalConfigurationOption;

	Modumate::BIM::FValueSpec idCodeLine1(BIM::EScope::Layer, BIM::EValueType::FixedText, BIM::Parameters::Code);

	Properties = sf.Properties;

	if (HasProperty(idCodeLine1.QN().ToString()))
	{
		Tag = FText::FromString(GetProperty(idCodeLine1.QN().ToString()));
	}

	Icon = sf.Icon;
	Color = sf.Color;
	EngineMaterial = sf.EngineMaterial;
	ProfileMesh = sf.ProfileMesh;
}

Modumate::FModumateCommandParameter FCraftingItem::GetProperty(
	Modumate::BIM::EScope scope,
	const FString &name) const
{
	return GetProperty(BIM::FValueSpec(scope, BIM::EValueType::None, *name).QN().ToString());
}

Modumate::FModumateCommandParameter FCraftingItem::GetProperty(const FString &qualifiedName) const
{
	return Properties.GetValue(qualifiedName, FString(TEXT("")));
}

void FCraftingItem::SetProperty(
	Modumate::BIM::EScope scope,
	const FString &param,
	const Modumate::FModumateCommandParameter &value)
{
	SetProperty(BIM::FValueSpec(scope, BIM::EValueType::None, *param).QN().ToString(), value);
}

void FCraftingItem::SetProperty(
	const FString &qualifiedName,
	const Modumate::FModumateCommandParameter &value)
{
	Properties.SetValue(qualifiedName, value);
}

const void FCraftingItem::GetPropertyNames(TArray<FString> &outNames) const
{
	Properties.GetValueNames(outNames);
}

bool FCraftingItem::HasProperty(const FString &qualifiedName) const
{
	return Properties.HasValue(qualifiedName);
}

bool FCraftingItem::HasProperty(Modumate::BIM::EScope scope, const FString &name) const
{
	return HasProperty(BIM::FValueSpec(scope, BIM::EValueType::None, *name).QN().ToString());
}

ECraftingResult UModumateCraftingNodeWidgetStatics::CreateNewNodeInstanceFromPreset(
	Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances,
	const Modumate::BIM::FCraftingPresetCollection &PresetCollection,
	int32 ParentID,
	const FName &PresetID,
	FCraftingNode &OutNode)
{
	BIM::FCraftingTreeNodeInstanceSharedPtr inst = NodeInstances.CreateNodeInstanceFromPreset(PresetCollection, ParentID, PresetID, true);

	if (ensureAlways(inst.IsValid()))
	{
		return CraftingNodeFromInstance(NodeInstances,inst->GetInstanceID(),PresetCollection,OutNode);
	}
	return ECraftingResult::Error;
}

ECraftingResult UModumateCraftingNodeWidgetStatics::CraftingNodeFromInstance(
	const Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances, 
	int32 InstanceID,
	const Modumate::BIM::FCraftingPresetCollection &PresetCollection, 
	FCraftingNode &OutNode)
{
	const BIM::FCraftingTreeNodeInstanceSharedPtr instance = NodeInstances.InstanceFromID(InstanceID);
	if (!instance.IsValid())
	{
		return ECraftingResult::Error;
	}
	OutNode.CanSwapPreset = instance->ParentInstance.IsValid();
	OutNode.InstanceID = instance->GetInstanceID();
	OutNode.ParentInstanceID = instance->ParentInstance.IsValid() ? instance->ParentInstance.Pin()->GetInstanceID() : 0;
	OutNode.PresetID = instance->PresetID;
	OutNode.CanAddToProject = false;
	OutNode.PresetStatus = instance->GetPresetStatus(PresetCollection);
	OutNode.ParentPinLabel = FText::FromString(OutNode.Label.ToString() + TEXT("ON A PIN!"));

	if (instance->InputPins.Num() == 0 && ensureAlways(OutNode.PresetStatus != ECraftingNodePresetStatus::ReadOnly))
	{
		OutNode.IsEmbeddedInParent = true;
	}

	const BIM::FCraftingTreeNodePreset *preset = PresetCollection.Presets.Find(instance->PresetID);
	if (ensureAlways(preset != nullptr))
	{
		const Modumate::BIM::FCraftingTreeNodeType *nodeType = PresetCollection.NodeDescriptors.Find(preset->NodeType);
		if (ensureAlways(nodeType != nullptr))
		{
			OutNode.NodeIconType = nodeType->IconType;
			OutNode.CanSwitchOrientation = nodeType->CanFlipOrientation;
		}
	
		OutNode.Label = FText::FromString(preset->GetDisplayName());
		BIM::FCraftingTreeNodeInstanceSharedPtr instIter = instance;
		while (preset !=nullptr && preset->IsReadOnly)
		{
			if (ensureAlways(instIter->InputPins.Num() == 1) && ensureAlways(instIter->InputPins[0].AttachedObjects.Num() == 1))
			{
				instIter = instIter->InputPins[0].AttachedObjects[0].Pin();
				preset = PresetCollection.Presets.Find(instIter->PresetID);
			}
		}
		if (preset != nullptr)
		{
			OutNode.ParentPinLabel = FText::FromString(preset->GetDisplayName());
		}
		else
		{
			OutNode.ParentPinLabel = OutNode.Label;
		}
	}

	OutNode.NodeIconOrientation = instance->CurrentOrientation;

	if (instance->ParentInstance.IsValid())
	{
		Modumate::BIM::FCraftingTreeNodeInstanceSharedPtr parent = instance->ParentInstance.Pin();
		parent->FindChildOrder(instance->GetInstanceID(),OutNode.ChildOrder);

		while (parent.IsValid())
		{
			if (parent->GetPresetStatus(PresetCollection) == ECraftingNodePresetStatus::ReadOnly)
			{
				OutNode.EmbeddedInstanceIDs.Add(parent->GetInstanceID());
				if (parent->ParentInstance.IsValid())
				{
					parent = parent->ParentInstance.Pin();
				}
				else
				{
					parent = nullptr;
				}
			}
			else
			{
				parent = nullptr;
			}
		}
	}
	else
	{
		OutNode.ChildOrder = 0;
	}

	return ECraftingResult::Success;
}

ECraftingResult UModumateCraftingNodeWidgetStatics::GetInstantiatedNodes(
	Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances, 
	const Modumate::BIM::FCraftingPresetCollection &PresetCollection,
	TArray<FCraftingNode> &OutNodes)
{
	if (ensureAlways(NodeInstances.GetInstancePool().Num() > 0))
	{
		Algo::Transform(NodeInstances.GetInstancePool(),OutNodes,[&PresetCollection,&NodeInstances](const Modumate::BIM::FCraftingTreeNodeInstanceSharedPtr &Node)
		{
			FCraftingNode outNode;
			CraftingNodeFromInstance(NodeInstances,Node->GetInstanceID(), PresetCollection, outNode);
			return outNode;
		});
		return ECraftingResult::Success;
	}
	return ECraftingResult::Error;
}

ECraftingResult UModumateCraftingNodeWidgetStatics::GetPinAttachmentToParentForNode(
	Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances, 
	int32 InstanceID, 
	int32 &OutPinGroupIndex, 
	int32 &OutPinIndex)
{
	BIM::FCraftingTreeNodeInstanceSharedPtr inst = NodeInstances.InstanceFromID(InstanceID);

	if (!inst.IsValid() || !inst->ParentInstance.IsValid())
	{
		return ECraftingResult::Error;
	}

	for (int32 p = 0; p < inst->ParentInstance.Pin()->InputPins.Num(); ++p)
	{
		auto &pg = inst->ParentInstance.Pin()->InputPins[p];
		for (int32 i = 0; i < pg.AttachedObjects.Num(); ++i)
		{
			if (pg.AttachedObjects[i].HasSameObject(inst.Get()))
			{
				OutPinGroupIndex = p;
				OutPinIndex = i;
				return ECraftingResult::Success;
			}
		}
	}
	return ECraftingResult::Error;
}

ECraftingResult UModumateCraftingNodeWidgetStatics::GetFormItemsForCraftingNode(
	Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances,
	const Modumate::BIM::FCraftingPresetCollection &PresetCollection,
	int32 InstanceID, TArray<FCraftingNodeFormItem> &OutForm)
{
	BIM::FCraftingTreeNodeInstanceSharedPtr node = NodeInstances.InstanceFromID(InstanceID);
	if (!node.IsValid())
	{
		return ECraftingResult::Error;
	}

	const BIM::FCraftingTreeNodePreset *preset = PresetCollection.Presets.Find(node->PresetID);
	const BIM::FCraftingTreeNodeType *descriptor = preset != nullptr ? PresetCollection.NodeDescriptors.Find(preset->NodeType) : nullptr;

	if (!ensureAlways(descriptor != nullptr))
	{
		return ECraftingResult::Error;
	}

	for (auto &kvp : descriptor->FormItemToProperty)
	{
		BIM::FValueSpec vs(*kvp.Value.ToString());
		FString outVal;
		if (ensureAlways(node->InstanceProperties.TryGetProperty(vs.Scope, vs.Name, outVal)))
		{
			FCraftingNodeFormItem &temp = OutForm.AddDefaulted_GetRef();
			temp.ItemValue = outVal;
			temp.DisplayLabel = FText::FromString(kvp.Key);
			temp.NodeInstance = InstanceID;
			temp.PropertyBindingQN = vs.QN().ToString();
		}
	}
	return ECraftingResult::Success;
}

ECraftingResult UModumateCraftingNodeWidgetStatics::GetFormItemsForPreset(const Modumate::BIM::FCraftingPresetCollection &PresetCollection, const FName &PresetID, TArray<FCraftingNodeFormItem> &OutForm)
{
	const BIM::FCraftingTreeNodePreset *preset = PresetCollection.Presets.Find(PresetID);
	const BIM::FCraftingTreeNodeType *descriptor = preset != nullptr ? PresetCollection.NodeDescriptors.Find(preset->NodeType) : nullptr;

	if (!ensureAlways(descriptor != nullptr))
	{
		return ECraftingResult::Error;
	}

	for (auto &kvp : descriptor->FormItemToProperty)
	{
		BIM::FValueSpec vs(*kvp.Value.ToString());
		FString outVal;
		if (ensureAlways(preset->Properties.TryGetProperty(vs.Scope, vs.Name, outVal)))
		{
			FCraftingNodeFormItem &temp = OutForm.AddDefaulted_GetRef();
			temp.ItemValue = outVal;
			temp.DisplayLabel = FText::FromString(kvp.Key);
			temp.PropertyBindingQN = vs.QN().ToString();
		}
	}
	return ECraftingResult::Success;
}

ECraftingResult UModumateCraftingNodeWidgetStatics::SetValueForFormItem(
	Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances, 
	const FCraftingNodeFormItem &FormItem, 
	const FString &Value)
{
	BIM::FCraftingTreeNodeInstanceSharedPtr node = NodeInstances.InstanceFromID(FormItem.NodeInstance);
	if (node.IsValid())
	{
		BIM::FValueSpec vs(*FormItem.PropertyBindingQN);
		node->InstanceProperties.SetProperty(vs.Scope, vs.Name, Value);
		return ECraftingResult::Success;
	}
	return ECraftingResult::Error;
}

ECraftingResult UModumateCraftingNodeWidgetStatics::SetValueForPreset(const FName &PresetID, const FString &Value)
{
	return ECraftingResult::Error;
}

ECraftingResult UModumateCraftingNodeWidgetStatics::RemoveNodeInstance(
	Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances, 
	int32 ParentID, 
	int32 InstanceID)
{
	if (InstanceID == 0)
	{
		return ECraftingResult::Error;
	}

	BIM::FCraftingTreeNodeInstanceSharedPtr parentInst = NodeInstances.InstanceFromID(ParentID);
	BIM::FCraftingTreeNodeInstanceSharedPtr childInst = NodeInstances.InstanceFromID(InstanceID);

	if (parentInst.IsValid() && !parentInst->CanRemoveChild(childInst))
	{
		return ECraftingResult::Error;
	}
	TArray<int32> destroyedChildren;
	NodeInstances.DestroyNodeInstance(InstanceID, destroyedChildren);
	return ECraftingResult::Success;
}

ECraftingResult UModumateCraftingNodeWidgetStatics::GetPinGroupsForNode(
	Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances, 
	const Modumate::BIM::FCraftingPresetCollection &PresetCollection, 
	int32 NodeID,
	TArray<FCraftingNodePinGroup> &OutPins)
{

	BIM::FCraftingTreeNodeInstanceSharedPtr node = NodeInstances.InstanceFromID(NodeID);
	if (!node.IsValid())
	{
		return ECraftingResult::Error;
	}
	for (auto &pin : node->InputPins)
	{
		FCraftingNodePinGroup &group = OutPins.AddDefaulted_GetRef();
		group.MaxPins = pin.MaxCount;
		group.MinPins = pin.MinCount;
		group.NodeType = pin.EligiblePresetSearch.NodeType;
		group.OwnerInstance = NodeID;

		PresetCollection.SearchPresets(pin.EligiblePresetSearch.NodeType, pin.EligiblePresetSearch.SelectionSearchTags, TArray<Modumate::BIM::FCraftingPresetTag>(), group.SupportedTypePins);

		for (auto &ao : pin.AttachedObjects)
		{
			group.AttachedChildren.Add(ao.Pin()->GetInstanceID());
		}

		// TODO: GroupName is used to pass SetName for now. Will determine which approach is more appropriate in the future 
		if (group.GroupName.IsNone())
		{
			group.GroupName = pin.SetName;
		}
	}
	return ECraftingResult::Success;
}

ECraftingResult UModumateCraftingNodeWidgetStatics::GetLayerIDFromNodeInstanceID(
	const Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances, 
	const Modumate::BIM::FCraftingPresetCollection &PresetCollection, 
	int32 InstanceID,
	int32 &OutLayerID, 
	int32 &NumberOfLayers)
{
	BIM::FCraftingTreeNodeInstanceSharedPtr thisNode = NodeInstances.InstanceFromID(InstanceID);
	BIM::FCraftingTreeNodeInstanceSharedPtr rootNode = thisNode;

	int32 childPinset=-1, childPinPosition = -1;

	while (rootNode.IsValid())
	{
		const Modumate::BIM::FCraftingTreeNodePreset *preset = PresetCollection.Presets.Find(rootNode->PresetID);
		if (preset != nullptr)
		{
			const Modumate::BIM::FCraftingTreeNodeType *descriptor = PresetCollection.NodeDescriptors.Find(preset->NodeType);
			if (descriptor != nullptr)
			{
				if (descriptor->ObjectType != EObjectType::OTNone)
				{
					break;
				}
			}
		}

		int32 childID = rootNode->GetInstanceID();

		rootNode = rootNode->ParentInstance.Pin();
		if (rootNode.IsValid())
		{
			rootNode->FindChild(childID, childPinset, childPinPosition);
			NumberOfLayers = rootNode->InputPins[childPinset].AttachedObjects.Num();
		}
	}

	OutLayerID = childPinPosition;

	return rootNode != nullptr && childPinPosition != -1 ? ECraftingResult::Success : ECraftingResult::Error;
}

ECraftingResult UModumateCraftingNodeWidgetStatics::GetPropertyTipsByIconType(const Modumate::ModumateObjectDatabase &InDB, EConfiguratorNodeIconType IconType, const Modumate::BIM::FModumateAssemblyPropertySpec &PresetProperties, TArray<FString> &OutTips)
{
	if (IconType == EConfiguratorNodeIconType::Module || IconType == EConfiguratorNodeIconType::Gap2D)
	{
		BIM::EScope scope = IconType == EConfiguratorNodeIconType::Module ? BIM::EScope::Module : BIM::EScope::Gap;

		FString xExtentName, yExtentName, zExtentName, dimension;
		PresetProperties.RootProperties.TryGetProperty(scope, BIM::Parameters::XExtents, xExtentName);
		PresetProperties.RootProperties.TryGetProperty(scope, BIM::Parameters::YExtents, yExtentName);
		PresetProperties.RootProperties.TryGetProperty(scope, BIM::Parameters::ZExtents, zExtentName);
		if (IconType == EConfiguratorNodeIconType::Module)
		{
			dimension = xExtentName + ", " + yExtentName + ", " + zExtentName;
		}
		else
		{
			dimension = xExtentName;
		}

		FString colorName, materialName, color, material;

		if (PresetProperties.RootProperties.TryGetProperty(scope, BIM::Parameters::Color, colorName))
		{
			const FCustomColor *customColor = InDB.GetCustomColorByKey(FName(*colorName));
			if (ensureAlways(customColor != nullptr))
			{
				color = customColor->DisplayName.ToString();
			}
		}
		
		if (PresetProperties.RootProperties.TryGetProperty(scope, BIM::Parameters::MaterialKey, materialName))
		{
			const FArchitecturalMaterial *mat = InDB.GetArchitecturalMaterialByKey(FName(*materialName));
			if (ensureAlways(mat != nullptr))
			{
				material = mat->DisplayName.ToString();
			}
		}

		OutTips = TArray<FString> { dimension, material, color };
		return ECraftingResult::Success;
	}
	if (IconType == EConfiguratorNodeIconType::Layer)
	{
		FString layerFunction, thickness, materialName, material;
		for (auto &curLayerProperties : PresetProperties.LayerProperties)
		{
			curLayerProperties.TryGetProperty(BIM::EScope::Layer, BIM::Parameters::Function, layerFunction);
			curLayerProperties.TryGetProperty(BIM::EScope::Layer, BIM::Parameters::Thickness, thickness);
			curLayerProperties.TryGetProperty(BIM::EScope::Module, BIM::Parameters::MaterialKey, materialName);
			if (materialName.IsEmpty())
			{
				curLayerProperties.TryGetProperty(BIM::EScope::Layer, BIM::Parameters::MaterialKey, materialName);
			}
			if (materialName.IsEmpty())
			{
				curLayerProperties.TryGetProperty(BIM::EScope::Gap, BIM::Parameters::MaterialKey, materialName);
			}
			const FArchitecturalMaterial *mat = InDB.GetArchitecturalMaterialByKey(FName(*materialName));
			if (ensureAlways(mat != nullptr))
			{
				material = mat->DisplayName.ToString();
			}
		}
		OutTips = TArray<FString>{ layerFunction, thickness, material };
		return ECraftingResult::Success;
	}
	return ECraftingResult::Error;
}

ECraftingResult UModumateCraftingNodeWidgetStatics::DragMovePinChild(
	Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances, 
	int32 InstanceID, 
	const FName &PinGroup, 
	int32 From, 
	int32 To)
{
	BIM::FCraftingTreeNodeInstanceSharedPtr node = NodeInstances.InstanceFromID(InstanceID);
	if (!node.IsValid())
	{
		return ECraftingResult::Error;
	}

	Modumate::BIM::FCraftingTreeNodePinSet *pinSet = nullptr;
	for (auto &pin : node->InputPins)
	{
		if (pin.SetName == PinGroup)
		{
			pinSet = &pin;
			break;
		}
	}
	if (pinSet == nullptr)
	{
		return ECraftingResult::Error;
	}

	//Reorder at AttachedObjects;
	if (pinSet->AttachedObjects.IsValidIndex(From) && pinSet->AttachedObjects.IsValidIndex(To))
	{
		Modumate::BIM::FCraftingTreeNodeInstanceWeakPtr nodeInstPtr = pinSet->AttachedObjects[From];
		pinSet->AttachedObjects.RemoveAt(From);
		pinSet->AttachedObjects.Insert(nodeInstPtr, To);
	}
	return ECraftingResult::Success;
}

ECraftingResult UModumateCraftingNodeWidgetStatics::GetEligiblePresetsForSwap(
	Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances, 
	const Modumate::BIM::FCraftingPresetCollection &PresetCollection, 
	int32 InstanceID, 
	TArray<FCraftingNode> &OutPresets)
{
	BIM::FCraftingTreeNodeInstanceSharedPtr instance = NodeInstances.InstanceFromID(InstanceID);
	if (!ensureAlways(instance.IsValid()))
	{
		return ECraftingResult::Error;
	}

	const BIM::FCraftingTreeNodePreset *currentPreset = PresetCollection.Presets.Find(instance->PresetID);

	if (!ensureAlways(currentPreset != nullptr))
	{
		return ECraftingResult::Error;
	}
	TArray<FName> eligiblePresets;
	TArray<Modumate::BIM::FCraftingPresetTag> tags;

	// If we have a parent, check its pin for search terms, otherwise use our own tags
	if (instance->ParentInstance.IsValid())
	{
		int32 pinGroup, pinIndex;
		ECraftingResult result = UModumateCraftingNodeWidgetStatics::GetPinAttachmentToParentForNode(NodeInstances, InstanceID, pinGroup, pinIndex);
		if (!ensureAlways(result == ECraftingResult::Success))
		{
			return result;
		}
		tags = instance->ParentInstance.Pin()->InputPins[pinGroup].EligiblePresetSearch.SelectionSearchTags;
	}
	else
	{ 
		for (auto &kvp : currentPreset->Tags)
		{
			tags.Append(kvp.Value);
		}
	}
	PresetCollection.SearchPresets(currentPreset->NodeType, tags, TArray<Modumate::BIM::FCraftingPresetTag>(), eligiblePresets);
	Algo::Transform(eligiblePresets, OutPresets,
		[&PresetCollection](const FName &PresetID)
	{
		return FCraftingNode::FromPreset(*PresetCollection.Presets.Find(PresetID));
	});
	
	return ECraftingResult::Success;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateCraftingUnitTest, "Modumate.Database.ModumateCrafting.UnitTest", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
bool FModumateCraftingUnitTest::RunTest(const FString &Parameters)
{
	/*
	Load WALL_ROOT preset
	*/
	TArray<FString> errors;
	Modumate::BIM::FCraftingPresetCollection presetCollection;
	if (!ensureAlways(presetCollection.ParseScriptFile(FPaths::ProjectContentDir() / TEXT("NonUAssets") / TEXT("BIMNodeGraph.mbn"), errors) == ECraftingResult::Success))
	{
		return false;
	}

	TArray<FName> outPresets;
	if (!ensureAlways(presetCollection.SearchPresets(FName(TEXT("WALL_ROOT")), { Modumate::BIM::FCraftingPresetTag(TEXT("Assembly.Default")) }, {}, outPresets) == ECraftingResult::Success))
	{
		return false;
	}

	if (!ensureAlways(outPresets.Num() != 0))
	{
		return false;
	}

	/*
	Check that preset matches itself
	*/
	Modumate::BIM::FCraftingTreeNodePreset *preset = presetCollection.Presets.Find(outPresets[0]);
	if (!ensureAlways(preset != nullptr))
	{
		return false;
	}

	if (!ensureAlways(preset->Matches(*preset)))
	{
		return false;
	}

	/*
	Check that serialized preset matches original when deserialized
	*/
	FCraftingPresetRecord record;
	preset->ToDataRecord(record);
	Modumate::BIM::FCraftingTreeNodePreset instancePreset;
	instancePreset.FromDataRecord(presetCollection, record);

	if (!ensureAlways(preset->Matches(instancePreset)))
	{
		return false;
	}

	if (!ensureAlways(instancePreset.Matches(*preset)))
	{
		return false;
	}

	/*
	Check that preset matches original when built from command parameters
	*/
	Modumate::FModumateFunctionParameterSet parameterSet;
	preset->ToParameterSet(parameterSet);
	instancePreset = Modumate::BIM::FCraftingTreeNodePreset();

	instancePreset.FromParameterSet(presetCollection, parameterSet);

	if (!ensureAlways(preset->Matches(instancePreset)))
	{
		return false;
	}

	if (!ensureAlways(instancePreset.Matches(*preset)))
	{
		return false;
	}


	/*
	Create the first default wall preset as a node
	*/
	Modumate::BIM::FCraftingTreeNodeInstancePool instancePool;
	Modumate::BIM::FCraftingTreeNodeInstanceSharedPtr instance = instancePool.CreateNodeInstanceFromPreset(presetCollection, 0, preset->PresetID, true);

	if (!ensureAlways(instance.IsValid()))
	{
		return false;
	}

	/*
	Convert node to preset and verify it matches the original
	*/
	instancePreset = Modumate::BIM::FCraftingTreeNodePreset();
	if (!ensureAlways(presetCollection.GetInstanceDataAsPreset(instance, instancePreset) == ECraftingResult::Success))
	{
		return false;
	}

	if (!ensureAlways(instancePreset.Matches(*preset)))
	{
		return false;
	}

	if (!ensureAlways(preset->Matches(instancePreset)))
	{
		return false;
	}

	/*
	Verify that input pins generate the correct list of eligible presets
	*/

	// Make a wood stud wall
	instancePool.ResetInstances();
	instance = instancePool.CreateNodeInstanceFromPreset(presetCollection, 0, FName(TEXT("Default2x4InteriorWoodStudWallPreset")), true);
	if (!ensureAlways(instance.IsValid()))
	{
		return false;
	}

	// Find the material node
	Modumate::BIM::FCraftingTreeNodeInstanceSharedPtr materialNode = instancePool.FindInstanceByPredicate(
		[](const Modumate::BIM::FCraftingTreeNodeInstanceSharedPtr &Instance)
	{
		return (Instance->PresetID.IsEqual(FName(TEXT("WoodSprucePineFir"))));
	});

	if (!ensureAlways(materialNode.IsValid()))
	{
		return false;
	}

	// Ask for eligible swaps of material node (should only be WoodSprucePineFir)
	TArray<FCraftingNode> craftingNodes;
	UModumateCraftingNodeWidgetStatics::GetEligiblePresetsForSwap(instancePool, presetCollection, materialNode->GetInstanceID(), craftingNodes);

	if (!ensureAlways(craftingNodes.Num() == 1))
	{
		return false;
	}

	if (!ensureAlways(craftingNodes[0].PresetID.IsEqual(FName(TEXT("WoodSprucePineFir")))))
	{
		return false;
	}
	return true;
}

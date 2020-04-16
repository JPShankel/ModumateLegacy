// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ModumateCrafting.h"
#include "ModumateObjectDatabase.h"
#include "ModumateArchitecturalMaterial.h"
#include "ModumateScriptProcessor.h"
#include "ModumateDecisionTreeImpl.h"
#include "ModumateCommands.h"
#include "Algo/Accumulate.h"
#include "Algo/Transform.h"

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

			// If any instance properties are different than the preset's copy, the preset is dirty
			if (!InstanceProperties.Matches(preset->Properties))
			{
				return ECraftingNodePresetStatus::Dirty;
			}

			// If the layout of children is different, the preset is dirty
			int32 numChildren = Algo::TransformAccumulate(InputPins, [](const FCraftingTreeNodePinSet &pinSet) {return pinSet.AttachedObjects.Num(); }, 0);

			if (numChildren != preset->ChildNodes.Num())
			{
				return ECraftingNodePresetStatus::Dirty;
			}

			for (int32 i = 0; i < numChildren; ++i)
			{
				int32 setIndex = preset->ChildNodes[i].PinSetIndex;
				int32 setPosition = preset->ChildNodes[i].PinSetPosition;

				// Badly specified children mean the preset is dirty
				if (!ensureAlways(setIndex < InputPins.Num()))
				{
					return ECraftingNodePresetStatus::Dirty;
				}

				if (!ensureAlways(setPosition < InputPins[setIndex].AttachedObjects.Num()))
				{
					return ECraftingNodePresetStatus::Dirty;
				}

				// Not the same kind of child? Dirty preset!
				if (ensureAlways(InputPins[setIndex].AttachedObjects[setPosition] != nullptr) &&
					InputPins[setIndex].AttachedObjects[setPosition].Pin()->PresetID != preset->ChildNodes[i].PresetIDs[0])
				{
					return ECraftingNodePresetStatus::Dirty;
				}
			}
			return ECraftingNodePresetStatus::UpToDate;
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

		FName FCraftingTreeNodeInstancePool::GetListForParentPin(int32 InstanceID) const
		{
			const FCraftingTreeNodeInstanceSharedPtr instance = InstanceFromID(InstanceID);
			if (!ensureAlways(instance != nullptr))
			{
				return NAME_None;
			}

			if (instance->ParentInstance == nullptr)
			{
				return NAME_None;
			}

			for (auto &inputPin : instance->ParentInstance.Pin()->InputPins)
			{
				if (inputPin.AttachedObjects.Contains(instance))
				{
					return inputPin.AssignedList;
				}
			}
			return NAME_None;
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

		bool FCraftingTreeNodeInstance::CanRemoveChild(const FCraftingTreeNodeInstanceSharedPtr &Child) const
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

		ECraftingResult FCraftingTreeNodeInstance::GetAvailableChildTypes(const FCraftingPresetCollection &PresetCollection, TArray<FName> &OutTypes) const
		{
			for (auto &pin : InputPins)
			{
				if (pin.MaxCount == -1 || pin.AttachedObjects.Num() < pin.MaxCount)
				{
					const TArray<FName> *typeList = PresetCollection.Lists.Find(pin.AssignedList);
					if (ensureAlways(typeList != nullptr))
					{
						OutTypes.Append(*typeList);

						if (typeList->Num() > 0)
						{
							// TODO: custom presets for a given node type are always available until we have user-managed lists
							const FCraftingTreeNodePreset *pPreset = PresetCollection.Presets.Find((*typeList)[0]);
							if (ensureAlways(pPreset != nullptr))
							{
								const TArray<FName> *pCustomPresets = PresetCollection.CustomPresetsByNodeType.Find(pPreset->NodeType);
								if (pCustomPresets != nullptr)
								{
									OutTypes.Append(*pCustomPresets);
								}
							}
						}
					}
				}
			}
			return ECraftingResult::Success;
		}

		bool FCraftingTreeNodeInstance::CanCreateChildOfType(const FCraftingPresetCollection &PresetCollection, const FName &TypeID) const
		{
			TArray<FName> supportedChildren;
			GetAvailableChildTypes(PresetCollection, supportedChildren);
			return supportedChildren.Contains(TypeID);
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
			auto pinSupportsPreset = [&Child, &PresetCollection](const FName &SupportedListID)
			{
				const TArray<FName> *supportedList = PresetCollection.Lists.Find(SupportedListID);
				if (ensureAlways(supportedList != nullptr))
				{
					for (auto &ps : *supportedList)
					{
						if (ps == Child->PresetID)
						{
							return true;
						}
					}

					if (supportedList->Num() > 0)
					{
						// TODO: All custom presets of a given type are legal until we introduce user-managed lists
						const FCraftingTreeNodePreset *preset = PresetCollection.Presets.Find((*supportedList)[0]);
						if (preset != nullptr)
						{
							const TArray<FName> *customPresets = PresetCollection.CustomPresetsByNodeType.Find(preset->NodeType);
							if (customPresets != nullptr)
							{
								for (auto &customPreset : *customPresets)
								{
									if (customPreset == Child->PresetID)
									{
										return true;
									}
								}
							}
						}
					}
				}

				return false;
			};

			for (auto &pin : InputPins)
			{
				if ((pin.MaxCount == -1 || pin.AttachedObjects.Num() < pin.MaxCount) && pinSupportsPreset(pin.AssignedList))
				{
					// Should never be present, so ensure instead of AddUnique
					if (ensureAlways(!pin.AttachedObjects.Contains(Child)))
					{ 
						pin.AttachedObjects.Add(Child);
						Child->ParentInstance = AsShared();
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

			InstanceProperties.ForEachProperty([&OutRecord](const FString &name, const Modumate::FModumateCommandParameter &param)
			{
				OutRecord.Properties.Add(*name, param);
			});

			return ECraftingResult::Success;
		}

		ECraftingResult FCraftingTreeNodeInstance::FromDataRecord(
			FCraftingTreeNodeInstancePool &InstancePool, 
			const FCraftingPresetCollection &PresetCollection, 
			const FCustomAssemblyCraftingNodeRecord &DataRecord)
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
			InstanceProperties = preset->Properties;

			for (auto &prop : DataRecord.Properties)
			{
				FValueSpec vs(*prop.Key, prop.Value);
				InstanceProperties.SetProperty(vs.Scope, vs.Name, vs.Value);
			}

			InputPins = descriptor->InputPins;
			for (auto &ip : preset->ChildNodes)
			{
				if (ensureAlways(ip.PinSetIndex < InputPins.Num() && ip.ListIDs.Num() != 0))
				{
					FCraftingTreeNodePinSet &pinSet = InputPins[ip.PinSetIndex];

					// The first designated list in a sequence is assigned to the top of the pinset, not the last
					// This ensures that categories will be chosen from a list of categories, not the user preset at the end of the sequence
					pinSet.AssignedList = ip.ListIDs[0];
				}
			}

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
						for (auto &sequencePreset : ip.PresetIDs)
						{ 
							// Note: only create default children (if necessary) for the last item in the list, the other items are children of each other
							child = InstancePool.CreateNodeInstanceFromPreset(PresetCollection, sequenceID, sequencePreset, sequencePreset == ip.PresetIDs.Last());
							if (ensureAlways(child.IsValid()))
							{
								sequenceID = child->GetInstanceID();
							}
						}					
					}
					else
					{
						InstancePool.SetNewPresetForNode(PresetCollection, pinSet.AttachedObjects[ip.PinSetPosition].Pin()->GetInstanceID(), ip.PresetIDs.Last());
					}
				}
			}
			return ECraftingResult::Success;
		}

		ECraftingResult FCraftingPresetCollection::GetInstanceDataAsPreset(const FCraftingTreeNodeInstanceSharedPtr &Instance, FCraftingTreeNodePreset &OutPreset) const
		{
			const FCraftingTreeNodePreset *basePreset = Presets.Find(Instance->PresetID);
			if (basePreset == nullptr)
			{
				return ECraftingResult::Error;
			}

			OutPreset.NodeType = basePreset->NodeType;
			OutPreset.Properties = Instance->InstanceProperties;
			OutPreset.PresetID = basePreset->PresetID;

			for (int32 pinSetIndex = 0; pinSetIndex < Instance->InputPins.Num(); ++pinSetIndex)
			{
				const FCraftingTreeNodePinSet &pinSet = Instance->InputPins[pinSetIndex];
				for (int32 pinSetPosition = 0; pinSetPosition < pinSet.AttachedObjects.Num(); ++pinSetPosition)
				{
					FCraftingTreeNodePreset::FPinSpec &pinSpec = OutPreset.ChildNodes.AddDefaulted_GetRef();
					pinSpec.PinSetIndex = pinSetIndex;
					pinSpec.Scope = pinSet.Scope;
					pinSpec.PinSetPosition = pinSetPosition;

					FCraftingTreeNodeInstanceSharedPtr attachedOb = pinSet.AttachedObjects[pinSetPosition].Pin();
					FName assignedList = pinSet.AssignedList;

					// If an attached object is ReadOnly, it is a category selector
					// In this case, iterate down the child list of ReadOnlies and record them all in the pin sequence
					while (attachedOb.IsValid())
					{ 
						pinSpec.PresetIDs.Add(attachedOb->PresetID);
						pinSpec.ListIDs.Add(assignedList);
						
						const FCraftingTreeNodePreset *attachedPreset = Presets.Find(attachedOb->PresetID);
						if (ensureAlways(attachedPreset != nullptr))
						{
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
									assignedList = attachedOb->InputPins[0].AssignedList;
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
			CustomPresetsByNodeType.Empty();

			for (auto &presetRecord : Records)
			{
				FCraftingTreeNodePreset newPreset;
				newPreset.FromDataRecord(*this,presetRecord);
				Presets.Add(newPreset.PresetID, newPreset);
				TArray<FName> &customPresetMapArray = CustomPresetsByNodeType.FindOrAdd(newPreset.NodeType);
				customPresetMapArray.Add(newPreset.PresetID);
			}

			return ECraftingResult::Success;
		}

		ECraftingResult FCraftingTreeNodePreset::ToDataRecord(FCraftingPresetRecord &OutRecord) const
		{
			OutRecord.DisplayName = GetDisplayName();
			OutRecord.NodeType = NodeType;
			OutRecord.PresetID = PresetID;
			OutRecord.IsReadOnly = IsReadOnly;

			Properties.ForEachProperty([&OutRecord](const FString &Name, const FModumateCommandParameter &Param) {

				OutRecord.PropertyMap.Add(Name,Param.AsJSON());
			});

			for (auto &childNode : ChildNodes)
			{
				if (ensureAlways(childNode.ListIDs.Num() == childNode.PresetIDs.Num()))
				{ 
					for (int32 i = 0; i < childNode.ListIDs.Num(); ++i)
					{
						OutRecord.ChildNodePinSetIndices.Add(childNode.PinSetIndex);
						OutRecord.ChildNodePinSetPositions.Add(childNode.PinSetPosition);
						OutRecord.ChildNodePinSetListIDs.Add(childNode.ListIDs[i]);
						OutRecord.ChildNodePinSetPresetIDs.Add(childNode.PresetIDs[i]);
					}
				}
			}

			return ECraftingResult::Success;
		}

		ECraftingResult FCraftingTreeNodePreset::FromDataRecord(const FCraftingPresetCollection &PresetCollection,const FCraftingPresetRecord &Record)
		{
			NodeType = Record.NodeType;

			const FCraftingTreeNodeType *nodeType = PresetCollection.NodeDescriptors.Find(NodeType);
			if (!ensureAlways(nodeType != nullptr))
			{
				return ECraftingResult::Error;
			}

			PresetID = Record.PresetID;
			IsReadOnly = Record.IsReadOnly;

			Properties.Empty();
			ChildNodes.Empty();

			for (auto &kvp : Record.PropertyMap)
			{
				FModumateCommandParameter param;
				param.FromJSON(kvp.Value);
				Properties.SetValue(kvp.Key, param);
			}


			int32 numChildren = Record.ChildNodePinSetIndices.Num();
			if (ensureAlways(
				numChildren == Record.ChildNodePinSetPositions.Num() &&
				numChildren == Record.ChildNodePinSetListIDs.Num() &&
				numChildren == Record.ChildNodePinSetPresetIDs.Num()))
			{
				FPinSpec *pinSpec = nullptr;
				for (int32 i = 0; i < numChildren; ++i)
				{
					// Pin specs hold sequences of child presets
					// A sequence is a list of zero or more ReadOnly presets followed by a single writable terminating preset
					// When the record contains the same set index/set position pair multiple times in a row, this defines a sequence
					if (pinSpec == nullptr || 
						pinSpec->PinSetIndex != Record.ChildNodePinSetIndices[i] || 
						pinSpec->PinSetPosition != Record.ChildNodePinSetPositions[i])
					{ 
						pinSpec = &ChildNodes.AddDefaulted_GetRef();
						pinSpec->PinSetIndex = Record.ChildNodePinSetIndices[i];
						pinSpec->PinSetPosition = Record.ChildNodePinSetPositions[i];

						if (ensureAlways(pinSpec->PinSetIndex < nodeType->InputPins.Num()))
						{ 
							pinSpec->Scope = nodeType->InputPins[pinSpec->PinSetIndex].Scope;
						}
					}

					pinSpec->ListIDs.Add(Record.ChildNodePinSetListIDs[i]);
					pinSpec->PresetIDs.Add(Record.ChildNodePinSetPresetIDs[i]);
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
			OutParameterSet.SetValue(Modumate::Parameters::kChildNodePinSetListIDs, dataRecord.ChildNodePinSetListIDs);
			OutParameterSet.SetValue(Modumate::Parameters::kChildNodePinSetPresetIDs, dataRecord.ChildNodePinSetPresetIDs);

			TArray<FString> propertyNames;
			dataRecord.PropertyMap.GenerateKeyArray(propertyNames);
			OutParameterSet.SetValue(Modumate::Parameters::kPropertyNames, propertyNames);

			TArray<FString> propertyValues;
			dataRecord.PropertyMap.GenerateValueArray(propertyValues);
			OutParameterSet.SetValue(Modumate::Parameters::kPropertyValues, propertyValues);

			return ECraftingResult::Success;
		}

		ECraftingResult FCraftingTreeNodePreset::FromParameterSet(const FCraftingPresetCollection &PresetCollection, const FModumateFunctionParameterSet &ParameterSet)
		{
			FCraftingPresetRecord dataRecord;

			TArray<FString> propertyNames = ParameterSet.GetValue(Modumate::Parameters::kPropertyNames);
			TArray<FString> propertyValues = ParameterSet.GetValue(Modumate::Parameters::kPropertyValues);

			if (!ensureAlways(propertyNames.Num() == propertyValues.Num()))
			{
				return ECraftingResult::Error;
			}

			int32 numProps = propertyNames.Num();
			for (int32 i = 0; i < numProps; ++i)
			{
				dataRecord.PropertyMap.Add(propertyNames[i], propertyValues[i]);
			}

			dataRecord.DisplayName = ParameterSet.GetValue(Modumate::Parameters::kDisplayName);
			dataRecord.NodeType = ParameterSet.GetValue(Modumate::Parameters::kNodeType);
			dataRecord.PresetID = ParameterSet.GetValue(Modumate::Parameters::kPresetKey);
			dataRecord.IsReadOnly = ParameterSet.GetValue(Modumate::Parameters::kReadOnly);
			dataRecord.ChildNodePinSetIndices = ParameterSet.GetValue(Modumate::Parameters::kChildNodePinSetIndices);
			dataRecord.ChildNodePinSetPositions = ParameterSet.GetValue(Modumate::Parameters::kChildNodePinSetPositions);
			dataRecord.ChildNodePinSetListIDs = ParameterSet.GetValue(Modumate::Parameters::kChildNodePinSetListIDs);
			dataRecord.ChildNodePinSetPresetIDs = ParameterSet.GetValue(Modumate::Parameters::kChildNodePinSetPresetIDs);

			return FromDataRecord(PresetCollection,dataRecord);
		}

		/*
		Given a preset ID, recurse through all its children and gather all other presets that this one depends on
		Note: we don't empty the container because the function is recursive
		*/
		ECraftingResult FCraftingPresetCollection::GetDependentPresets(const FName &PresetID, TSet<FName> &OutPresets) const
		{
			const FCraftingTreeNodePreset *pPreset = Presets.Find(PresetID);
			if (pPreset == nullptr)
			{
				return ECraftingResult::Error;
			}

			for (auto &childNode : pPreset->ChildNodes)
			{
				/* 
				Only recurse if we haven't processed this ID yet
				Even if the same preset appears multiple times in a tree, its children will always be the same
				*/
				for (auto &presetID : childNode.PresetIDs)
				{ 
					if (!OutPresets.Contains(presetID))
					{
						OutPresets.Add(presetID);
						GetDependentPresets(presetID, OutPresets);
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


		FCraftingTreeNodeInstanceSharedPtr FCraftingTreeNodeInstancePool::CreateNodeInstanceFromDataRecord(const BIM::FCraftingPresetCollection &PresetCollection,const FCustomAssemblyCraftingNodeRecord &DataRecord)
		{
			FCraftingTreeNodeInstanceSharedPtr instance = InstancePool.Add_GetRef(MakeShareable(new FCraftingTreeNodeInstance(DataRecord.InstanceID)));

			InstanceMap.Add(DataRecord.InstanceID, instance);
			NextInstanceID = FMath::Max(NextInstanceID+1,DataRecord.InstanceID + 1);

			instance->FromDataRecord(*this,PresetCollection,DataRecord);

			return instance;
		}

		ECraftingResult FCraftingTreeNodeInstancePool::SetNewPresetForNode(const FCraftingPresetCollection &PresetCollection, int32 InstanceID, const FName &PresetID)
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

			return inst->FromDataRecord(*this,PresetCollection,dataRecord);
		}

		FCraftingTreeNodeInstanceSharedPtr FCraftingTreeNodeInstancePool::CreateNodeInstanceFromPreset(const FCraftingPresetCollection &PresetCollection,int32 ParentID, const FName &PresetID, bool CreateDefaultReadOnlyChildren)
		{
			FCraftingTreeNodeInstanceSharedPtr parent = InstanceFromID(ParentID);
			if (parent != nullptr && !parent->CanCreateChildOfType(PresetCollection, PresetID))
			{
				return nullptr;
			}

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

			instance->InstanceProperties = preset->Properties;
			instance->InputPins = descriptor->InputPins;

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
					instance->InputPins[child.PinSetIndex].AssignedList = child.ListIDs[0];
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
				for (auto &presetID : child.PresetIDs)
				{
					sequenceParent = CreateNodeInstanceFromPreset(PresetCollection, sequenceParent->GetInstanceID(), presetID, false);
				}

				// If the sequence ends with a ReadOnly preset (which will be the case for presets defined in the DDL script),
				// then we march down the default child nodes (also defined in the DDL script) until we get to the end
				// In serialized presets (saved by the user), these values will be present in the sequence looped through above
				preset = PresetCollection.Presets.Find(sequenceParent->PresetID);
				while (preset != nullptr && preset->IsReadOnly)
				{
					if (ensureAlways(preset->ChildNodes.Num()==1) && ensureAlways(preset->ChildNodes[0].PresetIDs.Num()==1))
					{
						sequenceParent = CreateNodeInstanceFromPreset(PresetCollection, sequenceParent->GetInstanceID(), preset->ChildNodes[0].PresetIDs[0],false);
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

		bool FCraftingTreeNodeInstancePool::FromDataRecord(const FCraftingPresetCollection &PresetCollection, const TArray<FCustomAssemblyCraftingNodeRecord> &InDataRecords)
		{
			ResetInstances();

			for (auto &dr : InDataRecords)
			{
				// Parents recursively create their children who might also be in the data record array already, so make sure we don't double up
				if (InstanceFromID(dr.InstanceID) == nullptr)
				{ 
					FCraftingTreeNodeInstanceSharedPtr inst = CreateNodeInstanceFromDataRecord(PresetCollection,dr);
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

			for (auto inst : InstancePool)
			{
				ret = ensureAlways(myParentPointsToMe(inst)) && ret;
				ret = ensureAlways(myChildrenPointToMe(inst)) && ret;
			}

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

		bool FCraftingPresetCollection::ParseScriptFile(const FString &FilePath, TArray<FString> &OutMessages)
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


			//Match END
			compiler.AddRule(kEnd, [&state, &currentNode, &currentPreset, &currentListName, &currentListElements, this](
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
					currentListElements.Reset();
				}
				else if (state == ReadingPreset)
				{
					// TODO validate complete preset
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
			static const FString kListMatch = kList + kWhitespace + TEXT("(") + kSimpleWord + TEXT(")");

			compiler.AddRule(kListMatch, [this, &state, &currentListName, &currentListElements](const TCHAR *originalLine,
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

			static const FString kListAssignment = TEXT("(") + kSimpleWord + TEXT(")=(") + kSimpleWord + TEXT(".") + kSimpleWord + TEXT(")");
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

				FCraftingTreeNodeType *nodeType = NodeDescriptors.Find(currentPreset.NodeType);
				if (nodeType == nullptr)
				{
					errorReporter(*FString::Printf(TEXT("Unidentified preset node type %s line %d"), *currentPreset.NodeType.ToString(), lineNum));
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
						newSpec.PresetIDs.Add(listValue);
						newSpec.ListIDs.Add(listID);
						newSpec.Scope = nodeType->InputPins[i].Scope;
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

				return;
			}, false);


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
			},
				false);

			static const FString kQuotedString = TEXT("\"([\\w\\s\\-\\.\\,\"/:]+)\"");
			static const FString kPropertyMatch = TEXT("\\w+\\.\\w+");
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
				FCraftingTreeNodeType *pBaseDescriptor = NodeDescriptors.Find(currentPreset.NodeType);

				if (pBaseDescriptor == nullptr)
				{
					errorReporter(*FString::Printf(TEXT("Unrecognized PRESET type %s at line %d"), *currentPreset.NodeType.ToString(), lineNum));
					return;
				}

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

			return compiler.ParseFile(FilePath, [&OutMessages](const FString &msg) {OutMessages.Add(msg); });
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
	BIM::FCraftingTreeNodeInstanceSharedPtr parent = NodeInstances.InstanceFromID(ParentID);
	if (parent.IsValid() && !parent->CanCreateChildOfType(PresetCollection, PresetID))
	{
		return ECraftingResult::Error;
	}

	BIM::FCraftingTreeNodeInstanceSharedPtr inst = NodeInstances.CreateNodeInstanceFromPreset(PresetCollection, ParentID, PresetID, true);

	if (ensureAlways(inst.IsValid()))
	{
		return CraftingNodeFromInstance(inst,PresetCollection,OutNode);
	}
	return ECraftingResult::Error;
}

ECraftingResult UModumateCraftingNodeWidgetStatics::CraftingNodeFromInstance(
	const Modumate::BIM::FCraftingTreeNodeInstanceSharedPtr &Instance, 
	const Modumate::BIM::FCraftingPresetCollection &PresetCollection, 
	FCraftingNode &OutNode)
{
	OutNode.CanSwapPreset = Instance->ParentInstance.IsValid();
	OutNode.InstanceID = Instance->GetInstanceID();
	OutNode.Label = FText::FromString(Instance->InstanceProperties.GetProperty(BIM::EScope::Preset, BIM::Parameters::Name));
	OutNode.ParentInstanceID = Instance->ParentInstance.IsValid() ? Instance->ParentInstance.Pin()->GetInstanceID() : 0;
	OutNode.PresetID = Instance->PresetID;
	OutNode.CanAddToProject = false;
	OutNode.PresetStatus = Instance->GetPresetStatus(PresetCollection);

	if (Instance->InputPins.Num() == 0 && ensureAlways(OutNode.PresetStatus != ECraftingNodePresetStatus::ReadOnly))
	{
		OutNode.IsEmbeddedInParent = true;
	}

	if (Instance->ParentInstance.IsValid())
	{
		Modumate::BIM::FCraftingTreeNodeInstanceSharedPtr parent = Instance->ParentInstance.Pin();
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

	const Modumate::BIM::FCraftingTreeNodePreset *preset = PresetCollection.Presets.Find(Instance->PresetID);
	if (ensureAlways(preset != nullptr))
	{
		const Modumate::BIM::FCraftingTreeNodeType *nodeType = PresetCollection.NodeDescriptors.Find(preset->NodeType);
		if (ensureAlways(nodeType != nullptr))
		{
			OutNode.NodeIconType = nodeType->IconType;
		}
	}

	//TODO: Provide an array of ReadOnly nodes that can be embedded into this node
	//OutNode.EmbeddedInstanceIDs = Array of embedded instances;
	return ECraftingResult::Success;
}

ECraftingResult UModumateCraftingNodeWidgetStatics::GetInstantiatedNodes(
	Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances, 
	const Modumate::BIM::FCraftingPresetCollection &PresetCollection,
	TArray<FCraftingNode> &OutNodes)
{
	if (ensureAlways(NodeInstances.GetInstancePool().Num() > 0))
	{
		Algo::Transform(NodeInstances.GetInstancePool(),OutNodes,[&PresetCollection](const Modumate::BIM::FCraftingTreeNodeInstanceSharedPtr &Node)
		{
			FCraftingNode outNode;
			CraftingNodeFromInstance(Node, PresetCollection, outNode);
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
		group.OwnerInstance = NodeID;

		const TArray<FName> *assignedList = PresetCollection.Lists.Find(pin.AssignedList);
		if (ensureAlways(assignedList != nullptr))
		{
			for (auto &preset : *assignedList)
			{
				group.SupportedTypePins.Add(preset);
			}
		}

		for (auto &ao : pin.AttachedObjects)
		{
			group.AttachedChildren.Add(ao.Pin()->GetInstanceID());
		}

		// TODO: GroupName is used to pass SetName for now. Will determine which approach is more appropriate in the future 
		if (group.GroupName.IsNone())
		{
			group.GroupName = pin.SetName;
		}
#if 0
		// TODO: variable length pin lists will need a per-pin naming scheme, not just per-group
		if (group.AttachedChildren.Num() > 0)
		{
			BIM::FCraftingTreeNodeInstanceSharedPtr child = NodeInstances.InstanceFromID(group.AttachedChildren[0]);
			if (ensureAlways(child.IsValid()))
			{
				group.GroupName = child->InstanceProperties.GetProperty(BIM::EScope::Preset, BIM::Parameters::Name);
			}
		}

		if (group.GroupName.IsNone())
		{
			group.GroupName = pin.AssignedList;
		}
#endif
		const TArray<FName> *customPresets = PresetCollection.CustomPresetsByNodeType.Find(group.GroupName);
		if (customPresets != nullptr)
		{
			for (auto &customPreset : *customPresets)
			{
				group.SupportedTypePins.Add(customPreset);
			}
		}
	}
	return ECraftingResult::Success;
}

ECraftingResult UModumateCraftingNodeWidgetStatics::DragMovePinChild(Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances, int32 InstanceID, const FName &PinGroup, int32 From, int32 To)
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

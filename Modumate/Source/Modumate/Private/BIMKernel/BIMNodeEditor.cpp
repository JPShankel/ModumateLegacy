// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/BIMNodeEditor.h"

#include "Database/ModumateObjectDatabase.h"
#include "Database/ModumateArchitecturalMaterial.h"
#include "ModumateCore/ModumateScriptProcessor.h"
#include "DocumentManagement/ModumateCommands.h"
#include "BIMKernel/BIMWidgetStatics.h"
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
				for (auto &pin : currentNode->AttachedChildren)
				{
					for (auto &ao : pin.Children)
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

				const FCraftingTreeNodePreset *preset = PresetCollection.Presets.Find(inst->PresetID);
				if (ensureAlways(preset != nullptr))
				{
					const FCraftingTreeNodeType *nodeType = PresetCollection.NodeDescriptors.Find(preset->NodeType);
					if (ensureAlways(nodeType != nullptr))
					{
						if (nodeType->Scope == EBIMValueScope::Layer)
						{
							OutAssemblySpec.LayerProperties.AddDefaulted();
							currentSheet = &OutAssemblySpec.LayerProperties.Last();
						}
						if (nodeType->Scope != EBIMValueScope::None)
						{
							pinScope = nodeType->Scope;
						}
					}
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

				for (auto &inputPin : inst->AttachedChildren)
				{
					for (auto &ao : inputPin.Children)
					{
						nodeStack.Push(ao.Pin());
						scopeStack.Push(pinScope);
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
			for (auto &pin : AttachedChildren)
			{
				if (pin.Children.Contains(Child))
				{
					if (pin.Children.Num() <= pin.SetType.MinCount)
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
			int32 pinSetIndex = -1, pinSetPosition = -1;
			ECraftingResult result = FindChild(ChildID, pinSetIndex, pinSetPosition);
			if (result == ECraftingResult::Success)
			{
				Order = 0;
				for (int32 i = 0; i < pinSetIndex - 1; ++i)
				{
					Order += AttachedChildren[i].Children.Num();
				}
				Order += pinSetPosition;
			}
			return result;
		}

		ECraftingResult FCraftingTreeNodeInstance::FindChild(int32 ChildID, int32 &OutPinSetIndex, int32 &OutPinSetPosition)
		{
			for (int32 pinIndex = 0; pinIndex < AttachedChildren.Num(); ++pinIndex)
			{
				FCraftingTreeNodeAttachedChildren &pinSet = AttachedChildren[pinIndex];
				for (int32 pinPosition = 0; pinPosition < pinSet.Children.Num(); ++pinPosition)
				{
					if (pinSet.Children[pinPosition].Pin()->GetInstanceID() == ChildID)
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
				for (auto &pin : ParentInstance.Pin()->AttachedChildren)
				{
					for (auto ao : pin.Children)
					{
						if (ao.HasSameObject(this))
						{
							pin.Children.Remove(ao);
							break;
						}
					}
				}
			}
			ParentInstance = nullptr;
			return ECraftingResult::Success;
		}

		ECraftingResult FCraftingTreeNodeInstance::AttachChildAt(const FCraftingPresetCollection &PresetCollection, const FCraftingTreeNodeInstanceSharedPtr &Child, int32 PinSetIndex, int32 PinSetPosition)
		{
			if (AttachedChildren.Num() <= PinSetIndex)
			{
				AttachedChildren.SetNum(PinSetIndex+1);
			}
			if (AttachedChildren[PinSetIndex].Children.Num() <= PinSetPosition)
			{
				AttachedChildren[PinSetIndex].Children.SetNum(PinSetPosition + 1);
			}
			AttachedChildren[PinSetIndex].Children[PinSetPosition] = Child;
			return ECraftingResult::Success;
		}

		ECraftingResult FCraftingTreeNodeInstance::AttachChild(const FCraftingPresetCollection &PresetCollection, const FCraftingTreeNodeInstanceSharedPtr &Child)
		{
			if (AttachedChildren.Num() == 0)
			{
				AttachedChildren.AddDefaulted();
			}

			AttachedChildren[0].Children.Add(Child);

			return ECraftingResult::Success;
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
			for (auto &pin : AttachedChildren)
			{
				while (pin.Children.Num() > 0)
				{
					TArray<int32> destroyedChildren;
					InstancePool.DestroyNodeInstance(pin.Children[0].Pin()->GetInstanceID(), destroyedChildren);
				}
			}

			PresetID = DataRecord.PresetID;
			InstanceProperties.FromDataRecord(DataRecord.PropertyRecord);

			for (auto &ip : preset->ChildPresets)
			{
				if (ensureAlways(ip.ParentPinSetIndex < AttachedChildren.Num()))
				{
					// The first designated list in a sequence is assigned to the top of the pinset, not the last
					// This ensures that categories will be chosen from a list of categories, not the user preset at the end of the sequence
					FCraftingTreeNodeAttachedChildren &pinSet = AttachedChildren[ip.ParentPinSetIndex];
				}
			}

			if (!RecursePresets)
			{
				FCraftingTreeNodeInstanceSharedPtr parent = InstancePool.InstanceFromID(DataRecord.ParentID);
				if (parent.IsValid())
				{
					parent->AttachChildAt(PresetCollection, AsShared(), DataRecord.PinSetIndex, DataRecord.PinSetPosition);
				}
			}
			else
			{
				for (auto &ip : preset->ChildPresets)
				{
					if (ensureAlways(ip.ParentPinSetIndex < AttachedChildren.Num()))
					{
						FCraftingTreeNodeAttachedChildren &pinSet = AttachedChildren[ip.ParentPinSetIndex];
						if (ip.ParentPinSetPosition == pinSet.Children.Num())
						{
							InstancePool.CreateNodeInstanceFromPreset(PresetCollection, DataRecord.InstanceID, ip.PresetID, ip.ParentPinSetIndex,ip.ParentPinSetPosition);
						}
						else
						{
							InstancePool.SetNewPresetForNode(PresetCollection, pinSet.Children[ip.ParentPinSetPosition].Pin()->GetInstanceID(), ip.PresetID);
						}
					}
				}
			}
			return ECraftingResult::Success;
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

		FCraftingTreeNodeInstanceSharedPtr FCraftingTreeNodeInstancePool::CreateNodeInstanceFromDataRecord(const BIM::FCraftingPresetCollection &PresetCollection, const FCustomAssemblyCraftingNodeRecord &DataRecord, bool RecursePresets)
		{
			FCraftingTreeNodeInstanceSharedPtr instance = InstancePool.Add_GetRef(MakeShareable(new FCraftingTreeNodeInstance(DataRecord.InstanceID)));

			InstanceMap.Add(DataRecord.InstanceID, instance);
			NextInstanceID = FMath::Max(NextInstanceID + 1, DataRecord.InstanceID + 1);

			instance->FromDataRecord(*this, PresetCollection, DataRecord, RecursePresets);

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

		FCraftingTreeNodeInstanceSharedPtr FCraftingTreeNodeInstancePool::CreateNodeInstanceFromPreset(const FCraftingPresetCollection &PresetCollection, int32 ParentID, const FName &PresetID, int32 ParentSetIndex, int32 ParentSetPosition)
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
			instance->CurrentOrientation = preset->Orientation;
			instance->InstanceProperties = preset->Properties;

			FCraftingTreeNodeInstanceSharedPtr parent = InstanceFromID(ParentID);
			if (parent != nullptr)
			{
				instance->ParentInstance = parent;
				ensureAlways(parent->AttachChildAt(PresetCollection, instance,ParentSetIndex,ParentSetPosition) != ECraftingResult::Error);
			}

			for (auto &childPreset : preset->ChildPresets)
			{
				CreateNodeInstanceFromPreset(PresetCollection, instance->GetInstanceID(), childPreset.PresetID, childPreset.ParentPinSetIndex, childPreset.ParentPinSetPosition);
			}

			ValidatePool();
			return instance;
		}

		bool FCraftingTreeNodeInstancePool::FromDataRecord(const FCraftingPresetCollection &PresetCollection, const TArray<FCustomAssemblyCraftingNodeRecord> &InDataRecords, bool RecursePresets)
		{
			ResetInstances();

			for (auto &dr : InDataRecords)
			{
				// Parents recursively create their children who might also be in the data record array already, so make sure we don't double up
				if (InstanceFromID(dr.InstanceID) == nullptr)
				{
					FCraftingTreeNodeInstanceSharedPtr inst = CreateNodeInstanceFromDataRecord(PresetCollection, dr, RecursePresets);
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
				for (auto &op : me->AttachedChildren)
				{
					for (auto &oa : op.Children)
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
				for (auto &op : me->ParentInstance.Pin()->AttachedChildren)
				{
					for (auto &oa : op.Children)
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
	}
}
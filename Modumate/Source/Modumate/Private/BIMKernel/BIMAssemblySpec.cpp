// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/BIMAssemblySpec.h"
#include "BIMKernel/BIMPresets.h"
#include "BIMKernel/BIMNodeEditor.h"

ECraftingResult FBIMAssemblySpec::FromPreset(const FBIMPresetCollection& PresetCollection, const FName& PresetID)
{
	ECraftingResult ret = ECraftingResult::Success;
	RootPreset = PresetID;

	FBIMPropertySheet* currentSheet = &RootProperties;

	TArray<FName> presetStack;
	presetStack.Push(PresetID);

	TArray<EBIMValueScope> scopeStack;
	scopeStack.Push(EBIMValueScope::Assembly);

	while (presetStack.Num() > 0)
	{
		FName presetID = presetStack.Pop();
		EBIMValueScope pinScope = scopeStack.Pop();

		const FBIMPreset* preset = PresetCollection.Presets.Find(presetID);
		if (preset == nullptr)
		{
			ret = ECraftingResult::Error;
		}
		else
		{
			const FCraftingTreeNodeType* nodeType = PresetCollection.NodeDescriptors.Find(preset->NodeType);
			if (ensureAlways(nodeType != nullptr))
			{
				if (nodeType->Scope == EBIMValueScope::Layer)
				{
					LayerProperties.AddDefaulted();
					currentSheet = &LayerProperties.Last();
				}
				if (nodeType->Scope != EBIMValueScope::None)
				{
					pinScope = nodeType->Scope;
				}
			}

			EObjectType objectType = PresetCollection.GetPresetObjectType(presetID);
			if (objectType != EObjectType::OTNone && ensureAlways(ObjectType == EObjectType::OTNone))
			{
				ObjectType = objectType;
			}

			preset->Properties.ForEachProperty([this, &currentSheet, &preset, pinScope, PresetID](const FString& Name, const Modumate::FModumateCommandParameter& MCP)
			{
				FBIMPropertyValue vs(*Name);

				// 'Node' scope values inherit their scope from their parents, specified on the pin
				EBIMValueScope nodeScope = vs.Scope == EBIMValueScope::Node ? pinScope : vs.Scope;

				// Preset properties only apply to the preset itself, not to its children
				if (nodeScope != EBIMValueScope::Preset || preset->PresetID == PresetID)
				{
					currentSheet->SetProperty(nodeScope, vs.Name, MCP);
				}
			});

			for (auto& childPreset : preset->ChildPresets)
			{
				presetStack.Push(childPreset.PresetID);
				scopeStack.Push(pinScope);
			}
		}
	}

	// All assembly specs must bind to an object type
	if (ObjectType == EObjectType::OTNone)
	{
		return ECraftingResult::Error;
	}

	return ret;
}
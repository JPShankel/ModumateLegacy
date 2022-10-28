// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/Presets/BIMPresetNCPTaxonomy.h"
#include "ModumateCore/EnumHelpers.h"
#include "Algo/Sort.h"
#include "Json.h"


template<>
void FBIMPresetTaxonomyNode::PopulateField(TSharedPtr<FJsonObject> JsonObject, const FString& FieldName, FString& OutField)
{
	OutField = JsonObject->Values[FieldName]->AsString();
}

template <>
void FBIMPresetTaxonomyNode::PopulateField<FText>(TSharedPtr<FJsonObject> JsonObject, const FString& FieldName,
	FText& OutField)
{
	OutField = FText::FromString(JsonObject->Values[FieldName]->AsString());
}

template <>
void FBIMPresetTaxonomyNode::PopulateField<FName>(TSharedPtr<FJsonObject> JsonObject, const FString& FieldName,
	FName& OutField)
{
	OutField = FName(*JsonObject->Values[FieldName]->AsString());
}

template<>
void FBIMPresetTaxonomyNode::PopulateField(TSharedPtr<FJsonObject> JsonObject, const FString& FieldName, double& OutField)
{
	OutField = JsonObject->Values[FieldName]->AsNumber();
}

template<>
void FBIMPresetTaxonomyNode::PopulateField(TSharedPtr<FJsonObject> JsonObject, const FString& FieldName, float& OutField)
{
	OutField = JsonObject->Values[FieldName]->AsNumber();
}

template<>
void FBIMPresetTaxonomyNode::PopulateField(TSharedPtr<FJsonObject> JsonObject, const FString& FieldName, int& OutField)
{
	OutField = JsonObject->Values[FieldName]->AsNumber();
}

template<>
void FBIMPresetTaxonomyNode::PopulateField(TSharedPtr<FJsonObject> JsonObject, const FString& FieldName, TSharedPtr<FJsonObject>& OutField)
{
	OutField = JsonObject->Values[FieldName]->AsObject();
}

template<>
void FBIMPresetTaxonomyNode::PopulateField(TSharedPtr<FJsonObject> JsonObject, const FString& FieldName, TArray<TSharedPtr<FJsonValue>>& OutField)
{
	OutField = JsonObject->Values[FieldName]->AsArray();
}

template<>
void FBIMPresetTaxonomyNode::PopulateField(TSharedPtr<FJsonObject> JsonObject, const FString& FieldName, bool& OutField)
{
	OutField = JsonObject->Values[FieldName]->AsBool();
}

void FBIMPresetTaxonomyNode::FromJson(TSharedPtr<FJsonObject> JsonObject)
{
	FString ncpPath;
	if(PopulateFieldFromJson(JsonObject, TEXT("ncp"), EJson::String,  ncpPath, true))
	{
		this->tagPath = ncpPath;
	}

	FString taxoNodStr;
	this->tagPath.ToString(taxoNodStr);
	
	PopulateFieldFromJson(JsonObject, TEXT("displayName"), EJson::String, this->displayName, false);
	PopulateFieldFromJson(JsonObject, TEXT("BIMDesignerNodeTitle"), EJson::String,  this->designerTitle, false);
	PopulateFieldFromJson(JsonObject, TEXT("tooltip"), EJson::String,  this->toolTip, false);
	PopulateFieldFromJson(JsonObject, TEXT("stopNCPTraversal"), EJson::Boolean,  this->blockUpwardTraversal, true);
	

	PopulateEnumsFromJson(JsonObject, TEXT("objectType"), this->objectType, false);
	PopulateEnumsFromJson(JsonObject, TEXT("assetType"), this->assetType, false);
	PopulateEnumsFromJson(JsonObject, TEXT("scope"), this->Scope, false);
	PopulateEnumsFromJson(JsonObject, TEXT("measurementMethods"), this->MeasurementMethod, false);
	

	//PinSets
	TArray<TSharedPtr<FJsonValue>> pinSetsJson;
	if(PopulateFieldFromJson(JsonObject, TEXT("pinSets"), EJson::Array, pinSetsJson, false))
	{
		for(const auto& pinSetEntry : pinSetsJson)
		{
			if(pinSetEntry->Type == EJson::Object)
			{
				TSharedPtr<FJsonObject> pinObj = pinSetEntry->AsObject();
				FBIMPresetNodePinSet& pinSetNode = this->PinSets.AddDefaulted_GetRef();
				
				FString maxStr;
				FString minStr;

				PopulateFieldFromJson(pinObj, TEXT("max"), EJson::String, maxStr, true);
				PopulateFieldFromJson(pinObj, TEXT("min"), EJson::String, minStr, true);
				
				pinSetNode.MaxCount = maxStr.Len() != 0 ? FCString::Atoi(*maxStr) : -1;
				pinSetNode.MinCount = minStr.Len() != 0 ? FCString::Atoi(*minStr) : -1;

				FString setName;
				if(PopulateFieldFromJson(pinObj, TEXT("name"), EJson::String, setName, true))
				{
					pinSetNode.SetName = FName(*setName);	
				}

				PopulateEnumsFromJson(pinObj, TEXT("pinTarget"), pinSetNode.PinTarget, true);
			}
		}	
	}

	
	//Properties
	TSharedPtr<FJsonObject> propertiesObject;
	
	if(PopulateFieldFromJson(JsonObject, TEXT("properties"), EJson::Object, propertiesObject, false))
	{
		TMap<FString, TSharedPtr<FJsonValue>> propertiesEntries = propertiesObject->Values;
		for(const auto& entry : propertiesEntries)
		{
			FBIMWebPresetPropertyDef prop;
			prop.FromJson(entry.Value->AsObject());
			this->Properties.Add(entry.Key, prop);
		}
	}
}

EBIMResult FBIMPresetNCPTaxonomy::FromWebTaxonomy(TSharedPtr<FJsonObject> Taxonomy)
{
	EBIMResult result = EBIMResult::Error;
	TMap<FString, TSharedPtr<FJsonValue>> topLevelEntries = Taxonomy->Values;
	for(const auto& entry : topLevelEntries)
	{
		//If there are ANY entries, consider this a success!
		result = EBIMResult::Success;
		FBIMPresetTaxonomyNode& newNode = nodes.AddDefaulted_GetRef();
		TSharedPtr<FJsonObject> taxonomyEntry = entry.Value->AsObject();
		newNode.FromJson(taxonomyEntry);
	}

	return result;
}

EBIMResult FBIMPresetNCPTaxonomy::GetFirstPartialMatch(const FBIMTagPath& TagPath, FBIMPresetTaxonomyNode& OutNode) const
{
	for (auto& node : nodes)
	{
		if (node.tagPath.MatchesPartial(TagPath))
		{
			OutNode = node;
			return EBIMResult::Success;
		}
	}
	return EBIMResult::Error;
}

EBIMResult FBIMPresetNCPTaxonomy::GetAllPartialMatches(const FBIMTagPath& TagPath, TArray<FBIMPresetTaxonomyNode>& OutNodes) const
{
	for (auto& node : nodes)
	{
		if (node.tagPath.MatchesPartial(TagPath))
		{
			OutNodes.Add(node);
		}
	}
	return EBIMResult::Success;
}

EBIMResult FBIMPresetNCPTaxonomy::GetExactMatch(const FBIMTagPath& TagPath, FBIMPresetTaxonomyNode& OutNode) const
{
	for (auto& node : nodes)
	{
		if (node.tagPath.MatchesExact(TagPath))
		{
			OutNode = node;
			return EBIMResult::Success;
		}
	}
	return EBIMResult::Error;
}

int32 FBIMPresetNCPTaxonomy::GetNodePosition(const FBIMTagPath& TagPath) const
{
	for (const auto& node : nodes)
	{
		if (node.tagPath.MatchesExact(TagPath))
		{
			return &node - &nodes[0];
		}
	}
	return INDEX_NONE;
}

void FBIMPresetNCPTaxonomy::GetPropertiesForTaxonomyNode(const FBIMTagPath& TagPath, TMap<FString, FBIMWebPresetPropertyDef>& OutProperties) const
{
	FBIMTagPath path = TagPath;
	while (path.Tags.Num() > 0)
	{
		FBIMPresetTaxonomyNode node;
		if (GetExactMatch(path, node) != EBIMResult::Error)
		{
			OutProperties.Append(node.Properties);
		}
		path.Tags.Pop();
	}

	// Get the root node properties
	FBIMPresetTaxonomyNode rootNode;
	FBIMTagPath rootPath;
	rootPath.FromString(FString::Printf(TEXT("*")));
	if (GetExactMatch(rootPath, rootNode) != EBIMResult::Error)
	{
		OutProperties.Append(rootNode.Properties);
	}
	
}


bool FBIMPresetNCPTaxonomy::SearchBranchFor(const FBIMTagPath& TagPath, FBIMPresetTaxonomyNode& OutNode, bool(Evaluator)(const FBIMPresetTaxonomyNode& Node)) const
{
	FBIMTagPath searchPath = TagPath;
	do
	{
		FBIMPresetTaxonomyNode node;
		if(GetExactMatch(searchPath, node) != EBIMResult::Error)
		{
			if(Evaluator(node))
			{
				OutNode = node;
				return true;
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Failure to get node"));
		}
		searchPath = GetParent(searchPath);
	} while(searchPath.Tags.Num() != 0);

	return false;
}

EBIMResult FBIMPresetNCPTaxonomy::PopulateDefaults(FBIMPresetInstance& outPreset) const
{
	outPreset.ObjectType = GetObjectType(outPreset.MyTagPath);
	outPreset.AssetType = GetAssetType(outPreset.MyTagPath);
	outPreset.CategoryTitle = GetCategoryTitle(outPreset.MyTagPath);
	outPreset.NodeScope = GetNodeScope(outPreset.MyTagPath);
	outPreset.MeasurementMethod = GetMeasurementMethod(outPreset.MyTagPath);
	outPreset.PinSets = GetPinSets(outPreset.MyTagPath);

	return EBIMResult::Success;
}

EObjectType FBIMPresetNCPTaxonomy::GetObjectType(const FBIMTagPath& TagPath) const
{
	FBIMPresetTaxonomyNode taxoNode;
	if(SearchBranchFor(TagPath, taxoNode,
		[](const FBIMPresetTaxonomyNode& n){ return n.objectType != EObjectType::OTNone; }))
	{
		return taxoNode.objectType;
	}
	return EObjectType::OTNone;
}

EBIMValueScope FBIMPresetNCPTaxonomy::GetNodeScope(const FBIMTagPath& TagPath) const
{
	FBIMPresetTaxonomyNode taxoNode;
	if(SearchBranchFor(TagPath, taxoNode,
		[](const FBIMPresetTaxonomyNode& n){ return n.Scope != EBIMValueScope::None; }))
	{
		return taxoNode.Scope;
	}
	return EBIMValueScope::None;
}

EBIMAssetType FBIMPresetNCPTaxonomy::GetAssetType(const FBIMTagPath& TagPath) const
{
	FBIMPresetTaxonomyNode taxoNode;
	if(SearchBranchFor(TagPath, taxoNode,
		[](const FBIMPresetTaxonomyNode& n){ return n.assetType != EBIMAssetType::None; }))
	{
		return taxoNode.assetType;
	}
	return EBIMAssetType::None;
}

FBIMPresetForm FBIMPresetNCPTaxonomy::GetFormTemplate(const FBIMTagPath& TagPath) const
{
	FBIMPresetForm form;
	
	FBIMPresetTaxonomyNode ncpNode;
	// we should always have a valid tagpath
	if (ensure(GetExactMatch(TagPath, ncpNode) == EBIMResult::Success))
	{
		TMap<FString, FBIMWebPresetPropertyDef> props;
		GetPropertiesForTaxonomyNode(TagPath, props);
		for (auto prop : props)
		{
			// Add properties that are visible to the bimdesigner
			TArray<FString> parts;
	        prop.Key.ParseIntoArray(parts, TEXT("."));
	        if (ensureAlways(parts.Num() == 2))
	        {
	        	if (prop.Value.isBIMVisible && prop.Value.isEditable && prop.Value.formElement != EBIMPresetEditorField::None)
	        	{
	        		form.AddPropertyElement(FText::FromString(prop.Value.name), FName(*parts[0]), FName(*parts[1]), prop.Value.formElement);
	        	}
	        }
		}	
	}

	return form;
}

EPresetMeasurementMethod FBIMPresetNCPTaxonomy::GetMeasurementMethod(const FBIMTagPath& TagPath) const
{
	FBIMPresetTaxonomyNode taxoNode;
	if(SearchBranchFor(TagPath, taxoNode,
		[](const FBIMPresetTaxonomyNode& n){ return n.MeasurementMethod != EPresetMeasurementMethod::None; }))
	{
		return taxoNode.MeasurementMethod;
	}
	return EPresetMeasurementMethod::None;
}

FText FBIMPresetNCPTaxonomy::GetCategoryTitle(const FBIMTagPath& TagPath) const
{

	FBIMPresetTaxonomyNode taxoNode;
	if(SearchBranchFor(TagPath, taxoNode,
		[](const FBIMPresetTaxonomyNode& n){ return !n.designerTitle.IsEmpty();}))
	{
		return taxoNode.designerTitle;
	}
	return FText::FromString(TEXT(""));
}

TArray<FBIMPresetNodePinSet> FBIMPresetNCPTaxonomy::GetPinSets(const FBIMTagPath& TagPath) const
{
	FBIMPresetTaxonomyNode taxoNode;
	if(SearchBranchFor(TagPath, taxoNode,
	[](const FBIMPresetTaxonomyNode& n){ return n.PinSets.Num() > 0;}))
	{
		return taxoNode.PinSets;
	}
	return{};
}



FBIMTagPath FBIMPresetNCPTaxonomy::GetParent(const FBIMTagPath& TagPath)
{
	FBIMTagPath Parent = TagPath;
	if(Parent.Tags.Num() > 0)
	{
		Parent.Tags.Pop();	
	}
	return Parent;
}

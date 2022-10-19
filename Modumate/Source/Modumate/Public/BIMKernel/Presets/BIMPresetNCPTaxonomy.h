// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BIMKernel/Presets/BIMPresetInstance.h"
#include "BIMKernel/Presets/BIMPresetTypeDefinition.h"
#include "BIMKernel/Core/BIMProperties.h"

#include "BIMKernel/Core/BIMTagPath.h"
#include "BIMKernel/Presets/BIMWebPreset.h"
#include "Objects/ModumateObjectEnums.h"
#include "Serialization/Csv/CsvParser.h"
#include "BIMPresetNCPTaxonomy.generated.h"

USTRUCT()
struct FBIMPresetTaxonomyNode
{
	GENERATED_BODY()

	UPROPERTY()
	FBIMTagPath tagPath;

	UPROPERTY()
	FText displayName;

	UPROPERTY()
	FText designerTitle;

	UPROPERTY()
	FText toolTip;

	UPROPERTY()
	EObjectType objectType = EObjectType::OTNone;

	UPROPERTY()
	EBIMAssetType assetType = EBIMAssetType::None;

	UPROPERTY()
	EBIMValueScope Scope = EBIMValueScope::None;

	UPROPERTY()
	TArray<FBIMPresetNodePinSet> PinSets;

	UPROPERTY()
	TMap<FString, FBIMWebPresetPropertyDef> Properties;

	UPROPERTY()
	bool blockUpwardTraversal = false;

	UPROPERTY()
	EPresetMeasurementMethod MeasurementMethod = EPresetMeasurementMethod::None;
	
	void FromJson(TSharedPtr<FJsonObject> JsonObject);

	//PRESET_INTEGRATION_TODO: Place all this JSon template stuff in to its own Helper
	template<class T>
	void PopulateField(TSharedPtr<FJsonObject> JsonObject, const FString& FieldName, T& OutField)
	{
		UE_LOG(LogTemp, Warning, TEXT("Type specified is of unsupported type: %s"), *FieldName);
	}

	template<>
	void PopulateField(TSharedPtr<FJsonObject> JsonObject, const FString& FieldName, FString& OutField);
	template<>
	void PopulateField(TSharedPtr<FJsonObject> JsonObject, const FString& FieldName, FText& OutField);
	template<>
	void PopulateField(TSharedPtr<FJsonObject> JsonObject, const FString& FieldName, FName& OutField);
	template<>
	void PopulateField(TSharedPtr<FJsonObject> JsonObject, const FString& FieldName, double& OutField);
	template<>
	void PopulateField(TSharedPtr<FJsonObject> JsonObject, const FString& FieldName, float& OutField);
	template<>
	void PopulateField(TSharedPtr<FJsonObject> JsonObject, const FString& FieldName, int& OutField);
	template<>
	void PopulateField(TSharedPtr<FJsonObject> JsonObject, const FString& FieldName, TSharedPtr<FJsonObject>& OutField);
	template<>
	void PopulateField(TSharedPtr<FJsonObject> JsonObject, const FString& FieldName, TArray<TSharedPtr<FJsonValue>>& OutField);
	template<>
	void PopulateField(TSharedPtr<FJsonObject> JsonObject, const FString& FieldName, bool& OutField);
	
	
	
	template<class T>
	bool PopulateFieldFromJson(TSharedPtr<FJsonObject> JsonObject, const FString& FieldName, const EJson& FieldType, T& OutField, bool bIsRequired)
	{
		if(!JsonObject->Values[FieldName].IsValid())
		{
			if(bIsRequired) UE_LOG(LogTemp, Warning, TEXT("Invalid field specified: %s"), *FieldName);
			return false;
		}

		if(JsonObject->Values[FieldName]->IsNull())
		{
			if(bIsRequired) UE_LOG(LogTemp, Warning, TEXT("Field specified IsNull(): %s"), *FieldName);
			return false;
		}

		if(JsonObject->Values[FieldName]->Type != FieldType)
		{
			if(bIsRequired) UE_LOG(LogTemp, Warning, TEXT("Field specified is of wrong type: %s"), *FieldName);
			return false;
		}

		PopulateField(JsonObject, FieldName, OutField);
		return true;
	}

	template<class T>
	bool PopulateEnumsFromJson(TSharedPtr<FJsonObject> JsonObject,  const FString& FieldName, T& OutField, bool bIsRequired)
	{
		FString EnumAsString;
		if(PopulateFieldFromJson(JsonObject, FieldName, EJson::String, EnumAsString, bIsRequired))
		{
			bool rtn = FindEnumValueByName(*EnumAsString, OutField); 
			return rtn;
		}
		return false;
	}
};

USTRUCT()
struct FBIMPresetNCPTaxonomy
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FBIMPresetTaxonomyNode> nodes;

	EBIMResult FromWebTaxonomy(TSharedPtr<FJsonObject> Taxonomy);

	EBIMResult GetFirstPartialMatch(const FBIMTagPath& TagPath, FBIMPresetTaxonomyNode& OutNode) const;
	EBIMResult GetAllPartialMatches(const FBIMTagPath& TagPath, TArray<FBIMPresetTaxonomyNode>& OutNodes) const;
	EBIMResult GetExactMatch(const FBIMTagPath& TagPath, FBIMPresetTaxonomyNode& OutNode) const;
	int32 GetNodePosition(const FBIMTagPath& TagPath) const;

	void GetPropertiesForTaxonomyNode(const FBIMTagPath& TagPath, TMap<FString, FBIMWebPresetPropertyDef>& OutProperties) const;
	EObjectType GetObjectType(const FBIMTagPath& TagPath) const;
	EBIMValueScope GetNodeScope(const FBIMTagPath& TagPath) const;
	EBIMAssetType GetAssetType(const FBIMTagPath& TagPath) const;
	FBIMPresetForm GetFormTemplate(const FBIMTagPath& TagPath) const;
	
	EBIMResult PopulateDefaults(FBIMPresetInstance& outPreset) const;

	EPresetMeasurementMethod GetMeasurementMethod(const FBIMTagPath& TagPath) const;
	FText GetCategoryTitle(const FBIMTagPath& TagPath) const;
	
	TArray<FBIMPresetNodePinSet> GetPinSets(const FBIMTagPath& TagPath) const;
	static FBIMTagPath GetParent(const FBIMTagPath& TagPath);

private:
	bool SearchBranchFor(const FBIMTagPath& TagPath, FBIMPresetTaxonomyNode& OutNode, bool(Evaluator)(const FBIMPresetTaxonomyNode& Node)) const;
};
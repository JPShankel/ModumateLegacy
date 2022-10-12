// Copyright 2022 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BIMKernel/Core/BIMTagPath.h"
#include "BIMKernel/Presets/PresetOrigination.h"
#include "ModumateCore/EnumHelpers.h"

#include "BIMWebPreset.generated.h"

UENUM()
enum class EBIMWebPresetPropertyType : uint8
{
	none = 0,
	string,
	number,
	boolean,
};

USTRUCT()
struct MODUMATE_API FBIMWebPresetProperty
{
	GENERATED_BODY()

	UPROPERTY()
	FString key;
	
	UPROPERTY()
	TArray<FString> value;
};

USTRUCT()
struct MODUMATE_API FBIMWebPresetPropertyDef
{
	GENERATED_BODY()

	UPROPERTY()
	FString key;

	UPROPERTY()
	FString name;
	
	UPROPERTY()
	EBIMWebPresetPropertyType type = EBIMWebPresetPropertyType::string;

	UPROPERTY()
	EBIMValueType role = EBIMValueType::None;
	
	UPROPERTY()
	bool isEditable = false;
	
	UPROPERTY()
	bool isBIMVisible = false;

	UPROPERTY()
	bool isMarketplaceVisible = false;
	
	UPROPERTY()
	bool isScheduleVisible = false;

	void FromJson(TSharedPtr<FJsonObject> JsonObject)
	{
		this->key = JsonObject->Values["propertyKey"]->AsString();
		this->name = JsonObject->Values["displayName"]->AsString();
		FindEnumValueByName(*JsonObject->Values["type"]->AsString(), this->type);
		if (!JsonObject->Values["role"]->IsNull())
		{
			FindEnumValueByName(*JsonObject->Values["role"]->AsString(), this->role);
		}
		this->isEditable = JsonObject->Values["isEditable"]->AsBool();
		this->isBIMVisible = JsonObject->Values["isBIMVisible"]->AsBool();
		this->isMarketplaceVisible = JsonObject->Values["isMarketplaceVisible"]->AsBool();
		this->isScheduleVisible = JsonObject->Values["isScheduleVisible"]->AsBool();
	}
};

USTRUCT()
struct MODUMATE_API FBIMWebPreset
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid guid;

	UPROPERTY()
	FString name;

	UPROPERTY()
	EPresetOrigination origination;

	UPROPERTY()
	FGuid canonicalBase;
	
	UPROPERTY()
	FBIMTagPath tagPath;

	// deprecated
	UPROPERTY()
	FString customDataJSON;

	// deprecated
	UPROPERTY()
	FString typeMark;
	
	UPROPERTY()
	TMap<FString, FBIMWebPresetProperty> properties;
};

USTRUCT()
struct MODUMATE_API FWebQuantity
{
	GENERATED_BODY()

	UPROPERTY()
	float Count = 0.0f;

	UPROPERTY()
	float Linear = 0.0f;

	UPROPERTY()
	float Area = 0.0f;

	UPROPERTY()
	float Volume = 0.0f;

	UPROPERTY()
	float MaterialCost = 0.0f;

	UPROPERTY()
	float LaborCost = 0.0f;

};
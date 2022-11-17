// Copyright 2022 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/Presets/BIMSymbolPresetData.h"

void FBIMSymbolPresetData::ConvertToWebPreset(FBIMWebPreset& OutPreset)
{
	FString membersKey = TEXT("Symbol.Members");
	FString graphsKey = TEXT("Symbol.Graphs");
	FString surfaceKey = TEXT("Symbol.SurfaceGraphs");
	FString eidKey = TEXT("Symbol.EquivalentIDs");
	
	FCustomDataWebConvertable::ConvertToWebPreset(OutPreset);

	bool bUseFixes = FJsonObjectConverter::bUseModumateFixes;
	FJsonObjectConverter::bUseModumateFixes = true;
	
	FString conditionsJson, overridesJson;
	TSharedPtr<FJsonObject> docOb = FJsonObjectConverter::UStructToJsonObject<FBIMSymbolPresetData>(*this);
	auto membersValue = docOb->GetField<EJson::Object>(TEXT("Members"));
	auto graphsValue = docOb->GetField<EJson::Object>(TEXT("Graphs"));
	auto surfaceValue = docOb->GetField<EJson::Object>(TEXT("SurfaceGraphs"));
	auto eidValue = docOb->GetField<EJson::Object>(TEXT("EquivalentIDs"));

	OutPreset.properties.Add({membersKey}, {membersKey, {}});
	OutPreset.properties.Add({graphsKey}, {graphsKey, {}});
	OutPreset.properties.Add({surfaceKey}, {surfaceKey, {}});
	OutPreset.properties.Add({eidKey}, {eidKey, {}});
	
	WritePrettyJson(membersValue->AsObject(), OutPreset.properties[membersKey].value.AddDefaulted_GetRef());
	WritePrettyJson(graphsValue->AsObject(), OutPreset.properties[graphsKey].value.AddDefaulted_GetRef());
	WritePrettyJson(surfaceValue->AsObject(), OutPreset.properties[surfaceKey].value.AddDefaulted_GetRef());
	WritePrettyJson(eidValue->AsObject(), OutPreset.properties[eidKey].value.AddDefaulted_GetRef());
	
	FJsonObjectConverter::bUseModumateFixes = bUseFixes;
}

void FBIMSymbolPresetData::ConvertFromWebPreset(const FBIMWebPreset& InPreset)
{
	FString membersKey = TEXT("Symbol.Members");
	FString graphsKey = TEXT("Symbol.Graphs");
	FString surfaceKey = TEXT("Symbol.SurfaceGraphs");
	FString eidKey = TEXT("Symbol.EquivalentIDs");
	
	FCustomDataWebConvertable::ConvertFromWebPreset(InPreset);

	if(InPreset.properties.Contains(membersKey))
	{
		FString Json = InPreset.properties[membersKey].value[0];
		ReadJsonMap(Json, &Members);
	}

	if(InPreset.properties.Contains(graphsKey))
	{
		FString Json = InPreset.properties[graphsKey].value[0];
		ReadJsonMap(Json, &Graphs);
	}
	
	if(InPreset.properties.Contains(surfaceKey))
	{
		FString Json = InPreset.properties[surfaceKey].value[0];
		ReadJsonMap(Json, &SurfaceGraphs);
	}
	
	if(InPreset.properties.Contains(eidKey))
	{
		FString Json = InPreset.properties[eidKey].value[0];
		ReadJsonMap(Json, &EquivalentIDs);
	}
}

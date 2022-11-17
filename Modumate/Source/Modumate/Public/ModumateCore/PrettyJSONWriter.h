// Copyright 2021 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonSerializer.h"

typedef TJsonWriterFactory< TCHAR, TPrettyJsonPrintPolicy<TCHAR> > FPrettyJsonStringWriterFactory;
typedef TJsonWriter< TCHAR, TPrettyJsonPrintPolicy<TCHAR> > FPrettyJsonStringWriter;

/**
 * @brief Outputs the JsonObj as a Pretty Json String
 * @param JsonObj the json object write
 * @param  OutString the string to write it to
 * @return True if the serialization was successful
 */
static bool WritePrettyJson(TSharedPtr<FJsonObject> JsonObj, FString& OutString)
{
	TSharedRef<FPrettyJsonStringWriter> JsonStringWriter = FPrettyJsonStringWriterFactory::Create(&OutString);
	bool bSuccess = FJsonSerializer::Serialize(JsonObj.ToSharedRef(), JsonStringWriter);
	return bSuccess;
}

/**
 * @brief Writes InObject (USTRUCT) to OutJson as a json string
 * @details Template class. T must be a USTRUCT. Outputs as pretty json.
 * @param InObject the USTRUCT to write
 * @param OutJson the string to write it to
 */
template <class T>
static bool WriteJsonGeneric(FString& OutJson, const T* InObject)
{
	bool bUseFixes = FJsonObjectConverter::bUseModumateFixes;
	FJsonObjectConverter::bUseModumateFixes = true;

	TSharedPtr<FJsonObject> docOb = FJsonObjectConverter::UStructToJsonObject<T>(*InObject);
	bool bSuccess = WritePrettyJson(docOb, OutJson);
	FJsonObjectConverter::bUseModumateFixes = bUseFixes;

	return bSuccess;
};

/**
 * @brief Reads a JSON string and converts it to OutObject
 * @details Template class. T must be a USTRUCT.
 * @param OutObject the USTRUCT to deserialize to
 * @param InJson the string to read it from
 */
template <class T>
static bool ReadJsonGeneric(const FString& InJson, T* OutObject)
{
	TSharedPtr<FJsonObject> FileJsonRead;
	auto JsonReader = TJsonReaderFactory<>::Create(InJson);
	bool bSuccess = FJsonSerializer::Deserialize(JsonReader, FileJsonRead) && FileJsonRead.IsValid();

	if (bSuccess)
	{
		FJsonObjectConverter::JsonObjectToUStruct<T>(FileJsonRead.ToSharedRef(), OutObject);
	}

	return bSuccess;
};

/**
 * @brief Writes a TArray of Ustructs to a string as Json
 * @details Template class. T must be a USTRUCT
 * @param OutJson the Json to output to
 * @param InObjectArray the array to deserialize
 * @oaram FieldName the name of the array field in the json
 */
template <class T>
static bool WriteJsonUstructArray(FString& OutJson, const TArray<T>* InObjectArray, const FString& FieldName)
{
	bool bUseFixes = FJsonObjectConverter::bUseModumateFixes;
	FJsonObjectConverter::bUseModumateFixes = true;

	TSharedPtr<FJsonObject> docOb = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> values;
	for(auto& obj : *InObjectArray)
	{
		TSharedPtr<FJsonObject> asObject  = FJsonObjectConverter::UStructToJsonObject<T>(obj);
		TSharedPtr<FJsonValueObject> asValue = MakeShared<FJsonValueObject>(asObject);
		values.Add(asValue);
	}


	docOb->SetArrayField(FieldName, values);
	bool bSuccess = WritePrettyJson(docOb, OutJson);

	FJsonObjectConverter::bUseModumateFixes = bUseFixes;

	return bSuccess;
};

/**
 * @brief Reads a TArray of Ustructs from a JSON string
 * @details Template class. T must be a USTRUCT
 * @param InJson the Json to read from
 * @param OutObjectArray the array to serialize
 * @oaram FieldName the name of the array field in the json
 */
template <class T>
static bool ReadJsonUstructArray(FString& InJson, TArray<T>* OutObjectArray, const FString& FieldName)
{
	bool bUseFixes = FJsonObjectConverter::bUseModumateFixes;
	FJsonObjectConverter::bUseModumateFixes = true;

	TSharedPtr<FJsonObject> FileJsonRead;
	auto JsonReader = TJsonReaderFactory<>::Create(InJson);
	bool bSuccess = FJsonSerializer::Deserialize(JsonReader, FileJsonRead) && FileJsonRead.IsValid();

	if (bSuccess)
	{
		TArray<TSharedPtr<FJsonValue>> values = FileJsonRead->GetArrayField(FieldName);
		for(auto& val : values)
		{
			T& InsideTheArray = OutObjectArray->AddDefaulted_GetRef();
			TSharedRef<FJsonObject> asRef = val->AsObject().ToSharedRef();
			FJsonObjectConverter::JsonObjectToUStruct<T>(asRef, &InsideTheArray);
		}
	}
	
	FJsonObjectConverter::bUseModumateFixes = bUseFixes;

	return bSuccess;
};

/**
 * @brief Reads a Map<int32,USTRUCT> from a JSON string
 * @details Template class. T must be a USTRUCT
 * @param InJson the Json to read from
 * @param OutObjectArray the map to deserialize to
 */
template <class T>
static bool ReadJsonMap(FString& InJson, TMap<int32,T>* OutObjectArray)
{
	bool bUseFixes = FJsonObjectConverter::bUseModumateFixes;
	FJsonObjectConverter::bUseModumateFixes = true;

	TSharedPtr<FJsonObject> FileJsonRead;
	auto JsonReader = TJsonReaderFactory<>::Create(InJson);
	bool bSuccess = FJsonSerializer::Deserialize(JsonReader, FileJsonRead) && FileJsonRead.IsValid();

	if (bSuccess)
	{
		for(auto& val : FileJsonRead->Values)
		{
			int32 InnerKey = FCString::Atoi(*val.Key);
			TSharedPtr<FJsonObject> InnerObject = val.Value->AsObject();
			T InsideTheMap;
			FJsonObjectConverter::JsonObjectToUStruct<T>(InnerObject.ToSharedRef(), &InsideTheMap);
			OutObjectArray->Add(InnerKey, InsideTheMap);
		}
	}
	
	FJsonObjectConverter::bUseModumateFixes = bUseFixes;

	return bSuccess;
};


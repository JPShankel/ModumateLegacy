// Copyright 2021 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonSerializer.h"

typedef TJsonWriterFactory< TCHAR, TPrettyJsonPrintPolicy<TCHAR> > FPrettyJsonStringWriterFactory;
typedef TJsonWriter< TCHAR, TPrettyJsonPrintPolicy<TCHAR> > FPrettyJsonStringWriter;

template <class T>
static bool WriteJsonGeneric(FString& OutJson, const T* InObject)
{
	bool bFixID = FJsonObjectConverter::bFixID;
	FJsonObjectConverter::bFixID = false;

	TSharedPtr<FJsonObject> docOb = FJsonObjectConverter::UStructToJsonObject<T>(*InObject);
	TSharedRef<FPrettyJsonStringWriter> JsonStringWriter = FPrettyJsonStringWriterFactory::Create(&OutJson);

	//Return it prettified
	bool bSuccess = FJsonSerializer::Serialize(docOb.ToSharedRef(), JsonStringWriter);
	FJsonObjectConverter::bFixID = bFixID;

	return bSuccess;
};

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
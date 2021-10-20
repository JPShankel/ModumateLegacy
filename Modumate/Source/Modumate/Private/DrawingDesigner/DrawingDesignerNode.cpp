// Copyright 2021 Modumate, Inc. All Rights Reserved.
#include "DrawingDesigner/DrawingDesignerNode.h"
#include "JsonObjectConverter.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "DocumentManagement/ModumateSerialization.h"

bool FDrawingDesignerNode::WriteJson(FString& OutJson) const
{
	TSharedPtr<FJsonObject> docOb = NULL;
	if (AsJsonObject(docOb))
	{
		TSharedRef<FPrettyJsonStringWriter> JsonStringWriter = FPrettyJsonStringWriterFactory::Create(&OutJson);
		return FJsonSerializer::Serialize(docOb.ToSharedRef(), JsonStringWriter);
	}
	return false;
}

bool FDrawingDesignerNode::ReadJson(const FString& InJson)
{
	auto JsonReader = TJsonReaderFactory<>::Create(InJson);
	TSharedPtr<FJsonObject> FileJsonRead;
	bool bSuccess = FJsonSerializer::Deserialize(JsonReader, FileJsonRead) && FileJsonRead.IsValid();

	if (bSuccess)
	{
		bSuccess = FromJsonObject(FileJsonRead);
	}

	return bSuccess;
}

bool FDrawingDesignerNode::FromJsonObject(const TSharedPtr<FJsonObject> InValue)
{
	if (!InValue)
	{
		return false;
	}

	//This will de-serialize everything but the chunk field
	FJsonObjectConverter::JsonObjectToUStruct<FDrawingDesignerNode>(InValue.ToSharedRef(), this);

	//Read the chunk field
	const TSharedPtr<FJsonObject>* chunkObject;
	bool bSuccess = true;
	if (InValue->HasField("chunk"))
	{
		TSharedPtr<FJsonValue> chunk = InValue->TryGetField(TEXT("chunk"));
		if (chunk->Type == EJson::Object)
		{
			bSuccess = InValue->TryGetObjectField(TEXT("chunk"), chunkObject);
			if (bSuccess)
			{
				this->SerializedChunk = *chunkObject;
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("Failed to read chunk field"));
			}
		}
	}

	return bSuccess;
}

bool FDrawingDesignerNode::AsJsonObject(TSharedPtr<FJsonObject>& outObject) const
{
	//This will serialize everything but the chunk field
	TSharedPtr<FJsonObject> docOb = FJsonObjectConverter::UStructToJsonObject<FDrawingDesignerNode>(*this);
	if (docOb)
	{
		//Add the chunk field
		docOb->SetObjectField(TEXT("chunk"), this->SerializedChunk);
		outObject = docOb;
		return true;
	}

	return false;
}

bool FDrawingDesignerNode::operator==(const FDrawingDesignerNode& RHS) const
{
	if (this->id != RHS.id) 
	{
		return false;
	}

	if (this->parent != RHS.parent)
	{
		return false;
	}

	if (this->children != RHS.children)
	{ 
		return false;
	}


	//HACK: Dirty hack to prevent floating point == compares -JN
	// Equality is only needed for unit testing. If we need equality for
	// a production reason, this needs to be cleaned up.
#if UE_BUILD_SHIPPING
	for (auto& kvp : this->SerializedChunk->Values)
	{
		auto* other = RHS.SerializedChunk->Values.Find(kvp.Key);
		if (!other)
		{
			return false;
		}
	}
#else
	if (this->SerializedChunk)
	{
		FString LHS_Chunk;
		FString RHS_Chunk;

		TSharedRef<TJsonWriter<>> LHS_Writer = TJsonWriterFactory<>::Create(&LHS_Chunk);
		TSharedRef<TJsonWriter<>> RHS_Writer = TJsonWriterFactory<>::Create(&RHS_Chunk);

		FJsonSerializer::Serialize(this->SerializedChunk.ToSharedRef(), LHS_Writer);
		FJsonSerializer::Serialize(RHS.SerializedChunk.ToSharedRef(), RHS_Writer);

		if (!LHS_Chunk.Equals(RHS_Chunk))
		{
			return false;
		}
	}
	else
	{
		return !RHS.SerializedChunk;
	}

#endif

	return true;
}

bool FDrawingDesignerNode::operator!=(const FDrawingDesignerNode& RHS) const
{
	return !(*this == RHS);
}
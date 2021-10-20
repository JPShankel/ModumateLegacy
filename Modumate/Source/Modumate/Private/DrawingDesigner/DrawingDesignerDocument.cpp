// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "DrawingDesigner/DrawingDesignerDocument.h"

#include "JsonObjectConverter.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "DocumentManagement/ModumateSerialization.h"

DEFINE_LOG_CATEGORY(ModumateDrawingDesigner);

static constexpr TCHAR* DrawingDesignerDocumentLabel = TEXT("document");

FDrawingDesignerDocument::FDrawingDesignerDocument()
{
	//Create the 'root' node
	FDrawingDesignerNode root;
	root.id = 0;
	root.parent = INDEX_NONE;
	root.nodeType = ENodeSchemaType::document;
	nodes.Add(TEXT("0"), root);
}

bool FDrawingDesignerDocument::WriteJson(FString& OutJson) const
{
	//This will serialize everything but the map fields
	TSharedPtr<FJsonObject> docOb = FJsonObjectConverter::UStructToJsonObject<FDrawingDesignerDocument>(*this);
	TSharedRef<FPrettyJsonStringWriter> JsonStringWriter = FPrettyJsonStringWriterFactory::Create(&OutJson);
	TSharedPtr<FJsonObject> FileJsonWrite = MakeShared<FJsonObject>();
	

	if (docOb)
	{
		bool bSuccess = true;
		//Add the map fields
		TMap<FString, const FDrawingDesignerMap*> fields =
		{
			{TEXT("nodes"), &(this->nodes)},
		};

		//For all the maps above...
		for (auto& field : fields)
		{
			//Create a field for it...
			TSharedPtr<FJsonObject> jsonField = MakeShared<FJsonObject>();

			//And add all the items to the field (from the maps)
			for (auto& kvp : *(field.Value))
			{
				FDrawingDesignerNode node = kvp.Value;
				TSharedPtr<FJsonObject> nodeJson = NULL;
				if (node.AsJsonObject(nodeJson))
				{
					TSharedPtr<FJsonValue> asValue = MakeShareable<FJsonValueObject>(new FJsonValueObject{ nodeJson });
					jsonField->Values.Add(kvp.Key, asValue);
				}
				
			}
			
			//Add the created nodes field to the overall object
			docOb->SetObjectField(field.Key, jsonField);
		}

		//Return it prettified
		FileJsonWrite->SetObjectField(DrawingDesignerDocumentLabel, docOb);
		return bSuccess && FJsonSerializer::Serialize(FileJsonWrite.ToSharedRef(), JsonStringWriter);
	}

	return false;
}

bool FDrawingDesignerDocument::ReadJson(const FString& InJson)
{
	TSharedPtr<FJsonObject> FileJsonRead;
	const TSharedPtr<FJsonObject>* jsonDocument;
	auto JsonReader = TJsonReaderFactory<>::Create(InJson);
	bool bSuccess = FJsonSerializer::Deserialize(JsonReader, FileJsonRead) && FileJsonRead.IsValid();

	if (bSuccess)
	{
		if (FileJsonRead->TryGetObjectField(DrawingDesignerDocumentLabel, jsonDocument))
		{
			//This will de-serialize everything but the node map
			FJsonObjectConverter::JsonObjectToUStruct<FDrawingDesignerDocument>(jsonDocument->ToSharedRef(), this);

			//Do the maps manually...
			TMap<FString, FDrawingDesignerMap*> fields =
			{ 
				{TEXT("nodes"), &(this->nodes)},
			};

			//For all the maps above...
			for(auto& field : fields) 
			{
				//Try and find the json field associated with that map
				const TSharedPtr<FJsonObject>* jsonField;
				if ((*jsonDocument)->TryGetObjectField(field.Key, jsonField))
				{
					//In that json field, get all the key value pairs
					for (auto& kvp : (*jsonField)->Values)
					{
						//Create a node from the key and value pair
						FDrawingDesignerNode node;
						node.FromJsonObject(kvp.Value->AsObject());

						//Add it to the map
						field.Value->Add(kvp.Key, node);
					}
				}
				else
				{
					bSuccess = false;
				}
			}
		}
	}

	return bSuccess;
}

bool FDrawingDesignerDocument::operator==(const FDrawingDesignerDocument& RHS) const
{
	if (nodes.Num() != RHS.nodes.Num())
	{
		return false;
	}

	if (views.Num() != RHS.views.Num())
	{
		return false;
	}

	for (auto& kvp : nodes)
	{
		auto* other = RHS.nodes.Find(kvp.Key);
		if (!other || *other != kvp.Value)
		{
			return false;
		}
	}

	for (auto& kvp : views)
	{
		auto* other = RHS.views.Find(kvp.Key);
		if (!other || *other != kvp.Value)
		{
			return false;
		}
	}
	return true;
}

bool FDrawingDesignerDocument::operator!=(const FDrawingDesignerDocument& RHS) const
{
	return !(*this == RHS);
}


bool FDrawingDesignerDocument::Add(FDrawingDesignerNode obj)
{
	bool rtn = true;
	FString id = FString::FromInt(obj.id);
	if (!nodes.Contains(id))
	{
		if (obj.parent == INDEX_NONE)
		{
			UE_LOG(ModumateDrawingDesigner, Warning, TEXT("Parent ID was not set for add operation, id=%s"), *id);
			rtn = false;
		}
		else
		{
			FString parentID = FString::FromInt(obj.parent);
			if (nodes.Contains(parentID))
			{
				nodes[parentID].children.Add(obj.id);
			}
			else
			{
				UE_LOG(ModumateDrawingDesigner, Warning, TEXT("Parent ID does not exist in nodes, parent id=%s"), *parentID);
				rtn = false;
			}
		}
		
		if (rtn)
		{
			nodes.Add(id, obj);
		}
	}
	else
	{
		UE_LOG(ModumateDrawingDesigner, Warning, TEXT("Attempted to add ID that already exists in nodes, id=%d"), obj.id);
		rtn = false;
	}

	return rtn;
}

bool FDrawingDesignerDocument::Remove(const FString& id)
{
	bool rtn = true;
	if (id == TEXT("0"))
	{
		UE_LOG(ModumateDrawingDesigner, Warning, TEXT("Attemped to delete root node -- not a valid operation"));
		return false;
	}

	if (nodes.Contains(id))
	{
		FString parentID = FString::FromInt(nodes[id].parent);
		if (nodes.Contains(parentID))
		{
			//This makes me hate.
			nodes[parentID].children.Remove(nodes[id].id);
		}
		else
		{
			rtn = false;
		}
	}

	return rtn && RemoveRecurse(id);
}

bool FDrawingDesignerDocument::RemoveRecurse(const FString& id)
{
	bool found = false;

	if (nodes.Contains(id))
	{
		//Remove children first
		for (auto& child : nodes[id].children)
		{
			FString childId = FString::FromInt(child);
			RemoveRecurse(childId);
		}

		//Then Remove the ID from the maps...
		nodes.Remove(id);
		found = true;
	}
	else
	{
		UE_LOG(ModumateDrawingDesigner, Warning, TEXT("Asked to remove ID that doesn't exist, id=%s"), *id);
	}

	return found;
}

bool FDrawingDesignerDocument::Modify(FDrawingDesignerNode obj)
{
	FString id = FString::FromInt(obj.id);

	if (nodes.Contains(id))
	{
		FDrawingDesignerNode& old = nodes[id];

		if (old.parent != obj.parent)
		{
			//Parent changed..
			FString old_pid = FString::FromInt(old.parent);
			FString new_pid = FString::FromInt(obj.parent);

			//Remove child from old parent and add to new parent
			nodes[old_pid].children.Remove(obj.id);
			nodes[new_pid].children.Add(obj.id);
		}
		old = obj;
		return true;
	}
	else
	{
		UE_LOG(ModumateDrawingDesigner, Warning, TEXT("Asked to change ID that didn't exist, id=%s"), *id);
		return false;
	}
}

bool FDrawingDesignerDocument::Validate()
{
	TMap<FString, bool> seenNodeMap;
	FString rootId = TEXT("0");
	//All node maps MUST have a 0th node which contains a list
	if (nodes.Contains(rootId))
	{
		seenNodeMap.Add(TEXT("0"), true);
		for (auto& child : nodes[rootId].children)
		{
			FString childId = FString::FromInt(child);
			if (!ValidateRecurse(childId, seenNodeMap))
			{
				return false;
			}
		}
	}
	else
	{
		return false;
	}
	
	return true;
}

bool FDrawingDesignerDocument::ValidateRecurse(const FString& id, TMap<FString, bool>& seenNodeMap)
{
	if (seenNodeMap.Contains(id))
	{
		return false;
	}
	else
	{
		seenNodeMap.Add(id, true);
		for (auto& child : nodes[id].children)
		{
			FString childId = FString::FromInt(child);
			if (!ValidateRecurse(childId, seenNodeMap))
			{
				return false;
			}
		}
	}

	return true;
}
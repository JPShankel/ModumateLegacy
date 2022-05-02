// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "DrawingDesigner/DrawingDesignerDocument.h"

#include "JsonObjectConverter.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "DocumentManagement/ModumateSerialization.h"
#include "DrawingDesigner/DrawingDesignerRequests.h"

DEFINE_LOG_CATEGORY(ModumateDrawingDesigner);

static const TCHAR* DrawingDesignerDocumentLabel = TEXT("document");

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
	return WriteJsonGeneric<FDrawingDesignerDocument>(OutJson, this);;
}

bool FDrawingDesignerDocument::ReadJson(const FString& InJson)
{
	return ReadJsonGeneric<FDrawingDesignerDocument>(InJson, this);;
}

bool FDrawingDesignerDocument::operator==(const FDrawingDesignerDocument& RHS) const
{
	if (nodes.Num() != RHS.nodes.Num())
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

bool FDrawingDesignerDocument::Remove(const FString& Id)
{
	if (Id == TEXT("0"))
	{
		UE_LOG(ModumateDrawingDesigner, Warning, TEXT("Attemped to delete root node -- not a valid operation"));
		return false;
	}


	UE_LOG(ModumateDrawingDesigner, Log, TEXT("Asked to remove id=%s"), *Id);
	return RemoveRecurse(Id);
}

bool FDrawingDesignerDocument::RemoveRecurse(const FString& Id)
{
	bool found = false;

	if (nodes.Contains(Id))
	{
		//Remove children first
		//Copy here to prevent modification during iteration
		TArray<int32> iterArray(nodes[Id].children);
		
		for (auto& child : iterArray)
		{
			FString childId = FString::FromInt(child);
			RemoveRecurse(childId);
		}

		FString parentID = FString::FromInt(nodes[Id].parent);
		if (nodes.Contains(parentID))
		{
			nodes[parentID].children.Remove(nodes[Id].id);
		}
		else
		{
			UE_LOG(ModumateDrawingDesigner, Warning,
				TEXT("Parent of child does not know about child, parentId=%s id=%s"), *parentID, *Id);
		}

		UE_LOG(ModumateDrawingDesigner, Log,
			TEXT("Removing id=%s"), *Id);
		
		//Then Remove the ID from the map...
		nodes.Remove(Id);
		found = true;
	}
	else
	{
		//Clean all references to that node ID in children lists.
		UE_LOG(ModumateDrawingDesigner, Warning,
			TEXT("Asked to remove ID that doesn't exist, id=%s"), *Id);
		int32 idIdx = FCString::Atoi(*Id);
		found = RemoveGhosts(0, idIdx);;
	}

	return found;
}

bool FDrawingDesignerDocument::RemoveGhosts(const int32 RootId, const int32 Id)
{
	bool found = false;
	
	UE_LOG(ModumateDrawingDesigner, Warning,
			TEXT("Removing ghost child entry for unknown node=%d"), Id);
	
	if(nodes.Contains(FString::FromInt(Id)))
	{
		//Do not remove ghosts for nodes that are not ghosts
		return false;
	}

	if(nodes.Contains(FString::FromInt(RootId)))
	{
		FDrawingDesignerNode* root = &nodes[FString::FromInt(RootId)];
		size_t preCount = root->children.Num();
		root->children.RemoveAll([&](int32 childId){return childId == Id;});
		size_t postCount = root->children.Num();
		if(postCount < preCount)
		{
			found = true;
		}
		
		for(auto& child: root->children)
		{
			found = found || RemoveGhosts(child, Id);
		}
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
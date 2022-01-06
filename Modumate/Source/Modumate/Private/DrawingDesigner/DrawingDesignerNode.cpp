// Copyright 2021 Modumate, Inc. All Rights Reserved.
#include "DrawingDesigner/DrawingDesignerNode.h"
#include "JsonObjectConverter.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "DocumentManagement/ModumateSerialization.h"
#include "DrawingDesigner/DrawingDesignerRequests.h"

bool FDrawingDesignerNode::WriteJson(FString& OutJson) const
{
	return WriteJsonGeneric<FDrawingDesignerNode>(OutJson, this);
}

bool FDrawingDesignerNode::ReadJson(const FString& InJson)
{
	return ReadJsonGeneric<FDrawingDesignerNode>(InJson, this);
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

	if (!this->chunkString.Equals(RHS.chunkString))
	{
		return false;
	}

	return true;
}

bool FDrawingDesignerNode::operator!=(const FDrawingDesignerNode& RHS) const
{
	return !(*this == RHS);
}
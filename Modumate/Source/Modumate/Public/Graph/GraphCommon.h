// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FGraphVertexPair : public TPair<int32, int32>
{
public:
	FGraphVertexPair();
	FGraphVertexPair(int32 VertexIDA, int32 VertexIDB);

	static FGraphVertexPair MakeEdgeKey(int32 VertexIDA, int32 VertexIDB);
};

// Use this type alias to designate ID whose sign indicates a direction
// i.e. edges traversed opposite their geometric direction, or faces traversed opposite their normal
using FGraphSignedID = int32;

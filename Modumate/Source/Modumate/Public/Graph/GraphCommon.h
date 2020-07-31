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

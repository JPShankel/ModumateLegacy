// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once
#include "ModumateObjectAssembly.h"
#include "EditModelToolBase.h"

#include "EditModelFFETool.generated.h"

class ACompoundMeshActor;

UCLASS()
class MODUMATE_API UPlaceObjectTool : public UEditModelToolBase
{
	GENERATED_BODY()

private:
	AStaticMeshActor * Cursor;
	ACompoundMeshActor* CursorCompoundMesh;
public:
	UPlaceObjectTool(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	FModumateObjectAssembly CurrentFFEAssembly;

	virtual EToolMode GetToolMode() override { return EToolMode::VE_PLACEOBJECT; }
	virtual bool Activate() override;
	virtual bool ScrollToolOption(int32 dir) override;
	virtual bool Deactivate() override;

	virtual bool FrameUpdate() override;
	virtual bool BeginUse() override;
};

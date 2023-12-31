#pragma once

#include "CoreMinimal.h"

#include "GameFramework/Actor.h"

#include "CutPlaneCaptureActor.generated.h"

class ISceneCaptureObject;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;

UCLASS()
class MODUMATE_API ACutPlaneCaptureActor : public AActor
{
	GENERATED_BODY()

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
public:
	ACutPlaneCaptureActor();

	ISceneCaptureObject *Parent;

	int32 ObjID;
	int32 ScopeBoxID;
	FString CurrentFullPath;

	void ResetHiddenActorsToDefault();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	USceneCaptureComponent2D *CaptureComponent;

	UPROPERTY()
	UTextureRenderTarget2D *CurrentTextureTarget;

	bool bCaptureStarted = false;
	int32 TickCount;
};

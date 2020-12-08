// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CollisionQueryParams.h"
#include "EditModelPlayerPawn_CPP.generated.h"

UCLASS()
class MODUMATE_API AEditModelPlayerPawn_CPP : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	AEditModelPlayerPawn_CPP(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	FCollisionObjectQueryParams CachedCollisionObjQueryParams;
	FCollisionQueryParams CachedCollisionQueryParams;
	FCollisionQueryParams CachedCollisionQueryComplexParams;

	UPROPERTY()
	class AEditModelPlayerController_CPP* EMPlayerController;

public:
	// Objects types for collision detection
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<TEnumAsByte<EObjectTypeQuery>> CollisionQuery;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	class UCameraComponent* CameraComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	class USceneCaptureComponent2D *CameraCaptureComponent2D;

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable)
	bool SphereTraceForZoomLocation(const FVector &Start, const FVector &End, float Radius, FHitResult& OutHit);

	UFUNCTION(BlueprintCallable)
	bool LineTraceForCollisionLocation(const FVector &Start, const FVector &End, FHitResult& OutHit, bool TraceComplex);

	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent)
	bool ToggleWalkAround();

	bool SetCameraFOV(float NewFOV);

	bool bHaveEverBeenPossessed;
};

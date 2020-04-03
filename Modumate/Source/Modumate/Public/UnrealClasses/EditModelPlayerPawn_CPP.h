// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "CollisionQueryParams.h"
#include "EditModelPlayerPawn_CPP.generated.h"

UCLASS()
class MODUMATE_API AEditModelPlayerPawn_CPP : public APawn
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	AEditModelPlayerPawn_CPP();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	FCollisionObjectQueryParams CachedCollisionObjQueryParams;
	FCollisionQueryParams CachedCollisionQueryParams;
	FCollisionQueryParams CachedCollisionQueryComplexParams;

public:
	// Objects types for collision detection
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<TEnumAsByte<EObjectTypeQuery>> CollisionQuery;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	AActor* OrbitCursorActor;

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UFUNCTION(BlueprintCallable)
	bool SphereTraceForZoomLocation(const FVector &Start, const FVector &End, float Radius, FHitResult& OutHit);

	UFUNCTION(BlueprintCallable)
	bool LineTraceForCollisionLocation(const FVector &Start, const FVector &End, FHitResult& OutHit, bool TraceComplex);

	UFUNCTION(BlueprintImplementableEvent)
	class UCameraComponent *GetEditCameraComponent();

	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent)
	bool ToggleWalkAround();

	bool SetCameraFOV(float NewFOV);

	UFUNCTION(BlueprintCallable)
	bool SetCameraTransform(const FTransform &PlayerActorTransform, const FTransform &CameraTransform, const FRotator &ControlRotation);
};

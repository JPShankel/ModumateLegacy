// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EditModelToggleGravityPawn_CPP.generated.h"

UCLASS()
class MODUMATE_API AEditModelToggleGravityPawn_CPP : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	AEditModelToggleGravityPawn_CPP();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UFUNCTION()
	void OnAxisRotateYaw(float RotateYawValue);

	UFUNCTION()
	void OnAxisRotatePitch(float RotatePitchValue);

	UFUNCTION()
	void OnAxisMoveForward(float MoveForwardValue);

	UFUNCTION()
	void OnAxisMoveRight(float MoveRightValue);

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};

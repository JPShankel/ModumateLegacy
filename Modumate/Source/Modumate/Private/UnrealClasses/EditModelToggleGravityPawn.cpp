// Fill out your copyright notice in the Description page of Project Settings.

#include "UnrealClasses/EditModelToggleGravityPawn.h"

#include "Components/InputComponent.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "ModumateCore/EnumHelpers.h"

// Sets default values
AEditModelToggleGravityPawn::AEditModelToggleGravityPawn()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AEditModelToggleGravityPawn::BeginPlay()
{
	Super::BeginPlay();
}

void AEditModelToggleGravityPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis(GetEnumValueShortName(EInputMovementAxes::MoveYaw), this, &AEditModelToggleGravityPawn::OnAxisRotateYaw);
	PlayerInputComponent->BindAxis(GetEnumValueShortName(EInputMovementAxes::MovePitch), this, &AEditModelToggleGravityPawn::OnAxisRotatePitch);

	PlayerInputComponent->BindAxis(GetEnumValueShortName(EInputMovementAxes::MoveForward), this, &AEditModelToggleGravityPawn::OnAxisMoveForward);
	PlayerInputComponent->BindAxis(GetEnumValueShortName(EInputMovementAxes::MoveRight), this, &AEditModelToggleGravityPawn::OnAxisMoveRight);
}

void AEditModelToggleGravityPawn::OnAxisRotateYaw(float RotateYawValue)
{
	if (RotateYawValue != 0.0f)
	{
		AddControllerYawInput(RotateYawValue);
	}
}

void AEditModelToggleGravityPawn::OnAxisRotatePitch(float RotatePitchValue)
{
	if (RotatePitchValue != 0.0f)
	{
		AddControllerPitchInput(-RotatePitchValue);
	}
}

void AEditModelToggleGravityPawn::OnAxisMoveForward(float MoveForwardValue)
{
	if (MoveForwardValue != 0.0f)
	{
		AddMovementInput(MoveForwardValue * GetActorForwardVector());
	}
}

void AEditModelToggleGravityPawn::OnAxisMoveRight(float MoveRightValue)
{
	if (MoveRightValue != 0.0f)
	{
		AddMovementInput(MoveRightValue * GetActorRightVector());
	}
}

void AEditModelToggleGravityPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

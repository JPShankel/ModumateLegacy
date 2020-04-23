// Fill out your copyright notice in the Description page of Project Settings.

#include "EditModelToggleGravityPawn_CPP.h"
#include "Components/InputComponent.h"

// Sets default values
AEditModelToggleGravityPawn_CPP::AEditModelToggleGravityPawn_CPP()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AEditModelToggleGravityPawn_CPP::BeginPlay()
{
	Super::BeginPlay();
}

void AEditModelToggleGravityPawn_CPP::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis(FName(TEXT("MoveYaw")), this, &AEditModelToggleGravityPawn_CPP::OnAxisRotateYaw);
	PlayerInputComponent->BindAxis(FName(TEXT("MovePitch")), this, &AEditModelToggleGravityPawn_CPP::OnAxisRotatePitch);

	PlayerInputComponent->BindAxis(FName(TEXT("MoveForward")), this, &AEditModelToggleGravityPawn_CPP::OnAxisMoveForward);
	PlayerInputComponent->BindAxis(FName(TEXT("MoveRight")), this, &AEditModelToggleGravityPawn_CPP::OnAxisMoveRight);
}

void AEditModelToggleGravityPawn_CPP::OnAxisRotateYaw(float RotateYawValue)
{
	if (RotateYawValue != 0.0f)
	{
		AddControllerYawInput(RotateYawValue);
	}
}

void AEditModelToggleGravityPawn_CPP::OnAxisRotatePitch(float RotatePitchValue)
{
	if (RotatePitchValue != 0.0f)
	{
		AddControllerPitchInput(-RotatePitchValue);
	}
}

void AEditModelToggleGravityPawn_CPP::OnAxisMoveForward(float MoveForwardValue)
{
	if (MoveForwardValue != 0.0f)
	{
		AddMovementInput(MoveForwardValue * GetActorForwardVector());
	}
}

void AEditModelToggleGravityPawn_CPP::OnAxisMoveRight(float MoveRightValue)
{
	if (MoveRightValue != 0.0f)
	{
		AddMovementInput(MoveRightValue * GetActorRightVector());
	}
}

void AEditModelToggleGravityPawn_CPP::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

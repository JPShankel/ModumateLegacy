// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CollisionQueryParams.h"
#include "EditModelPlayerPawn.generated.h"


class UModumateClientIcon;
class UModumateClientCursor;

UCLASS()
class MODUMATE_API AEditModelPlayerPawn : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	AEditModelPlayerPawn();

	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	FCollisionObjectQueryParams CachedCollisionObjQueryParams;
	FCollisionQueryParams CachedCollisionQueryParams;
	FCollisionQueryParams CachedCollisionQueryComplexParams;

public:
	// Objects types for collision detection
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<TEnumAsByte<EObjectTypeQuery>> CollisionQuery;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	class UCameraComponent* CameraComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	class USceneCaptureComponent2D *CameraCaptureComponent2D;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	class USceneCaptureComponent2D* ScreenshotTaker;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	class UStaticMeshComponent* RemoteMeshComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UModumateClientIcon> ClientIconClass = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UModumateClientCursor> ClientCursorClass = nullptr;

	UPROPERTY()
	UModumateClientIcon* ClientIconWidget = nullptr;

	UPROPERTY()
	UModumateClientCursor* ClientCursorWidget = nullptr;

	UPROPERTY()
	class UMaterialInstanceDynamic* RemoteMeshMaterial = nullptr;

	bool TryInitClientVisuals();

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable)
	bool SphereTraceForZoomLocation(const FVector &Start, const FVector &End, float Radius, FHitResult& OutHit);

	UFUNCTION(BlueprintCallable)
	bool LineTraceForCollisionLocation(const FVector &Start, const FVector &End, FHitResult& OutHit, bool TraceComplex);

	bool SetCameraFOV(float NewFOV);

	bool SetCameraOrtho(bool bOrtho);

	bool bHaveEverBeenPossessed;
};

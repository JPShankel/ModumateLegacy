// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "SkyActor.generated.h"

UCLASS()
class MODUMATE_API ASkyActor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ASkyActor();

	UPROPERTY(EditAnywhere)
	USceneComponent* Root;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	class UDirectionalLightComponent* DirectionalLight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	class USkyAtmosphereComponent* SkyAtmosphere;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	class UStaticMeshComponent* SkyDomeMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	class USkyLightComponent* SkyLight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	class UStaticMeshComponent* BackgroundSkyPlane;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY()
	class AEditModelPlayerController *Controller;

	FDateTime CurrentDateTime;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	float DayLightBrightness = 8.f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	float RTDayLightBrightness = 400.f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	float NightLightBrightness = 0.f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FLinearColor DayColor = FLinearColor::White;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FLinearColor DuskColor = FLinearColor::Yellow;

	// The pitch angle for DirectionalLight to consider as fully daytime
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	float DayAnglePitch = -30.f;

	// The pitch angle for DirectionalLight to consider as dawn or dusk
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	float DawnAnglePitch = -4.f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 StartYear = 2020;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 StartMonth = 9;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 StartDay = 21;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 StartHour = 13;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 StartMinute = 30;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	float Latitude = 45.f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	float Longitude = -73.f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	float TrueNorth = 0.f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	bool IsDayLightSaving = false;

	FDateTime GetCurrentDateTime() const { return CurrentDateTime; }
	void UpdateComponentsWithDateTime(const FDateTime &DateTime);
	void SetCurrentDateTime(const FDateTime &NewDateTime);
	void SetCurrentMonth(int32 NewMonth);
	void SetCurrentDay(int32 NewDay);
	void SetCurrentHour(int32 NewHour);
	void SetCurrentMinute(int32 NewMinute);
	void ToggleCurrentMeridiem();
	void AddTimespanToCurrentDateTime(const FTimespan &Timespan);
	void SetSkyDomePositionScale(const FVector& Position, const FVector& Scale);
	void SetSkyDomePositionScaleByCameraProjection(bool bIsPerspective);
	void ToggleBackgroundSkyPlane(bool bVisible, const FVector& CameraPosition = FVector::ZeroVector, const FRotator& CameraRotation = FRotator::ZeroRotator);
	void UpdateCoordinate(float InLatitude, float InLongitude, float InTrueNorth);
};

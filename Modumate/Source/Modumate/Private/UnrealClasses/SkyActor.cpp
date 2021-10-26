// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/SkyActor.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkyLightComponent.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "SunPosition/Public/SunPosition.h"

// Magic constants for orthogonal/perspective changes.
// The persp dome transform is the UE4 default for a new level;
// the ortho version are the changes to get it visible.
// Note the scale values give an ellipsoid - this is not an error!
static const FVector PerspectiveDomePosition(0.0f, 0.0f, -7.3e6f);
static const FVector PerspectiveDomeScale(1e6f, 150000.0f, 1e6f);
static const FVector OrthogonalDomePosition(0);
static const FVector OrthogonalDomeScale(2e4f, 2e3f, 2e4f);
static const float BackgroundSkyPlanePreviewDistance(1e5f);

// Sets default values
ASkyActor::ASkyActor()
	: Controller(nullptr)
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	DirectionalLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("DirectionalLight"));
	DirectionalLight->SetupAttachment(Root);
	SkyAtmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("SkyAtmosphere"));
	SkyAtmosphere->SetupAttachment(Root);
	SkyDomeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SkyDomeMesh"));
	SkyDomeMesh->SetupAttachment(Root);
	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(Root);
	BackgroundSkyPlane = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BackgroundSkyPlane"));
	BackgroundSkyPlane->SetupAttachment(Root);
}

// Called when the game starts or when spawned
void ASkyActor::BeginPlay()
{
	Super::BeginPlay();
	CurrentDateTime = FDateTime(StartYear, StartMonth, StartDay, StartHour, StartMinute);
	UpdateComponentsWithDateTime(CurrentDateTime);

#if !UE_SERVER
	// Controller needs SkyActor for time of day changes in ViewMenu to work
	Controller = GetWorld()->GetFirstPlayerController<AEditModelPlayerController>();
	if (ensureAlways(Controller))
	{
		Controller->SkyActor = this;
	}
#endif
}

// Called every frame
void ASkyActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// SkyDomeMesh needs to follow camera location for its atmosphere material to work
	if (Controller && Controller->PlayerCameraManager)
	{
		SetActorLocation(Controller->PlayerCameraManager->GetCameraLocation());
	}
}

void ASkyActor::UpdateComponentsWithDateTime(const FDateTime &DateTime)
{
	FSunPositionData sunPositionData;
	USunPositionFunctionLibrary::GetSunPosition(
		Latitude,
		Longitude,
		TimeZone,
		IsDayLightSaving,
		DateTime.GetYear(),
		DateTime.GetMonth(),
		DateTime.GetDay(),
		DateTime.GetHour(),
		DateTime.GetMinute(),
		DateTime.GetSecond(),
		sunPositionData);

	DirectionalLight->SetWorldRotation(FRotator(sunPositionData.CorrectedElevation, (sunPositionData.Azimuth - 90.f), 0.f));

	// Change light intensity based on angle
	float newIntensity = UKismetMathLibrary::MapRangeClamped(
		DirectionalLight->GetComponentRotation().Pitch,
		DayAnglePitch,
		0.f,
		DayLightBrightness,
		NightLightBrightness);
	DirectionalLight->SetIntensity(newIntensity);

	// Change light color based on angle
	float newLightColorAlpha = UKismetMathLibrary::MapRangeClamped(
		DirectionalLight->GetComponentRotation().Pitch,
		DawnAnglePitch,
		0.f,
		0.f,
		1.f);
	DirectionalLight->SetLightColor(UKismetMathLibrary::LinearColorLerp(DayColor, DuskColor, newLightColorAlpha));

	SkyLight->RecaptureSky();

}

void ASkyActor::SetCurrentDateTime(const FDateTime &NewDateTime)
{
	CurrentDateTime = NewDateTime;
	UpdateComponentsWithDateTime(CurrentDateTime);
}

void ASkyActor::SetCurrentMonth(int32 NewMonth)
{
	int32 clampMonth = FMath::Clamp(NewMonth, 1, 12);
	int32 clampDay = FMath::Clamp(CurrentDateTime.GetDay(), 1, CurrentDateTime.DaysInMonth(CurrentDateTime.GetYear(), clampMonth));
	CurrentDateTime = FDateTime(CurrentDateTime.GetYear(), clampMonth, clampDay, CurrentDateTime.GetHour(), CurrentDateTime.GetMinute());
	UpdateComponentsWithDateTime(CurrentDateTime);
}

void ASkyActor::SetCurrentDay(int32 NewDay)
{
	int32 clampDay = FMath::Clamp(NewDay, 1, CurrentDateTime.DaysInMonth(CurrentDateTime.GetYear(), CurrentDateTime.GetMonth()));
	CurrentDateTime = FDateTime(CurrentDateTime.GetYear(), CurrentDateTime.GetMonth(), clampDay, CurrentDateTime.GetHour(), CurrentDateTime.GetMinute());
	UpdateComponentsWithDateTime(CurrentDateTime);
}

void ASkyActor::SetCurrentHour(int32 NewHour)
{
	// Note: FDateTime uses 24-hour format, but our menu uses 12-Hour format with am/pm
	int32 clampHour = FMath::Clamp(NewHour, 1, 12) % 12;
	if (CurrentDateTime.IsAfternoon())
	{
		clampHour += 12;
	}

	CurrentDateTime = FDateTime(CurrentDateTime.GetYear(), CurrentDateTime.GetMonth(), CurrentDateTime.GetDay(), clampHour, CurrentDateTime.GetMinute());
	UpdateComponentsWithDateTime(CurrentDateTime);
}

void ASkyActor::SetCurrentMinute(int32 NewMinute)
{
	int32 clampMin = FMath::Clamp(NewMinute, 0, 59);
	CurrentDateTime = FDateTime(CurrentDateTime.GetYear(), CurrentDateTime.GetMonth(), CurrentDateTime.GetDay(), CurrentDateTime.GetHour(), clampMin);
	UpdateComponentsWithDateTime(CurrentDateTime);
}

void ASkyActor::ToggleCurrentMeridiem()
{
	int32 newHourDelta = CurrentDateTime.IsMorning() ? 12 : -12;
	CurrentDateTime = FDateTime(CurrentDateTime.GetYear(), CurrentDateTime.GetMonth(), CurrentDateTime.GetDay(), CurrentDateTime.GetHour() + newHourDelta, CurrentDateTime.GetMinute());
	UpdateComponentsWithDateTime(CurrentDateTime);
}

void ASkyActor::AddTimespanToCurrentDateTime(const FTimespan &Timespan)
{
	CurrentDateTime = CurrentDateTime + Timespan;
	UpdateComponentsWithDateTime(CurrentDateTime);
}

void ASkyActor::SetSkyDomePositionScale(const FVector& Position, const FVector& Scale)
{
	if (SkyDomeMesh)
	{
		SkyDomeMesh->SetRelativeLocation_Direct(Position);
		SkyDomeMesh->SetRelativeScale3D_Direct(Scale);
		SkyDomeMesh->UpdateComponentToWorld();
	}
}

void ASkyActor::SetSkyDomePositionScaleByCameraProjection(bool bIsOrtho)
{
	if (bIsOrtho)
	{
		SetSkyDomePositionScale(OrthogonalDomePosition, OrthogonalDomeScale);
	}
	else
	{
		SetSkyDomePositionScale(PerspectiveDomePosition, PerspectiveDomeScale);
	}
}

void ASkyActor::ToggleBackgroundSkyPlane(bool bVisible, const FVector& CameraPosition, const FRotator& CameraRotation)
{
	BackgroundSkyPlane->SetVisibility(bVisible);
	if (bVisible)
	{
		FVector pos = (CameraRotation.Vector() * BackgroundSkyPlanePreviewDistance) + CameraPosition;
		BackgroundSkyPlane->SetWorldLocation(pos);
		BackgroundSkyPlane->SetWorldRotation(CameraRotation);
		BackgroundSkyPlane->AddRelativeRotation(FRotator(90.f, 0.f, 0.f));
	}
}

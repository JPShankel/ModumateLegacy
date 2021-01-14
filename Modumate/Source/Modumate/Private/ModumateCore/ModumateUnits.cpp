#include "ModumateCore/ModumateUnits.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "CoreMinimal.h"

static constexpr float DrawingPointsPerInch = 72.0f;

float FModumateUnitValue::AsPoints(float WorldToFloorplan) const
{
	return AsFloorplanInches(WorldToFloorplan) * DrawingPointsPerInch;
}

float FModumateUnitValue::AsFloorplanInches(float WorldToFloorplan) const
{
	switch (Type)
	{
	case EModumateUnitType::Points: return Value / DrawingPointsPerInch;
	case EModumateUnitType::FloorplanInches: return Value;
	case EModumateUnitType::WorldInches: return Value / WorldToFloorplan;
	case EModumateUnitType::FontHeight: return Value / 100.0f;
	case EModumateUnitType::AngleRadians: ensureAlways(false); return NAN;
	case EModumateUnitType::AngleDegrees: ensureAlways(false); return NAN;
	case EModumateUnitType::WorldCentimeters: return Value / (WorldToFloorplan * UModumateDimensionStatics::InchesToCentimeters);
	default: ensureAlways(false); return NAN;
	};
}

float FModumateUnitValue::AsFontHeight(float WorldToFloorplan) const
{
	return AsFloorplanInches(WorldToFloorplan) * 100.0f;
}

float FModumateUnitValue::AsRadians() const
{
	switch (Type)
	{
	case EModumateUnitType::AngleDegrees: return FMath::DegreesToRadians(Value);
	case EModumateUnitType::AngleRadians: return Value;
	default: ensureAlways(false); return NAN;
	};
}

float FModumateUnitValue::AsDegrees() const
{
	switch (Type)
	{
	case EModumateUnitType::AngleDegrees: return Value;
	case EModumateUnitType::AngleRadians: return FMath::RadiansToDegrees(Value);
	default: ensureAlways(false); return NAN;
	}
}

float FModumateUnitValue::AsWorldCentimeters(float WorldToFloorplan) const
{
	switch (Type)
	{
	case EModumateUnitType::WorldInches: return Value * UModumateDimensionStatics::InchesToCentimeters;
	case EModumateUnitType::WorldCentimeters: return Value;
	}
	return AsFloorplanInches(WorldToFloorplan) * WorldToFloorplan * UModumateDimensionStatics::InchesToCentimeters;
}

float FModumateUnitValue::AsWorldInches(float WorldToFloorplan) const
{
	switch (Type)
	{
	case EModumateUnitType::WorldInches: return Value;
	case EModumateUnitType::WorldCentimeters: return Value / UModumateDimensionStatics::InchesToCentimeters;
	}
	return AsFloorplanInches(WorldToFloorplan) * WorldToFloorplan;
}

float FModumateUnitValue::AsNative() const
{
	return Value;
}

float FModumateUnitValue::AsUnit(EModumateUnitType UnitType) const
{
	switch (UnitType)
	{
	case EModumateUnitType::Points: return AsPoints();
	case EModumateUnitType::FloorplanInches: return AsFloorplanInches();
	case EModumateUnitType::FontHeight: return AsFontHeight();
	case EModumateUnitType::AngleRadians: return AsRadians();
	case EModumateUnitType::AngleDegrees: return AsDegrees();
	case EModumateUnitType::WorldCentimeters: return AsWorldCentimeters();
	case EModumateUnitType::WorldInches: return AsWorldInches();
	default: return Value;
	}
}

FModumateUnitValue FModumateUnitValue::FloorplanInches(float v) { return FModumateUnitValue(v, EModumateUnitType::FloorplanInches); }
FModumateUnitValue FModumateUnitValue::Points(float v) { return FModumateUnitValue(v, EModumateUnitType::Points); }
FModumateUnitValue FModumateUnitValue::FontHeight(float v) { return FModumateUnitValue(v, EModumateUnitType::FontHeight); }
FModumateUnitValue FModumateUnitValue::Degrees(float v) { return FModumateUnitValue(v, EModumateUnitType::AngleDegrees); }
FModumateUnitValue FModumateUnitValue::Radians(float v) { return FModumateUnitValue(v, EModumateUnitType::AngleRadians); }
FModumateUnitValue FModumateUnitValue::WorldCentimeters(float v) { return FModumateUnitValue(v, EModumateUnitType::WorldCentimeters); }
FModumateUnitValue FModumateUnitValue::WorldInches(float v) { return FModumateUnitValue(v, EModumateUnitType::WorldInches); }

FModumateUnitCoord2D FModumateUnitCoord2D::operator+(const FModumateUnitCoord2D &rhs) const
{
	return FModumateUnitCoord2D(X + rhs.X, Y + rhs.Y);
}

FModumateUnitCoord2D FModumateUnitCoord2D::operator-(const FModumateUnitCoord2D &rhs) const
{
	return FModumateUnitCoord2D(X - rhs.X, Y - rhs.Y);
}

FModumateUnitCoord2D FModumateUnitCoord2D::operator*(float v) const
{
	return FModumateUnitCoord2D(X*v, Y*v);
}

FModumateUnitCoord2D &FModumateUnitCoord2D::operator+=(const FModumateUnitCoord2D &rhs)
{
	*this = *this + rhs;
	return *this;
}

FModumateUnitCoord2D &FModumateUnitCoord2D::operator-=(const FModumateUnitCoord2D &rhs)
{
	*this = *this - rhs;
	return *this;
}

FModumateUnitCoord2D &FModumateUnitCoord2D::operator*=(float rhs)
{
	*this = *this * rhs;
	return *this;
}

ModumateUnitParams::FLength FModumateUnitCoord2D::Size() const
{
	// Arbitrarily choosing X here, making an assumption that X and Y are the same type, 
	// which is an assumption that is made by the FModumateUnitCoord2D::As<T> functions as well
	return ModumateUnitParams::FLength(FModumateUnitValue(AsNative().Size(), X.GetUnitType()));
}

bool FModumateUnitCoord2D::operator==(const FModumateUnitCoord2D &rhs) const
{
	return rhs.X == X && rhs.Y == Y;
}

bool FModumateUnitCoord2D::operator>(const FModumateUnitCoord2D &rhs) const
{
	return AsWorldCentimeters() > rhs.AsWorldCentimeters();
}

bool FModumateUnitCoord2D::operator<(const FModumateUnitCoord2D &rhs) const
{
	return AsWorldCentimeters() < rhs.AsWorldCentimeters();
}

bool FModumateUnitCoord2D::operator>=(const FModumateUnitCoord2D &rhs) const
{
	return AsWorldCentimeters() >= rhs.AsWorldCentimeters();
}

bool FModumateUnitCoord2D::operator<=(const FModumateUnitCoord2D &rhs) const
{
	return AsWorldCentimeters() <= rhs.AsWorldCentimeters();
}

FModumateUnitCoord2D FModumateUnitCoord2D::Points(const FModumateUnitCoord2D &v)
{
	return FModumateUnitCoord2D::Points(v.AsPoints());
}

FModumateUnitCoord2D FModumateUnitCoord2D::FloorplanInches(const FModumateUnitCoord2D &v)
{
	return FModumateUnitCoord2D::FloorplanInches(v.AsFloorplanInches());
}

FModumateUnitCoord2D FModumateUnitCoord2D::FontHeight(const FModumateUnitCoord2D &v)
{
	return FModumateUnitCoord2D::FontHeight(v.AsFloorplanInches());
}

FModumateUnitCoord2D FModumateUnitCoord2D::Radians(const FModumateUnitCoord2D &v)
{
	return FModumateUnitCoord2D::Radians(v.AsRadians());
}

FModumateUnitCoord2D FModumateUnitCoord2D::Degrees(const FModumateUnitCoord2D &v)
{
	return FModumateUnitCoord2D::Degrees(v.AsDegrees());
}

FModumateUnitCoord2D FModumateUnitCoord2D::WorldCentimeters(const FModumateUnitCoord2D &v)
{
	return FModumateUnitCoord2D::WorldCentimeters(v.AsWorldCentimeters());
}

FModumateUnitCoord2D FModumateUnitCoord2D::WorldInches(const FModumateUnitCoord2D &v)
{
	return FModumateUnitCoord2D::WorldInches(v.AsWorldInches());
}

FModumateUnitCoord2D operator*(float v, const FModumateUnitCoord2D &rhs)
{
	return rhs * v;
}

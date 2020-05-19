#include "ModumateCore/ModumateUnits.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "CoreMinimal.h"

namespace Modumate
{
	namespace Units {
		float FUnitValue::AsPoints(float WorldToFloorplan) const
		{
			return AsFloorplanInches(WorldToFloorplan) * 72.0f;
		}

		float FUnitValue::AsFloorplanInches(float WorldToFloorplan) const
		{
			switch (Type)
			{
			case EUnitType::Points: return Value / 72.0f;
			case EUnitType::FloorplanInches: return Value;
			case EUnitType::WorldInches: return Value / WorldToFloorplan;
			case EUnitType::FontHeight: return Value / 100.0f;
			case EUnitType::AngleRadians: ensureAlways(false); return NAN;
			case EUnitType::AngleDegrees: ensureAlways(false); return NAN;
			case EUnitType::WorldCentimeters: return Value / (WorldToFloorplan * InchesToCentimeters);
			default: ensureAlways(false); return NAN;
			};
		}

		float FUnitValue::AsFontHeight(float WorldToFloorplan) const
		{
			return AsFloorplanInches(WorldToFloorplan) * 100.0f;
		}

		float FUnitValue::AsRadians() const
		{
			switch (Type)
			{
			case EUnitType::AngleDegrees: return FMath::DegreesToRadians(Value);
			case EUnitType::AngleRadians: return Value;
			default: ensureAlways(false); return NAN;
			};
		}

		float FUnitValue::AsDegrees() const
		{
			switch (Type)
			{
			case EUnitType::AngleDegrees: return Value;
			case EUnitType::AngleRadians: return FMath::RadiansToDegrees(Value);
			default: ensureAlways(false); return NAN;
			}
		}

		float FUnitValue::AsWorldCentimeters(float WorldToFloorplan) const
		{
			switch (Type)
			{
			case EUnitType::WorldInches: return Value * InchesToCentimeters;
			case EUnitType::WorldCentimeters: return Value;
			}
			return AsFloorplanInches(WorldToFloorplan) * WorldToFloorplan * InchesToCentimeters;
		}

		float FUnitValue::AsWorldInches(float WorldToFloorplan) const
		{
			switch (Type)
			{
			case EUnitType::WorldInches: return Value ;
			case EUnitType::WorldCentimeters: return Value / InchesToCentimeters;
			}
			return AsFloorplanInches(WorldToFloorplan) * WorldToFloorplan;
		}

		float FUnitValue::AsNative() const
		{
			return Value;
		}

		float FUnitValue::AsUnit(EUnitType unitType) const
		{
			switch (unitType)
			{
			case EUnitType::Points: return AsPoints();
			case EUnitType::FloorplanInches: return AsFloorplanInches();
			case EUnitType::FontHeight: return AsFontHeight();
			case EUnitType::AngleRadians: return AsRadians();
			case EUnitType::AngleDegrees: return AsDegrees();
			case EUnitType::WorldCentimeters: return AsWorldCentimeters();
			case EUnitType::WorldInches: return AsWorldInches();
			default: return Value;
			}
		}

		FUnitValue FUnitValue::FloorplanInches(float v) { return FUnitValue(v, EUnitType::FloorplanInches); }
		FUnitValue FUnitValue::Points(float v) { return FUnitValue(v, EUnitType::Points); }
		FUnitValue FUnitValue::FontHeight(float v) { return FUnitValue(v, EUnitType::FontHeight); }
		FUnitValue FUnitValue::Degrees(float v) { return FUnitValue(v, EUnitType::AngleDegrees); }
		FUnitValue FUnitValue::Radians(float v) { return FUnitValue(v, EUnitType::AngleRadians); }
		FUnitValue FUnitValue::WorldCentimeters(float v) { return FUnitValue(v, EUnitType::WorldCentimeters); }
		FUnitValue FUnitValue::WorldInches(float v) { return FUnitValue(v, EUnitType::WorldInches); }

		FCoordinates2D FCoordinates2D::operator+(const FCoordinates2D &rhs) const
		{
			return FCoordinates2D(X + rhs.X, Y + rhs.Y);
		}

		FCoordinates2D FCoordinates2D::operator-(const FCoordinates2D &rhs) const
		{
			return FCoordinates2D(X - rhs.X, Y - rhs.Y);
		}

		FCoordinates2D FCoordinates2D::operator*(float v) const
		{
			return FCoordinates2D(X*v, Y*v);
		}

		FCoordinates2D &FCoordinates2D::operator+=(const FCoordinates2D &rhs)
		{
			*this = *this + rhs;
			return *this;
		}

		FCoordinates2D &FCoordinates2D::operator-=(const FCoordinates2D &rhs)
		{
			*this = *this - rhs;
			return *this;
		}

		FCoordinates2D &FCoordinates2D::operator*=(float rhs)
		{
			*this = *this * rhs;
			return *this;
		}

		FLength FCoordinates2D::Size() const
		{
			// Arbitrarily choosing X here, making an assumption that X and Y are the same type, 
			// which is an assumption that is made by the FCoordinates2D::As<T> functions as well
			return FLength(FUnitValue(AsNative().Size(), X.GetUnitType()));
		}

		bool FCoordinates2D::operator==(const FCoordinates2D &rhs) const
		{
			return rhs.X == X && rhs.Y == Y;
		}

		bool FCoordinates2D::operator>(const FCoordinates2D &rhs) const
		{
			return AsWorldCentimeters() > rhs.AsWorldCentimeters();
		}

		bool FCoordinates2D::operator<(const FCoordinates2D &rhs) const
		{
			return AsWorldCentimeters() < rhs.AsWorldCentimeters();
		}

		bool FCoordinates2D::operator>=(const FCoordinates2D &rhs) const
		{
			return AsWorldCentimeters() >= rhs.AsWorldCentimeters();
		}

		bool FCoordinates2D::operator<=(const FCoordinates2D &rhs) const
		{
			return AsWorldCentimeters() <= rhs.AsWorldCentimeters();
		}

		FCoordinates2D FCoordinates2D::Points(const FCoordinates2D &v)
		{
			return FCoordinates2D::Points(v.AsPoints());
		}

		FCoordinates2D FCoordinates2D::FloorplanInches(const FCoordinates2D &v)
		{
			return FCoordinates2D::FloorplanInches(v.AsFloorplanInches());
		}

		FCoordinates2D FCoordinates2D::FontHeight(const FCoordinates2D &v)
		{
			return FCoordinates2D::FontHeight(v.AsFloorplanInches());
		}

		FCoordinates2D FCoordinates2D::Radians(const FCoordinates2D &v)
		{
			return FCoordinates2D::Radians(v.AsRadians());
		}

		FCoordinates2D FCoordinates2D::Degrees(const FCoordinates2D &v)
		{
			return FCoordinates2D::Degrees(v.AsDegrees());
		}

		FCoordinates2D FCoordinates2D::WorldCentimeters(const FCoordinates2D &v)
		{
			return FCoordinates2D::WorldCentimeters(v.AsWorldCentimeters());
		}

		FCoordinates2D FCoordinates2D::WorldInches(const FCoordinates2D &v)
		{
			return FCoordinates2D::WorldInches(v.AsWorldInches());
		}
	}
}

Modumate::Units::FCoordinates2D operator*(float v, const Modumate::Units::FCoordinates2D &rhs)
{
	return rhs * v;
}

// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModumateUnits.generated.h"

/*
* 
* Provides syntactic decoration for common unit conversions (ie imperial to metric or world units to drawing page units)
* The motivation is to make unit conversions consistent and to eliminate the ambiguity of bare numbers
* 
* To get 1 inch in centimeters: FModumateUnitValue::WorldInches(1).AsWorldCentimeters() 
* This replaces ad-hoc multiplication in-line
* 
* Also provides templated types for numerical parameters (like X coordinate or Width)
* The motivation is to provide syntactic distinction for the role units play
* 
* For example: auto myRect = MakeRect(1,2,3,4);
* Is this a rectangle at 1,2 extending to 3,4? Or at 1,2 with width 3 and height 4? 
* If the function simply takes 4 floats, it can be easy to get confused
* 
* With templated param types, we can distiguish the two cases at compile time
* 
* FRect MakeRect(
*	ModumateUnitParams::FXCoord X1,
*	ModumateUnitParams::FYCoord Y1,
*	ModumateUnitParams::FXCoord X2,
*	ModumateUnitParams::FYCoord Y2);
*
*	..or..
*
* FRect MakeRect(
*	ModumateUnitParams::FXCoord X1,
*	ModumateUnitParams::FYCoord Y1,
*	ModumateUnitParams::FWidth Width,
*	ModumateUnitParams::FHeight Height);
* 
* The role of parameters is now known at the call point:
* 
* auto myRect = MakeRect(
*	ModumateUnitParams::FXCoord(1),
*	ModumateUnitParams::FYCoord(2),
*	ModumateUnitParams::Width(3),
*	ModumateUnitParams::Height(4));
*
*/

UENUM()
enum class EModumateUnitType : uint8 
{
	Points = 0,
	FloorplanInches,
	FontHeight,
	AngleRadians,
	AngleDegrees,
	WorldCentimeters,
	WorldInches,
	NUM
};

USTRUCT()
struct MODUMATE_API FModumateUnitValue
{
	GENERATED_BODY()

	private:
		UPROPERTY()
		float Value;

		UPROPERTY()
		EModumateUnitType Type;

	public:
		static constexpr float DefaultWorldToFloorplan = 48.0f; // (1/4" = 1')

		FModumateUnitValue() : Value(0.0f), Type(EModumateUnitType::WorldCentimeters) {}
		FModumateUnitValue(float v, EModumateUnitType t) : Value(v), Type(t) {}
		float AsFloorplanInches(float WorldToFloorplan = DefaultWorldToFloorplan) const;
		float AsPoints(float WorldToFloorplan = DefaultWorldToFloorplan) const;
		float AsFontHeight(float WorldToFloorplan = DefaultWorldToFloorplan) const;
		float AsDegrees() const;
		float AsWorldCentimeters(float WorldToFloorplan = DefaultWorldToFloorplan) const;
		float AsRadians() const;
		float AsWorldInches(float WorldToFloorplan = DefaultWorldToFloorplan) const;
		float AsNative() const;
		float AsUnit(EModumateUnitType UnitType) const;
		EModumateUnitType GetUnitType() const { return Type; }

		static FModumateUnitValue FloorplanInches(float v);
		static FModumateUnitValue Points(float v);
		static FModumateUnitValue FontHeight(float v);
		static FModumateUnitValue Degrees(float v);
		static FModumateUnitValue WorldCentimeters(float v);
		static FModumateUnitValue Radians(float v);
		static FModumateUnitValue WorldInches(float v);
};

UENUM()
enum class EModumateUnitParam : uint8
{
	X = 0,
	Y,
	Z,
	FontSize,
	Thickness,
	Radius,
	Angle,
	Margin,
	Offset,
	Width,
	Height,
	Phase,
	Length
};

template <EModumateUnitParam T>
class MODUMATE_API TModumateUnitParam
{
	friend class TModumateUnitParam<EModumateUnitParam::X>;
	friend class TModumateUnitParam<EModumateUnitParam::Y>;
	friend class TModumateUnitParam<EModumateUnitParam::Z>;
	friend class TModumateUnitParam<EModumateUnitParam::FontSize>;
	friend class TModumateUnitParam<EModumateUnitParam::Thickness>;
	friend class TModumateUnitParam<EModumateUnitParam::Radius>;
	friend class TModumateUnitParam<EModumateUnitParam::Angle>;
	friend class TModumateUnitParam<EModumateUnitParam::Margin>;
	friend class TModumateUnitParam<EModumateUnitParam::Offset>;
	friend class TModumateUnitParam<EModumateUnitParam::Width>;
	friend class TModumateUnitParam<EModumateUnitParam::Height>;
	friend class TModumateUnitParam<EModumateUnitParam::Phase>;
	friend class TModumateUnitParam<EModumateUnitParam::Length>;

private:
	FModumateUnitValue Value;

public:
	TModumateUnitParam(const FModumateUnitValue& v) : Value(v) {}

	template<EModumateUnitParam M>
	TModumateUnitParam(const TModumateUnitParam<M>& v) : Value(v.Value) {}
	TModumateUnitParam() : Value(FModumateUnitValue::WorldCentimeters(0.0)) {};

	float AsPoints(float scale = FModumateUnitValue::DefaultWorldToFloorplan) const { return Value.AsPoints(scale); }
	float AsFloorplanInches(float scale = FModumateUnitValue::DefaultWorldToFloorplan) const { return Value.AsFloorplanInches(scale); }
	float AsFontHeight(float scale = FModumateUnitValue::DefaultWorldToFloorplan) const { return Value.AsFontHeight(scale); }
	float AsRadians() const { return Value.AsRadians(); }
	float AsDegrees() const { return Value.AsDegrees(); }
	float AsWorldCentimeters(float scale = FModumateUnitValue::DefaultWorldToFloorplan) const { return Value.AsWorldCentimeters(scale); }
	float AsWorldInches(float scale = FModumateUnitValue::DefaultWorldToFloorplan) const { return Value.AsWorldInches(scale); }
	float AsNative() const { return Value.AsNative(); }
	EModumateUnitType GetUnitType() const { return Value.GetUnitType(); }

	static TModumateUnitParam<T> Points(float v) { return TModumateUnitParam<T>(FModumateUnitValue::Points(v)); }
	static TModumateUnitParam<T> FloorplanInches(float v) { return TModumateUnitParam<T>(FModumateUnitValue::FloorplanInches(v)); }
	static TModumateUnitParam<T> FontHeight(float v) { return TModumateUnitParam<T>(FModumateUnitValue::FontHeight(v)); }
	static TModumateUnitParam<T> Radians(float v) { return TModumateUnitParam<T>(FModumateUnitValue::Radians(v)); }
	static TModumateUnitParam<T> Degrees(float v) { return TModumateUnitParam<T>(FModumateUnitValue::Degrees(v)); }
	static TModumateUnitParam<T> WorldCentimeters(float v) { return TModumateUnitParam<T>(FModumateUnitValue::WorldCentimeters(v)); }
	static TModumateUnitParam<T> WorldInches(float v) { return TModumateUnitParam<T>(FModumateUnitValue::WorldInches(v)); }

	inline bool operator>(const TModumateUnitParam<T>& rhs) const { return Value.AsWorldCentimeters() > rhs.AsWorldCentimeters(); }
	inline bool operator<(const TModumateUnitParam<T>& rhs) const { return Value.AsWorldCentimeters() < rhs.AsWorldCentimeters(); }
	inline bool operator==(const TModumateUnitParam<T>& rhs) const { return Value.AsWorldCentimeters() == rhs.AsWorldCentimeters(); }
	inline bool operator>=(const TModumateUnitParam<T>& rhs) const { return Value.AsWorldCentimeters() >= rhs.AsWorldCentimeters(); }
	inline bool operator<=(const TModumateUnitParam<T>& rhs) const { return Value.AsWorldCentimeters() <= rhs.AsWorldCentimeters(); }

	// these operators attempt to avoid unit conversions when they aren't necessary
	TModumateUnitParam<T> operator+(const TModumateUnitParam<T>& rhs) const
	{
		auto type = Value.GetUnitType();
		if (type == rhs.GetUnitType())
		{
			return FModumateUnitValue(Value.AsNative() + rhs.AsNative(), type);
		}
		return TModumateUnitParam<T>::WorldCentimeters(Value.AsWorldCentimeters() + rhs.AsWorldCentimeters());
	}

	TModumateUnitParam<T> operator-(const TModumateUnitParam<T>& rhs) const
	{
		auto type = Value.GetUnitType();
		if (type == rhs.GetUnitType())
		{
			return FModumateUnitValue(Value.AsNative() - rhs.AsNative(), type);
		}
		return TModumateUnitParam<T>::WorldCentimeters(Value.AsWorldCentimeters() - rhs.AsWorldCentimeters());
	}

	TModumateUnitParam<T> operator*(float rhs) const { return FModumateUnitValue(Value.AsNative() * rhs, GetUnitType()); }

	TModumateUnitParam<T> operator/(float rhs) const { return FModumateUnitValue(Value.AsNative() / rhs, GetUnitType()); }

	inline TModumateUnitParam<T>& operator+=(const TModumateUnitParam<T>& rhs) { *this = *this + rhs; return *this; }
	inline TModumateUnitParam<T>& operator-=(const TModumateUnitParam<T>& rhs) { *this = *this - rhs; return *this; }
	inline TModumateUnitParam<T>& operator*=(float rhs) { *this = *this * rhs; return *this; }
	inline TModumateUnitParam<T>& operator/=(float rhs) { *this = *this / rhs; return *this; }

	template<EModumateUnitParam M>
	inline TModumateUnitParam<M> As() { return TModumateUnitParam<M>(Value); }
};

namespace ModumateUnitParams
{
	using FXCoord = TModumateUnitParam<EModumateUnitParam::X>;
	using FYCoord = TModumateUnitParam<EModumateUnitParam::Y>;
	using FZCoord = TModumateUnitParam<EModumateUnitParam::Z>;
	using FFontSize = TModumateUnitParam<EModumateUnitParam::FontSize>;
	using FThickness = TModumateUnitParam<EModumateUnitParam::Thickness>;
	using FAngle = TModumateUnitParam<EModumateUnitParam::Angle>;
	using FRadius = TModumateUnitParam<EModumateUnitParam::Radius>;
	using FMarginParam = TModumateUnitParam<EModumateUnitParam::Margin>;
	using FWidth = TModumateUnitParam<EModumateUnitParam::Width>;
	using FPhase = TModumateUnitParam<EModumateUnitParam::Phase>;
	using FHeight = TModumateUnitParam<EModumateUnitParam::Height>;
	using FOffset = TModumateUnitParam<EModumateUnitParam::Offset>;
	using FLength = TModumateUnitParam<EModumateUnitParam::Length>;
}

/*
* Provides a 2D vector using UnitParams (used in drawing to translate between world and page coordinates)
*/
struct MODUMATE_API FModumateUnitCoord2D
{
	ModumateUnitParams::FXCoord X;
	ModumateUnitParams::FYCoord Y;

	FModumateUnitCoord2D() : X(ModumateUnitParams::FXCoord::Points(0)), Y(ModumateUnitParams::FYCoord::Points(0)) {}
	FModumateUnitCoord2D(const ModumateUnitParams::FXCoord& x, const ModumateUnitParams::FYCoord& y) : X(x), Y(y) {}

	FModumateUnitCoord2D operator+(const FModumateUnitCoord2D& rhs) const;
	FModumateUnitCoord2D operator-(const FModumateUnitCoord2D& rhs) const;
	FModumateUnitCoord2D operator*(float rhs) const;

	FModumateUnitCoord2D& operator+=(const FModumateUnitCoord2D& rhs);
	FModumateUnitCoord2D& operator-=(const FModumateUnitCoord2D& rhs);
	FModumateUnitCoord2D& operator*=(float rhs);
	ModumateUnitParams::FLength Size() const;

	bool operator==(const FModumateUnitCoord2D& rhs) const;
	bool operator>(const FModumateUnitCoord2D& rhs) const;
	bool operator<(const FModumateUnitCoord2D& rhs) const;
	bool operator>=(const FModumateUnitCoord2D& rhs) const;
	bool operator<=(const FModumateUnitCoord2D& rhs) const;

	FVector2D AsPoints(float scale = FModumateUnitValue::DefaultWorldToFloorplan) const { return FVector2D(X.AsPoints(scale), Y.AsPoints(scale)); }
	FVector2D AsFloorplanInches(float scale = FModumateUnitValue::DefaultWorldToFloorplan) const { return FVector2D(X.AsFloorplanInches(scale), Y.AsFloorplanInches(scale)); }
	FVector2D AsFontHeight(float scale = FModumateUnitValue::DefaultWorldToFloorplan) const { return FVector2D(X.AsFontHeight(scale), Y.AsFontHeight(scale)); }
	FVector2D AsRadians() const { return FVector2D(X.AsRadians(), Y.AsRadians()); }
	FVector2D AsDegrees() const { return FVector2D(X.AsDegrees(), Y.AsDegrees()); }
	FVector2D AsWorldCentimeters(float scale = FModumateUnitValue::DefaultWorldToFloorplan) const { return FVector2D(X.AsWorldCentimeters(scale), Y.AsWorldCentimeters(scale)); }
	FVector2D AsWorldInches(float scale = FModumateUnitValue::DefaultWorldToFloorplan) const { return FVector2D(X.AsWorldInches(scale), Y.AsWorldInches(scale)); }
	FVector2D AsNative() const { return FVector2D(X.AsNative(), Y.AsNative()); }

	FVector2D Normalized() const { return AsNative().GetSafeNormal(); }

	static FModumateUnitCoord2D Points(const FVector2D& v) { return FModumateUnitCoord2D(ModumateUnitParams::FXCoord::Points(v.X), ModumateUnitParams::FYCoord::Points(v.Y)); }
	static FModumateUnitCoord2D FloorplanInches(const FVector2D& v) { return FModumateUnitCoord2D(ModumateUnitParams::FXCoord::FloorplanInches(v.X), ModumateUnitParams::FYCoord::FloorplanInches(v.Y)); }
	static FModumateUnitCoord2D FontHeight(const FVector2D& v) { return FModumateUnitCoord2D(ModumateUnitParams::FXCoord::FontHeight(v.X), ModumateUnitParams::FYCoord::FontHeight(v.Y)); }
	static FModumateUnitCoord2D Radians(const FVector2D& v) { return FModumateUnitCoord2D(ModumateUnitParams::FXCoord::Radians(v.X), ModumateUnitParams::FYCoord::Radians(v.Y)); }
	static FModumateUnitCoord2D Degrees(const FVector2D& v) { return FModumateUnitCoord2D(ModumateUnitParams::FXCoord::Degrees(v.X), ModumateUnitParams::FYCoord::Degrees(v.Y)); }
	static FModumateUnitCoord2D WorldCentimeters(const FVector2D& v) { return FModumateUnitCoord2D(ModumateUnitParams::FXCoord::WorldCentimeters(v.X), ModumateUnitParams::FYCoord::WorldCentimeters(v.Y)); }
	static FModumateUnitCoord2D WorldInches(const FVector2D& v) { return FModumateUnitCoord2D(ModumateUnitParams::FXCoord::WorldInches(v.X), ModumateUnitParams::FYCoord::WorldInches(v.Y)); }

	static FModumateUnitCoord2D Points(const FModumateUnitCoord2D& v);
	static FModumateUnitCoord2D FloorplanInches(const FModumateUnitCoord2D& v);
	static FModumateUnitCoord2D FontHeight(const FModumateUnitCoord2D& v);
	static FModumateUnitCoord2D Radians(const FModumateUnitCoord2D& v);
	static FModumateUnitCoord2D Degrees(const FModumateUnitCoord2D& v);
	static FModumateUnitCoord2D WorldCentimeters(const FModumateUnitCoord2D& v);
	static FModumateUnitCoord2D WorldInches(const FModumateUnitCoord2D& v);
};

FModumateUnitCoord2D operator*(float v, const FModumateUnitCoord2D &rhs);

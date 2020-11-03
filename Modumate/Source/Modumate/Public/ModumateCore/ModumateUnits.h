// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

namespace Modumate
{
	static constexpr float InchesToCentimeters = 2.54f;
	static constexpr float CentimetersToInches = 1.0f / InchesToCentimeters;
	static constexpr float InchesPerFoot = 12.0f;
	static constexpr float DefaultWorldToFloorplan = 48.0f; // (1/4" = 1')

	namespace Units {

		enum EUnitType
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

//		static void SetWorldToFloorplanScale(float scale) { WorldToFloorplan = scale; };

		class MODUMATE_API FUnitValue
		{
		private:
			float Value;
			EUnitType Type;
		public:
			FUnitValue() : Value(0.0f), Type(EUnitType::WorldCentimeters) {}
			FUnitValue(float v, EUnitType t) : Value(v), Type(t) {}
			float AsFloorplanInches(float WorldToFloorplan = DefaultWorldToFloorplan) const;
			float AsPoints(float WorldToFloorplan = DefaultWorldToFloorplan) const;
			float AsFontHeight(float WorldToFloorplan = DefaultWorldToFloorplan) const;
			float AsDegrees() const;
			float AsWorldCentimeters(float WorldToFloorplan = DefaultWorldToFloorplan) const;
			float AsRadians() const;
			float AsWorldInches(float WorldToFloorplan = DefaultWorldToFloorplan) const;
			float AsNative() const;
			float AsUnit(EUnitType unitType) const;
			EUnitType GetUnitType() const { return Type; }


			static FUnitValue FloorplanInches(float v);
			static FUnitValue Points(float v);
			static FUnitValue FontHeight(float v);
			static FUnitValue Degrees(float v);
			static FUnitValue WorldCentimeters(float v);
			static FUnitValue Radians(float v);
			static FUnitValue WorldInches(float v);
		};

		enum EParamType
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

		template <EParamType T>
		class MODUMATE_API TParam
		{
			friend class TParam<EParamType::X>;
			friend class TParam<EParamType::Y>;
			friend class TParam<EParamType::Z>;
			friend class TParam<EParamType::FontSize>;
			friend class TParam<EParamType::Thickness>;
			friend class TParam<EParamType::Radius>;
			friend class TParam<EParamType::Angle>;
			friend class TParam<EParamType::Margin>;
			friend class TParam<EParamType::Offset>;
			friend class TParam<EParamType::Width>;
			friend class TParam<EParamType::Height>;
			friend class TParam<EParamType::Phase>;
			friend class TParam<EParamType::Length>;

		private:
			FUnitValue Value;

		public:

			TParam(const FUnitValue &v) : Value(v) {}

			template<EParamType M>
			TParam(const TParam<M> &v) : Value(v.Value){}
			TParam() : Value(FUnitValue::WorldCentimeters(0.0)) {};

			float AsPoints(float scale = DefaultWorldToFloorplan) const { return Value.AsPoints(scale); }
			float AsFloorplanInches(float scale = DefaultWorldToFloorplan) const { return Value.AsFloorplanInches(scale); }
			float AsFontHeight(float scale = DefaultWorldToFloorplan) const { return Value.AsFontHeight(scale); }
			float AsRadians() const { return Value.AsRadians(); }
			float AsDegrees() const { return Value.AsDegrees(); }
			float AsWorldCentimeters(float scale = DefaultWorldToFloorplan) const { return Value.AsWorldCentimeters(scale); }
			float AsWorldInches(float scale = DefaultWorldToFloorplan) const { return Value.AsWorldInches(scale); }
			float AsNative() const { return Value.AsNative(); }
			EUnitType GetUnitType() const { return Value.GetUnitType(); }

			static TParam<T> Points(float v) { return TParam<T>(FUnitValue::Points(v)); }
			static TParam<T> FloorplanInches(float v) { return TParam<T>(FUnitValue::FloorplanInches(v)); }
			static TParam<T> FontHeight(float v) { return TParam<T>(FUnitValue::FontHeight(v)); }
			static TParam<T> Radians(float v) { return TParam<T>(FUnitValue::Radians(v)); }
			static TParam<T> Degrees(float v) { return TParam<T>(FUnitValue::Degrees(v)); }
			static TParam<T> WorldCentimeters(float v) { return TParam<T>(FUnitValue::WorldCentimeters(v)); }
			static TParam<T> WorldInches(float v) { return TParam<T>(FUnitValue::WorldInches(v)); }

			inline bool operator>(const TParam<T> &rhs) const { return Value.AsWorldCentimeters() > rhs.AsWorldCentimeters(); }
			inline bool operator<(const TParam<T> &rhs) const { return Value.AsWorldCentimeters() < rhs.AsWorldCentimeters(); }
			inline bool operator==(const TParam<T> &rhs) const { return Value.AsWorldCentimeters() == rhs.AsWorldCentimeters(); }
			inline bool operator>=(const TParam<T> &rhs) const { return Value.AsWorldCentimeters() >= rhs.AsWorldCentimeters(); }
			inline bool operator<=(const TParam<T> &rhs) const { return Value.AsWorldCentimeters() <= rhs.AsWorldCentimeters(); }

			// these operators attempt to avoid unit conversions when they aren't necessary
			TParam<T> operator+(const TParam<T> &rhs) const
			{ 
				auto type = Value.GetUnitType();
				if (type == rhs.GetUnitType())
				{
					return FUnitValue(Value.AsNative() + rhs.AsNative(), type);
				}
				return TParam<T>::WorldCentimeters(Value.AsWorldCentimeters() + rhs.AsWorldCentimeters()); 
			}

			TParam<T> operator-(const TParam<T> &rhs) const
			{ 
				auto type = Value.GetUnitType();
				if (type == rhs.GetUnitType())
				{
					return FUnitValue(Value.AsNative() - rhs.AsNative(), type);
				}
				return TParam<T>::WorldCentimeters(Value.AsWorldCentimeters() - rhs.AsWorldCentimeters()); 
			}

			TParam<T> operator*(float rhs) const { return FUnitValue(Value.AsNative() * rhs, GetUnitType()); }

			TParam<T> operator/(float rhs) const { return FUnitValue(Value.AsNative() / rhs, GetUnitType()); }

			inline TParam<T>& operator+=(const TParam<T> &rhs) { *this = *this + rhs; return *this; }
			inline TParam<T>& operator-=(const TParam<T> &rhs) { *this = *this - rhs; return *this; }
			inline TParam<T>& operator*=(float rhs) { *this = *this * rhs; return *this; }
			inline TParam<T>& operator/=(float rhs) { *this = *this / rhs; return *this; }

			template<EParamType M>
			inline TParam<M> As() { return TParam<M>(Value); }
		};

		typedef TParam<X> FXCoord;
		typedef TParam<Y> FYCoord;
		typedef TParam<Z> FZCoord;
		typedef TParam<FontSize> FFontSize;
		typedef TParam<Thickness> FThickness;
		typedef TParam<Angle> FAngle;
		typedef TParam<Radius> FRadius;
		typedef TParam<Margin> FMarginParam;
		typedef TParam<Width> FWidth;
		typedef TParam<Phase> FPhase;
		typedef TParam<Height> FHeight;
		typedef TParam<Offset> FOffset;
		typedef TParam<Length> FLength;

		class FCoordinates2D
		{
		public:
			FXCoord X;
			FYCoord Y;

			FCoordinates2D() : X(FXCoord::Points(0)), Y(FYCoord::Points(0)) {}
			FCoordinates2D(const FXCoord &x, const FYCoord &y) : X(x), Y(y) {}

			FCoordinates2D operator+(const FCoordinates2D &rhs) const;
			FCoordinates2D operator-(const FCoordinates2D &rhs) const;
			FCoordinates2D operator*(float rhs) const;

			FCoordinates2D &operator+=(const FCoordinates2D &rhs);
			FCoordinates2D &operator-=(const FCoordinates2D &rhs);
			FCoordinates2D &operator*=(float rhs);
			FLength Size() const;

			bool operator==(const FCoordinates2D &rhs) const;
			bool operator>(const FCoordinates2D &rhs) const;
			bool operator<(const FCoordinates2D &rhs) const;
			bool operator>=(const FCoordinates2D &rhs) const;
			bool operator<=(const FCoordinates2D &rhs) const;

			FVector2D AsPoints(float scale = DefaultWorldToFloorplan) const { return FVector2D(X.AsPoints(scale),Y.AsPoints(scale)); }
			FVector2D AsFloorplanInches(float scale = DefaultWorldToFloorplan) const { return FVector2D(X.AsFloorplanInches(scale),Y.AsFloorplanInches(scale)); }
			FVector2D AsFontHeight(float scale = DefaultWorldToFloorplan) const { return FVector2D(X.AsFontHeight(scale), Y.AsFontHeight(scale)); }
			FVector2D AsRadians() const { return FVector2D(X.AsRadians(), Y.AsRadians()); }
			FVector2D AsDegrees() const { return FVector2D(X.AsDegrees(), Y.AsDegrees()); }
			FVector2D AsWorldCentimeters(float scale = DefaultWorldToFloorplan) const { return FVector2D(X.AsWorldCentimeters(scale), Y.AsWorldCentimeters(scale)); }
			FVector2D AsWorldInches(float scale = DefaultWorldToFloorplan) const { return FVector2D(X.AsWorldInches(scale), Y.AsWorldInches(scale)); }
			FVector2D AsNative() const { return FVector2D(X.AsNative(), Y.AsNative()); }

			FVector2D Normalized() const { return AsNative().GetSafeNormal(); }

			static FCoordinates2D Points(const FVector2D &v) { return FCoordinates2D(FXCoord::Points(v.X),FYCoord::Points(v.Y)); }
			static FCoordinates2D FloorplanInches(const FVector2D &v) { return FCoordinates2D(FXCoord::FloorplanInches(v.X), FYCoord::FloorplanInches(v.Y)); }
			static FCoordinates2D FontHeight(const FVector2D &v) { return FCoordinates2D(FXCoord::FontHeight(v.X), FYCoord::FontHeight(v.Y)); }
			static FCoordinates2D Radians(const FVector2D &v) { return FCoordinates2D(FXCoord::Radians(v.X), FYCoord::Radians(v.Y)); }
			static FCoordinates2D Degrees(const FVector2D &v) { return FCoordinates2D(FXCoord::Degrees(v.X), FYCoord::Degrees(v.Y)); }
			static FCoordinates2D WorldCentimeters(const FVector2D &v) { return FCoordinates2D(FXCoord::WorldCentimeters(v.X), FYCoord::WorldCentimeters(v.Y)); }
			static FCoordinates2D WorldInches(const FVector2D &v) { return FCoordinates2D(FXCoord::WorldInches(v.X), FYCoord::WorldInches(v.Y)); }

			static FCoordinates2D Points(const FCoordinates2D &v);
			static FCoordinates2D FloorplanInches(const FCoordinates2D &v);
			static FCoordinates2D FontHeight(const FCoordinates2D &v);
			static FCoordinates2D Radians(const FCoordinates2D &v);
			static FCoordinates2D Degrees(const FCoordinates2D &v);
			static FCoordinates2D WorldCentimeters(const FCoordinates2D &v);
			static FCoordinates2D WorldInches(const FCoordinates2D &v);
		};
	}
}

Modumate::Units::FCoordinates2D operator*(float v, const Modumate::Units::FCoordinates2D &rhs);

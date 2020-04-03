// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ExpressionEvaluator.h"
#include <regex>
#include "Modumate.h"
#include "Math/BasicMathExpressionEvaluator.h"

#define USE_UE4_EVALUATOR 1

namespace Modumate
{
	namespace Expression
	{
		bool ReplaceVariable(FString &exprString, const FString &varName, float varValue)
		{
			// Adapated from FString::Replace, but with some expression-specific features and optimizations.

			if (exprString.IsEmpty())
			{
				return false;
			}

			// get a pointer into the character data
			const TCHAR* exprPtr = *exprString;
			const TCHAR* travelPtr = exprPtr;

			// precalc the lengths of the replacement strings
			const TCHAR* varNamePtr = *varName;
			int32 varNameLen = FCString::Strlen(varNamePtr);

			FString varValueStr;
			const TCHAR* varValueStrPtr = nullptr;
			int32 varValueStrLen = 0;

			FString resultString;
			while (true)
			{
				// look for From in the remaining string
				const TCHAR* fromLocation = FCString::Strstr(travelPtr, varNamePtr);
				if (!fromLocation)
					break;

				// populate the replacement string once it's first found
				if (varValueStrPtr == nullptr)
				{
					varValueStr = FString::Printf(TEXT("%f"), varValue);
					varValueStrPtr = *varValueStr;
					varValueStrLen = varValueStr.Len();
				}

				// copy everything up to FromLocation
				resultString.AppendChars(travelPtr, fromLocation - travelPtr);

				// if the variable occurs immediately after a number or group, then interpret it as multiplication.
				bool bInsertMultiply = false;
				if (fromLocation != exprPtr)
				{
					TCHAR prevChar = *(fromLocation - 1);

					if (prevChar == TEXT(')'))
					{
						// detected the end of a group
						bInsertMultiply = true;
					}
					else if (FChar::IsDigit(prevChar))
					{
						// search for a complete number (and not another variable ending in a number)
						bInsertMultiply = true;
						bool bFoundDecimal = false;
						for (const TCHAR* numSearchPtr = fromLocation - 2; numSearchPtr >= exprPtr; --numSearchPtr)
						{
							prevChar = *numSearchPtr;
							if (prevChar == TEXT('.'))
							{
								if (bFoundDecimal)
								{
									bInsertMultiply = false;
									break;
								}
								else
								{
									bFoundDecimal = true;
								}
							}
							else if (FChar::IsAlpha(prevChar))
							{
								// digits are at the end of a symbol; not a number
								bInsertMultiply = false;
								break;
							}
							else if (FChar::IsPunct(prevChar))
							{
								// end of number
								break;
							}
						}
					}
				}

				if (bInsertMultiply)
				{
					resultString.AppendChar(TEXT('*'));
				}

				// copy over the To
				resultString.AppendChars(varValueStrPtr, varValueStrLen);

				travelPtr = fromLocation + varNameLen;
			}

			// copy anything left over
			resultString += travelPtr;

			exprString = resultString;
			return true;
		}

		bool Evaluate(const TMap<FString, float> &vars, const FString &expr, float &result)
		{
			// strip spaces
			FString fexpr = expr.Replace(TEXT(" "), TEXT(""));

			// replace all vars with literal values - sort keys to guarantee that more specific names get bound first (ie "Hank1" vs "Hank")
			TArray<FString> keys;
			vars.GetKeys(keys);
			keys.Sort([](const FString &rhs, const FString &lhs) {return rhs.Len() > lhs.Len(); });
			for (auto &key : keys)
			{
				const float *v = vars.Find(key);
				if (v != nullptr)
				{
					ReplaceVariable(fexpr, key, *v);
				}
			}

			return Evaluate(fexpr, result);
		}

		bool Evaluate(const TMap<FName, Units::FUnitValue> &vars, const FString &expr, Units::FUnitValue &result)
		{
			// First, determine the common units of the variables in the expression, so they can be converted to the same type
			Units::EUnitType commonUnit = Units::EUnitType::WorldCentimeters;
			int32 maxUnitUsage = 0;
			int32 unitUsage[Units::EUnitType::NUM];
			FMemory::Memset(unitUsage, 0);

			for (auto &kvp : vars)
			{
				Units::EUnitType unitType = kvp.Value.GetUnitType();
				int32 unitIdx = (int32)unitType;
				if (++unitUsage[unitIdx] > maxUnitUsage)
				{
					maxUnitUsage = unitUsage[unitIdx];
					commonUnit = unitType;
				}
			}

			// If we haven't determined a common unit from input variables, abort.
			if ((maxUnitUsage == 0) && (vars.Num() > 0))
			{
				return false;
			}

			// strip spaces
			FString fexpr = expr.Replace(TEXT(" "), TEXT(""));

			// replace all vars with literal values - sort keys to guarantee that more specific names get bound first (ie "Hank1" vs "Hank")
			TArray<FName> keys;
			vars.GetKeys(keys);
			keys.Sort([](const FName &rhs, const FName &lhs) {
				return rhs.GetDisplayNameEntry()->GetNameLength() > lhs.GetDisplayNameEntry()->GetNameLength();
			});
			FString tempKeyString;
			for (auto &key : keys)
			{
				if (const Units::FUnitValue *unitValue = vars.Find(key))
				{
					key.ToString(tempKeyString);
					float convertedValue = unitValue->AsUnit(commonUnit);
					ReplaceVariable(fexpr, tempKeyString, convertedValue);
				}
			}

			float resultValue;
			if (Evaluate(fexpr, resultValue))
			{
				result = Units::FUnitValue(resultValue, commonUnit);
				return true;
			}

			return false;
		}

		bool Evaluate(const FString &fexpr, float &result)
		{
#if USE_UE4_EVALUATOR
			// Note: it seems like we should be able to use the simpler FMath::Eval() here,
			// but it isn't used anywhere else in the engine, and doesn't handle grouping correctly, so it must not be maintained.
			// (Specifically: (2 * 4.5) / (1 + 2) evaulates to 9, not 3)
			// So we're using FBasicMathExpressionEvaluator, which is currently used by UE4 in various widgets.

			FBasicMathExpressionEvaluator evaluator;
			auto resultOption = evaluator.Evaluate(*fexpr);
			if (resultOption.IsValid())
			{
				result = resultOption.GetValue();
				return true;
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("Evaluation error on expression \"%s\": %s"),
					*fexpr, *resultOption.GetError().Text.ToString());
				return false;
			}
#else
			std::wstring rexpr = *fexpr;
			std::wstring numPattern = L"\\-?[0-9]+\\.?[0-9]*";

			auto reduceParens = [numPattern](const std::wstring &pat, std::wstring &outPat)
			{
				std::wsmatch m;
				std::wregex parenEx(L"\\(" + numPattern + L"\\)");
				std::wstring newPat = pat;
				bool ret = false;
				while (std::regex_search(newPat, m, parenEx))
				{
					for (auto x : m)
					{
						std::wstring parend = x.str();
						if (parend.length() > 2)
						{
							ret = true;
							parend = parend.substr(1, parend.length() - 2);
						}
						else
						{
							parend = L"";
						}
						newPat = m.prefix().str() + parend + m.suffix().str();
					}
				}
				outPat = newPat;
				return ret;
			};

			auto reduceBinaryOp = [numPattern](const std::wstring &pat, const std::wregex &ex, std::wstring &outPat)
			{
				std::wsmatch m;
				std::wstring newPat = pat;
				bool ret = false;
				while (std::regex_search(newPat, m, ex))
				{
					for (auto x : m)
					{
						std::wstring binOp = x.str();
						size_t v = binOp.find_first_of(L"*-+/");
						float lhs, rhs;
						if (v != std::wstring::npos)
						{
							ret = true;
							lhs = FCString::Atof(binOp.substr(0, v).c_str());
							rhs = FCString::Atof(binOp.substr(v + 1, binOp.length() - v).c_str());
							if (binOp.find(L'*') != std::wstring::npos)
							{
								lhs = lhs * rhs;
							}
							else if (binOp.find(L'/') != std::wstring::npos)
							{
								lhs = rhs != 0.0f ? lhs / rhs : 0.0f;
							}
							else if (binOp.find(L'+') != std::wstring::npos)
							{
								lhs = lhs + rhs;
							}
							else if (binOp.find(L'-') != std::wstring::npos)
							{
								lhs = lhs - rhs;
							}
							newPat = m.prefix().str() + *FString::Printf(TEXT("%f"), lhs) + m.suffix().str();
						}
					}
				}
				outPat = newPat;
				return ret;

			};

			std::wregex muldiv(numPattern+L"[*/]"+numPattern);
			std::wregex addsub(numPattern + L"[+-]" + numPattern);

			// order of eval:
			// 1) eliminate all parens around bare numbers
			// 2) perform all binary multiplications and divisions
			// 3) perform all binary additions and subtractions
			// 4) repeat until no action can be performed
			bool didSomething = true;
			while (didSomething)
			{
				didSomething = reduceParens(rexpr, rexpr);
				didSomething = reduceBinaryOp(rexpr, muldiv, rexpr) || didSomething;
				didSomething = reduceBinaryOp(rexpr, addsub, rexpr) || didSomething;
			}

			result = FCString::Atof(rexpr.c_str());
			return true;
#endif
		}

		float Evaluate(const TMap<FString, float> &vars, const FString &expr)
		{
			float ret = 0.0f;
			Evaluate(vars, expr, ret);
			return ret;
		}
	}
}

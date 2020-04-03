// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ModumateScriptProcessor.h"
#include "AutomationTest.h"
#include "Misc/FileHelper.h"

FModumateScriptProcessor::~FModumateScriptProcessor()
{
	for (auto &rule : Rules)
	{
		delete rule.Action;
	}
	Rules.Empty();
}

/*
A rule binds a regular expression to a lambda callback
*/
bool FModumateScriptProcessor::AddRule(const FString &rule, TRuleHandler onRule, bool allowSuffix)
{
	FRuleMapping mapping;
	mapping.RuleString = rule;
	mapping.RulePattern = std::wregex(*rule);
	// Lambdas don't pass well by value, so make a dynamic copy
	mapping.Action = new TRuleHandler(onRule);
	mapping.AllowSuffix = allowSuffix;
	Rules.Add(mapping);
	return true;
}

/*
If a given rulemapping matches the input line, call the action
*/
bool FModumateScriptProcessor::TryRule(const FRuleMapping *ruleMapping, const FString &line, int32 lineNum, TErrorReporter errorReporter) const
{
	std::wsmatch m;
	std::wstring wsLine(*line);
	if (ruleMapping != nullptr)
	{
		if (ruleMapping->AllowSuffix ? std::regex_search(wsLine, m, ruleMapping->RulePattern) : std::regex_match(wsLine, m, ruleMapping->RulePattern))
		{
			(*ruleMapping->Action)(*line, m, lineNum, errorReporter);
			return true;
		}
	}
	return false;
}

/*
For a given line, try all the rules...if none apply, this was a syntax error
*/
bool FModumateScriptProcessor::TryRules(const FString &line, int32 lineNum, TErrorReporter errorReporter) const
{
	for (const auto &rule : Rules)
	{
		if (TryRule(&rule, line, lineNum, errorReporter))
		{
			return true;
		}
	}
	errorReporter(FString::Printf(TEXT("Syntax error at line %d"), lineNum));
	return false;
}

bool FModumateScriptProcessor::ParseLines(const TArray<FString> &lines, TErrorReporter errorReporter)
{
	bool ret = true;
	for (int32 i = 0; i < lines.Num(); ++i)
	{
		// TODO: allow clients to define their own comment pattern
		FString trimLine = lines[i].TrimStartAndEnd();

		if (trimLine.IsEmpty() || trimLine.Left(2) == TEXT("--"))
		{
			continue;
		}

		ret = TryRules(*trimLine, i + 1, errorReporter) && ret;
	}
	return ret;
}

bool FModumateScriptProcessor::ParseFile(const FString &filepath, TErrorReporter errorReporter)
{
	TArray<FString> lines;
	if (!FFileHelper::LoadFileToStringArray(lines, *filepath))
	{
		errorReporter(FString::Printf(TEXT("Could not find file %s in FModumateScriptProcessor::Parse"), *filepath));
		return false;
	}
	return ParseLines(lines, errorReporter);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateScriptProcessorUnitTest, "Modumate.Core.ScriptProcessor.UnitTest", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
bool FModumateScriptProcessorUnitTest::RunTest(const FString &Parameters)
{
	bool ret = true;

	// List and character match
	{
		TArray<FString> errors;
		FModumateScriptProcessor compiler;
		const FString kAllCapsWord = TEXT("[A-Z0-9]+");
		const FString kAllCapsWordList = TEXT("(") + kAllCapsWord + TEXT(")") + TEXT("(\\s+") + kAllCapsWord + TEXT(")*");

		compiler.AddRule(kAllCapsWordList, [](
			const TCHAR *originalLine,
			const FModumateScriptProcessor::TRegularExpressionMatch &match,
			int32 lineNum,
			FModumateScriptProcessor::TErrorReporter errorReporter)
			{

			},
			false);

		TArray<FString> lines;
		lines.Add(TEXT("WORD ONE TWO THREE"));
		lines.Add(TEXT("Not L^gals"));
		lines.Add(TEXT("AWORD"));

		compiler.ParseLines(lines, [&errors](const FString &err) {errors.Add(err); });

		ret = ret && (errors.Num() == 1);
	}

	//Group match
	{
		TArray<FString> errors;
		FModumateScriptProcessor compiler;
		const FString kManBearPig = TEXT("(MAN|BEAR|PIG)");

		compiler.AddRule(kManBearPig, [](
			const TCHAR *originalLine,
			const FModumateScriptProcessor::TRegularExpressionMatch &match,
			int32 lineNum,
			FModumateScriptProcessor::TErrorReporter errorReporter)
		{

		},
			false);

		TArray<FString> lines;
		lines.Add(TEXT("MANBEARPIG"));
		lines.Add(TEXT("MAN"));
		lines.Add(TEXT("BEAR"));
		lines.Add(TEXT("MANBEAR"));
		lines.Add(TEXT("BEARPIG"));

		compiler.ParseLines(lines, [&errors](const FString &err) {errors.Add(err); });

		ret = ret && (errors.Num() == 3);
	}

	// Suffix match
	{
		TArray<FString> errors;
		TArray<FString> suffices;
		FModumateScriptProcessor compiler;
		const FString kWord = TEXT("\\w+\\s*");

		compiler.AddRule(kWord, [&suffices](
			const TCHAR *originalLine,
			const FModumateScriptProcessor::TRegularExpressionMatch &match,
			int32 lineNum,
			FModumateScriptProcessor::TErrorReporter errorReporter)
			{
				if (match.suffix().str().length() > 0)
				{
					suffices.Add(match.suffix().str().c_str());
				}
			},
			true);

		TArray<FString> lines;
		lines.Add(TEXT("SINGLE"));
		lines.Add(TEXT("SINGLE WITH WORD SUFFICES"));
		lines.Add(TEXT("SINGLE W&^th *rt&r 9ffices"));

		compiler.ParseLines(lines, [&errors](const FString &err) {errors.Add(err); });

		ret = ret && (errors.Num() == 0);
		ret = ret && (suffices.Num() == 2);
		if (suffices.Num() == 2)
		{
			ret = ret && suffices[0] == TEXT("WITH WORD SUFFICES");
			ret = ret && suffices[1] == TEXT("W&^th *rt&r 9ffices");
		}
	}

	return ret;
}
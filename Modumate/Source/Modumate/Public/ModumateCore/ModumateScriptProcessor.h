// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include <functional>
#include <regex>

/*
A simple script processor that reads files one line at a time and attempts to apply rules
*/

class FModumateScriptProcessor
{
public:

	typedef std::wregex TRegularExpression;
	typedef std::wsmatch TRegularExpressionMatch;

	typedef std::function<void(const FString &err)> TErrorReporter;
	typedef std::function<void(const TCHAR *originalLine, const TRegularExpressionMatch &match, int32 lineNum, TErrorReporter errorReporter)> TRuleHandler;

	//Some rules allow suffixes (additional text after the match) and some require exact matches
	bool AddRule(const FString &rule, TRuleHandler onRule, bool allowSuffix = true);
	bool ParseFile(const FString &filepath, TErrorReporter errorReporter);
	bool ParseLines(const TArray<FString> &lines, TErrorReporter errorReporter);

	~FModumateScriptProcessor();
	FModumateScriptProcessor() {};

private:

	// Script processor manages dynamic memory, so forbid assignment and copying
	FModumateScriptProcessor(const FModumateScriptProcessor &c) {};
	const FModumateScriptProcessor &operator=(const FModumateScriptProcessor &c) { return *this; }

	struct FRuleMapping
	{
		bool AllowSuffix;
		FString RuleString;
		TRegularExpression RulePattern;
		TRuleHandler *Action;
	};

	// Rules stored in array so they will be tried in their order they are declared
	TArray<FRuleMapping> Rules;

	bool TryRule(const FRuleMapping *ruleMapping, const FString &line, int32 lineNum, TErrorReporter errorReporter) const;
	bool TryRules(const FString &rule, int32 lineNum, TErrorReporter errorReporter) const;
};
#pragma once
#include <JuceHeader.h>
#include "Arguments.h"
#include "Input.h"
#include "Stdio.h"

struct Count
{
    static Arguments getArguments (juce::Span<const std::string_view> argumentList);

    static Stdio::Output run (const Arguments& arguments, const Input::Source& input);
};

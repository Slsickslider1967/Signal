#include "MDU/mduParser.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
    // Trim leading/trailing whitespace from a string.
    std::string Trim(const std::string &Str)
    {
        std::size_t start = 0;
        while (start < Str.size() && std::isspace(static_cast<unsigned char>(Str[start])))
        {
            start++;
        }

        std::size_t end = Str.size();
        while (end > start && std::isspace(static_cast<unsigned char>(Str[end - 1])))
        {
            --end;
        }

        return Str.substr(start, end - start);
    }

    // Return true when Text begins with Prefix.
    bool StartsWith(const std::string &Text, const std::string &Prefix)
    {
        return Text.size() >= Prefix.size() && Text.compare(0, Prefix.size(), Prefix) == 0;
    }

    // Strip wrapping single/double quotes from a value when present.
    std::string StripQuotes(const std::string &Str)
    {
        std::string Output = Trim(Str);
        if (Output.size() >= 2)
        {
            char a = Output.front();
            char b = Output.back();
            if ((a == '"' && b == '"') || (a == '\'' && b == '\''))
            {
                return Output.substr(1, Output.size() - 2);
            }
        }
        return Output;
    }

    // Split comma-separated text while respecting quoted segments.
    std::vector<std::string> SplitCommaRespectQuotes(const std::string& Line)
    {
        std::vector<std::string> parts;
        std::string current;
        bool inSingle = false;
        bool inDouble = false;

        for (char Character : Line)
        {
            if (Character == '\'' && !inDouble)
            {
                inSingle = !inSingle;
                current.push_back(Character);
                continue;
            }
            if (Character == '"' && !inSingle)
            {
                inDouble = !inDouble;
                current.push_back(Character);
                continue;
            }

            if (Character == ',' && !inSingle && !inDouble)
            {
                parts.push_back(Trim(current));
                current.clear();
                continue;
            }

            current.push_back(Character);
        }

        if (!current.empty())
        {
            parts.push_back(Trim(current));
        }

        return parts;
    }

    // Parse inline key:value objects used in pin/parameter lines.
    std::unordered_map<std::string, std::string> ParseInlineObject(const std::string& Line)
    {
        std::unordered_map<std::string, std::string> Output;
        const auto parts = SplitCommaRespectQuotes(Line);

        for (const auto& Part : parts)
        {
            const auto Position = Part.find(':');
            if (Position == std::string::npos)
            {
                continue;
            }

            std::string key = Trim(Part.substr(0, Position));
            std::string value = Trim(Part.substr(Position + 1));
            Output[key] = StripQuotes(value);
        }

        return Output;
    }

    // Parse a float and ensure the entire string was consumed.
    bool ToFloat(const std::string& Str, float& Out)
    {
        try
        {
            size_t Index = 0;
            Out = std::stof(Str, &Index);
            return Index == Str.size();
        }
        catch (...)
        {
            return false;
        }
    }
}

namespace MDU
{
    // Map a type string to ParameterType, case-insensitively.
    ParameterType ParameterTypeFromString(const std::string& text)
    {
        std::string Str = text;
        std::transform(Str.begin(), Str.end(), Str.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });

        if (Str == "knob") return ParameterType::Knob;
        if (Str == "slider") return ParameterType::Slider;
        if (Str == "stepped") return ParameterType::Stepped;
        if (Str == "toggle") return ParameterType::Toggle;
        if (Str == "combo") return ParameterType::Combo;

        return ParameterType::Knob;
    }

    // Convert ParameterType enum back to a display string.
    const char* ParameterTypeToString(ParameterType type)
    {
        switch (type)
        {
            case ParameterType::Knob: return "Knob";
            case ParameterType::Slider: return "Slider";
            case ParameterType::Stepped: return "Stepped";
            case ParameterType::Toggle: return "Toggle";
            case ParameterType::Combo: return "Combo";
            default: return "Unknown";
        }
    }

    // Read an .mdu file from disk and parse its metadata header.
    ParseResult ParseMDUFile(const std::string& filePath)
    {
        std::ifstream file(filePath);
        if (!file.is_open())
        {
            return ParseResult{false, {}, "[MDP000] Failed to open .mdu file: " + filePath};
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        return ParseMDUText(buffer.str());
    }

    // Parse MDU header text and return structured metadata plus validation errors.
    ParseResult ParseMDUText(const std::string& SourceText)
    {
        const std::string BeginTag = "/*! Module";
        const std::string BeginTagUpper = "/*! MODULE";
        const std::string EndTag = "*/";

        auto BeginPosition = SourceText.find(BeginTag);
        if (BeginPosition == std::string::npos)
        {
            BeginPosition = SourceText.find(BeginTagUpper);
        }

        if (BeginPosition == std::string::npos)        
            return ParseResult{false, {}, "[MDP001] MDU header not found. Expected '/*! Module' or '/*! MODULE'."};
        
        const auto EndPosition = SourceText.find(EndTag, BeginPosition + BeginTag.size());
        if (EndPosition == std::string::npos)
            return ParseResult{false, {}, "[MDP002] MDU header end not found. Expected '*/'."};

        std::string HeaderContent = SourceText.substr(BeginPosition + BeginTag.size(), EndPosition - (BeginPosition + BeginTag.size()));
        std::stringstream stream(HeaderContent);

        enum class Section
        {
            TopLevel,
            InputPins,
            OutputPins,
            Parameters, 
            Dependancies
        };

        MetaData metadata;
        Section CurrentSection = Section::TopLevel;
        std::string Line; 
        int LineNumber = 0;

        while (std::getline(stream, Line))
        {
            ++LineNumber;
            std::string Text = Trim(Line);

            if (Text.empty())
            {
                continue;
            }

            if (Text == "Input Pins:" || Text == "Inputs")
            {
                CurrentSection = Section::InputPins;
                continue;
            }
            if (Text == "Output Pins:" || Text == "Outputs")
            {
                CurrentSection = Section::OutputPins; 
                continue;  
            }
            if (Text == "Parameters:" || Text == "Params")
            {
                CurrentSection = Section::Parameters;
                continue;
            }
            if (Text == "Dependencies:" || Text == "dependencies")
            {
                CurrentSection = Section::Dependancies;
                continue;
            }

            if (StartsWith(Text, "-"))
            {
                std::string TextContent = Trim(Text.substr(1));

                if (CurrentSection == Section::InputPins || CurrentSection == Section::OutputPins)
                {
                    auto pinFields = ParseInlineObject(TextContent);

                    PinDefinition Pin; 
                    auto PinID = pinFields.find("id");
                    auto PinLabel = pinFields.find("label");

                    // Check for a PinID in the line.
                    if (PinID == pinFields.end())
                    {
                        return {false, {}, "[MDP003] Pin is missing an id at header line " + std::to_string(LineNumber)};
                    }

                    Pin.ID = PinID -> second;
                    Pin.label = (PinLabel != pinFields.end()) ? PinLabel->second : Pin.ID;

                        // Route parsed pins into input or output collections.
                    if (CurrentSection == Section::InputPins)
                        metadata.InputPins.push_back(Pin);
                    else 
                        metadata.OutputPins.push_back(Pin);

                    continue;
                }
            }

            if (CurrentSection == Section::Parameters)
            {
                std::string paramText = StartsWith(Text, "-") ? Trim(Text.substr(1)) : Text;
                auto ParsedInLineObject = ParseInlineObject(paramText);

                if (ParsedInLineObject.empty())
                {
                    continue;
                }

                ParameterDefinition Parameter;
                auto ParamID = ParsedInLineObject.find("id");
                auto ParamLabel = ParsedInLineObject.find("label");
                auto ParamType = ParsedInLineObject.find("type");
                auto ParamMin = ParsedInLineObject.find("min");
                auto ParamMax = ParsedInLineObject.find("max");
                auto ParamDefault = ParsedInLineObject.find("default");

                // Error for missing ID
                if (ParamID == ParsedInLineObject.end())
                {
                    return {false, {}, "[MDP004] Parameter is missing an id at header line " + std::to_string(LineNumber)};
                }


                Parameter.ID = ParamID->second;
                Parameter.label = (ParamLabel != ParsedInLineObject.end()) ? ParamLabel->second : Parameter.ID;
                Parameter.type = (ParamType != ParsedInLineObject.end()) ? ParameterTypeFromString(ParamType->second) : ParameterType::Knob;

                // Errors for invalid min, max, default values (if they exist)
                if (ParamMin != ParsedInLineObject.end() && !ToFloat(ParamMin->second, Parameter.minValue))
                {
                    return {false, {}, "[MDP005] Invalid min value for parameter '" + Parameter.ID + "' at line " + std::to_string(LineNumber)};
                }

                if (ParamMax != ParsedInLineObject.end() && !ToFloat(ParamMax->second, Parameter.maxValue))
                {
                    return {false, {}, "[MDP006] Invalid max value for parameter '" + Parameter.ID + "' at line " + std::to_string(LineNumber)};
                }

                if (ParamDefault != ParsedInLineObject.end() && !ToFloat(ParamDefault->second, Parameter.defaultValue))
                {
                    return {false, {}, "[MDP007] Invalid default value for parameter '" + Parameter.ID + "' at line " + std::to_string(LineNumber)};
                }

                // Parse option lists for combo/stepped style parameters.
                auto OptionsIt = ParsedInLineObject.find("options");
                if (OptionsIt != ParsedInLineObject.end())
                {
                    std::string OptionsStr = OptionsIt->second;
                    if (!OptionsStr.empty() && OptionsStr.front() == '[' && OptionsStr.back() == ']')
                    {
                        OptionsStr = OptionsStr.substr(1, OptionsStr.size() - 2);

                        auto OptionParts = SplitCommaRespectQuotes(OptionsStr);
                        for (const auto& Option : OptionParts)
                        {
                            std::string Clean = StripQuotes(Option);
                            if (!Clean.empty())
                            {
                                Parameter.options.push_back(Clean);
                            }
                        }
                    }
                }

                metadata.Parameters.push_back(Parameter);
                continue;
            }
            else if (CurrentSection == Section::Dependancies)
            {
                if (!Text.empty())
                    metadata.Dependancies.push_back(StripQuotes(Text));

                continue;
            }
            else
            {
                // Parse top-level metadata fields like ModuleName/Author.
                auto Position = Text.find(':');
                if (Position != std::string::npos)
                {
                    std::string Key = Trim(Text.substr(0, Position));
                    std::string Value = StripQuotes(Trim(Text.substr(Position + 1)));

                    if (Key == "ModuleName")
                    {
                        metadata.ModuleName = Value;
                    }
                    else if (Key == "ModuleType")
                    {
                        metadata.ModuleType = Value;
                    }
                    else if (Key == "ModuleVersion")
                    {
                        metadata.ModuleVersion = Value;
                    }
                    else if (Key == "Author")
                    {
                        metadata.Author = Value;
                    }
                }
            }
        }

        if (metadata.ModuleName.empty())
        {
            return {false, {}, "[MDP008] ModuleName is required but missing from header."};
        }
        if (metadata.InputPins.empty() && metadata.OutputPins.empty())
        {
            return {false, {}, "[MDP009] At least one input or output pin is required."};
        }
        if (metadata.Parameters.empty())
        {
            return {false, {}, "[MDP010] At least one parameter is required."};
        }
        if (metadata.ModuleType.empty())
        {
            return {false, {}, "[MDP011] ModuleType is required but missing from header."};
        }

        return {true, metadata, ""};
    }
}
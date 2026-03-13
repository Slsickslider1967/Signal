#include "../../include/Tools/mduParser.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{

    // For whitespace to the left and right for a clean string
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

    bool StartsWith(const std::string &Text, const std::string &Prefix)
    {
        return Text.size() >= Prefix.size() && Text.compare(0, Prefix.size(), Prefix) == 0;
    }

    // To remove quotes and other unnecessary characters from a string value, obvs
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

    //Splits comma separated values while respecting quoted sections
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

    // Gives Key-Value pairs for inline objects like "id: value, label: value"
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
    // Case sensitive mapping to ParameterType
    ParameterType ParamTypeFromString(const std::string& text)
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

    // Reverse mapping
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

    // Read the MDU file and extract the header content, then pass it to the text parser
    ParseResult ParseMDUFile(const std::string& filePath)
    {
        std::ifstream file(filePath);
        if (!file.is_open())
        {
            return ParseResult{false, {}, "Failed to open .mdu file:"};
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        return ParseMDUText(buffer.str());
    }

    // Main parsing function that takes the content of the MDU file as a string and extracts the metadata
    ParseResult ParseMDUText(const std::string& SourceText)
    {
        const std::string BeginTag = "/*! Module";
        const std::string EndTag = "*/";

        const auto BeginPosition = SourceText.find(BeginTag);
        if (BeginPosition == std::string::npos)        
            return ParseResult{false, {}, "MDU header not found: ERROR 001"};
        
        const auto EndPosition = SourceText.find(EndTag, BeginPosition + BeginTag.size());
        if (EndPosition == std::string::npos)
            return ParseResult{false, {}, "MDU header end not found: ERROR 002"};

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
                    auto RENAME = ParseInlineObject(TextContent);

                    PinDefinition Pin; 
                    auto PinID = RENAME.find("id");
                    auto PinLabel = RENAME.find("label");

                    // Check for a PinID in the line.
                    if (PinID == RENAME.end())
                    {
                        return {false, {}, "Pin is missing an ID in the header line " + std::to_string(LineNumber) + ": ERROR 003 "};
                    }

                    Pin.ID = PinID -> second;
                    Pin.label = (PinLabel != RENAME.end()) ? PinLabel->second : Pin.ID;

                    // Add the pin to the appropriate section.
                    if (CurrentSection == Section::InputPins)
                        metadata.InputPins.push_back(Pin);
                    else 
                        metadata.OutputPins.push_back(Pin);

                    continue;
                }
            }

            if (CurrentSection == Section::Parameters)
            {
                auto ParsedInLineObject = ParseInlineObject(Text);

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
                    return {false, {}, "Parameter is missing an ID in the header line " + std::to_string(LineNumber) + ": ERROR 004"};
                }


                Parameter.ID = ParamID->second;
                Parameter.label = (ParamLabel != ParsedInLineObject.end()) ? ParamLabel->second : Parameter.ID;
                Parameter.type = (ParamType != ParsedInLineObject.end()) ? ParamTypeFromString(ParamType->second) : ParameterType::Knob;

                // Errors for invalid min, max, default values (if they exist)
                if (ParamMin != ParsedInLineObject.end() && !ToFloat(ParamMin->second, Parameter.minValue))
                {
                    return {false, {}, "Invalid min value for parameter '" + Parameter.ID + "' in line " + std::to_string(LineNumber) + ": ERROR 005"};
                }

                if (ParamMax != ParsedInLineObject.end() && !ToFloat(ParamMax->second, Parameter.maxValue))
                {
                    return {false, {}, "Invalid max value for parameter '" + Parameter.ID + "' in line " + std::to_string(LineNumber) + ": ERROR 006"};
                }

                if (ParamDefault != ParsedInLineObject.end() && !ToFloat(ParamDefault->second, Parameter.defaultValue))
                {
                    return {false, {}, "Invalid default value for parameter '" + Parameter.ID + "' in line " + std::to_string(LineNumber) + ": ERROR 007"};
                }

                // Handle options for combo and stepped types
                auto OptionsIt = ParsedInLineObject.find("options");
                if (OptionsIt != ParsedInLineObject.end())
                {
                    std::string OptionsStr = OptionsIt->second;
                    if (!OptionsStr.empty() && OptionsStr.front() == '[' && OptionsStr.back() == ']')
                    {
                        OptionsStr = OptionsStr.substr(1, OptionsStr.size() - 2);
                        Parameter.options = SplitCommaRespectQuotes(OptionsStr);

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
                    metadata.Dependancies.push_back(Text);

                metadata.Dependancies.push_back(Text);
                continue;
            }
            else
            {
                // Top level key-value pairs
                auto Position = Text.find(':');
                if (Position != std::string::npos)
                {
                    std::string Key = Trim(Text.substr(0, Position));
                    std::string Value = Trim(Text.substr(Position + 1));

                    if (Key == "ModuleName")
                    {
                        metadata.ModuleName = Value;
                    }
                }
            }
        }

        if (metadata.ModuleName.empty())
        {
            return {false, {}, "ModuleName is required but not found in the header: ERROR 008"};
        }
        if (metadata.InputPins.empty() && metadata.OutputPins.empty())
        {
            return {false, {}, "At least one input or output pin is required: ERROR 009"};
        }
        if (metadata.Parameters.empty())
        {
            return {false, {}, "At least one parameter is required: ERROR 010"};
        }
        if (metadata.ModuleType.empty())
        {
            return {false, {}, "ModuleType is required but not found in the header: ERROR 011"};
        }

        return {true, metadata, ""};
    }
}
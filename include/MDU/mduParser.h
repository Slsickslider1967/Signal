#pragma once 

#include <string>

#include "MDU/mdu_api.h"

namespace MDU
{
    // For valid MDU files and parsing results.

    struct ParseResult
    {
        bool ValidMDUFile = false;
        MetaData metadata;
        std::string Error;
    };

    // Opens the MDU and parsess the content 
    ParseResult ParseMDUFile(const std::string& filePath);
    // Checks the .MDU for the header and valid syntax then returns the result
    ParseResult ParseMDUText(const std::string& mduText);

    // Tools for parsing and reading
    ParameterType ParameterTypeFromString(const std::string& Text);
    const char* ParameterTypeToString(ParameterType type);
}
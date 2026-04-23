#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>

#include "MDU/CreateMDU.h"
#include "Functions/ConsoleHandling.h"

namespace MDU
{
    // Create and write a ready-to-edit template .mdu file at the target location.
    void CreateTemplateMDU(const std::filesystem::path &targetPath)
    {
        std::string templateContent =
        R"MDU(
        /*! Module
        ModuleName: "Template"
        ModuleType: "Template"
        ModuleVersion: "1.0.0"
        Author: "YOU"

        Input Pins:
        - id: rate_cv, label: "Audio In"

        Output Pins:
        - id: cv_out, label: "Audio Out"
        */

        #include "mdu_api.h"

        class TemplateModule : public MDU::Module
        {
            private:
                float TemplateID = 1.0f;

            public:
                static MDU::MetaData BuildMetadata()
                {
                    MDU::MetaData metadata;
                    metadata.ModuleName = "Template";
                    metadata.ModuleType = "Template";
                    metadata.ModuleVersion = "1.0.0";
                    metadata.Author = "YOU";

                    metadata.InputPins.push_back({"rate_cv", "Audio In"});
                    metadata.OutputPins.push_back({"cv_out", "Audio Out"});

                    return metadata;
                }

                void Process(const MDU::BufferView &bufferView, float /*value*/) override
                {
                    if (bufferView.OutputPins.empty() || bufferView.OutputPins[0] == nullptr)
                    {
                        return;
                    }

                    const float *input = (bufferView.InputPins.empty() ? nullptr : bufferView.InputPins[0]);
                    float *output = bufferView.OutputPins[0];
                    for (size_t i = 0; i < bufferView.NumberOfSamples; ++i)
                    {
                        output[i] = (input != nullptr) ? input[i] : 0.0f;
                    }
                }

                void DrawEditor() override {}
        };


        MDU_REGISTER(TemplateModule)
        )MDU";

        std::filesystem::path outputPath;
        
        if (targetPath.empty()) {
            outputPath = std::filesystem::current_path() / "TemplateModule.mdu";
        } else {
            outputPath = targetPath;
        }
        std::ofstream outFile(outputPath);
        if (outFile)      
        {
            outFile << templateContent;
            outFile.close();
            Console::AppendConsoleLine("Template MDU created at: " + outputPath.string());
        }        
        else
        {
            Console::AppendConsoleLine("Failed to create template MDU at: " + outputPath.string());
        }   
    }
}
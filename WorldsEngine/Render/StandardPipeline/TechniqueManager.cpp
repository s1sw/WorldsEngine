#include "StandardPipeline.hpp"
#include <Core/Log.hpp>
#include <R2/VKPipeline.hpp>

namespace worlds
{
    TechniqueManager::TechniqueManager() = default;

    TechniqueManager::~TechniqueManager()
    {
        for (Technique& t : techniques)
        {
            for (int i = 0; i < 4; i++)
            {
                logMsg("deleting variant %i", i);
                delete t.pipelineVariants[i];
                logMsg("deleted variant %i", i);
            }
        }
        techniques.clear();
    }

    uint16_t TechniqueManager::createTechnique()
    {
        auto id = (uint16_t)techniques.size();
        auto& t = techniques.emplace_back();
        t.id = id;
        for (auto& pipelineVariant : t.pipelineVariants) pipelineVariant = nullptr;
        return id;
    }

    void TechniqueManager::registerVariant(uint16_t id, R2::VK::Pipeline *pipeline, worlds::VariantFlags flags)
    {
        techniques[id].pipelineVariants[(int)flags] = pipeline;
    }

    R2::VK::Pipeline* TechniqueManager::getPipelineVariant(uint16_t techniqueId, worlds::VariantFlags flags)
    {
        return techniques[techniqueId].pipelineVariants[(int)flags];
    }
}
#include <vk_initializers.h>
#include <vk_engine.h>
#include <vk_pipelines.h>
#include <vk_materials.h>

PbrMaterial::PbrMaterial(VulkanEngine* engine): mEngine(engine)
{
}

void PbrMaterial::create_material()
{
    PipelineOptions options { mData.doubleSided, mData.alphaMode };
    mPipeline = mEngine->create_material_pipeline(options);
}

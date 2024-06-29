#include <vk_initializers.h>
#include <vk_engine.h>
#include <vk_pipelines.h>
#include <vk_materials.h>

PbrMaterial::PbrMaterial(VulkanEngine* engine): mEngine(engine)
{
}

void PbrMaterial::create_material()
{
    mData.doubleSided = true; // TODO either materials specified wrong, or implementation is wrong
    mPipeline = mEngine->create_pipeline(mData.doubleSided, mData.alphaMode);
}

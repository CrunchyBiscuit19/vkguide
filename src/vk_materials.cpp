#include <vk_initializers.h>
#include <vk_engine.h>
#include <vk_pipelines.h>
#include <vk_materials.h>

PbrMaterial::PbrMaterial(VulkanEngine* engine): engine(engine)
{
}

void PbrMaterial::create_material()
{
    pipeline = engine->create_pipeline(data.doubleSided, data.alphaMode);
}

#include <vk_meshes.h>
#include "vk_engine.h"

void Node::refreshTransform(const glm::mat4& parentMatrix)
{
    worldTransform = parentMatrix * localTransform;
    for (const auto& c : children)
        c->refreshTransform(worldTransform);
}

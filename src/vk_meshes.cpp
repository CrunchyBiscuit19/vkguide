#include <vk_meshes.h>
#include "vk_engine.h"

void Node::refreshTransform(const glm::mat4& parentMatrix)
{
    mWorldTransform = parentMatrix * mLocalTransform;
    for (const auto& c : mChildren)
        c->refreshTransform(mWorldTransform);
}

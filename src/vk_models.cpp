#pragma once

#include <vk_engine.h>
#include <vk_models.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <ranges>

GLTFModel::GLTFModel(VulkanEngine* engine, fastgltf::Asset& asset, std::filesystem::path modelPath)
    : mEngine(engine)
    , mName(modelPath.stem().string())
{
    // Load samplers
    for (fastgltf::Sampler& sampler : asset.samplers) {
        VkSamplerCreateInfo sampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr };
        sampl.maxLod = VK_LOD_CLAMP_NONE;
        sampl.minLod = 0;
        sampl.magFilter = extract_filter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
        sampl.minFilter = extract_filter(sampler.minFilter.value_or(fastgltf::Filter::Nearest));
        sampl.mipmapMode = extract_mipmap_mode(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

        VkSampler newSampler;
        vkCreateSampler(engine->mDevice, &sampl, nullptr, &newSampler);
        mSamplers.push_back(newSampler);
    }

    // Temporal arrays for all the objects to use while creating the GLTF data
    std::vector<uint32_t> modelIndices;
    std::vector<Vertex> modelVertices;

    // Load textures, with checkerboard as placeholder for loading errors
    int imageIndex = 0;
    mImages.resize(asset.images.size());

    for (fastgltf::Image& image : asset.images) {
        std::optional<AllocatedImage> loadedImage = load_image(engine, asset, image);
        if (loadedImage.has_value()) {
            mImages[imageIndex] = *loadedImage;
        } else {
            // Failed to load -> default checkerboard texture
            mImages[imageIndex] = engine->mStockImages["checkerboard"];
        }
        imageIndex++;
    }

    // Load materials
    int materialIndex = 0;
    mMaterials.resize(asset.materials.size());

    for (fastgltf::Material& mat : asset.materials) {
        std::shared_ptr<PbrMaterial> newMat = std::make_shared<PbrMaterial>(engine);

        auto matName = std::string(mat.name);
        if (matName.empty()) {
            matName = fmt::format("{}", materialIndex);
        }
        newMat->mName = fmt::format("{}_mat_{}", mName, matName);

        // Constants
        newMat->mData.constants.baseFactor.x = mat.pbrData.baseColorFactor[0];
        newMat->mData.constants.baseFactor.y = mat.pbrData.baseColorFactor[1];
        newMat->mData.constants.baseFactor.z = mat.pbrData.baseColorFactor[2];
        newMat->mData.constants.baseFactor.w = mat.pbrData.baseColorFactor[3];
        newMat->mData.constants.metallicFactor = mat.pbrData.metallicFactor;
        newMat->mData.constants.roughnessFactor = mat.pbrData.roughnessFactor;
        newMat->mData.constants.emissiveFactor.x = mat.emissiveFactor[0];
        newMat->mData.constants.emissiveFactor.y = mat.emissiveFactor[1];
        newMat->mData.constants.emissiveFactor.z = mat.emissiveFactor[2];
        newMat->mData.alphaMode = mat.alphaMode;
        newMat->mData.doubleSided = mat.doubleSided;

        // Default the material textures
        newMat->mData.resources.base.image = engine->mStockImages["white"];
        newMat->mData.resources.base.sampler = engine->mDefaultSamplerLinear;
        newMat->mData.resources.metallicRoughness.image = engine->mStockImages["white"];
        newMat->mData.resources.metallicRoughness.sampler = engine->mDefaultSamplerLinear;
        newMat->mData.resources.normal.image = engine->mStockImages["white"];
        newMat->mData.resources.normal.sampler = engine->mDefaultSamplerLinear;
        newMat->mData.resources.occlusion.image = engine->mStockImages["white"];
        newMat->mData.resources.occlusion.sampler = engine->mDefaultSamplerLinear;
        newMat->mData.resources.emissive.image = engine->mStockImages["white"];
        newMat->mData.resources.emissive.sampler = engine->mDefaultSamplerLinear;

        // Grab textures from gltf file
        if (mat.pbrData.baseColorTexture.has_value()) {
            size_t img = asset.textures[mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
            size_t sampler = asset.textures[mat.pbrData.baseColorTexture.value().textureIndex].samplerIndex.value();
            newMat->mData.resources.base.image = mImages[img];
            newMat->mData.resources.base.sampler = mSamplers[sampler];
        }
        if (mat.pbrData.metallicRoughnessTexture.has_value()) {
            size_t img = asset.textures[mat.pbrData.metallicRoughnessTexture.value().textureIndex].imageIndex.value();
            size_t sampler = asset.textures[mat.pbrData.metallicRoughnessTexture.value().textureIndex].samplerIndex.value();
            newMat->mData.resources.metallicRoughness.image = mImages[img];
            newMat->mData.resources.metallicRoughness.sampler = mSamplers[sampler];
        }
        if (mat.normalTexture.has_value()) {
            size_t img = asset.textures[mat.normalTexture.value().textureIndex].imageIndex.value();
            size_t sampler = asset.textures[mat.normalTexture.value().textureIndex].samplerIndex.value();
            newMat->mData.resources.normal.image = mImages[img];
            newMat->mData.resources.normal.sampler = mSamplers[sampler];
        }
        if (mat.occlusionTexture.has_value()) {
            size_t img = asset.textures[mat.occlusionTexture.value().textureIndex].imageIndex.value();
            size_t sampler = asset.textures[mat.occlusionTexture.value().textureIndex].samplerIndex.value();
            newMat->mData.resources.occlusion.image = mImages[img];
            newMat->mData.resources.occlusion.sampler = mSamplers[sampler];
        }
        if (mat.emissiveTexture.has_value()) {
            size_t img = asset.textures[mat.emissiveTexture.value().textureIndex].imageIndex.value();
            size_t sampler = asset.textures[mat.emissiveTexture.value().textureIndex].samplerIndex.value();
            newMat->mData.resources.emissive.image = mImages[img];
            newMat->mData.resources.emissive.sampler = mSamplers[sampler];
        }

        newMat->create_material();
        mMaterials[materialIndex] = newMat;
        materialIndex++;
    }

    // Load meshes
    int meshIndex = 0;
    mMeshes.resize(asset.meshes.size());

    for (fastgltf::Mesh& mesh : asset.meshes) {
        std::shared_ptr<MeshData> newMesh = std::make_shared<MeshData>();
        newMesh->mName = fmt::format("{}_mesh_{}", mName, mesh.name);

        // Load primitives (of each mesh)
        for (auto&& p : mesh.primitives) {
            Primitive newPrimitive;
            newPrimitive.indexCount = static_cast<uint32_t>(asset.accessors[p.indicesAccessor.value()].count);

            {
                // Add the indices of current primitive to the previous ones
                fastgltf::Accessor& indexaccessor = asset.accessors[p.indicesAccessor.value()];
                newPrimitive.indices.reserve(indexaccessor.count);
                fastgltf::iterateAccessor<std::uint32_t>(asset, indexaccessor,
                    [&](std::uint32_t idx) {
                        newPrimitive.indices.push_back(idx);
                    });
            }
            {
                // Add the vertices of current primitive to the previous ones
                fastgltf::Accessor& posAccessor = asset.accessors[p.findAttribute("POSITION")->second];
                newPrimitive.vertices.resize(posAccessor.count);
                newPrimitive.vertexCount = posAccessor.count;
                fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, posAccessor, // Default all the params
                    [&](glm::vec3 v, size_t index) {
                        Vertex newvtx;
                        newvtx.position = v;
                        newvtx.normal = { 1, 0, 0 };
                        newvtx.color = glm::vec4(1.f);
                        newvtx.uv_x = 0;
                        newvtx.uv_y = 0;
                        newPrimitive.vertices[index] = newvtx;
                    });
            }

            auto normals = p.findAttribute("NORMAL");
            if (normals != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, asset.accessors[normals->second],
                    [&](glm::vec3 v, size_t index) {
                        newPrimitive.vertices[index].normal = v;
                    });
            }

            auto uv = p.findAttribute("TEXCOORD_0");
            if (uv != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec2>(asset, asset.accessors[uv->second],
                    [&](glm::vec2 v, size_t index) {
                        newPrimitive.vertices[index].uv_x = v.x;
                        newPrimitive.vertices[index].uv_y = v.y;
                    });
            }

            auto colors = p.findAttribute("COLOR_0");
            if (colors != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec4>(asset, asset.accessors[colors->second],
                    [&](glm::vec4 v, size_t index) {
                        newPrimitive.vertices[index].color = v;
                    });
            }

            if (p.materialIndex.has_value())
                newPrimitive.material = mMaterials[p.materialIndex.value()];
            else
                newPrimitive.material = mMaterials[0];

            // Loop the vertices of this surface, find min/max bounds
            glm::vec3 minpos = newPrimitive.vertices[0].position;
            glm::vec3 maxpos = newPrimitive.vertices[0].position;
            for (const auto& vertex : newPrimitive.vertices) {
                minpos = glm::min(minpos, vertex.position);
                maxpos = glm::max(maxpos, vertex.position);
            }
            // Calculate origin and extents from the min/max, use extent length for radius
            newPrimitive.bounds.origin = (maxpos + minpos) / 2.f;
            newPrimitive.bounds.extents = (maxpos - minpos) / 2.f;
            newPrimitive.bounds.sphereRadius = glm::length(newPrimitive.bounds.extents);

            newMesh->mPrimitives.push_back(newPrimitive);
        }

        mMeshes[meshIndex] = newMesh;
        meshIndex++;
    }

    // Load vertices and indices if the meshes
    for (const auto& mesh : mMeshes) {
        for (auto& primitive : mesh->mPrimitives) {
            modelIndices.insert(modelIndices.end(), primitive.indices.begin(), primitive.indices.end());
            modelVertices.insert(modelVertices.end(), primitive.vertices.begin(), primitive.vertices.end());
            primitive.indices.clear();
            primitive.vertices.clear();
        }
    }

    // Load all nodes and their meshes
    int nodeIndex = 0;
    mNodes.resize(asset.nodes.size());

    for (fastgltf::Node& node : asset.nodes) {
        std::shared_ptr<Node> newNode;
        // Find if the node has a mesh, and if it does hook it to the mesh pointer and allocate it with the meshnode class
        if (node.meshIndex.has_value()) {
            newNode = std::make_shared<MeshNode>();
            dynamic_cast<MeshNode*>(newNode.get())->mMesh = mMeshes[*node.meshIndex];
        } else {
            newNode = std::make_shared<Node>();
        }

        newNode->mName = fmt::format("{}_node_{}", mName, node.name);

        // First function if it's a mat4 transform, second function if it's separate transform / rotate / scale quaternion or vec3
        std::visit(fastgltf::visitor { [&](const fastgltf::Node::TransformMatrix& matrix) {
                                          memcpy(&newNode->mLocalTransform, matrix.data(), sizeof(matrix));
                                      },
                       [&](const fastgltf::Node::TRS& transform) {
                           const glm::vec3 tl(transform.translation[0], transform.translation[1],
                               transform.translation[2]);
                           const glm::quat rot(transform.rotation[3], transform.rotation[0], transform.rotation[1],
                               transform.rotation[2]);
                           const glm::vec3 sc(transform.scale[0], transform.scale[1], transform.scale[2]);

                           const glm::mat4 tm = glm::translate(glm::mat4(1.f), tl);
                           const glm::mat4 rm = glm::toMat4(rot);
                           const glm::mat4 sm = glm::scale(glm::mat4(1.f), sc);

                           newNode->mLocalTransform = tm * rm * sm;
                       } },
            node.transform);

        mNodes[nodeIndex] = newNode;
        nodeIndex++;
    }

    // Loop GLTF asset nodes, then loop their child node indexes, then use those indexes to access and connect the temporal varirable nodes
    for (int i = 0; i < asset.nodes.size(); i++) {
        fastgltf::Node& gltfNode = asset.nodes[i];
        std::shared_ptr<Node>& sceneNode = mNodes[i];

        for (auto& childNodeIndex : gltfNode.children) {
            sceneNode->mChildren.push_back(mNodes[childNodeIndex]);
            mNodes[childNodeIndex]->mParent = sceneNode;
        }
    }

    // Find the top nodes, with no parents
    for (auto& node : mNodes) {
        if (node->mParent.lock() == nullptr) {
            mTopNodes.push_back(node);
            node->refreshTransform(glm::mat4 { 1.f });
        }
    }

    mModelBuffers = mEngine->upload_model(modelIndices, modelVertices);
}

GLTFModel::~GLTFModel()
{
    cleanup();
}

VkFilter GLTFModel::extract_filter(fastgltf::Filter filter)
{
    switch (filter) {
    case fastgltf::Filter::Nearest:
    case fastgltf::Filter::NearestMipMapNearest:
    case fastgltf::Filter::NearestMipMapLinear:
        return VK_FILTER_NEAREST;

    case fastgltf::Filter::Linear:
    case fastgltf::Filter::LinearMipMapNearest:
    case fastgltf::Filter::LinearMipMapLinear:
    default:
        return VK_FILTER_LINEAR;
    }
}

VkSamplerMipmapMode GLTFModel::extract_mipmap_mode(fastgltf::Filter filter)
{
    switch (filter) {
    case fastgltf::Filter::NearestMipMapNearest:
    case fastgltf::Filter::LinearMipMapNearest:
        return VK_SAMPLER_MIPMAP_MODE_NEAREST;

    case fastgltf::Filter::NearestMipMapLinear:
    case fastgltf::Filter::LinearMipMapLinear:
    default:
        return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }
}

std::optional<AllocatedImage> GLTFModel::load_image(VulkanEngine* engine, fastgltf::Asset& asset, fastgltf::Image& image)
{
    AllocatedImage newImage {};
    int width, height, nrChannels;

    std::visit(
        fastgltf::visitor {
            // Image stored outside of GLTF / GLB file.
            [&](const fastgltf::sources::URI& filePath) {
                assert(filePath.fileByteOffset == 0); // We don't support offsets with stbi.
                assert(filePath.uri.isLocalPath()); // We're only capable of loading local files.

                const std::string path(filePath.uri.path().begin(), filePath.uri.path().end()); // Thanks C++.
                if (unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 4)) {
                    VkExtent3D imagesize;
                    imagesize.width = width;
                    imagesize.height = height;
                    imagesize.depth = 1;
                    newImage = engine->create_image(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, true);
                    stbi_image_free(data);
                }
            },
            // Image is loaded directly into a std::vector. If the texture is on base64, or if we instruct it to load external image files (fastgltf::Options::LoadExternalImages).
            [&](const fastgltf::sources::Vector& vector) {
                if (unsigned char* data = stbi_load_from_memory(vector.bytes.data(), static_cast<int>(vector.bytes.size()),
                        &width, &height, &nrChannels, 4)) {
                    VkExtent3D imagesize;
                    imagesize.width = width;
                    imagesize.height = height;
                    imagesize.depth = 1;
                    newImage = engine->create_image(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, true);
                    stbi_image_free(data);
                }
            },
            // Image embedded into the binary GLB file.
            [&](const fastgltf::sources::BufferView& view) {
                const auto& bufferView = asset.bufferViews[view.bufferViewIndex];
                auto& buffer = asset.buffers[bufferView.bufferIndex];
                // We only care about VectorWithMime here, because we specify LoadExternalBuffers, meaning all buffers are already loaded into a vector.
                std::visit(fastgltf::visitor {
                               [&](const fastgltf::sources::Vector& vector) {
                                   if (unsigned char* data = stbi_load_from_memory(vector.bytes.data() + bufferView.byteOffset,
                                           static_cast<int>(bufferView.byteLength),
                                           &width, &height, &nrChannels, 4)) {
                                       VkExtent3D imagesize;
                                       imagesize.width = width;
                                       imagesize.height = height;
                                       imagesize.depth = 1;
                                       newImage = engine->create_image(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM,
                                           VK_IMAGE_USAGE_SAMPLED_BIT, true);
                                       stbi_image_free(data);
                                   }
                               },
                               [](const auto& arg) {},
                           },
                    buffer.data);
            },
            [](const auto& arg) {},
        },
        image.data);
    // Move the lambda taking const auto to the bottom. Otherwise it always get runs and the other lambdas don't.
    // Needs to be exactly const auto&?
    // No idea why it's even needed in the first place.

    if (newImage.image == VK_NULL_HANDLE)
        return {};
    return newImage;
}

void GLTFModel::cleanup() const
{
    const VkDevice device = mEngine->mDevice;

    for (const auto& sampler : mSamplers)
        vkDestroySampler(device, sampler, nullptr);
}

std::optional<std::shared_ptr<GLTFModel>> load_gltf_model(VulkanEngine* engine, std::filesystem::path filePath)
{
    fmt::println("Loading GLTF Model: {}", filePath.filename().string());

    fastgltf::Parser parser {};
    fastgltf::Asset gltf;
    fastgltf::GltfDataBuffer data;
    constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble | fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers | fastgltf::Options::LoadExternalImages;

    data.loadFromFile(filePath);

    auto type = fastgltf::determineGltfFileType(&data);
    if (type == fastgltf::GltfType::Invalid) {
        fmt::println("Failed to determine GLTF continer");
        return {};
    }

    auto load = (type == fastgltf::GltfType::glTF) ? (parser.loadGLTF(&data, filePath.parent_path(), gltfOptions)) : (parser.loadBinaryGLTF(&data, filePath.parent_path(), gltfOptions));
    if (load) {
        gltf = std::move(load.get());
    } else {
        fmt::println("Failed to load GLTF Model: {}", fastgltf::to_underlying(load.error()));
        return {};
    }

    return std::make_shared<GLTFModel>(engine, gltf, filePath);
}

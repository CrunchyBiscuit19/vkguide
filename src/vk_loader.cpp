﻿#include "vk_engine.h"
#include "vk_types.h"
#include <vk_loader.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>
#include <glm/gtx/quaternion.hpp>

#include <iostream>
#include <variant>

VkFilter extract_filter(fastgltf::Filter filter)
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

VkSamplerMipmapMode extract_mipmap_mode(fastgltf::Filter filter)
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

std::optional<std::shared_ptr<LoadedGLTF>> load_gltf(VulkanEngine* engine, std::string_view filePath)
{
    fmt::print("Loading GLTF: {}\n", filePath);

    fastgltf::Parser parser {};
    fastgltf::Asset gltf;
    fastgltf::GltfDataBuffer data;
    data.loadFromFile(filePath);
    constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble | fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers; // fastgltf::Options::LoadExternalImages;
    std::filesystem::path path = filePath;

    std::shared_ptr<LoadedGLTF> scene = std::make_shared<LoadedGLTF>();
    scene->creator = engine;
    LoadedGLTF& file = *scene;

    // Load data
    auto type = fastgltf::determineGltfFileType(&data);
    if (type == fastgltf::GltfType::Invalid) {
        std::cerr << "Failed to determine glTF container" << std::endl;
        return {};
    }
    auto load = (type == fastgltf::GltfType::glTF) ? (parser.loadGLTF(&data, path.parent_path(), gltfOptions)) : (parser.loadBinaryGLTF(&data, path.parent_path(), gltfOptions));
    if (load) {
        gltf = std::move(load.get());
    } else {
        std::cerr << "Failed to load glTF: " << fastgltf::to_underlying(load.error()) << std::endl;
        return {};
    }

    // Descriptor sets and descriptor types
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = { { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 } };
    file.descriptorPool.init(engine->_device, gltf.materials.size(), sizes);

    // Load samplers
    for (fastgltf::Sampler& sampler : gltf.samplers) {
        VkSamplerCreateInfo sampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr };
        sampl.maxLod = VK_LOD_CLAMP_NONE;
        sampl.minLod = 0;
        sampl.magFilter = extract_filter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
        sampl.minFilter = extract_filter(sampler.minFilter.value_or(fastgltf::Filter::Nearest));
        sampl.mipmapMode = extract_mipmap_mode(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

        VkSampler newSampler;
        vkCreateSampler(engine->_device, &sampl, nullptr, &newSampler);
        file.samplers.push_back(newSampler);
    }

    // Temporal arrays for all the objects to use while creating the GLTF data
    std::vector<std::shared_ptr<MeshAsset>> meshes;
    std::vector<std::shared_ptr<Node>> nodes;
    std::vector<AllocatedImage> images;
    std::vector<std::shared_ptr<GLTFMaterial>> materials;

    // Load textures, with checkerboard as placeholder for loading errors
    images.reserve(gltf.images.size());
    for (fastgltf::Image& image : gltf.images) {
        if (std::optional<AllocatedImage> img = load_image(engine, gltf, image); img.has_value()) {
            images.push_back(*img);
            file.images[image.name.c_str()] = *img;
        } else {
            // Failed to load -> default checkerboard texture
            images.push_back(engine->_stockImages["errorCheckerboard"]);
            std::cout << "gltf failed to load texture " << image.name << std::endl;
        }
    }

    // Create buffer to hold the material data
    file.materialDataBuffer = engine->create_buffer(sizeof(GLTFMetallic_Roughness::MaterialConstants) * gltf.materials.size(),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    int data_index = 0;
    auto sceneMaterialConstants = static_cast<GLTFMetallic_Roughness::MaterialConstants*>(file.materialDataBuffer.info.pMappedData);

    // Load materials
    for (fastgltf::Material& mat : gltf.materials) {
        std::shared_ptr<GLTFMaterial> newMat = std::make_shared<GLTFMaterial>();
        materials.push_back(newMat);
        file.materials[mat.name.c_str()] = newMat;

        GLTFMetallic_Roughness::MaterialConstants constants;
        constants.colorFactors.x = mat.pbrData.baseColorFactor[0];
        constants.colorFactors.y = mat.pbrData.baseColorFactor[1];
        constants.colorFactors.z = mat.pbrData.baseColorFactor[2];
        constants.colorFactors.w = mat.pbrData.baseColorFactor[3];
        constants.metal_rough_factors.x = mat.pbrData.metallicFactor;
        constants.metal_rough_factors.y = mat.pbrData.roughnessFactor;
        sceneMaterialConstants[data_index] = constants; // Write material parameters to buffer

        MaterialPass passType = MaterialPass::MainColor;
        if (mat.alphaMode == fastgltf::AlphaMode::Blend)
            passType = MaterialPass::Transparent;

        // Default the material textures
        GLTFMetallic_Roughness::MaterialResources materialResources;
        materialResources.colorImage = engine->_stockImages["white"];
        materialResources.colorSampler = engine->_defaultSamplerLinear;
        materialResources.metalRoughImage = engine->_stockImages["white"];
        materialResources.metalRoughSampler = engine->_defaultSamplerLinear;

        // Set the uniform buffer for the material data
        materialResources.dataBuffer = file.materialDataBuffer.buffer;
        materialResources.dataBufferOffset = data_index * sizeof(GLTFMetallic_Roughness::MaterialConstants);
        // Grab textures from gltf file
        if (mat.pbrData.baseColorTexture.has_value()) {
            size_t img = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
            size_t sampler = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].samplerIndex.value();
            materialResources.colorImage = images[img];
            materialResources.colorSampler = file.samplers[sampler];
        }
        // Build material
        newMat->data = engine->metalRoughMaterial.write_material(engine->_device, passType, materialResources, file.descriptorPool);

        data_index++;
    }

    // Load meshes
    // Use the same vectors for all meshes so that the memory doesnt reallocate as often
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;
    for (fastgltf::Mesh& mesh : gltf.meshes) {
        std::shared_ptr<MeshAsset> newmesh = std::make_shared<MeshAsset>();
        meshes.push_back(newmesh);
        file.meshes[mesh.name.c_str()] = newmesh;
        newmesh->name = mesh.name;

        // Clear the mesh arrays each mesh, we dont want to merge them by error
        indices.clear();
        vertices.clear();
        // Load primitives (of each mesh)
        for (auto&& p : mesh.primitives) {
            GeoSurface newSurface;
            newSurface.startIndex = static_cast<uint32_t>(indices.size());
            newSurface.count = static_cast<uint32_t>(gltf.accessors[p.indicesAccessor.value()].count);

            size_t initialVerticesSize = vertices.size();
            // Load indexes
            {
                // Add the indices of current primitive to the previous ones
                fastgltf::Accessor& indexaccessor = gltf.accessors[p.indicesAccessor.value()];
                indices.reserve(indices.size() + indexaccessor.count);
                fastgltf::iterateAccessor<std::uint32_t>(gltf, indexaccessor,
                    [&](std::uint32_t idx) {
                        // Add the vertices vector size so indices would reference vertices of current primitive instead of first set of vertices added
                        indices.push_back(idx + initialVerticesSize);
                    });
            }

            // Load vertex positions
            {
                // Add the vertices of current primitive to the previous ones
                fastgltf::Accessor& posAccessor = gltf.accessors[p.findAttribute("POSITION")->second];
                vertices.resize(vertices.size() + posAccessor.count);
                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor, // Default all the params
                    [&](glm::vec3 v, size_t index) {
                        Vertex newvtx;
                        newvtx.position = v;
                        newvtx.normal = { 1, 0, 0 };
                        newvtx.color = glm::vec4 { 1.f };
                        newvtx.uv_x = 0;
                        newvtx.uv_y = 0;
                        vertices[initialVerticesSize + index] = newvtx;
                    });
            }

            // Load vertex normals
            auto normals = p.findAttribute("NORMAL");
            if (normals != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[normals->second],
                    [&](glm::vec3 v, size_t index) {
                        vertices[initialVerticesSize + index].normal = v;
                    });
            }

            // Load UVs
            auto uv = p.findAttribute("TEXCOORD_0");
            if (uv != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[uv->second],
                    [&](glm::vec2 v, size_t index) {
                        vertices[initialVerticesSize + index].uv_x = v.x;
                        vertices[initialVerticesSize + index].uv_y = v.y;
                    });
            }

            // Load vertex colors
            auto colors = p.findAttribute("COLOR_0");
            if (colors != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[(*colors).second],
                    [&](glm::vec4 v, size_t index) {
                        vertices[initialVerticesSize + index].color = v;
                    });
            }

            if (p.materialIndex.has_value())
                newSurface.material = materials[p.materialIndex.value()];
            else
                newSurface.material = materials[0];

            // Loop the vertices of this surface, find min/max bounds
            glm::vec3 minpos = vertices[initialVerticesSize].position;
            glm::vec3 maxpos = vertices[initialVerticesSize].position;
            for (int i = initialVerticesSize; i < vertices.size(); i++) {
                minpos = glm::min(minpos, vertices[i].position);
                maxpos = glm::max(maxpos, vertices[i].position);
            }
            // Calculate origin and extents from the min/max, use extent lenght for radius
            newSurface.bounds.origin = (maxpos + minpos) / 2.f;
            newSurface.bounds.extents = (maxpos - minpos) / 2.f;
            newSurface.bounds.sphereRadius = glm::length(newSurface.bounds.extents);

            newmesh->surfaces.push_back(newSurface);
        }

        newmesh->meshBuffers = engine->upload_mesh(indices, vertices);
    }

    // Load all nodes and their meshes
    for (fastgltf::Node& node : gltf.nodes) {
        std::shared_ptr<Node> newNode;

        // Find if the node has a mesh, and if it does hook it to the mesh pointer and allocate it with the meshnode class
        if (node.meshIndex.has_value()) {
            newNode = std::make_shared<MeshNode>();
            dynamic_cast<MeshNode*>(newNode.get())->mesh = meshes[*node.meshIndex]; // Optional return value, not dereference
        } else {
            newNode = std::make_shared<Node>();
        }

        nodes.push_back(newNode);
        file.nodes[node.name.c_str()];

        // First function if it's a mat4 transform, second function if it's separate transform / rotate / scale quaternion or vec3
        std::visit(fastgltf::visitor { [&](const fastgltf::Node::TransformMatrix& matrix) {
                                          memcpy(&newNode->localTransform, matrix.data(), sizeof(matrix));
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

                           newNode->localTransform = tm * rm * sm;
                       } },
            node.transform);
    }

    // Run loop again to setup transform hierarchy
    for (int i = 0; i < gltf.nodes.size(); i++) {
        fastgltf::Node& node = gltf.nodes[i];
        std::shared_ptr<Node>& sceneNode = nodes[i];

        for (auto& c : node.children) {
            sceneNode->children.push_back(nodes[c]);
            nodes[c]->parent = sceneNode;
        }
    }

    // Find the top nodes, with no parents
    for (auto& node : nodes) {
        if (node->parent.lock() == nullptr) {
            file.topNodes.push_back(node);
            node->refreshTransform(glm::mat4 { 1.f });
        }
    }

    return scene;
}

std::optional<AllocatedImage> load_image(VulkanEngine* engine, fastgltf::Asset& asset, fastgltf::Image& image)
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

void LoadedGLTF::Draw(const glm::mat4& topMatrix, DrawContext& ctx)
{
    // create renderables from the scenenodes
    for (auto& n : topNodes) {
        n->Draw(topMatrix, ctx);
    }
}

void LoadedGLTF::clearAll()
{
    const VkDevice device = creator->_device;

    descriptorPool.destroy_pools(device);
    for (const auto& sampler : samplers)
        vkDestroySampler(device, sampler, nullptr);

    // Added to deletion queue in the create_<object> functions
    /*creator->destroy_buffer(materialDataBuffer);
    for (const auto& v : meshes | std::views::values) {

        creator->destroy_buffer(v->meshBuffers.indexBuffer);
        creator->destroy_buffer(v->meshBuffers.vertexBuffer);
    }
    for (const auto& v : images | std::views::values) {

        if (v.image == creator->_errorCheckerboardImage.image)
            continue;
        creator->destroy_image(v);
    }*/
}

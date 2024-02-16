#pragma once
#include "../shader/standard.glsl"
#include "Scene.hpp"

struct ObjectDataBuffer {
    void init(const rv::Context& context) {
        data.resize(maxObjectCount);
        buffer = context.createBuffer({
            .usage = rv::BufferUsage::Storage,
            .memory = rv::MemoryUsage::Device,
            .size = sizeof(ObjectData) * data.size(),
            .debugName = "ObjectDataBuffer",
        });
    }

    void clear() {
        std::ranges::fill(data, ObjectData{});
    }

    void update(const rv::CommandBuffer& commandBuffer, Scene& scene) {
        auto& objects = scene.getObjects();
        for (uint32_t index : scene.getUpdatedObjectIndices()) {
            auto& object = objects[index];
            auto* mesh = object.get<Mesh>();
            auto* transform = object.get<Transform>();
            if (!mesh) {
                return;
            }

            // TODO: マテリアル情報はバッファを分けてGPU側でインデックス参照する
            //       materialIndexはpushConstantでもいいかも
            if (Material* material = mesh->material) {
                data[index].baseColor = material->baseColor;
                data[index].emissive.xyz = material->emissive;
                data[index].metallic = material->metallic;
                data[index].roughness = material->roughness;
                data[index].ior = material->ior;
                data[index].baseColorTextureIndex = material->baseColorTextureIndex;
                data[index].metallicRoughnessTextureIndex = material->metallicRoughnessTextureIndex;
                data[index].normalTextureIndex = material->normalTextureIndex;
                data[index].occlusionTextureIndex = material->occlusionTextureIndex;
                data[index].emissiveTextureIndex = material->emissiveTextureIndex;
                data[index].enableNormalMapping = static_cast<int>(material->enableNormalMapping);
            }
            if (transform) {
                const auto& model = transform->computeTransformMatrix();
                data[index].modelMatrix = model;
                data[index].normalMatrix = transform->computeNormalMatrix();
            }
        }

        commandBuffer.copyBuffer(buffer, data.data());
        commandBuffer.bufferBarrier(
            buffer,  //
            vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllGraphics,
            vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead);
    }

    int maxObjectCount = 1000;
    std::vector<ObjectData> data{};
    rv::BufferHandle buffer;
};

struct SceneDataBuffer {
    void init(const rv::Context& context) {
        buffer = context.createBuffer({
            .usage = rv::BufferUsage::Uniform,
            .memory = rv::MemoryUsage::Device,
            .size = sizeof(SceneData),
            .debugName = "Renderer::buffer",
        });
    }

    void clear() {
        data = SceneData{};
    }

    void update(const rv::CommandBuffer& commandBuffer,
                Scene& scene,
                vk::Extent3D imageExtent,
                bool enableFXAA,
                bool enableSSR,
                float exposure) {
        // Update buffer
        // NOTE: Shadow map用の行列も更新するのでShadow map passより先に計算
        Camera* camera = &scene.getDefaultCamera();
        if (scene.isMainCameraAvailable()) {
            camera = scene.getMainCamera();
        }

        const auto& view = camera->getView();
        const auto& proj = camera->getProj();
        data.cameraView = view;
        data.cameraProj = proj;
        data.cameraViewProj = proj * view;
        data.cameraPos.xyz = camera->getPosition();

        data.screenResolution.x = static_cast<float>(imageExtent.width);
        data.screenResolution.y = static_cast<float>(imageExtent.height);
        data.enableFXAA = static_cast<int>(enableFXAA);
        data.enableSSR = static_cast<int>(enableSSR);
        data.exposure = exposure;

        if (Object* dirLightObj = scene.findObject<DirectionalLight>()) {
            DirectionalLight* dirLight = dirLightObj->get<DirectionalLight>();
            data.existDirectionalLight = true;
            data.lightDirection.xyz = dirLight->getDirection();
            data.lightColorIntensity.xyz = dirLight->color;
            data.lightColorIntensity.w = dirLight->intensity;
            data.shadowViewProj = dirLight->getViewProj(scene.getAABB());
            data.shadowBias = dirLight->shadowBias;
            data.enableShadowMapping = dirLight->enableShadow;
        } else {
            data.existDirectionalLight = false;
            data.enableShadowMapping = false;
        }
        if (Object* ambLightObj = scene.findObject<AmbientLight>()) {
            auto* light = ambLightObj->get<AmbientLight>();
            data.ambientColorIntensity.xyz = light->color;
            data.ambientColorIntensity.w = light->intensity;
            data.irradianceTexture = light->irradianceTexture;
            data.radianceTexture = light->radianceTexture;
        }

        // TODO: DeviceHostに変更した方がいいかどうかプロファイリング
        commandBuffer.copyBuffer(buffer, &data);
        commandBuffer.bufferBarrier(
            buffer,  //
            vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllGraphics,
            vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead);
    }

    SceneData data{};
    rv::BufferHandle buffer;
};

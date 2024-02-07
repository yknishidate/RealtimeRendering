#pragma once
#include <reactive/reactive.hpp>

struct RenderImages {
    // Images
    vk::Format colorFormat = vk::Format::eR16G16B16A16Sfloat;
    vk::Format depthFormat = vk::Format::eD32Sfloat;
    rv::ImageHandle baseColorImage;
    rv::ImageHandle depthImage;

    void createImages(const rv::Context& context, uint32_t width, uint32_t height) {
        baseColorImage = context.createImage({
            .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage |
                     vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc |
                     vk::ImageUsageFlagBits::eColorAttachment,
            .extent = {width, height, 1},
            .format = colorFormat,
            .debugName = "ViewportRenderer::colorImage",
        });
        baseColorImage->createImageView();
        baseColorImage->createSampler();

        depthImage = context.createImage({
            .usage = rv::ImageUsage::DepthAttachment,
            .extent = {width, height, 1},
            .format = depthFormat,
            .debugName = "ViewportRenderer::depthImage",
        });
        depthImage->createImageView(vk::ImageViewType::e2D, vk::ImageAspectFlagBits::eDepth);
        depthImage->createSampler();

        context.oneTimeSubmit([&](rv::CommandBufferHandle commandBuffer) {
            commandBuffer->transitionLayout(baseColorImage, vk::ImageLayout::eGeneral);
            commandBuffer->transitionLayout(depthImage, vk::ImageLayout::eDepthAttachmentOptimal);
        });
    }
};

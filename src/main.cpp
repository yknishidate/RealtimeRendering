#include <imgui_impl_vulkan.h>

#include <reactive/reactive.hpp>

using namespace rv;

std::string vertCode = R"(
#version 450
layout(location = 0) out vec4 outColor;
vec3 positions[] = vec3[](vec3(0), vec3(1, 0, 0), vec3(0, 1, 0));
vec3 colors[] = vec3[](vec3(0), vec3(1, 0, 0), vec3(0, 1, 0));
void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 1);
    outColor = vec4(colors[gl_VertexIndex], 1);
})";

std::string fragCode = R"(
#version 450
layout(location = 0) in vec4 inColor;
layout(location = 0) out vec4 outColor;
void main() {
    outColor = inColor;
})";

class HelloApp : public App {
public:
    HelloApp()
        : App({
              .width = 1920,
              .height = 1080,
              .title = "HelloGraphics",
              .vsync = false,
              .layers = {Layer::Validation, Layer::FPSMonitor},
          }) {}

    void onStart() override {
        std::vector<ShaderHandle> shaders(2);
        shaders[0] = context.createShader({
            .code = Compiler::compileToSPV(vertCode, vk::ShaderStageFlagBits::eVertex),
            .stage = vk::ShaderStageFlagBits::eVertex,
        });

        shaders[1] = context.createShader({
            .code = Compiler::compileToSPV(fragCode, vk::ShaderStageFlagBits::eFragment),
            .stage = vk::ShaderStageFlagBits::eFragment,
        });

        descSet = context.createDescriptorSet({
            .shaders = shaders,
        });

        pipeline = context.createGraphicsPipeline({
            .descSetLayout = descSet->getLayout(),
            .vertexShader = shaders[0],
            .fragmentShader = shaders[1],
        });

        colorImage = context.createImage({
            .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage |
                     vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc |
                     vk::ImageUsageFlagBits::eColorAttachment,
            .extent = {1920, 1080, 1},
            .format = vk::Format::eB8G8R8A8Unorm,
            .debugName = "ViewportWindow::colorImage",
        });

        context.oneTimeSubmit([&](auto commandBuffer) {
            commandBuffer->transitionLayout(colorImage, vk::ImageLayout::eGeneral);
        });

        // Create desc set
        ImGui_ImplVulkan_RemoveTexture(imguiDescSet);
        imguiDescSet = ImGui_ImplVulkan_AddTexture(colorImage->getSampler(), colorImage->getView(),
                                                   VK_IMAGE_LAYOUT_GENERAL);

        gpuTimer = context.createGPUTimer({});
    }

    void showMenuBar() const {
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Open..", "Ctrl+O")) { /* Do stuff */
                }
                if (ImGui::MenuItem("Save", "Ctrl+S")) { /* Do stuff */
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
    }

    void showViewport() const {
        if (ImGui::Begin("Viewport")) {
            ImVec2 windowSize = ImGui::GetContentRegionAvail();
            ImGui::Image(imguiDescSet, windowSize, ImVec2(0, 1), ImVec2(1, 0));
            ImGui::End();
        }
    }

    void showDockspace(ImGuiWindowFlags windowFlags) {
        if (dockspaceOpen) {
            ImGui::Begin("DockSpace", &dockspaceOpen, windowFlags);
            ImGui::PopStyleVar(3);

            ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

            showMenuBar();

            showViewport();

            if (ImGui::Begin("Performance")) {
                if (frame > 0) {
                    for (int i = 0; i < TIME_BUFFER_SIZE - 1; i++) {
                        times[i] = times[i + 1];
                    }
                    float time = gpuTimer->elapsedInMilli();
                    times[TIME_BUFFER_SIZE - 1] = time;
                    ImGui::Text("GPU timer: %.3f ms", time);
                    ImGui::PlotLines("Times", times, TIME_BUFFER_SIZE, 0, nullptr, FLT_MAX, FLT_MAX,
                                     {300, 150});
                }

                ImGui::End();
            }

            ImGui::End();
        }
    }

    void onRender(const CommandBufferHandle& commandBuffer) override {
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
        windowFlags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        windowFlags |= ImGuiWindowFlags_MenuBar;

        showDockspace(windowFlags);

        commandBuffer->clearColorImage(colorImage, {0.0f, 0.0f, 0.5f, 1.0f});
        commandBuffer->transitionLayout(colorImage, vk::ImageLayout::eGeneral);
        commandBuffer->setViewport(width, height);
        commandBuffer->setScissor(width, height);
        commandBuffer->bindDescriptorSet(descSet, pipeline);
        commandBuffer->bindPipeline(pipeline);
        commandBuffer->beginTimestamp(gpuTimer);
        commandBuffer->beginRendering(colorImage, nullptr, {0, 0}, {width, height});
        commandBuffer->draw(3, 1, 0, 0);
        commandBuffer->endRendering();
        commandBuffer->endTimestamp(gpuTimer);
        frame++;
    }

    // TODO: Rendererはimageを外からセットされるように
    ImageHandle colorImage;
    vk::DescriptorSet imguiDescSet;

    static constexpr int TIME_BUFFER_SIZE = 300;
    float times[TIME_BUFFER_SIZE] = {0};
    DescriptorSetHandle descSet;
    GraphicsPipelineHandle pipeline;
    GPUTimerHandle gpuTimer;
    int frame = 0;
    bool dockspaceOpen = true;
};

int main() {
    try {
        HelloApp app{};
        app.run();
    } catch (const std::exception& e) {
        spdlog::error(e.what());
    }
}

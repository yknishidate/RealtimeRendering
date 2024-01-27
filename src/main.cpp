
#include <future>
#include <reactive/App.hpp>

#include <nlohmann/json.hpp>

#include "Renderer.hpp"
#include "Scene.hpp"
#include "editor/AssetWindow.hpp"
#include "editor/AttributeWindow.hpp"
#include "editor/SceneWindow.hpp"
#include "editor/ViewportWindow.hpp"

class Editor : public rv::App {
public:
    Editor()
        : App({
              .width = 2560,
              .height = 1440,
              .title = "Reactive Editor",
              .layers = {rv::Layer::Validation},
              .extensions = {rv::Extension::RayTracing},
          }) {}

    void onStart() override {
        std::ifstream jsonFile(DEV_ASSET_DIR / "scenes" / "two_boxes.json");
        if (!jsonFile.is_open()) {
            throw std::runtime_error("Failed to open scene file.");
        }
        nlohmann::json json;
        jsonFile >> json;

        for (const auto& mesh : json["meshes"]) {
            if (mesh["type"] == "Cube") {
                scene.meshes.push_back(rv::Mesh::createCubeMesh(context, {}));
            } else if (mesh["type"] == "Plane") {
                rv::PlaneMeshCreateInfo createInfo{
                    .width = mesh["width"],
                    .height = mesh["height"],
                    .widthSegments = mesh["widthSegments"],
                    .heightSegments = mesh["heightSegments"],
                };
                scene.meshes.push_back(rv::Mesh::createPlaneMesh(context, createInfo));
            }
        }

        for (const auto& material : json["materials"]) {
            if (material["type"] == "Standard") {
                const auto& baseColor = material["baseColor"];
                scene.materials.push_back(Material{
                    .baseColor = {baseColor[0], baseColor[1], baseColor[2], baseColor[3]},
                    .name = material["name"],
                });
            } else {
                assert(false && "Not implemented");
            }
        }

        // Add object
        Object object;
        object.name = "Cube 0";
        object.mesh = &scene.meshes[0];
        object.material = &scene.materials[0];
        object.transform.translation = glm::vec3{-1.5, 0, 0};
        scene.objects.push_back(object);

        object.name = "Cube 1";
        object.material = &scene.materials[1];
        object.transform.translation = glm::vec3{1.5, 0, 0};
        scene.objects.push_back(object);

        object.name = "Plane 0";
        object.mesh = &scene.meshes[1];
        object.material = &scene.materials[2];
        object.transform.translation = glm::vec3{0, -1, 0};
        scene.objects.push_back(object);

        camera = rv::OrbitalCamera{this, 1920, 1080};
        camera.fovY = glm::radians(30.0f);
        camera.distance = 10.0f;

        iconManager.init(context);
        assetWindow.init(context, scene, iconManager);
        viewportWindow.init(context, iconManager, 1920, 1080);
        attributeWindow.init(context, scene, iconManager);

        renderer.init(context);
    }

    void onUpdate() override {
        camera.processDragDelta(viewportWindow.dragDelta);
        camera.processMouseScroll(viewportWindow.mouseScroll);
        frame++;
    }

    void onRender(const rv::CommandBufferHandle& commandBuffer) override {
        if (viewportWindow.needsRecreate()) {
            context.getDevice().waitIdle();
            viewportWindow.createImages(static_cast<uint32_t>(viewportWindow.width),
                                        static_cast<uint32_t>(viewportWindow.height));
            camera.aspect = viewportWindow.width / viewportWindow.height;
        }
        static bool dockspaceOpen = true;
        commandBuffer->clearColorImage(getCurrentColorImage(), {0.0f, 0.0f, 0.0f, 1.0f});

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

        if (dockspaceOpen) {
            ImGui::Begin("DockSpace", &dockspaceOpen, windowFlags);
            ImGui::PopStyleVar(3);

            if (ImGui::BeginMenuBar()) {
                if (ImGui::BeginMenu("File")) {
                    if (ImGui::MenuItem("Open..", "Ctrl+O")) { /* Do stuff */
                    }
                    if (ImGui::MenuItem("Save", "Ctrl+S")) { /* Do stuff */
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Edit")) {
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Create")) {
                    ImGui::EndMenu();
                }
                ImGui::EndMenuBar();
            }

            ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

            if (frame > 1) {
                if (ImGui::Begin("Performance")) {
                    ImGui::Text("Rendering: %f ms", renderer.getRenderingTimeMs());
                    ImGui::End();
                }
            }

            sceneWindow.show(scene, &selectedObject);
            int message = Message::None;
            message |= attributeWindow.show(selectedObject);
            message |= viewportWindow.show(scene, selectedObject, camera, frame);
            assetWindow.show();

            renderer.render(*commandBuffer, viewportWindow.colorImage, viewportWindow.depthImage,
                            scene, camera, frame);
            viewportWindow.drawGrid(*commandBuffer, camera);

            ImGui::End();
        }
    }

    // Scene
    rv::OrbitalCamera camera;
    Scene scene;
    int frame = 0;

    // Renderer
    Renderer renderer;

    // ImGui
    Object* selectedObject = nullptr;

    // Editor
    SceneWindow sceneWindow;
    ViewportWindow viewportWindow;
    AttributeWindow attributeWindow;
    AssetWindow assetWindow;
    IconManager iconManager;
};

int main() {
    try {
        Editor app{};
        app.run();
    } catch (const std::exception& e) {
        spdlog::error(e.what());
    }
}

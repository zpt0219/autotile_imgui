#include "app.h"
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#ifdef _WIN32
#include <windows.h>
#include <GL/gl.h>
#else
#include <GL/gl.h>
#endif
#include <iostream>

namespace atm_desktop {

App::App() {
    view_model_.register_panel(&library_panel_);
    view_model_.register_panel(&recipe_panel_);
    view_model_.register_panel(&preview_panel_);
    view_model_.register_panel(&variant_panel_);
    view_model_.register_panel(&batch_export_panel_);
    view_model_.register_panel(&log_panel_);
}

App::~App() {
    cleanup();
}

void App::set_dark_theme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    style.WindowRounding = 6.0f;
    style.ChildRounding = 4.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;

    colors[ImGuiCol_Text]                  = ImVec4(0.92f, 0.93f, 0.95f, 1.00f);
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.50f, 0.53f, 0.58f, 1.00f);
    colors[ImGuiCol_WindowBg]              = ImVec4(0.12f, 0.13f, 0.15f, 1.00f);
    colors[ImGuiCol_ChildBg]               = ImVec4(0.10f, 0.11f, 0.13f, 1.00f);
    colors[ImGuiCol_PopupBg]               = ImVec4(0.14f, 0.15f, 0.18f, 0.98f);
    colors[ImGuiCol_Border]                = ImVec4(0.22f, 0.24f, 0.28f, 1.00f);
    colors[ImGuiCol_FrameBg]               = ImVec4(0.18f, 0.20f, 0.24f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.24f, 0.26f, 0.32f, 1.00f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.28f, 0.32f, 0.40f, 1.00f);
    colors[ImGuiCol_TitleBg]               = ImVec4(0.10f, 0.11f, 0.13f, 1.00f);
    colors[ImGuiCol_TitleBgActive]         = ImVec4(0.15f, 0.17f, 0.21f, 1.00f);
    colors[ImGuiCol_MenuBarBg]             = ImVec4(0.10f, 0.11f, 0.13f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.10f, 0.11f, 0.13f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.24f, 0.27f, 0.33f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.32f, 0.36f, 0.44f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.40f, 0.46f, 0.56f, 1.00f);
    colors[ImGuiCol_CheckMark]             = ImVec4(0.26f, 0.65f, 0.96f, 1.00f);
    colors[ImGuiCol_SliderGrab]            = ImVec4(0.26f, 0.65f, 0.96f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.36f, 0.75f, 1.00f, 1.00f);
    colors[ImGuiCol_Button]                = ImVec4(0.20f, 0.23f, 0.28f, 1.00f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.28f, 0.32f, 0.40f, 1.00f);
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.34f, 0.40f, 0.50f, 1.00f);
    colors[ImGuiCol_Header]                = ImVec4(0.18f, 0.21f, 0.26f, 1.00f);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.24f, 0.28f, 0.36f, 1.00f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.30f, 0.36f, 0.46f, 1.00f);
    colors[ImGuiCol_Tab]                   = ImVec4(0.13f, 0.15f, 0.18f, 1.00f);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.24f, 0.28f, 0.36f, 1.00f);
    colors[ImGuiCol_TabActive]             = ImVec4(0.20f, 0.23f, 0.29f, 1.00f);
    colors[ImGuiCol_TabUnfocused]          = ImVec4(0.12f, 0.13f, 0.15f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.16f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_DockingPreview]        = ImVec4(0.26f, 0.65f, 0.96f, 0.30f);
    colors[ImGuiCol_Separator]             = ImVec4(0.20f, 0.22f, 0.26f, 1.00f);
    colors[ImGuiCol_ResizeGrip]            = ImVec4(0.24f, 0.27f, 0.33f, 1.00f);
}

bool App::initialize() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);

    window_ = glfwCreateWindow(1440, 900, "AutoTile Mixer (Desktop)", nullptr, nullptr);
    if (!window_) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1); // Enable vsync

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    set_dark_theme();

    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    view_model_.sheet_renderer().initialize();
    is_running_ = true;
    return true;
}

void App::cleanup() {
    if (!is_running_) return;

    view_model_.sheet_renderer().cleanup();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    glfwTerminate();
    is_running_ = false;
}

void App::render_menu_bar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Recipe", "Ctrl+N")) {
                view_model_.show_new_recipe_modal = true;
            }
            if (ImGui::MenuItem("Import Share Code...", "Ctrl+I")) {
                view_model_.show_import_share_modal = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Save Library...", "Ctrl+S")) {
                view_model_.handler().library()->save_to_file("library.atmlib");
                view_model_.log("Saved library to library.atmlib", "INFO");
            }
            if (ImGui::MenuItem("Open Library...", "Ctrl+O")) {
                auto loaded = atm::RecipeLibrary::load_from_file("library.atmlib");
                if (loaded) {
                    view_model_.handler().set_library(std::move(loaded), &view_model_);
                    view_model_.log("Opened library from library.atmlib", "INFO");
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                glfwSetWindowShouldClose(window_, GLFW_TRUE);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, view_model_.can_undo())) {
                view_model_.undo();
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, view_model_.can_redo())) {
                view_model_.redo();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            bool lib_open = library_panel_.is_open();
            if (ImGui::MenuItem("Recipe Library", nullptr, &lib_open)) library_panel_.set_open(lib_open);

            bool rec_open = recipe_panel_.is_open();
            if (ImGui::MenuItem("Recipe Inspector", nullptr, &rec_open)) recipe_panel_.set_open(rec_open);

            bool prev_open = preview_panel_.is_open();
            if (ImGui::MenuItem("Sheet Preview", nullptr, &prev_open)) preview_panel_.set_open(prev_open);

            bool var_open = variant_panel_.is_open();
            if (ImGui::MenuItem("Variant Matrix", nullptr, &var_open)) variant_panel_.set_open(var_open);

            bool exp_open = batch_export_panel_.is_open();
            if (ImGui::MenuItem("Batch Export", nullptr, &exp_open)) batch_export_panel_.set_open(exp_open);

            bool log_open = log_panel_.is_open();
            if (ImGui::MenuItem("Activity Log", nullptr, &log_open)) log_panel_.set_open(log_open);

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About AutoTile Mixer")) {
                view_model_.log("AutoTile Mixer Desktop v1.0.0 (Native C++ / ImGui)", "INFO");
            }
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

void App::setup_dockspace() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("MainDockSpaceRoot", nullptr, window_flags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspace_id = ImGui::GetID("AutoTileDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

    ImGui::End();
}

void App::run() {
    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();

        // Keyboard shortcuts
        ImGuiIO& io = ImGui::GetIO();
        if (io.KeyCtrl && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z)) {
            view_model_.undo();
        }
        if ((io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) || (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z))) {
            view_model_.redo();
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        render_menu_bar();
        setup_dockspace();

        // Drain worker progress events on the main thread
        view_model_.drain_progress_queue();

        // Draw Panels
        library_panel_.draw(view_model_);
        recipe_panel_.draw(view_model_);
        preview_panel_.draw(view_model_);
        variant_panel_.draw(view_model_);
        batch_export_panel_.draw(view_model_);
        log_panel_.draw(view_model_);

        ImGui::Render();

        int display_w, display_h;
        glfwGetFramebufferSize(window_, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.12f, 0.13f, 0.15f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window_);
    }
}

} // namespace atm_desktop

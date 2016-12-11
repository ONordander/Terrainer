#include "terrainer.hpp"
#include "helpers.hpp"
#include "node.hpp"
#include "parametric_shapes.hpp"
#include "marching_tables.hpp"

#include "config.hpp"
#include "external/glad/glad.h"
#include "core/Bonobo.h"
#include "core/FPSCamera.h"
#include "core/GLStateInspection.h"
#include "core/GLStateInspectionView.h"
#include "core/InputHandler.h"
#include "core/Log.h"
#include "core/LogView.h"
#include "core/Misc.h"
#include "core/utils.h"
#include "core/Window.h"
#include <imgui.h>
#include "external/imgui_impl_glfw_gl3.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "external/glad/glad.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vector>
#include <array>
#include <cstdlib>
#include <stdexcept>

enum class polygon_mode_t : unsigned int {
    fill = 0u,
    line,
    point
};

static polygon_mode_t get_next_mode(polygon_mode_t mode)
{
    return static_cast<polygon_mode_t>((static_cast<unsigned int>(mode) + 1u) % 3u);
}

namespace constant
{
    constexpr uint32_t shadowmap_res_x = 1024;
    constexpr uint32_t shadowmap_res_y = 1024;

    constexpr size_t lights_nb           = 4;
    constexpr float  light_intensity     = 720000.0f;
    constexpr float  light_angle_falloff = 0.8f;
    constexpr float  light_cutoff        = 0.05f;
}

static eda221::mesh_data loadCone();

edan35::Terrainer::Terrainer()
{
    Log::View::Init();

    window = Window::Create("Terrainer", config::resolution_x,
                            config::resolution_y, config::msaa_rate, false, false);
    if (window == nullptr) {
        Log::View::Destroy();
        throw std::runtime_error("Failed to get a window: aborting!");
    }
    inputHandler = new InputHandler();
    window->SetInputHandler(inputHandler);

    GLStateInspection::Init();
    GLStateInspection::View::Init();

    eda221::init();
}

edan35::Terrainer::~Terrainer()
{
    eda221::deinit();

    GLStateInspection::View::Destroy();
    GLStateInspection::Destroy();

    delete inputHandler;
    inputHandler = nullptr;

    Window::Destroy(window);
    window = nullptr;

    Log::View::Destroy();
}

void
edan35::Terrainer::run()
{
    auto const window_size = window->GetDimensions();
    auto table = edan35::get_edge_tables();
    printf("element number 12 in edge table %d\n", table.edge_table[12]);

    //
    // Setup the camera
    //
    FPSCameraf mCamera(bonobo::pi / 4.0f,
                       static_cast<float>(window_size.x) / static_cast<float>(window_size.y),
                       1.0f, 10000.0f);
    mCamera.mWorld.SetTranslate(glm::vec3(0.0f, 0.0f, 6.0f));
    mCamera.mMouseSensitivity = 0.003f;
    mCamera.mMovementSpeed = 0.25f;
    window->SetCamera(&mCamera);

    auto const cube = parametric_shapes::create_cube(4u);
    if (cube.vao == 0u) {
        LogError("Failed to load marching cube");
        return;
    }

    /*
        Create Quad
    */
    auto const quad = parametric_shapes::createQuad(2, 2, 10, 10);
    if (quad.vao == 0u) {
        LogError("Failed to load quad shape");
        return;
    }

    //
    // Load all the shader programs used
    //
    auto fallback_shader = eda221::createProgram("fallback.vert", "fallback.frag");
    if (fallback_shader == 0u) {
        LogError("Failed to load fallback shader");
        return;
    }

    auto const reload_shader = [fallback_shader](std::string const& vertex_path,
                                                 std::string const& geo_path,
                                                 std::string const& fragment_path,
                                                 GLuint& program) {
        if (program != 0u && program != fallback_shader)
            glDeleteProgram(program);
        program = eda221::createProgramWithGeo("TERRAINER/", vertex_path, geo_path, fragment_path);
        if (program == 0u) {
            LogError("Failed to load \"%s\", \"%s\", and \"%s\"", vertex_path.c_str(), fragment_path.c_str(), geo_path.c_str());
            program = fallback_shader;
        }
    };

    GLuint marching_shader = 0u;
    auto const reload_shaders = [&reload_shader, &marching_shader]() {
        LogInfo("Reloading shaders");
        reload_shader("marching.vert", "marching.geo", "marching.frag", marching_shader);
    };
    reload_shaders();

    auto const light_position = glm::vec3(-2.0f, 4.0f, 2.0f);
    auto const light_ambient = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f);
    auto const light_diffuse = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f);
    auto const light_specular = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f);
    auto const set_uniforms = [&light_position, &light_ambient, &light_diffuse, &light_specular](GLuint program){
        glUniform3fv(glGetUniformLocation(program, "light_position"), 1, glm::value_ptr(light_position));
        glUniform4fv(glGetUniformLocation(program, "light_ambient"), 1, glm::value_ptr(light_ambient));
        glUniform4fv(glGetUniformLocation(program, "light_diffuse"), 1, glm::value_ptr(light_diffuse));
        glUniform4fv(glGetUniformLocation(program, "light_specular"), 1, glm::value_ptr(light_specular));
    };

    auto quad_node = Node();
    quad_node.set_geometry(quad);
    quad_node.set_program(marching_shader, set_uniforms);
    quad_node.set_has_indices(false);

    auto seconds_nb = 0.0f;

    glEnable(GL_DEPTH_TEST);

    double ddeltatime;
    size_t fpsSamples = 0;
    double nowTime, lastTime = GetTimeMilliseconds();
    double fpsNextTick = lastTime + 1000.0;

    while (!glfwWindowShouldClose(window->GetGLFW_Window())) {
        nowTime = GetTimeMilliseconds();
        ddeltatime = nowTime - lastTime;
        if (nowTime > fpsNextTick) {
            fpsNextTick += 1000.0;
            fpsSamples = 0;
        }
        fpsSamples++;
        seconds_nb += static_cast<float>(ddeltatime / 1000.0);

        glfwPollEvents();
        inputHandler->Advance();
        mCamera.Update(ddeltatime, *inputHandler);
        quad_node.rotate_x(0.05f);

        ImGui_ImplGlfwGL3_NewFrame();

        if (inputHandler->GetKeycodeState(GLFW_KEY_R) & JUST_PRESSED) {
            reload_shaders();
        }

        auto const window_size = window->GetDimensions();
        glViewport(0, 0, window_size.x, window_size.y);
        glClearDepthf(1.0f);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

        quad_node.render(mCamera.GetWorldToClipMatrix(), quad_node.get_transform());

        GLStateInspection::View::Render();

        bool opened = ImGui::Begin("Render Time", nullptr, ImVec2(120, 50), -1.0f, 0);
        if (opened)
            ImGui::Text("%.3f ms", ddeltatime);
        ImGui::End();

        Log::View::Render();
        ImGui::Render();

        window->Swap();
        lastTime = nowTime;
    }

    glDeleteProgram(fallback_shader);
    fallback_shader = 0u;
    glDeleteProgram(marching_shader);
    marching_shader = 0u;
}

int main()
{
    Bonobo::Init();
    try {
        edan35::Terrainer terrainer;
        terrainer.run();
    } catch (std::runtime_error const& e) {
        LogError(e.what());
    }
    Bonobo::Destroy();
}

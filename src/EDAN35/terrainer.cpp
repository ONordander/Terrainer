#include "terrainer.hpp"
#include "helpers.hpp"
#include "node.hpp"
#include "parametric_shapes.cpp"

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

    //
    // Setup the camera
    //
    FPSCameraf mCamera(bonobo::pi / 4.0f,
                       static_cast<float>(window_size.x) / static_cast<float>(window_size.y),
                       1.0f, 10000.0f);
    mCamera.mWorld.SetTranslate(glm::vec3(0.0f, 100.0f, 180.0f));
    mCamera.mWorld.LookAt(glm::vec3(0.0f, 0.0f, 0.0f));
    mCamera.mMouseSensitivity = 0.003f;
    mCamera.mMovementSpeed = 0.25f;
    window->SetCamera(&mCamera);

    eda221::mesh_data quad = parametric_shapes::createQuad(100, 100, 100, 100);

    //
    // Load all the shader programs used
    //
    auto fallback_shader = eda221::createProgram("fallback.vert", "fallback.frag");
    if (fallback_shader == 0u) {
        LogError("Failed to load fallback shader");
        return;
    }

    auto const reload_shader = [fallback_shader](std::string const& vertex_path, std::string const& fragment_path, GLuint& program){
        if (program != 0u && program != fallback_shader)
            glDeleteProgram(program);
        program = eda221::createProgram("../EDAN35/" + vertex_path, "../EDAN35/" + fragment_path);
        if (program == 0u) {
            LogError("Failed to load \"%s\" and \"%s\"", vertex_path.c_str(), fragment_path.c_str());
            program = fallback_shader;
        }
    };

    GLuint fill_gbuffer_shader = 0u, fill_shadowmap_shader = 0u, accumulate_lights_shader = 0u, resolve_deferred_shader = 0u;
    auto const reload_shaders = [&reload_shader,&fill_gbuffer_shader,&fill_shadowmap_shader,&accumulate_lights_shader,&resolve_deferred_shader](){
        LogInfo("Reloading shaders");
        reload_shader("fill_gbuffer.vert",      "fill_gbuffer.frag",      fill_gbuffer_shader);
        reload_shader("fill_shadowmap.vert",    "fill_shadowmap.frag",    fill_shadowmap_shader);
        reload_shader("accumulate_lights.vert", "accumulate_lights.frag", accumulate_lights_shader);
        reload_shader("resolve_deferred.vert",  "resolve_deferred.frag",  resolve_deferred_shader);
    };
    reload_shaders();

    auto const set_uniforms = [](GLuint /*program*/){};

    auto quad_node = Node();
    quad_node.set_geometry(quad);
    quad_node.set_program(fallback_shader, set_uniforms);

    /*
    // Setup textures
    //
    auto const diffuse_texture                     = eda221::createTexture(window_size.x, window_size.y);
    auto const specular_texture                    = eda221::createTexture(window_size.x, window_size.y);
    auto const normal_texture                      = eda221::createTexture(window_size.x, window_size.y);
    auto const light_diffuse_contribution_texture  = eda221::createTexture(window_size.x, window_size.y);
    auto const light_specular_contribution_texture = eda221::createTexture(window_size.x, window_size.y);
    auto const depth_texture                       = eda221::createTexture(window_size.x, window_size.y, GL_TEXTURE_2D, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT);
    auto const shadowmap_texture                   = eda221::createTexture(constant::shadowmap_res_x, constant::shadowmap_res_y, GL_TEXTURE_2D, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT);
    */


    /*
    //
    // Setup FBOs
    //
    auto const deferred_fbo  = eda221::createFBO({diffuse_texture, specular_texture, normal_texture}, depth_texture);
    auto const shadowmap_fbo = eda221::createFBO({}, shadowmap_texture);
    auto const light_fbo     = eda221::createFBO({light_diffuse_contribution_texture, light_specular_contribution_texture}, depth_texture);

    //
    // Setup samplers
    //
    auto const default_sampler = eda221::createSampler([](GLuint sampler){
        glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    });

    auto const depth_sampler = eda221::createSampler([](GLuint sampler){
        glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    });

    auto const shadow_sampler = eda221::createSampler([](GLuint sampler){
        glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glSamplerParameteri(sampler, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glSamplerParameteri(sampler, GL_TEXTURE_COMPARE_FUNC, GL_LESS);
        GLfloat border_color[4] = { 1.0f, 0.0f, 0.0f, 0.0f};
        glSamplerParameterfv(sampler, GL_TEXTURE_BORDER_COLOR, border_color);
    });

    auto const bind_texture_with_sampler = [](GLenum target, unsigned int slot, GLuint program, std::string const& name, GLuint texture, GLuint sampler){
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(target, texture);
        glUniform1i(glGetUniformLocation(program, name.c_str()), static_cast<GLint>(slot));
        glBindSampler(slot, sampler);
    };
    */

    auto seconds_nb = 0.0f;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

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

        ImGui_ImplGlfwGL3_NewFrame();

        if (inputHandler->GetKeycodeState(GLFW_KEY_R) & JUST_PRESSED) {
            reload_shaders();
        }

        glCullFace(GL_BACK);
        glDepthFunc(GL_ALWAYS);
	quad_node.render(mCamera.GetWorldToClipMatrix(), quad_node.get_transform());
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        GLStateInspection::View::Render();
        Log::View::Render();


        bool opened = ImGui::Begin("Render Time", nullptr, ImVec2(120, 50), -1.0f, 0);
        if (opened)
            ImGui::Text("%.3f ms", ddeltatime);
        ImGui::End();

        ImGui::Render();

        window->Swap();
        lastTime = nowTime;
    }

    glDeleteProgram(resolve_deferred_shader);
    resolve_deferred_shader = 0u;
    glDeleteProgram(accumulate_lights_shader);
    accumulate_lights_shader = 0u;
    glDeleteProgram(fill_shadowmap_shader);
    fill_shadowmap_shader = 0u;
    glDeleteProgram(fill_gbuffer_shader);
    fill_gbuffer_shader = 0u;
    glDeleteProgram(fallback_shader);
    fallback_shader = 0u;
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

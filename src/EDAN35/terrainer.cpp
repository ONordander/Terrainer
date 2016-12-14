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

//extern int _edge_table[256];
//extern int _edge_connections[256][20];

enum class polygon_mode_t : unsigned int {
    fill = 0u,
    line,
    point
};

static polygon_mode_t get_next_mode(polygon_mode_t mode)
{
    return static_cast<polygon_mode_t>((static_cast<unsigned int>(mode) + 1u) % 3u);
}

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
    mCamera.mWorld.SetTranslate(glm::vec3(0.0f, 2.0f, 6.0f));
    mCamera.mMouseSensitivity = 0.003f;
    mCamera.mMovementSpeed = 0.05f;
    window->SetCamera(&mCamera);

    auto const cube = parametric_shapes::create_cube(64u);
    if (cube.vao == 0u) {
        LogError("Failed to load marching cube");
        return;
    }
    float const cube_step = static_cast<float>(2.0 / 64.0);

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

    auto const light_position = glm::vec3(0.0f, 0.0f, -2.0f);
    auto const light_ambient = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f);
    auto const light_diffuse = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f);
    auto const light_specular = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f);
    auto const set_uniforms = [&light_position, &light_ambient, &light_diffuse, &light_specular, &cube_step, &mCamera](GLuint program){
        glUniform3fv(glGetUniformLocation(program, "light_position"), 1, glm::value_ptr(light_position));
        glUniform4fv(glGetUniformLocation(program, "light_ambient"), 1, glm::value_ptr(light_ambient));
        glUniform4fv(glGetUniformLocation(program, "light_diffuse"), 1, glm::value_ptr(light_diffuse));
        glUniform4fv(glGetUniformLocation(program, "light_specular"), 1, glm::value_ptr(light_specular));
        glUniform4fv(glGetUniformLocation(program, "camera_pos"), 1, glm::value_ptr(mCamera.mWorld.GetTranslation()));
    	glUniform1f(glGetUniformLocation(program, "cube_step"), cube_step);
    };

    int* edge_conn = create_edge_conn();

    GLuint edge_tex = 0u;
    glGenTextures(1, &edge_tex);
    assert(edge_tex != 0u);
    glBindTexture(GL_TEXTURE_1D, edge_tex);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_R32I, 256*20, 0, GL_RED_INTEGER, GL_INT, edge_conn);
    glBindTexture(GL_TEXTURE_1D, 0u);
    auto cube_node = Node();
    cube_node.set_geometry(cube);
    cube_node.set_program(marching_shader, set_uniforms);
    cube_node.scale(glm::vec3(5.0f, 5.0f, 5.0f));
    cube_node.add_texture("edge_tex", edge_tex, GL_TEXTURE_1D);
    auto noise_tex = eda221::loadTexture2D("noise.png");
    cube_node.add_texture("noise_tex", noise_tex, GL_TEXTURE_2D);

    //set up shader programs for the first pass
    GLuint density_program = eda221::createProgramWithGeo("TERRAINER/", "density.vert", "density.geo", "density.frag");
    //Set up el buffero
	auto const density_texture = eda221::createTexture(window_size.x, window_size.y, GL_TEXTURE_3D);
	auto const depth_texture = eda221::createTexture(window_size.x, window_size.y, GL_TEXTURE_2D, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT);
	auto const density_fbo = eda221::createFBO({density_texture}, depth_texture);
	
    float noise[32][32][32];
    for (int y = 0; y < 32; y++)
    for (int x = 0; x < 32; x++)
    for (int z = 0; z < 32; z++) {
        noise[z][y][x] = (rand() % 32768) / 32768.0;
    }
    GLuint noise_t = 0u;
    glGenTextures(1, &noise_t);
    assert(noise_t != 0u);
    glBindTexture(GL_TEXTURE_3D, noise_t);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, 32, 32, 32, 0, GL_RED, GL_FLOAT, noise);
    glBindTexture(GL_TEXTURE_3D, 0u);
    cube_node.add_texture("noise_t", noise_t, GL_TEXTURE_3D);

    //try to load the noise volumes as a 3d texture
    //auto noise_tex = eda221::load_volume_texture("packednoise_half_16cubed_mips_00.vol");
    auto seconds_nb = 0.0f;

    glEnable(GL_DEPTH_TEST);
    /*
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    */

    double ddeltatime;
    size_t fpsSamples = 0;
    double nowTime, lastTime = GetTimeMilliseconds();
    double fpsNextTick = lastTime + 1000.0;
    GLuint mode = 0u;

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
        if (inputHandler->GetKeycodeState(GLFW_KEY_L) & JUST_PRESSED) {
            mode = GL_LINE;
        }
        if (inputHandler->GetKeycodeState(GLFW_KEY_F) & JUST_PRESSED) {
            mode = GL_FILL;
        }
        glPolygonMode(GL_FRONT_AND_BACK, mode);

        auto const window_size = window->GetDimensions();
        glViewport(0, 0, window_size.x, window_size.y);
        glClearDepthf(1.0f);
        glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

	//render pass 1, build the density volume
	//glBindFramebuffer(GL_FRAMEBUFFER, density_fbo);
	GLenum const density_draw_buffers[1] = {GL_COLOR_ATTACHMENT0};
	//glDrawBuffers(1, density_draw_buffers);

        cube_node.render(mCamera.GetWorldToClipMatrix(), cube_node.get_transform());

        bool opened = ImGui::Begin("Render Time", nullptr, ImVec2(120, 50), -1.0f, 0);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
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

int*
edan35::Terrainer::create_edge_conn()
{
    static int edge_conn[256*20] = {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     0, 1, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     1, 8, 3, -1, 9, 8, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     0, 8, 3, -1, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     9, 2, 10, -1, 0, 2, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     2, 8, 3, -1, 2, 10, 8, -1, 10, 9, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     0, 11, 2, -1, 8, 11, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     1, 9, 0, -1, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     1, 11, 2, -1, 1, 9, 11, -1, 9, 8, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     3, 10, 1, -1, 11, 10, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     0, 10, 1, -1, 0, 8, 10, -1, 8, 11, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     3, 9, 0, -1, 3, 11, 9, -1, 11, 10, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     9, 8, 10, -1, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     4, 3, 0, -1, 7, 3, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     0, 1, 9, -1, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     4, 1, 9, -1, 4, 7, 1, -1, 7, 3, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     1, 2, 10, -1, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     3, 4, 7, -1, 3, 0, 4, -1, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     9, 2, 10, -1, 9, 0, 2, -1, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     2, 10, 9, -1, 2, 9, 7, -1, 2, 7, 3, -1, 7, 9, 4, -1, -1, -1, -1, -1,
     8, 4, 7, -1, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     11, 4, 7, -1, 11, 2, 4, -1, 2, 0, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     9, 0, 1, -1, 8, 4, 7, -1, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     4, 7, 11, -1, 9, 4, 11, -1, 9, 11, 2, -1, 9, 2, 1, -1, -1, -1, -1, -1,
     3, 10, 1, -1, 3, 11, 10, -1, 7, 8, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     1, 11, 10, -1, 1, 4, 11, -1, 1, 0, 4, -1, 7, 11, 4, -1, -1, -1, -1, -1,
     4, 7, 8, -1, 9, 0, 11, -1, 9, 11, 10, -1, 11, 0, 3, -1, -1, -1, -1, -1,
     4, 7, 11, -1, 4, 11, 9, -1, 9, 11, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     9, 5, 4, -1, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     0, 5, 4, -1, 1, 5, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     8, 5, 4, -1, 8, 3, 5, -1, 3, 1, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     1, 2, 10, -1, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     3, 0, 8, -1, 1, 2, 10, -1, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     5, 2, 10, -1, 5, 4, 2, -1, 4, 0, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     2, 10, 5, -1, 3, 2, 5, -1, 3, 5, 4, -1, 3, 4, 8, -1, -1, -1, -1, -1,
     9, 5, 4, -1, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     0, 11, 2, -1, 0, 8, 11, -1, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     0, 5, 4, -1, 0, 1, 5, -1, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     2, 1, 5, -1, 2, 5, 8, -1, 2, 8, 11, -1, 4, 8, 5, -1, -1, -1, -1, -1,
     10, 3, 11, -1, 10, 1, 3, -1, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     4, 9, 5, -1, 0, 8, 1, -1, 8, 10, 1, -1, 8, 11, 10, -1, -1, -1, -1, -1,
     5, 4, 0, -1, 5, 0, 11, -1, 5, 11, 10, -1, 11, 0, 3, -1, -1, -1, -1, -1,
     5, 4, 8, -1, 5, 8, 10, -1, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     9, 7, 8, -1, 5, 7, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     9, 3, 0, -1, 9, 5, 3, -1, 5, 7, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     0, 7, 8, -1, 0, 1, 7, -1, 1, 5, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     1, 5, 3, -1, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     9, 7, 8, -1, 9, 5, 7, -1, 10, 1, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     10, 1, 2, -1, 9, 5, 0, -1, 5, 3, 0, -1, 5, 7, 3, -1, -1, -1, -1, -1,
     8, 0, 2, -1, 8, 2, 5, -1, 8, 5, 7, -1, 10, 5, 2, -1, -1, -1, -1, -1,
     2, 10, 5, -1, 2, 5, 3, -1, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     7, 9, 5, -1, 7, 8, 9, -1, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     9, 5, 7, -1, 9, 7, 2, -1, 9, 2, 0, -1, 2, 7, 11, -1, -1, -1, -1, -1,
     2, 3, 11, -1, 0, 1, 8, -1, 1, 7, 8, -1, 1, 5, 7, -1, -1, -1, -1, -1,
     11, 2, 1, -1, 11, 1, 7, -1, 7, 1, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     9, 5, 8, -1, 8, 5, 7, -1, 10, 1, 3, -1, 10, 3, 11, -1, -1, -1, -1, -1,
     5, 7, 0, -1, 5, 0, 9, -1, 7, 11, 0, -1, 1, 0, 10, -1, 11, 10, 0, -1,
     11, 10, 0, -1, 11, 0, 3, -1, 10, 5, 0, -1, 8, 0, 7, -1, 5, 7, 0, -1,
     11, 10, 5, -1, 7, 11, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     0, 8, 3, -1, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     9, 0, 1, -1, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     1, 8, 3, -1, 1, 9, 8, -1, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     1, 6, 5, -1, 2, 6, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     1, 6, 5, -1, 1, 2, 6, -1, 3, 0, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     9, 6, 5, -1, 9, 0, 6, -1, 0, 2, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     5, 9, 8, -1, 5, 8, 2, -1, 5, 2, 6, -1, 3, 2, 8, -1, -1, -1, -1, -1,
     2, 3, 11, -1, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     11, 0, 8, -1, 11, 2, 0, -1, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     0, 1, 9, -1, 2, 3, 11, -1, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     5, 10, 6, -1, 1, 9, 2, -1, 9, 11, 2, -1, 9, 8, 11, -1, -1, -1, -1, -1,
     6, 3, 11, -1, 6, 5, 3, -1, 5, 1, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     0, 8, 11, -1, 0, 11, 5, -1, 0, 5, 1, -1, 5, 11, 6, -1, -1, -1, -1, -1,
     3, 11, 6, -1, 0, 3, 6, -1, 0, 6, 5, -1, 0, 5, 9, -1, -1, -1, -1, -1,
     6, 5, 9, -1, 6, 9, 11, -1, 11, 9, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     5, 10, 6, -1, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     4, 3, 0, -1, 4, 7, 3, -1, 6, 5, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     1, 9, 0, -1, 5, 10, 6, -1, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     10, 6, 5, -1, 1, 9, 7, -1, 1, 7, 3, -1, 7, 9, 4, -1, -1, -1, -1, -1,
     6, 1, 2, -1, 6, 5, 1, -1, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     1, 2, 5, -1, 5, 2, 6, -1, 3, 0, 4, -1, 3, 4, 7, -1, -1, -1, -1, -1,
     8, 4, 7, -1, 9, 0, 5, -1, 0, 6, 5, -1, 0, 2, 6, -1, -1, -1, -1, -1,
     7, 3, 9, -1, 7, 9, 4, -1, 3, 2, 9, -1, 5, 9, 6, -1, 2, 6, 9, -1,
     3, 11, 2, -1, 7, 8, 4, -1, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     5, 10, 6, -1, 4, 7, 2, -1, 4, 2, 0, -1, 2, 7, 11, -1, -1, -1, -1, -1,
     0, 1, 9, -1, 4, 7, 8, -1, 2, 3, 11, -1, 5, 10, 6, -1, -1, -1, -1, -1,
     9, 2, 1, -1, 9, 11, 2, -1, 9, 4, 11, -1, 7, 11, 4, -1, 5, 10, 6, -1,
     8, 4, 7, -1, 3, 11, 5, -1, 3, 5, 1, -1, 5, 11, 6, -1, -1, -1, -1, -1,
     5, 1, 11, -1, 5, 11, 6, -1, 1, 0, 11, -1, 7, 11, 4, -1, 0, 4, 11, -1,
     0, 5, 9, -1, 0, 6, 5, -1, 0, 3, 6, -1, 11, 6, 3, -1, 8, 4, 7, -1,
     6, 5, 9, -1, 6, 9, 11, -1, 4, 7, 9, -1, 7, 11, 9, -1, -1, -1, -1, -1,
     10, 4, 9, -1, 6, 4, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     4, 10, 6, -1, 4, 9, 10, -1, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     10, 0, 1, -1, 10, 6, 0, -1, 6, 4, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     8, 3, 1, -1, 8, 1, 6, -1, 8, 6, 4, -1, 6, 1, 10, -1, -1, -1, -1, -1,
     1, 4, 9, -1, 1, 2, 4, -1, 2, 6, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     3, 0, 8, -1, 1, 2, 9, -1, 2, 4, 9, -1, 2, 6, 4, -1, -1, -1, -1, -1,
     0, 2, 4, -1, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     8, 3, 2, -1, 8, 2, 4, -1, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     10, 4, 9, -1, 10, 6, 4, -1, 11, 2, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     0, 8, 2, -1, 2, 8, 11, -1, 4, 9, 10, -1, 4, 10, 6, -1, -1, -1, -1, -1,
     3, 11, 2, -1, 0, 1, 6, -1, 0, 6, 4, -1, 6, 1, 10, -1, -1, -1, -1, -1,
     6, 4, 1, -1, 6, 1, 10, -1, 4, 8, 1, -1, 2, 1, 11, -1, 8, 11, 1, -1,
     9, 6, 4, -1, 9, 3, 6, -1, 9, 1, 3, -1, 11, 6, 3, -1, -1, -1, -1, -1,
     8, 11, 1, -1, 8, 1, 0, -1, 11, 6, 1, -1, 9, 1, 4, -1, 6, 4, 1, -1,
     3, 11, 6, -1, 3, 6, 0, -1, 0, 6, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     6, 4, 8, -1, 11, 6, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     7, 10, 6, -1, 7, 8, 10, -1, 8, 9, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     0, 7, 3, -1, 0, 10, 7, -1, 0, 9, 10, -1, 6, 7, 10, -1, -1, -1, -1, -1,
     10, 6, 7, -1, 1, 10, 7, -1, 1, 7, 8, -1, 1, 8, 0, -1, -1, -1, -1, -1,
     10, 6, 7, -1, 10, 7, 1, -1, 1, 7, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     1, 2, 6, -1, 1, 6, 8, -1, 1, 8, 9, -1, 8, 6, 7, -1, -1, -1, -1, -1,
     2, 6, 9, -1, 2, 9, 1, -1, 6, 7, 9, -1, 0, 9, 3, -1, 7, 3, 9, -1,
     7, 8, 0, -1, 7, 0, 6, -1, 6, 0, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     7, 3, 2, -1, 6, 7, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     2, 3, 11, -1, 10, 6, 8, -1, 10, 8, 9, -1, 8, 6, 7, -1, -1, -1, -1, -1,
     2, 0, 7, -1, 2, 7, 11, -1, 0, 9, 7, -1, 6, 7, 10, -1, 9, 10, 7, -1,
     1, 8, 0, -1, 1, 7, 8, -1, 1, 10, 7, -1, 6, 7, 10, -1, 2, 3, 11, -1,
     11, 2, 1, -1, 11, 1, 7, -1, 10, 6, 1, -1, 6, 7, 1, -1, -1, -1, -1, -1,
     8, 9, 6, -1, 8, 6, 7, -1, 9, 1, 6, -1, 11, 6, 3, -1, 1, 3, 6, -1,
     0, 9, 1, -1, 11, 6, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     7, 8, 0, -1, 7, 0, 6, -1, 3, 11, 0, -1, 11, 6, 0, -1, -1, -1, -1, -1,
     7, 11, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     3, 0, 8, -1, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     0, 1, 9, -1, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     8, 1, 9, -1, 8, 3, 1, -1, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     10, 1, 2, -1, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     1, 2, 10, -1, 3, 0, 8, -1, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     2, 9, 0, -1, 2, 10, 9, -1, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     6, 11, 7, -1, 2, 10, 3, -1, 10, 8, 3, -1, 10, 9, 8, -1, -1, -1, -1, -1,
     7, 2, 3, -1, 6, 2, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     7, 0, 8, -1, 7, 6, 0, -1, 6, 2, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     2, 7, 6, -1, 2, 3, 7, -1, 0, 1, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     1, 6, 2, -1, 1, 8, 6, -1, 1, 9, 8, -1, 8, 7, 6, -1, -1, -1, -1, -1,
     10, 7, 6, -1, 10, 1, 7, -1, 1, 3, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     10, 7, 6, -1, 1, 7, 10, -1, 1, 8, 7, -1, 1, 0, 8, -1, -1, -1, -1, -1,
     0, 3, 7, -1, 0, 7, 10, -1, 0, 10, 9, -1, 6, 10, 7, -1, -1, -1, -1, -1,
     7, 6, 10, -1, 7, 10, 8, -1, 8, 10, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     6, 8, 4, -1, 11, 8, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     3, 6, 11, -1, 3, 0, 6, -1, 0, 4, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     8, 6, 11, -1, 8, 4, 6, -1, 9, 0, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     9, 4, 6, -1, 9, 6, 3, -1, 9, 3, 1, -1, 11, 3, 6, -1, -1, -1, -1, -1,
     6, 8, 4, -1, 6, 11, 8, -1, 2, 10, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     1, 2, 10, -1, 3, 0, 11, -1, 0, 6, 11, -1, 0, 4, 6, -1, -1, -1, -1, -1,
     4, 11, 8, -1, 4, 6, 11, -1, 0, 2, 9, -1, 2, 10, 9, -1, -1, -1, -1, -1,
     10, 9, 3, -1, 10, 3, 2, -1, 9, 4, 3, -1, 11, 3, 6, -1, 4, 6, 3, -1,
     8, 2, 3, -1, 8, 4, 2, -1, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     0, 4, 2, -1, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     1, 9, 0, -1, 2, 3, 4, -1, 2, 4, 6, -1, 4, 3, 8, -1, -1, -1, -1, -1,
     1, 9, 4, -1, 1, 4, 2, -1, 2, 4, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     8, 1, 3, -1, 8, 6, 1, -1, 8, 4, 6, -1, 6, 10, 1, -1, -1, -1, -1, -1,
     10, 1, 0, -1, 10, 0, 6, -1, 6, 0, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     4, 6, 3, -1, 4, 3, 8, -1, 6, 10, 3, -1, 0, 3, 9, -1, 10, 9, 3, -1,
     10, 9, 4, -1, 6, 10, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     4, 9, 5, -1, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     0, 8, 3, -1, 4, 9, 5, -1, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     5, 0, 1, -1, 5, 4, 0, -1, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     11, 7, 6, -1, 8, 3, 4, -1, 3, 5, 4, -1, 3, 1, 5, -1, -1, -1, -1, -1,
     9, 5, 4, -1, 10, 1, 2, -1, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     6, 11, 7, -1, 1, 2, 10, -1, 0, 8, 3, -1, 4, 9, 5, -1, -1, -1, -1, -1,
     7, 6, 11, -1, 5, 4, 10, -1, 4, 2, 10, -1, 4, 0, 2, -1, -1, -1, -1, -1,
     3, 4, 8, -1, 3, 5, 4, -1, 3, 2, 5, -1, 10, 5, 2, -1, 11, 7, 6, -1,
     7, 2, 3, -1, 7, 6, 2, -1, 5, 4, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     9, 5, 4, -1, 0, 8, 6, -1, 0, 6, 2, -1, 6, 8, 7, -1, -1, -1, -1, -1,
     3, 6, 2, -1, 3, 7, 6, -1, 1, 5, 0, -1, 5, 4, 0, -1, -1, -1, -1, -1,
     6, 2, 8, -1, 6, 8, 7, -1, 2, 1, 8, -1, 4, 8, 5, -1, 1, 5, 8, -1,
     9, 5, 4, -1, 10, 1, 6, -1, 1, 7, 6, -1, 1, 3, 7, -1, -1, -1, -1, -1,
     1, 6, 10, -1, 1, 7, 6, -1, 1, 0, 7, -1, 8, 7, 0, -1, 9, 5, 4, -1,
     4, 0, 10, -1, 4, 10, 5, -1, 0, 3, 10, -1, 6, 10, 7, -1, 3, 7, 10, -1,
     7, 6, 10, -1, 7, 10, 8, -1, 5, 4, 10, -1, 4, 8, 10, -1, -1, -1, -1, -1,
     6, 9, 5, -1, 6, 11, 9, -1, 11, 8, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     3, 6, 11, -1, 0, 6, 3, -1, 0, 5, 6, -1, 0, 9, 5, -1, -1, -1, -1, -1,
     0, 11, 8, -1, 0, 5, 11, -1, 0, 1, 5, -1, 5, 6, 11, -1, -1, -1, -1, -1,
     6, 11, 3, -1, 6, 3, 5, -1, 5, 3, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     1, 2, 10, -1, 9, 5, 11, -1, 9, 11, 8, -1, 11, 5, 6, -1, -1, -1, -1, -1,
     0, 11, 3, -1, 0, 6, 11, -1, 0, 9, 6, -1, 5, 6, 9, -1, 1, 2, 10, -1,
     11, 8, 5, -1, 11, 5, 6, -1, 8, 0, 5, -1, 10, 5, 2, -1, 0, 2, 5, -1,
     6, 11, 3, -1, 6, 3, 5, -1, 2, 10, 3, -1, 10, 5, 3, -1, -1, -1, -1, -1,
     5, 8, 9, -1, 5, 2, 8, -1, 5, 6, 2, -1, 3, 8, 2, -1, -1, -1, -1, -1,
     9, 5, 6, -1, 9, 6, 0, -1, 0, 6, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     1, 5, 8, -1, 1, 8, 0, -1, 5, 6, 8, -1, 3, 8, 2, -1, 6, 2, 8, -1,
     1, 5, 6, -1, 2, 1, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     1, 3, 6, -1, 1, 6, 10, -1, 3, 8, 6, -1, 5, 6, 9, -1, 8, 9, 6, -1,
     10, 1, 0, -1, 10, 0, 6, -1, 9, 5, 0, -1, 5, 6, 0, -1, -1, -1, -1, -1,
     0, 3, 8, -1, 5, 6, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     10, 5, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     11, 5, 10, -1, 7, 5, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     11, 5, 10, -1, 11, 7, 5, -1, 8, 3, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     5, 11, 7, -1, 5, 10, 11, -1, 1, 9, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     10, 7, 5, -1, 10, 11, 7, -1, 9, 8, 1, -1, 8, 3, 1, -1, -1, -1, -1, -1,
     11, 1, 2, -1, 11, 7, 1, -1, 7, 5, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     0, 8, 3, -1, 1, 2, 7, -1, 1, 7, 5, -1, 7, 2, 11, -1, -1, -1, -1, -1,
     9, 7, 5, -1, 9, 2, 7, -1, 9, 0, 2, -1, 2, 11, 7, -1, -1, -1, -1, -1,
     7, 5, 2, -1, 7, 2, 11, -1, 5, 9, 2, -1, 3, 2, 8, -1, 9, 8, 2, -1,
     2, 5, 10, -1, 2, 3, 5, -1, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     8, 2, 0, -1, 8, 5, 2, -1, 8, 7, 5, -1, 10, 2, 5, -1, -1, -1, -1, -1,
     9, 0, 1, -1, 5, 10, 3, -1, 5, 3, 7, -1, 3, 10, 2, -1, -1, -1, -1, -1,
     9, 8, 2, -1, 9, 2, 1, -1, 8, 7, 2, -1, 10, 2, 5, -1, 7, 5, 2, -1,
     1, 3, 5, -1, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     0, 8, 7, -1, 0, 7, 1, -1, 1, 7, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     9, 0, 3, -1, 9, 3, 5, -1, 5, 3, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     9, 8, 7, -1, 5, 9, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     5, 8, 4, -1, 5, 10, 8, -1, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     5, 0, 4, -1, 5, 11, 0, -1, 5, 10, 11, -1, 11, 3, 0, -1, -1, -1, -1, -1,
     0, 1, 9, -1, 8, 4, 10, -1, 8, 10, 11, -1, 10, 4, 5, -1, -1, -1, -1, -1,
     10, 11, 4, -1, 10, 4, 5, -1, 11, 3, 4, -1, 9, 4, 1, -1, 3, 1, 4, -1,
     2, 5, 1, -1, 2, 8, 5, -1, 2, 11, 8, -1, 4, 5, 8, -1, -1, -1, -1, -1,
     0, 4, 11, -1, 0, 11, 3, -1, 4, 5, 11, -1, 2, 11, 1, -1, 5, 1, 11, -1,
     0, 2, 5, -1, 0, 5, 9, -1, 2, 11, 5, -1, 4, 5, 8, -1, 11, 8, 5, -1,
     9, 4, 5, -1, 2, 11, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     2, 5, 10, -1, 3, 5, 2, -1, 3, 4, 5, -1, 3, 8, 4, -1, -1, -1, -1, -1,
     5, 10, 2, -1, 5, 2, 4, -1, 4, 2, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     3, 10, 2, -1, 3, 5, 10, -1, 3, 8, 5, -1, 4, 5, 8, -1, 0, 1, 9, -1,
     5, 10, 2, -1, 5, 2, 4, -1, 1, 9, 2, -1, 9, 4, 2, -1, -1, -1, -1, -1,
     8, 4, 5, -1, 8, 5, 3, -1, 3, 5, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     0, 4, 5, -1, 1, 0, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     8, 4, 5, -1, 8, 5, 3, -1, 9, 0, 5, -1, 0, 3, 5, -1, -1, -1, -1, -1,
     9, 4, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     4, 11, 7, -1, 4, 9, 11, -1, 9, 10, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     0, 8, 3, -1, 4, 9, 7, -1, 9, 11, 7, -1, 9, 10, 11, -1, -1, -1, -1, -1,
     1, 10, 11, -1, 1, 11, 4, -1, 1, 4, 0, -1, 7, 4, 11, -1, -1, -1, -1, -1,
     3, 1, 4, -1, 3, 4, 8, -1, 1, 10, 4, -1, 7, 4, 11, -1, 10, 11, 4, -1,
     4, 11, 7, -1, 9, 11, 4, -1, 9, 2, 11, -1, 9, 1, 2, -1, -1, -1, -1, -1,
     9, 7, 4, -1, 9, 11, 7, -1, 9, 1, 11, -1, 2, 11, 1, -1, 0, 8, 3, -1,
     11, 7, 4, -1, 11, 4, 2, -1, 2, 4, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     11, 7, 4, -1, 11, 4, 2, -1, 8, 3, 4, -1, 3, 2, 4, -1, -1, -1, -1, -1,
     2, 9, 10, -1, 2, 7, 9, -1, 2, 3, 7, -1, 7, 4, 9, -1, -1, -1, -1, -1,
     9, 10, 7, -1, 9, 7, 4, -1, 10, 2, 7, -1, 8, 7, 0, -1, 2, 0, 7, -1,
     3, 7, 10, -1, 3, 10, 2, -1, 7, 4, 10, -1, 1, 10, 0, -1, 4, 0, 10, -1,
     1, 10, 2, -1, 8, 7, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     4, 9, 1, -1, 4, 1, 7, -1, 7, 1, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     4, 9, 1, -1, 4, 1, 7, -1, 0, 8, 1, -1, 8, 7, 1, -1, -1, -1, -1, -1,
     4, 0, 3, -1, 7, 4, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     4, 8, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     9, 10, 8, -1, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     3, 0, 9, -1, 3, 9, 11, -1, 11, 9, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     0, 1, 10, -1, 0, 10, 8, -1, 8, 10, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     3, 1, 10, -1, 11, 3, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     1, 2, 11, -1, 1, 11, 9, -1, 9, 11, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     3, 0, 9, -1, 3, 9, 11, -1, 1, 2, 9, -1, 2, 11, 9, -1, -1, -1, -1, -1,
     0, 2, 11, -1, 8, 0, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     3, 2, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     2, 3, 8, -1, 2, 8, 10, -1, 10, 8, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     9, 10, 2, -1, 0, 9, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     2, 3, 8, -1, 2, 8, 10, -1, 0, 1, 8, -1, 1, 10, 8, -1, -1, -1, -1, -1,
     1, 10, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     1, 3, 8, -1, 9, 1, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     0, 9, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     0, 3, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
    };

    return edge_conn;
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

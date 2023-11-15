#include <windows.h>

// Imgui headers for UI
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>

#include "DebugCallback.h"
#include "InitShader.h"    // Functions for loading shaders from text files
#include "Camera.h"

#include <chrono>

namespace window
{
    // Some VS specific code
    // const char* const title = USER_NAME " " PROJECT_NAME; //defined in project settings
    int size[2] = {800, 600};
    float clear_color[4] = {0.35f, 0.35f, 0.35f, 0.0f};
}

namespace scene
{
    Camera camera;
    float fov = 90.0f;

    float angle = glm::pi<float>() / 2.0;
    float scale = 1.0f;
   
    const std::string shader_dir = "shaders/";
    const std::string vertex_shader("fractals_vs.glsl");
    const std::string fragment_shader("fractals_fs.glsl");

    float yaw = -90.f;
    float pitch = 0.f;

    GLuint shader = -1;
    GLuint textureID = -1;

    int color_palette = 0;

    glm::vec3 color1 = glm::vec3(0.0, 0.0, 1.0); // Blue
    glm::vec3 color2 = glm::vec3(0.0, 1.0, 1.0); // Cyan
    glm::vec3 color3 = glm::vec3(0.0, 1.0, 0.0); // Green
    glm::vec3 color4 = glm::vec3(1.0, 1.0, 0.0); // Yellow
    glm::vec3 color5 = glm::vec3(1.0, 0.0, 0.0); // Red
}

namespace mouse
{
    bool altPressed = false;

    double xlast;
    double ylast;
    double xoffset;
    double yoffset;

    float sensitivity = 0.1f;
}

namespace grid
{
    int resolution = 128;
    //std::vector <float> points(resolution* resolution* resolution *4);
    std::vector <float> points;
    std::vector <double> density;
    unsigned int points_vbo = -1;
    unsigned int points_vao = -1;
    unsigned int density_vbo = -1;
    unsigned int density_vao = -1;

    const int pos_loc = 0;
    const int density_loc = 1;

    float dim = 2.0;
    float res = 64.0;
    float point_size = 3.0;

    float order = 8.f;
    int max_iterations = 10;
    float step_size = 0.05;

    int fractal_type = 0;
}

int MandelbulbIteration(glm::vec3 point, float order, int maxIterations) {
    glm::vec3 z = point;
    float dr = 1.0;
    float r = 0.0;
    int i = 0;
    for (i = 0; i < maxIterations; i++) {
        r = glm::length(z);
        if (r > 2.0)
        {
            //std::cout << i << std::endl;
            break; // Escape condition
        }
        // Convert to polar coordinates
        float theta = acos(z.z / r);
        float phi = atan2(z.y, z.x);
        dr = pow(r, order - 1.0) * order * dr + 1.0;

        // Scale and rotate the point
        float zr = pow(r, order);
        theta *= order;
        phi *= order;

        // Convert back to Cartesian coordinates
        z = glm::vec3(sin(theta) * cos(phi), sin(phi) * sin(theta), cos(theta)) * zr + point;
    }
    return i;
}

// Grid of voxels
void init_grid()
{
    grid::points.push_back(-1.0);
    grid::points.push_back(1.0);
    grid::points.push_back(0.0);

    grid::points.push_back(-1.0);
    grid::points.push_back(-1.0);
    grid::points.push_back(0.0);

    grid::points.push_back(1.0);
    grid::points.push_back(1.0);
    grid::points.push_back(0.0);

    grid::points.push_back(1.0);
    grid::points.push_back(-1.0);
    grid::points.push_back(0.0);

    glBindAttribLocation(scene::shader, grid::pos_loc, "pos_attrib");

    glGenVertexArrays(1, &grid::points_vao);
    glBindVertexArray(grid::points_vao);

    glGenBuffers(1, &grid::points_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, grid::points_vbo);
    glBufferData(GL_ARRAY_BUFFER, grid::points.size() * sizeof(float), &grid::points[0], GL_STATIC_DRAW);

    glEnableVertexAttribArray(grid::pos_loc);
    glVertexAttribPointer(grid::pos_loc, 3, GL_FLOAT, 1, 0, 0);
}

void init_voxels()
{
    int gridSize = 512;
    std::ifstream inFile("../cache/voxels_512_density.bin", std::ios::binary);
    std::vector<float> densityData(gridSize * gridSize * gridSize);
    inFile.read(reinterpret_cast<char*>(densityData.data()), densityData.size() * sizeof(float));

    if (!inFile.is_open()) {
        std::cerr << "Failed to open the file" << std::endl;
        return;
    }

    glBindAttribLocation(scene::shader, grid::pos_loc, "pos_attrib");

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_3D, textureID);
    // set the texture wrapping/filtering options (on the currently bound texture object)
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);


    glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, gridSize, gridSize, gridSize, 0, GL_RED, GL_FLOAT, densityData.data());
}

void color_palettes(int paletteNum)
{
    switch (paletteNum)
    {
        case 0:
        {
            scene::color1 = glm::vec3(0.0, 0.0, 1.0); // Blue
            scene::color2 = glm::vec3(0.0, 1.0, 1.0); // Cyan
            scene::color3 = glm::vec3(0.0, 1.0, 0.0); // Green
            scene::color4 = glm::vec3(1.0, 1.0, 0.0); // Yellow
            scene::color5 = glm::vec3(1.0, 0.0, 0.0); // Red
            break;
        }
        case 1:
        {
            scene::color1 = glm::vec3(0.969, 0.145, 0.522);
            scene::color2 = glm::vec3(0.447, 0.035, 0.718);
            scene::color3 = glm::vec3(0.227, 0.047, 0.064);
            scene::color4 = glm::vec3(0.263, 0.380, 0.933);
            scene::color5 = glm::vec3(0.298, 0.788, 0.941);
            break;
        }
        case 2:
        {
            scene::color1 = glm::vec3(0.051, 0.106, 0.165);
            scene::color2 = glm::vec3(0.106, 0.149, 0.231);
            scene::color3 = glm::vec3(0.255, 0.353, 0.467);
            scene::color4 = glm::vec3(0.467, 0.553, 0.663);
            scene::color5 = glm::vec3(0.878, 0.882, 0.867);
            break;
        }
        default:
        {
            break;
        }
    }
}

// Draw the ImGui user interface
void draw_gui(GLFWwindow* window)
{
    // Begin ImGui Frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Draw Gui
    /*ImGui::Begin("Debug window");
    if (ImGui::Button("Quit"))
    {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::Text("Camera Position: (%.3f, %.3f, %.3f)", scene::camera.position().x, scene::camera.position().y, scene::camera.position().z);*/
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::Separator();
    if (ImGui::RadioButton("Color Palette 1", &scene::color_palette, 0)) color_palettes(scene::color_palette);
    if (ImGui::RadioButton("Color Palette 2", &scene::color_palette, 1)) color_palettes(scene::color_palette);
    if (ImGui::RadioButton("Color Palette 3", &scene::color_palette, 2)) color_palettes(scene::color_palette);
    ImGui::ColorEdit3("Color 1", glm::value_ptr(scene::color1));
    ImGui::ColorEdit3("Color 2", glm::value_ptr(scene::color2));
    ImGui::ColorEdit3("Color 3", glm::value_ptr(scene::color3));
    ImGui::ColorEdit3("Color 4", glm::value_ptr(scene::color4));
    ImGui::ColorEdit3("Color 5", glm::value_ptr(scene::color5));
    //ImGui::RadioButton("Mandelbulb", &grid::fractal_type, 0);
    //ImGui::RadioButton("Mandelbox", &grid::fractal_type, 1);
    //ImGui::RadioButton("Menger Sponge", &grid::fractal_type, 2);
    //ImGui::SliderFloat("Order", &grid::order, 1.0, 16.0);
    //ImGui::SliderFloat("Step Size", &grid::step_size, 0.0001, 1.0);
    //ImGui::SliderInt("Max Iterations", &grid::max_iterations, 1, 100);
    //ImGui::SliderFloat("FOV (degrees)", &scene::fov, 1.0, 179.0);
    //ImGui::SliderFloat("Point Size", &grid::point_size, 1.0, 10.0);
    ImGui::End();

    // End ImGui Frame
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}


// This function gets called every time the scene gets redisplayed
void display(GLFWwindow* window)
{
    // Clear the screen to the color previously specified in the glClearColor(...) call.
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::mat4 M = glm::rotate(scene::angle, glm::vec3(0.0f, 0.0f, 1.0f));
    glm::mat4 V = glm::lookAt(scene::camera.position(), scene::camera.front(), scene::camera.up());
    glm::mat4 P = glm::perspective(glm::pi<float>()/2.0f * (scene::fov / 90.0f), (float)window::size[0] / (float)window::size[1], 0.1f, 1000.0f);
 
    glViewport(0, 0, window::size[0], window::size[1]);
 
    glUseProgram(scene::shader);
 
    // Get location for shader uniform variable
    int PVM_loc = glGetUniformLocation(scene::shader, "PVM");
    if (PVM_loc != -1)
    {
       glm::mat4 PVM = P * V * M;
       glUniformMatrix4fv(PVM_loc, 1, false, glm::value_ptr(PVM));
    }
    int P_loc = glGetUniformLocation(scene::shader, "P");
    if (P_loc != -1)
    {
        glUniformMatrix4fv(P_loc, 1, false, glm::value_ptr(P));
    }
    int V_loc = glGetUniformLocation(scene::shader, "V");
    if (V_loc != -1)
    {
        glUniformMatrix4fv(V_loc, 1, false, glm::value_ptr(V));
    }
    int order_loc = glGetUniformLocation(scene::shader, "order");
    if (order_loc != -1)
    {
        glUniform1f(order_loc, grid::order);
    }
    int max_iterations_loc = glGetUniformLocation(scene::shader, "max_iterations");
    if (max_iterations_loc != -1)
    {
        glUniform1i(max_iterations_loc, grid::max_iterations);
    }
    int point_size_loc = glGetUniformLocation(scene::shader, "point_size");
    if (point_size_loc != -1)
    {
        glUniform1f(point_size_loc, grid::point_size);
    }
    int fractal_type_loc = glGetUniformLocation(scene::shader, "fractal_type");
    if (fractal_type_loc != -1)
    {
        glUniform1i(fractal_type_loc, grid::fractal_type);
    }
    int step_size_loc = glGetUniformLocation(scene::shader, "step_size");
    if (step_size_loc != -1)
    {
        glUniform1f(step_size_loc, grid::step_size);
    }
    int width_loc = glGetUniformLocation(scene::shader, "window_width");
    if (width_loc != -1)
    {
        glUniform1i(width_loc, window::size[0]);
    }
    int height_loc = glGetUniformLocation(scene::shader, "window_height");
    if (height_loc != -1)
    {
        glUniform1i(height_loc, window::size[1]);
    }

    int cam_pos_loc = glGetUniformLocation(scene::shader, "cam_pos");
    if (cam_pos_loc != -1)
    {
        glUniform3fv(cam_pos_loc, 1, glm::value_ptr(scene::camera.position()));
    }

    int color1_loc = glGetUniformLocation(scene::shader, "color1");
    if (color1_loc != -1)
    {
        glUniform3fv(color1_loc, 1, glm::value_ptr(scene::color1));
    }
    int color2_loc = glGetUniformLocation(scene::shader, "color2");
    if (color2_loc != -1)
    {
        glUniform3fv(color2_loc, 1, glm::value_ptr(scene::color2));
    }
    int color3_loc = glGetUniformLocation(scene::shader, "color3");
    if (color3_loc != -1)
    {
        glUniform3fv(color3_loc, 1, glm::value_ptr(scene::color3));
    }
    int color4_loc = glGetUniformLocation(scene::shader, "color4");
    if (color4_loc != -1)
    {
        glUniform3fv(color4_loc, 1, glm::value_ptr(scene::color4));
    }
    int color5_loc = glGetUniformLocation(scene::shader, "color5");
    if (color5_loc != -1)
    {
        glUniform3fv(color5_loc, 1, glm::value_ptr(scene::color5));
    }


    glBindVertexArray(grid::points_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, grid::points.size() / 3);

    draw_gui(window);

    // Swap front and back buffers
    glfwSwapBuffers(window);
}

void idle()
{
    float time_sec = static_cast<float>(glfwGetTime());

    // Pass time_sec value to the shaders
    int time_loc = glGetUniformLocation(scene::shader, "time");
    if (time_loc != -1)
    {
        glUniform1f(time_loc, time_sec);
    }
}

void reload_shader()
{
    std::string vs = scene::shader_dir + scene::vertex_shader;
    std::string fs = scene::shader_dir + scene::fragment_shader;
 
    GLuint new_shader = InitShader(vs.c_str(), fs.c_str());
 
    if (new_shader == -1) // loading failed
    {
        glClearColor(1.0f, 0.0f, 1.0f, 0.0f); // change clear color if shader can't be compiled
    }
    else
    {
       glClearColor(window::clear_color[0], window::clear_color[1], window::clear_color[2], window::clear_color[3]);
 
       if (scene::shader != -1)
       {
           glDeleteProgram(scene::shader);
       }
       scene::shader = new_shader;
    }
}

// This function gets called when a key is pressed
void keyboard(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    // std::cout << "key : " << key << ", " << char(key) << ", scancode: " << scancode << ", action: " << action << ", mods: " << mods << std::endl;

    if (action == GLFW_PRESS)
    {
        switch (key)
        {
        case 'r':
        case 'R':
            reload_shader();
            break;

        case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            break;
        case GLFW_KEY_LEFT_ALT:
            mouse::altPressed = true;
            break;
        case GLFW_KEY_RIGHT_ALT:
            mouse::altPressed = true;
            break;
        }
    }

    if (action == GLFW_RELEASE)
    {
        switch (key)
        {
        case GLFW_KEY_LEFT_ALT:
            mouse::altPressed = false;
            break;
        case GLFW_KEY_RIGHT_ALT:
            mouse::altPressed = false;
            break;
        }
    }
}

void orbit_camera()
{
    scene::yaw += mouse::xoffset;
    scene::pitch += mouse::yoffset;

    scene::camera.orbit(scene::yaw, scene::pitch);
}

void pan_camera()
{
    float translateSensitivity = 0.02;

    scene::camera.pan((float)mouse::xoffset * -translateSensitivity, (float)mouse::yoffset * translateSensitivity);
}

void zoom_camera()
{
    scene::camera.zoom((float)mouse::yoffset * 0.01f);
}

void cursor_offset(double xcurrent, double ycurrent)
{
    mouse::xoffset = xcurrent - mouse::xlast;
    mouse::yoffset = ycurrent - mouse::ylast;
    mouse::xlast = xcurrent;
    mouse::ylast = ycurrent;

    mouse::xoffset *= mouse::sensitivity;
    mouse::yoffset *= mouse::sensitivity;
}

// This function gets called when the mouse moves over the window.
void mouse_cursor(GLFWwindow* window, double x, double y)
{
    if (mouse::altPressed)
    {
        cursor_offset(x, y);

        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) orbit_camera();
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS) pan_camera();
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) zoom_camera();
    }
}

// This function gets called when a mouse button is pressed.
void mouse_button(GLFWwindow* window, int button, int action, int mods)
{
    // std::cout << "button : " << button << ", action: " << action << ", mods: " << mods << std::endl;
    if (action == GLFW_PRESS) glfwGetCursorPos(window, &mouse::xlast, &mouse::ylast);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    scene::camera.zoom((float)yoffset * -0.1f);
}

void window_size(GLFWwindow* window, int width, int height)
{
    window::size[0] = width;
    window::size[1] = height;
}

//Initialize OpenGL state. This function only gets called once.
void init()
{
    glewInit();
    RegisterDebugCallback();

    // I Think this is Windows specific code
    // std::ostringstream oss;
    // //Get information about the OpenGL version supported by the graphics driver.	
    // oss << "Vendor: "       << glGetString(GL_VENDOR)                    << std::endl;
    // oss << "Renderer: "     << glGetString(GL_RENDERER)                  << std::endl;
    // oss << "Version: "      << glGetString(GL_VERSION)                   << std::endl;
    // oss << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION)  << std::endl;

    // Output info to console
    // std::cout << oss.str();

    // Output info to file
    // std::fstream info_file("info.txt", std::ios::out | std::ios::trunc);
    // info_file << oss.str();
    // info_file.close();

    reload_shader();

    init_grid();
    init_voxels();

    // Set the color the screen will be cleared to when glClear is called
    glClearColor(window::clear_color[0], window::clear_color[1], window::clear_color[2], window::clear_color[3]);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glDisable(GL_CULL_FACE);
}


// C++ programs start executing in the main() function.
int main(void)
{
    GLFWwindow* window;

    // Initialize the library
    if (!glfwInit())
    {
        return -1;
    }

#ifdef _DEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#endif

    // Create a windowed mode window and its OpenGL context
    window = glfwCreateWindow(window::size[0], window::size[1], "Fractals", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    //Register callback functions with glfw. 
    glfwSetKeyCallback(window, keyboard);
    glfwSetCursorPosCallback(window, mouse_cursor);
    glfwSetMouseButtonCallback(window, mouse_button);
    glfwSetWindowSizeCallback(window, window_size);

    // Make the window's context current
    glfwMakeContextCurrent(window);

    init();

    // New in Lab 2: Init ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 150");

    // Loop until the user closes the window 
    while (!glfwWindowShouldClose(window))
    {
        idle();
        display(window);

        // Poll for and process events 
        glfwPollEvents();
    }

    // New in Lab 2: Cleanup ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
 
    glfwTerminate();
    return 0;
}
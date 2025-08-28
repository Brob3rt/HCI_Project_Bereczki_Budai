


#include <iostream>
#include <opencv2/opencv.hpp>// OpenGL Extension Wrangler: allow all multiplatform GL functions
#include <GL/glew.h> // WGLEW = Windows GL Extension Wrangler (change for different platform) platform specific functions (in this case Windows)
#include <GL/wglew.h> // GLFW toolkit. Uses GL calls to open GL context, i.e. GLEW must be first.
#include <GLFW/glfw3.h> // OpenGL math
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <unordered_map>
#include <chrono>
#include <irrKlang/irrKlang.h>
#include <cstdlib>  // For rand() and srand()
#include <ctime>    // For time()

#include "assets.hpp"
#include "app.hpp"
#include "gl_err_callback.h"    //Included for error checking of glew, wglew and glfw3
#include "ShaderProgram.hpp"    // compiles shaders, defines setUniform functions
#include "Mesh.hpp"             // Create and initialize VAO, VBO, EBO and parameters. 
#include "Model.hpp"            //creates model from on .obj file using given shaders and calls draw mesh function. The update of the model matrix (translation, rotation and schaling of the loaded model in view space) also happens here.
#include "camera.hpp"           // handles the movement of the camera (by updating he view matrix)
#include "Heightmap.hpp"
#include "FaceTracker.hpp"

//---------------------------------------------------------------------



App::App()
{
    //------ default constructor ------
    std::cout << "Constructed...\n";
}


bool App::init()
{
    //------ Set Error Callback ------ 
    glfwSetErrorCallback(error_callback);

    //------ Initialize the library ------
    if (!glfwInit())
        return -1;

    //------ Set the application to use core profile version 4.6 ------
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    //------ Create a windowed mode window and its OpenGL context ------
    window = glfwCreateWindow(640, 480, "Prototype app", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    glfwSetWindowUserPointer(window, this);
    glfwSetKeyCallback(window, key_callback);

    //------ Make the window's context current ------
    glfwMakeContextCurrent(window);

    //------ Initialise GLEW and WGLEW with error checking ------
    GLenum glew_ret;
    glew_ret = glewInit();
    if (glew_ret != GLEW_OK) {
        throw std::runtime_error(std::string("GLEW failed with error: ")
            + reinterpret_cast<const char*>(glewGetErrorString(glew_ret)));
    }
    else {
        std::cout << "GLEW successfully initialized to version: " << glewGetString(GLEW_VERSION) << std::endl;
    }

    glew_ret = wglewInit(); // Platform specific init
    if (glew_ret != GLEW_OK) {
        throw std::runtime_error(std::string("WGLEW failed with error: ")
            + reinterpret_cast<const char*>(glewGetErrorString(glew_ret)));
    }
    else {
        std::cout << "WGLEW successfully initialized platform specific functions." << std::endl;
    }

    //------ Check if we are in core or compatibility profile ------
    GLint myint;
    glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &myint);

    if (myint & GL_CONTEXT_CORE_PROFILE_BIT) {
        std::cout << "We are using CORE profile\n";
    }
    else {
        if (myint & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT) {
            std::cout << "We are using COMPATIBILITY profile\n";
        }
        else {
            throw std::runtime_error("What??");
        }
    }

    //------ Enable debug output ------
    if (GLEW_ARB_debug_output) {
        glDebugMessageCallback(MessageCallback, 0);
        //glEnable(GL_DEBUG_OUTPUT);    //asynchronous debug output (default)
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        std::cout << "GL_DEBUG enabled." << std::endl;
    }
    else {
        std::cout << "GL_DEBUG NOT SUPPORTED!" << std::endl;
    }

    //------ Check if DSA have been initialised or not ------
    if (!GLEW_ARB_direct_state_access)
        throw std::runtime_error("No DSA :-(");

    //------ Get some glfw info ------
    int major, minor, revision;
    glfwGetVersion(&major, &minor, &revision);
    std::cout << "Running GLFW DLL " << major << '.' << minor << '.' << revision << std::endl;
    std::cout << "Compiled against GLFW " << 
        GLFW_VERSION_MAJOR << '.' << GLFW_VERSION_MINOR << '.' << GLFW_VERSION_REVISION << std::endl;

    init_assets();  // Initialise: shaders, textures, models

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    engine = irrklang::createIrrKlangDevice();
    if (!engine)
        throw std::exception("Can not create 3D sound device");
    BackgroundEngine = irrklang::createIrrKlangDevice();
    if (!BackgroundEngine)
        throw std::exception("Can not create background sound device");

    
    if (!tracker.init(0)) return -1; // camera index 0

    return true;
}

void App::init_assets(void) {
    // Initialize pipeline: compile, link and use shaders
    // 
    // -----shaders------: load, compile, link, initialize params (may be moved global variables - if all models used same shader)
    my_shader = ShaderProgram("lighting_shader.vert", "lighting_shader.frag");

    //------textures-----
    my_texture = textureInit("resources/textures/tex_2048.png");
    GLuint tower = textureInit("resources/textures/stone-wall.png");
    GLuint glass = textureInit("resources/textures/glass.jpg");
    GLuint Fireball = textureInit("resources/textures/fire.png");    

    // ------Heightmap------
    //Ground = Heightmap("resources/heightmaps/iceland_heightmap.png", my_shader, my_texture);
    Ground = Heightmap("resources/heightmaps/ground_v5.jpeg", my_shader, my_texture);

    // ------ Models ------: load model file, assign shader used to draw a model

    // --- Random coordinate generation for the trees ---
    const int numPoints = 100;
    const int minCoordinate = -100;
    const int maxCoordinate = 100;
    const int minborder = -15;
    const int maxborder = 15;
    glm::vec2 treecoords = glm::vec2(0.0f);
    std::srand(static_cast<unsigned int>(std::time(0)));
    // --- ---

    float positionx = 0.0f;
    float positionz = 0.0f;
    
    Model my_model = Model("resources/objects/cube_triangles_vnt.obj", my_shader, my_texture);
    Model base = my_model;
    Model transparent_model = my_model;
    Model bottle = Model("resources/objects/bottle.obj", my_shader, glass);
    Model Trees = Model("resources/objects/fir.obj", my_shader , my_texture);
    Model Camp = Model("resources/objects/towers.obj", my_shader , tower);
    Model mini_tower =Camp;
    Model firefly = Model("resources/objects/firefly.obj", my_shader , my_texture);
    Model torch = Model("resources/objects/Torch.obj", my_shader, my_texture);
    projectile = Model("resources/objects/sphere.obj", my_shader, Fireball);

    positionz = -3.0f;
    float terrainYm = getTerrainHeight(positionx, positionz, Ground.heightmap);
    my_model.origin = glm::vec3(positionx, terrainYm + 0.7f, positionz);
    my_model.scale = glm::vec3(1.5f);
    for (int i = 0; i < numPoints; ++i) {
        float x, z;
        do {
            x = minCoordinate + static_cast<float>(std::rand()) / RAND_MAX * (maxCoordinate - minCoordinate);
            z = minCoordinate + static_cast<float>(std::rand()) / RAND_MAX * (maxCoordinate - minCoordinate);
        } while (x > minborder && x < maxborder && z > minborder && z < maxborder);
        terrainYm = getTerrainHeight(x, z, Ground.heightmap);
        Trees.origin = glm::vec3(x, terrainYm, z);
        Trees.scale = glm::vec3(2.0f);
        scene.insert({ std::string("Tree:").append(std::to_string(i)).c_str(), Trees });
    }
    positionx = 20.0f;
    positionz = 20.0f;
    terrainYm = getTerrainHeight(positionx, positionz, Ground.heightmap);
    Camp.origin = glm::vec3(positionx, terrainYm + 10.0f, positionz);
    Camp.scale = glm::vec3(3.0f);
    positionx = 0.0f;
    positionz = 3.0f;
    terrainYm = getTerrainHeight(positionx, positionz, Ground.heightmap);
    mini_tower.origin = glm::vec3(positionx, terrainYm + 1.5f, positionz);
    mini_tower.scale = glm::vec3(0.05f);
    base.origin = glm::vec3(positionx, terrainYm + 0.5f, positionz);
    transparent_model.origin = glm::vec3(positionx, terrainYm + 1.45f, positionz);
    transparent_model.transparent = true;
    positionx = 0.0f;
    positionz = -3.5f;
    terrainYm = getTerrainHeight(positionx, positionz, Ground.heightmap);
    bottle.origin = glm::vec3(positionx, terrainYm + 1.6f, positionz);
    bottle.scale = glm::vec3(0.05f);
    bottle.transparent = true;
    positionx = 5.0f;
    positionz = 5.0f;
    terrainYm = getTerrainHeight(positionx, positionz, Ground.heightmap);
    firefly.origin = glm::vec3(positionx, terrainYm+0.5f, positionz);
    firefly.orientation = glm::vec3(glm::radians(-90.0f),0.0f, 0.0f);
    firefly.scale = glm::vec3(0.05f);
    positionx = 13.5f;
    positionz = 15.5f;
    terrainYm = getTerrainHeight(positionx, positionz, Ground.heightmap);
    torch.origin = glm::vec3(positionx, terrainYm + 16.0f, positionz);
    torch.scale = glm::vec3(0.5f);
    positionx = 0.0f;
    positionz = 0.0f;
    projectile.origin = glm::vec3(positionx, 0.5f, positionz);
    projectile.scale = glm::vec3(0.1f);

    // put model to scene
    scene.insert({ "my_first_object", my_model });
    scene.insert({ "trasparent_block", transparent_model });
    scene.insert({ "trasparent_bottle", bottle });
    scene.insert({ "Tower", Camp });
    scene.insert({ "Moving_model", firefly });
    scene.insert({ "light_2", torch });
    scene.insert({ "wooden_base", base });
    scene.insert({ "minitower", mini_tower });
    

    my_model.meshes.clear();
}

GLuint App::textureInit(const std::filesystem::path& file_name){
    // Initialise texture based on given image

    cv::Mat image = cv::imread(file_name.string(), cv::IMREAD_UNCHANGED);  // Read with (potential) Alpha
    if (image.empty()) {
        throw std::runtime_error("No texture in file: " + file_name.string());
    }
    
    // or print warning, and generate synthetic image with checkerboard pattern 
    // using OpenCV and use as a texture replacement 

    GLuint texture = gen_tex(image);

    return texture;
}

GLuint App::gen_tex(cv::Mat& image){
    // Generate texture from loaded image file

    GLuint ID = 0;

    if (image.empty()) {
        throw std::runtime_error("Image empty?\n");
    }

    // Generates an OpenGL texture object
    glCreateTextures(GL_TEXTURE_2D, 1, &ID);
    glObjectLabel(GL_TEXTURE, ID, -1, "Mytexture");

    switch (image.channels()) {
    case 3:
        // Create and clear space for data - immutable format
        glTextureStorage2D(ID, 1, GL_RGB8, image.cols, image.rows);
        // Assigns the image to the OpenGL Texture object
        glTextureSubImage2D(ID, 0, 0, 0, image.cols, image.rows, GL_BGR, GL_UNSIGNED_BYTE, image.data);
        break;
    case 4:
        glTextureStorage2D(ID, 1, GL_RGBA8, image.cols, image.rows);
        glTextureSubImage2D(ID, 0, 0, 0, image.cols, image.rows, GL_BGRA, GL_UNSIGNED_BYTE, image.data);
        break;
    default:
        throw std::runtime_error("unsupported channel cnt. in texture:" + std::to_string(image.channels()));
    }

    // MIPMAP filtering + automatic MIPMAP generation - nicest, needs more memory. Notice: MIPMAP is only for image minifying.
    glTextureParameteri(ID, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // bilinear magnifying
    glTextureParameteri(ID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // trilinear minifying
    glGenerateTextureMipmap(ID);  //Generate mipmaps now.

    // Configures the way the texture repeats
    glTextureParameteri(ID, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(ID, GL_TEXTURE_WRAP_T, GL_REPEAT);

    return ID;
}

void App::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {   //Handles the callback events for all the key inputs
    auto app = static_cast<App*>(glfwGetWindowUserPointer(window)); // Static cast needed to link key callbacks for the current active window (Filters out possible errors)

    if ((action == GLFW_PRESS) || (action == GLFW_REPEAT))
    {
        switch (key)
        {
        case GLFW_KEY_ESCAPE:   // Stop the application
            std::cout << "ESC has been pressed!\n";
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            break;
        case GLFW_KEY_V:    // Toggle VSync on-off by glfwSwapInterval(...)
            app->vsync_on = !app->vsync_on;  // Toggle the flag
            glfwSwapInterval(app->vsync_on ? 1 : 0);  // Apply new VSync setting
            break;
        case GLFW_KEY_TAB:  // Toggle between full screen and vindowed mode
            if (app->fullscreen == FALSE) {
                app->switch_to_fullscreen();
                //app->update_projection_matrix();
                app->fullscreen = TRUE;
            }
            else {
                glfwSetWindowMonitor(window, app->last_window_monitor, app->last_window_xpos,
                    app->last_window_ypos, app->last_window_width, app->last_window_height, GLFW_DONT_CARE);
               // app->update_projection_matrix();
                app->fullscreen = FALSE;
            }
            break; 
        case GLFW_KEY_C:    //Toggle the status of the cursor between locked and free
            if (app->cursor_state == FALSE) {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                app->cursor_state = TRUE;
            }
            else{
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                app->cursor_state = FALSE;
            }
            break;
        case GLFW_KEY_M:    // Mute/unmute the audio
            if (app->mute == FALSE) {
                app->mute = TRUE;
            }
            else {
                app->mute = FALSE;
            }
            break;
        case GLFW_KEY_F:    // toggle flashlight
            if (app->flashlight == FALSE) {
                app->flashlight = TRUE;
                app->brightness = 10.0f;
                app->my_shader.setUniform("lights[1].ambientM", glm::vec3(0.05f, 0.05f, 0.05f));
                app->my_shader.setUniform("lights[1].diffuseM", glm::vec3(1.0f * app->brightness, 0.95f * app->brightness, 0.8f * app->brightness));
                app->my_shader.setUniform("lights[1].specularM", glm::vec3(1.0f * app->brightness, 0.95f * app->brightness, 0.9f * app->brightness));
            }
            else {
                app->flashlight = FALSE;
                app->my_shader.setUniform("lights[1].ambientM", glm::vec3(0.0f, 0.0f, 0.0f));
                app->my_shader.setUniform("lights[1].diffuseM", glm::vec3(0.0f, 0.0f, 0.0f));
                app->my_shader.setUniform("lights[1].specularM", glm::vec3(0.0f, 0.0f, 0.0f));
            }
            break;
        case GLFW_KEY_N:    // Change day/night
            if (app->night == FALSE) {//set to night
                app->night = TRUE;
                app->brightness = 0.1f;
                app->my_shader.setUniform("fog_color", glm::vec4(glm::vec3(0.0f), 1.0f));
                app->my_shader.setUniform("lights[0].ambientM", glm::vec3(0.05f, 0.05f, 0.1f));
                app->my_shader.setUniform("lights[0].diffuseM", glm::vec3(0.2f * app->brightness, 0.2f * app->brightness, 0.35f * app->brightness));
                app->my_shader.setUniform("lights[0].specularM", glm::vec3(0.3f * app->brightness, 0.3f * app->brightness, 0.5f * app->brightness));
            }
            else {//set to day
                app->night = FALSE;
                app->my_shader.setUniform("fog_color", glm::vec4(glm::vec3(0.85f), 1.0f));
                app->my_shader.setUniform("lights[0].ambientM", glm::vec3(0.2f, 0.2f, 0.2f));
                app->my_shader.setUniform("lights[0].diffuseM", glm::vec3(1.0f, 0.95f, 0.8f));
                app->my_shader.setUniform("lights[0].specularM", glm::vec3(1.0f, 0.95f, 0.9f));
            }
            break;

        default:
            break;
        }
    }
}

void App::update_projection_matrix() {  //Update the projection matrix
    glfwGetFramebufferSize(window, &width, &height);
    if (height <= 0) // avoid division by 0
        height = 1;

    float ratio = static_cast<float>(width) / height;

    projection_matrix = glm::perspective(
        glm::radians(45.0f), // The vertical Field of View, in radians: the amount of "zoom". Think "camera lens". Usually between 90deg (extra wide) and 30deg (quite zoomed in)
        ratio,			     // Aspect Ratio. Depends on the size of your window.
        0.1f,                // Near clipping plane. Keep as big as possible, or you'll get precision issues.
        300.0f               // Far clipping plane. Keep as little as possible.
    );
    my_shader.setUniform("uP_m", projection_matrix);
}

void App::updateFPS() { // Calculate the FPS of the application by counting the frames for 1 second
    frame_count++;

    TimePoint currentTime = Clock::now();
    float elapsed = std::chrono::duration<float>(currentTime - last_time).count();

    if (elapsed >= 1.0f) {
        fps = frame_count;
        //std::cout << "FPS: " << fps << std::endl;
        frame_count = 0;
        last_time = currentTime;
    }
}

void App::error_callback(int error, const char* description) {  //General error callback function
    std::cerr << "Error: " << description << std::endl;
}

void App::framebuffer_size_callback(GLFWwindow* window, int width, int height) {    // Update the projection matrix based on window size. Needed for window scaling
    auto app = static_cast<App*>(glfwGetWindowUserPointer(window));
    glViewport(0, 0, width, height);
    if (height <= 0) // avoid division by 0
        height = 1;

    float ratio = static_cast<float>(width) / height;

    app->projection_matrix = glm::perspective(glm::radians(45.0f), ratio, 0.1f, 20000.0f);
    //app->my_shader.setUniform("uP_m", app->projection_matrix);
}

void App::switch_to_fullscreen(void) {
    // First, save position, size and placement for position recovery
    last_window_monitor = glfwGetWindowMonitor(window);
    glfwGetWindowSize(window, &last_window_width, &last_window_height);
    glfwGetWindowPos(window, &last_window_xpos, &last_window_xpos);
    // Switch to fullscreen
    // Multimonitor support
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    // Get resolution of primary monitor
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    // Switch to full screen
    glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
}

float App::getTerrainHeight(float x, float z, const std::vector<std::vector<float>>& heightmap) {
    int terrainWidth = Ground.width;
    int terrainHeight = Ground.height;
    float terrainScale = Ground.scale.x;  // Only need to be changed if the scale of the heightmap is changed
    float fx = (x + terrainHeight / 2.0f) / terrainScale;
    float fz = (z + terrainWidth / 2.0f) / terrainScale;

    int ix = (int)fx;
    int iz = (int)fz;

    // Clamp to terrain boundaries
    ix = std::max(0, std::min(ix, terrainWidth - 2));
    iz = std::max(0, std::min(iz, terrainHeight - 2));

    // Heights of surrounding points
    float h00 = heightmap[ix][iz];
    float h10 = heightmap[ix + 1][iz];
    float h01 = heightmap[ix][iz + 1];
    float h11 = heightmap[ix + 1][iz + 1];

    // Fractional part for interpolation
    float fracx = fx - ix;
    float fracz = fz - iz;

    // Bilinear interpolation
    float h0 = h00 * (1 - fracx) + h10 * fracx;
    float h1 = h01 * (1 - fracx) + h11 * fracx;
    return h0 * (1 - fracz) + h1 * fracz;
}


void App::cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    auto app = static_cast<App*>(glfwGetWindowUserPointer(window));

    app->camera.ProcessMouseMovement(xpos - app->cursorLastX, (ypos - app->cursorLastY) * -1.0);
    app->cursorLastX = xpos;
    app->cursorLastY = ypos;

}

void App::mouse_button_callback(GLFWwindow* window, int button, int action, int mods) { //General functionality to check if the callback is working or not
    auto app = static_cast<App*>(glfwGetWindowUserPointer(window));

    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
        app->r = 0.0f;
        app->b = 1.0f;
    }
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        if(app->leftclick == false){
            app->leftclick = true;
            app->projectile.origin = app->camera.Position;
            app->projectile.velocity = glm::normalize(app->camera.Front)*10.0f;
            app->scene.insert({ "throwable_rock", app->projectile });
        }
        
    }
}

/*  General window close callback funstion. I did nothing with it yet...
void App::window_close_callback(GLFWwindow* window) {
    if (!time_to_close) // is it really time to quit?
        glfwSetWindowShouldClose(window, GLFW_FALSE); // You can cancel the request.
}*/

int App::run(void)
{
    //glfwSetWindowCloseCallback(window, window_close_callback); // Setting the window close callback function so it will be active during runtime

    // ------ Enabling the depth test and faceculling (only for the back faces) ------
    glEnable(GL_DEPTH_TEST);
    
    glCullFace(GL_BACK);  // The default
    glEnable(GL_CULL_FACE); // assume ALL objects are non-transparent 
    
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);    // Disable cursor, so that it can not leave window, and we can process movement
    glfwGetCursorPos(window, &cursorLastX, &cursorLastY);           // get first position of mouse cursor

    update_projection_matrix();
    glViewport(0, 0, width, height);    //Set viewport

    camera.Position = glm::vec3(0.0, 10.0, 0.0);    // Setting the camera starting position

    glm::vec4 my_rgba = glm::vec4(r,g,b,a); // Creatiing the vector for the color input of the object
    a = 0.1f;
    glm::vec4 transparent_rgba = glm::vec4(r, g, b, a);
    float tile_size = 1.0f / 16;            // Size of one tile on the texture atlas
    glm::vec2 tile_offset = glm::vec2(0.0f* tile_size, 0.0f * tile_size);   // Setting the position of the desired tile of the texture atlas

    // Setting variables for the FPS calculations
    double last_frame_time = glfwGetTime();
    last_time = Clock::now();
    frame_count = 0;

    my_shader.activate();   // Because we only have one shader

    // ----- Setting the parameters of the desired lights. (All parameters needs to be set from the s_lights struct for it to work >.<)------
    // Currently these parameters generatte a green and a blue pointlight at the top and bottom of the loaded in textured cube
    const int maxlights = 4;
    my_shader.setUniform("N_matrix", Ground.normal_matrix); //Needed for light calculations

    brightness = 5;
    //glm::vec3 spotDir = glm::normalize(glm::vec3(glm::inverse(camera.GetViewMatrix()) * eyeCoords));
    float terrainY = getTerrainHeight(13.5f, 17.5f, Ground.heightmap);

    for (int i = 0; i < maxlights; ++i) {
        if (i == 0) {
            my_shader.setUniform("lights[0].position", glm::vec4(0.0f, 100.0f, 0.0f, 0.0f));
            my_shader.setUniform("lights[0].ambientM", glm::vec3(0.2f, 0.2f, 0.2f));
            my_shader.setUniform("lights[0].diffuseM", glm::vec3(1.0f, 0.95f, 0.8f));
            my_shader.setUniform("lights[0].specularM", glm::vec3(1.0f, 0.95f, 0.9f));
            my_shader.setUniform("lights[0].consAttenuation", 1.0f);
            my_shader.setUniform("lights[0].linAttenuation", 1.0f);
            my_shader.setUniform("lights[0].quadAttenuation", 1.0f);
            my_shader.setUniform("lights[0].cutoff", 180.0f);
            my_shader.setUniform("lights[0].direction", glm::vec3(0.0f, 0.0f, 0.0f));
            my_shader.setUniform("lights[0].exponent", 0);
        }
        else if (i == 1) {
            my_shader.setUniform("lights[1].position", glm::vec4(camera.Position, 1.0f));
            my_shader.setUniform("lights[1].ambientM", glm::vec3(0.0f, 0.0f, 0.0f));
            my_shader.setUniform("lights[1].diffuseM", glm::vec3(0.0f, 0.0f, 0.0f));
            my_shader.setUniform("lights[1].specularM", glm::vec3(0.0f, 0.0f, 0.0f));
            my_shader.setUniform("lights[1].consAttenuation", 1.0f);
            my_shader.setUniform("lights[1].linAttenuation", 0.09f);
            my_shader.setUniform("lights[1].quadAttenuation", 0.032f);
            my_shader.setUniform("lights[1].cutoff", 20.0f);
            my_shader.setUniform("lights[1].direction", camera.Front);
            my_shader.setUniform("lights[1].exponent", 20.0f);
        }
        else if (i == 2) {
            my_shader.setUniform("lights[2].position", glm::vec4(13.5f, terrainY + 19.0f, 20.5f, 1.0f));
            my_shader.setUniform("lights[2].ambientM", glm::vec3(0.3f, 0.12f, 0.0f));
            my_shader.setUniform("lights[2].diffuseM", glm::vec3(1.0f * brightness, 0.4f * brightness, 0.0f * brightness));
            my_shader.setUniform("lights[2].specularM", glm::vec3(1.0f * brightness, 0.6f * brightness, 0.2f * brightness));
            my_shader.setUniform("lights[2].consAttenuation", 1.0f);
            my_shader.setUniform("lights[2].linAttenuation", 0.09f);
            my_shader.setUniform("lights[2].quadAttenuation", 0.032f);
            my_shader.setUniform("lights[2].cutoff", 180.0f);
            my_shader.setUniform("lights[2].direction", glm::vec3(0.0f, 0.0f, 0.0f));
            my_shader.setUniform("lights[2].exponent", 20.0f);
        }
        else if (i == 3) {
            my_shader.setUniform("lights[3].position", glm::vec4(5.0f, 0.5f, 5.0f, 1.0f));
            my_shader.setUniform("lights[3].ambientM", glm::vec3(0.08f, 0.18f, 0.06f));
            my_shader.setUniform("lights[3].diffuseM", glm::vec3(0.3f * brightness, 0.95f * brightness, 0.3f * brightness));
            my_shader.setUniform("lights[3].specularM", glm::vec3(0.6f * brightness, 1.0f * brightness, 0.6f * brightness));
            my_shader.setUniform("lights[3].consAttenuation", 1.0f);
            my_shader.setUniform("lights[3].linAttenuation", 0.09f);
            my_shader.setUniform("lights[3].quadAttenuation", 0.032f);
            my_shader.setUniform("lights[3].cutoff", 180.0f);
            my_shader.setUniform("lights[3].direction", glm::vec3(0.0f, 0.0f, 0.0f));
            my_shader.setUniform("lights[3].exponent", 20.0f);
        }
    }
    // --- Set general parameters for all lights ---
    my_shader.setUniform("ambient_intensity", glm::vec3(1.0f, 1.0f, 1.0f));
    my_shader.setUniform("diffuse_intensity", glm::vec3(1.0f, 1.0f, 1.0f));
    my_shader.setUniform("specular_intensity", glm::vec3(1.0f, 1.0f, 1.0f));
    my_shader.setUniform("specular_shinines", 80.0f);
    //------ ------

    std::vector<Model*> transparent;    // temporary, vector of pointers to transparent objects
    transparent.reserve(scene.size());  // reserve size for all objects to avoid reallocation
    
    //----- 2D & 3D audio -----    
    // position, playLooped = true, startPaused = true, track = true
    irrklang::ISound* music = engine->play3D("resources/music/birds.mp3", irrklang::vec3df(0, 0, 0), false, true, true); // loop, start paused, enable 3D sound
    irrklang::ISound* BackgroundMusic = BackgroundEngine -> play2D("resources/music/Relaxing_Green_Nature_David_Fesliyan.mp3", true, true, false); // loop, start paused, enable 3D sound
    // The minimum distance is the distance in which the sound gets played at maximum volume.
    music->setMinDistance(5.0f); // Make sound source bigger. (Default = 1.0 = "small" sound source.)
    music->setIsPaused(false); // Start playing
    
    if (BackgroundMusic) {
        std::cout << "Current volume:" << BackgroundMusic->getVolume(); // float, [0.0 to 1.0]
            // Prepare sound parameters: set volume, effects, etc.
        BackgroundMusic->setVolume(0.1);
        // Unpause
        BackgroundMusic->setIsPaused(false);
    }

    if (music) {
        std::cout << "Current volume:" << music->getVolume(); // float, [0.0 to 1.0]
        // Prepare sound parameters: set volume, effects, etc.
        music->setVolume(0.8);
        // Unpause
        music->setIsPaused(false);
    }

    float eyeHeight = 1.8f;
    // Start background worker
    if (!tracker.startWorker()) return -1;

    std::uint64_t last_seq = 0;

    while (!glfwWindowShouldClose(window)) {    //Main loop of the application
        
        // Set all the callback functions we want to be active during the runtime of the application (Only the set functions with declaration will be active, just declaring a callback function is not enough)
        glfwSetCursorPosCallback(window, cursor_position_callback);
        glfwSetMouseButtonCallback(window, mouse_button_callback);
        glfwSetWindowTitle(window, std::string("FPS: ").append(std::to_string(fps)).append(" Vsync: ").append(std::to_string(vsync_on)).c_str());   //Set the window title to show current FPS of the application and if Vsync is active or not
        glfwSetWindowSizeCallback(window,framebuffer_size_callback);

        //glClearColor(0.0f, 0.0f, 0.0f, 1.0f);   // Set background color
        if (night) {
            glClearColor(0.02f, 0.02f, 0.08f, 1.0f);
        }
        else { glClearColor(0.53f, 0.81f, 0.92f, 1.0f); }  // sky blue RGBA
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // clear canvas

        double current_frame_time = glfwGetTime(); //Needed for FPS calculation

        double delta_t = current_frame_time - last_frame_time; // render time of the last frame 
        last_frame_time = current_frame_time;
        camera.ProcessInput(window, delta_t); // process keys etc.
        //--- Process ground colision ---
        float terrainY = getTerrainHeight(camera.Position.x, camera.Position.z, Ground.heightmap);
        float minEyeY = terrainY + eyeHeight;
        if (camera.Position.y < minEyeY) {
            camera.Position.y = minEyeY;
            camera.Velocity.y = 0.0f;
            camera.onground = true;
        }
        else {
            camera.onground = false;
        }
        //--- ---

        my_shader.setUniform("uV_m", camera.GetViewMatrix());   // Update the view matrix based on the viewmatrix of the camera
        my_shader.setUniform("uP_m", projection_matrix);        
        
        // --- Set the color and texture tile (from texture atlas) of the object ---     
        tile_offset = glm::vec2(14.0f * tile_size, 4.0f * tile_size);
        my_shader.setUniform("my_color", my_rgba);
        my_shader.setUniform("tileSize", tile_size);
        my_shader.setUniform("tileOffset", tile_offset);  


        my_shader.setUniform("lights[1].position", glm::vec4(camera.Position, 1.0f));
        my_shader.setUniform("lights[1].direction", glm::vec3(camera.Front.x * delta_t, camera.Front.y * delta_t, camera.Front.z * delta_t));

        // --- set the 3D audio ---
        // move sound source
        irrklang::vec3df newPosition(20.0, 10.0, 20.0);
        music->setPosition(newPosition);
        // move Listener (similar to Camera)
        irrklang::vec3df position(camera.Position.x, camera.Position.y, camera.Position.z); // position of the listener
        irrklang::vec3df lookDirection(camera.Front.x, camera.Front.y, camera.Front.z); // the direction the listener looks into
        irrklang::vec3df velPerSecond(0, 0, 0); // only relevant for doppler effects
        irrklang::vec3df upVector(camera.Up.x, camera.Up.y, camera.Up.z); // where 'up' is in your 3D scene
        engine->setListenerPosition(position, lookDirection, velPerSecond, upVector);

        if (mute) {
            music->setIsPaused(true);
            BackgroundMusic->setIsPaused(true);
        }
        else {
            music->setIsPaused(false);
            BackgroundMusic->setIsPaused(false);
        }
    
        if (music && music->isFinished()){
            music->drop();
            music = nullptr;
        }
                        
        // draw all models in the scene
        glFrontFace(GL_CW);
        Ground.draw(translate, rotate, scale);
        glFrontFace(GL_CCW);
        if (auto res = tracker.getLatest(last_seq)) {
            if (res->face_found) {
                std::cout << "Face at px: " << res->center_px
                    << " norm: " << res->center_norm << '\n';
                float ndcX = -(res->center_norm.x * 2.0f - 1.0f);
                float ndcY = 1.0f - res->center_norm.y * 2.0f;
                glm::vec3 targetPos(ndcX, ndcY, 0.0f); // new position from face tracker
                float alpha = 0.1f; // smoothing factor: smaller = smoother, slower
                FaceTracResult = alpha * targetPos + (1.0f - alpha) * FaceTracResult;
            }
        }
        else {
            std::cout << "No face detected\n";
            //FaceTracResult = glm::vec3(0.0f, 0.0f, 0.0f);
        }
                
        transparent.clear();
        // FIRST PART - draw all non-transparent in any order
        for (auto& [name, model] : scene) {
            my_shader.setUniform("N_matrix", model.normal_matrix); //Needed for light calculations
            if (!model.transparent) {
                if (name == "my_first_object") {
                    tile_offset = glm::vec2(4.0f * tile_size, 0.0f * tile_size);
                    my_shader.setUniform("tileOffset", tile_offset);
                    model.draw(translate, rotate, scale);
                }else if (name == "Moving_model") {
                    tile_offset = glm::vec2(0.0f * tile_size, 3.0f * tile_size);
                    my_shader.setUniform("tileOffset", tile_offset);
                    float height = getTerrainHeight(model.origin.x,model.origin.z,Ground.heightmap);
                    model.circlepath(delta_t,height);
                    my_shader.setUniform("lights[3].position", glm::vec4(model.origin, 1.0f));
                    model.draw(translate, rotate, scale);
                }
                else if (name == "wooden_base") {
                    tile_offset = glm::vec2(8.0f * tile_size, 1.0f * tile_size);
                    my_shader.setUniform("tileOffset", tile_offset);
                    model.draw(translate, rotate, scale);
                }
                else if (name == "light_2") {
                    tile_offset = glm::vec2(1.0f * tile_size, 1.0f * tile_size);
                    my_shader.setUniform("tileOffset", tile_offset);
                    model.draw(translate, rotate, scale);
                }
                else if (name == "throwable_rock") {
                    if (leftclick) {
                            model.flyghtpath(delta_t, FaceTracResult);
                            model.draw(translate, rotate, scale);
                            if (model.origin.y < getTerrainHeight(model.origin.x, model.origin.z, Ground.heightmap)) {
                                leftclick = false;
                                continue;
                            }
                        }
                        else {
                            continue;
                        }
                    
                }
                else{
                    tile_offset = glm::vec2(5.0f * tile_size, 8.0f * tile_size);
                    my_shader.setUniform("tileOffset", tile_offset);
                    model.draw(translate, rotate, scale);
                }
                
            }
            else
                transparent.emplace_back(&model); // save pointer for painters algorithm
        }

        if (!leftclick)
            scene.erase("throwable_rock");

        tile_offset = glm::vec2(3.0f * tile_size, 4.0f * tile_size);
        my_shader.setUniform("tileOffset", tile_offset);
        my_shader.setUniform("my_color", transparent_rgba);

        // SECOND PART - draw only transparent - painter's algorithm (sort by distance from camera, from far to near)
        std::sort(transparent.begin(), transparent.end(), [&](Model const* a, Model const* b) {
            glm::vec3 translation_a = glm::vec3(a->model_matrix[3]);  // get 3 values from last column of model matrix = translation
            glm::vec3 translation_b = glm::vec3(b->model_matrix[3]);  // dtto for model B
            return glm::distance(camera.Position, translation_a) < glm::distance(camera.Position, translation_b); // sort by distance from camera
            });

        // set GL for transparent objects // TODO: from lectures
        glEnable(GL_BLEND);
        glDepthMask(GL_FALSE); 
        glDisable(GL_CULL_FACE);
        // draw sorted transparent
        for (auto p : transparent) {
            my_shader.setUniform("N_matrix", p->normal_matrix);
            my_shader.setUniform("uM_m", p->model_matrix);
            p->draw();
        }
        // restore GL properties for non-transparent objects // TODO: from lectures
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
        glEnable(GL_CULL_FACE);

        updateFPS();
        glfwSwapBuffers(window);        
        glfwPollEvents();
    }

    tracker.stopWorker();
    // Close OpenGL window if opened and terminate GLFW
    if (window)
        glfwDestroyWindow(window);

    return EXIT_SUCCESS;

}

App::~App()
{
    // clean-up
    cv::destroyAllWindows();
    glfwTerminate();
    my_shader.clear();
    glDeleteTextures(1, &my_texture);
    if (engine) {
        engine->drop();
        engine = nullptr;
    }
    if (BackgroundEngine) {
        BackgroundEngine->drop();
        BackgroundEngine = nullptr;
    }
    std::cout << "Bye...\n";

}

App app;


int main()
{
    if (!app.init()) {
        std::cerr << "App initialization failed.\n";
        return 3; 
    }
    return app.run();
}

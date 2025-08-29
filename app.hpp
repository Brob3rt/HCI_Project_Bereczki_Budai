// icp.cpp 
// author: JJ
#include <iostream>
#include <opencv2/opencv.hpp>
#include <unordered_map>
#include <chrono>
#include <irrKlang/irrKlang.h>

#include "assets.hpp"
#include "Model.hpp"
#include "camera.hpp"
#include "Heightmap.hpp"
#include "FaceTracker.hpp"


#pragma once


class App {
public:
    App(); // default constructor, called on app instance definition

    //------ public methods ------
    bool init(void);
    int run(void);
    void init_assets(void);
    void updateFPS(void);
    void update_projection_matrix(void);
    void switch_to_fullscreen(void);
    float getTerrainHeight(float x, float z, const std::vector<std::vector<float>>& heightmap);

    //------ For textures ------
    GLuint textureInit(const std::filesystem::path& file_name);
    GLuint gen_tex(cv::Mat& image);

    //------ Functions for video analisys ------
    //void draw_cross_normalized(cv::Mat& img, cv::Point2f center_relative, int size);
    //void draw_cross_relative(cv::Mat& img, const cv::Point2f center_relative, const int size);
    //cv::Point2f object_search(cv::Mat& img);
    //cv::Point2f find_face(cv::Mat& frame);

    ~App(); //default destructor, called on app instance destruction
private:
    cv::CascadeClassifier face_cascade = cv::CascadeClassifier("resources/haarcascade_frontalface_default.xml"); //variable needed for the face tracking
    GLfloat r{ 1.0f }, g{ 1.0f }, b{ 1.0f }, a{ 1.0f };
    GLFWwindow* window = NULL;
    bool cursor_state = false;
    bool mute = false;
    bool night = false;
    bool flashlight = false;
    bool leftclick = false;
    float brightness = 0.0;
 
    //------Callback funcions start------
    static void error_callback(int error, const char* description);
    static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos);
    static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
    static void framebuffer_size_callback(GLFWwindow* window, int width, int height);
    //void window_close_callback(GLFWwindow* window);
    //------Callback funcions end------
    
    //------For model matrix manipulation------
    glm::vec3 translate = glm::vec3(0.0f);
    glm::vec3 rotate = glm::vec3(0.0f);
    glm::vec3 scale = glm::vec3(1.0f);

    //------ For the camera transformation matrix ------
    int width, height;
    float fov;
    glm::mat4 projection_matrix;
    double cursorLastX{ 0 };
    double cursorLastY{ 0 };

    //------ For fullscreen mode ------
    int last_window_xpos = 0;
    int last_window_ypos = 0;
    int last_window_height = 0;
    int last_window_width = 0;
    GLFWmonitor* last_window_monitor;
    bool fullscreen = false;

    //------ For vsync ------
    bool vsync_on = true;

    //------ For FPS counting -----
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    TimePoint last_time;
    int frame_count;
    int fps = 0;
    
    //------ For 3D sound ------
    irrklang::ISoundEngine* engine = nullptr;
    irrklang::ISoundEngine* BackgroundEngine = nullptr;

    glm::vec3 throw_start = glm::vec3(0.0f);
    glm::vec3 throw_dir = glm::vec3(0.0f);
    Model projectile;
    glm::vec3 FaceTracResult = glm::vec3(0.0f, 0.0f, 0.0f);
    

protected:
    cv::VideoCapture capture;  // global variable, move to app class, protected
    Camera camera;
    ShaderProgram my_shader;
    GLuint my_texture;
    Heightmap Ground;
    std::unordered_map<std::string, Model> scene;   // all objects of the scene addressable by name
    FaceTracker tracker;

};


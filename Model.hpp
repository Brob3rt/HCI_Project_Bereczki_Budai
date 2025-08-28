#pragma once

#include <filesystem>
#include <string>
#include <vector> 
#include <glm/glm.hpp> 

#include "assets.hpp"
#include "Mesh.hpp"
#include "ShaderProgram.hpp"
#include "OBJloader.hpp"

class Model {
public:
    std::vector<Mesh> meshes;
    std::string name;
    glm::vec3 origin{0.0};
    glm::vec3 orientation{0.0};  //rotation by x,y,z axis, in radians
    glm::vec3 scale{1.0};
    glm::mat4 model_matrix{};
    glm::mat4 local_model_matrix{}; //for complex transformations
    glm::mat3 normal_matrix{}; //for normals calculation
    GLuint texture_id{ 0 };
    ShaderProgram shader;
    std::vector<vertex> vertices{};
    glm::vec3 velocity;
    glm::vec3 gravity = glm::vec3(0.0f, -5.0f, 0.0f);
    bool transparent{ false };// For handling transparent models
    //transparency = final fragment alpha < 1.0; this can happen usually because -> model has transparent material -> model has transparent texture
    // (when updating material or texture, check alpha and set to TRUE when needed)

    Model()
        :origin(0.0f),
        orientation(0.0f),
        scale(1.0f),
        model_matrix{},
        local_model_matrix(glm::identity<glm::mat4>()),
        normal_matrix(glm::identity<glm::mat3>()),
        texture_id(0),
        velocity(0.0f),
        gravity(0.0f, -5.0f, 0.0f)
    {
    }

    Model(const std::filesystem::path& filename, ShaderProgram shader, GLuint const texture_id = 0) {
        // load mesh (all meshes) of the model, (in the future: load material of each mesh, load textures...)
        // TODO: call LoadOBJFile, LoadMTLFile (if exist), process data, create mesh and set its properties
        //    notice: you can load multiple meshes and place them to proper positions, 
        //            multiple textures (with reusing) etc. to construct single complicated Model   

        std::vector< glm::vec3 > positions;
        std::vector< glm::vec2 > texcoords;
        std::vector< glm::vec3 > normals;

        if (!loadOBJ(filename.string().c_str(), positions, texcoords, normals)) {
            std::cerr << "Failed to load OBJ file: " << filename << std::endl;
            return;
        }

        for (auto& tex : texcoords) {
            tex.x = 1.0f - tex.x;
            tex.y = 1.0f - tex.y;
            //std::cout << "(" << tex.x << ", " << tex.y << ")\n";
        }

        //std::vector<vertex> vertices{};
        for (size_t i = 0; i < positions.size(); ++i) {
            vertex v;
            v.position = positions[i];
            v.normal = (i < normals.size()) ? normals[i] : glm::vec3(0.0f);
            v.texcoord = (i < texcoords.size()) ? texcoords[i] : glm::vec2(0.0f);
            vertices.push_back(v);
        }

        std::vector<GLuint> indices(vertices.size());
        for (GLuint i = 0; i < indices.size(); ++i) {
            indices[i] = i;
        }

        Mesh Mesh(GL_TRIANGLES, shader, vertices, indices, origin, orientation, texture_id);
        /* Mesh mesh( primitive type, shader to use, vertex list, index list,
        origin for this mesh (relative to model), orientation for this mesh (relative to model));*/

        meshes.push_back(std::move(Mesh));


    }

    // update position etc. based on running time
    float angle = 0.0f;       // current angle around the circle
    void circlepath(float delta_t,float height) {    

            float radius = 10.0f;         // circle radius
            float angular_speed = 1.0f;  // radians per second

            // advance angle
            angle += angular_speed * delta_t;

            // wrap around if too large
            if (angle > glm::two_pi<float>())
                angle -= glm::two_pi<float>();

            // compute new position
            origin.x = radius * cos(angle);
            origin.z = radius * sin(angle);
            origin.y = height+0.5f;
    }

    void flyghtpath(float delta_t,glm::vec3 input) {
        velocity += gravity * delta_t;
        //origin += velocity * delta_t;
        if (input.x == 0.0f) {
            origin.x += velocity.x * delta_t;
        }else{
            origin.x += input.x;
        }   
        origin.y += velocity.y * delta_t;
        origin.z += velocity.z * delta_t;

    }

    void draw(glm::vec3 const& offset = glm::vec3(0.0f),
        glm::vec3 const& rotation = glm::vec3(0.0f),
        glm::vec3 const& scale_change = glm::vec3(1.0f)) {

        // compute complete transformation
        
        glm::mat4 t = glm::translate(glm::mat4(1.0f), origin);
        glm::mat4 rx = glm::rotate(glm::mat4(1.0f), orientation.x, glm::vec3(1.0f, 0.0f, 0.0f));
        glm::mat4 ry = glm::rotate(glm::mat4(1.0f), orientation.y, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 rz = glm::rotate(glm::mat4(1.0f), orientation.z, glm::vec3(0.0f, 0.0f, 1.0f));
        glm::mat4 s = glm::scale(glm::mat4(1.0f), scale);

        glm::mat4 m_off = glm::translate(glm::mat4(1.0f), offset);
        glm::mat4 m_rx = glm::rotate(glm::mat4(1.0f), rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
        glm::mat4 m_ry = glm::rotate(glm::mat4(1.0f), rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 m_rz = glm::rotate(glm::mat4(1.0f), rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
        glm::mat4 m_s = glm::scale(glm::mat4(1.0f), scale_change);
        
        glm::mat4 mv_m = glm::identity<glm::mat4>();
        //local_model_matrix = mv_m * s * rz * ry * rx * t;
        //model_matrix = local_model_matrix * m_s * m_rz * m_ry * m_rx * m_off;
        local_model_matrix = mv_m * t * rx * ry * rz* s;
        //model_matrix = local_model_matrix * m_off* m_rx* m_ry* m_rz*m_s;
        model_matrix = local_model_matrix * m_s * m_rz * m_ry * m_rx * m_off;
        normal_matrix = glm::mat3(glm::inverseTranspose(model_matrix));

        // call draw() on mesh (all meshes)
        for (auto& mesh : meshes) {
            mesh.draw(model_matrix);
        }
    }

    void draw(glm::mat4 const& model_matrix) {
        for (auto& mesh : meshes) {
            mesh.draw(local_model_matrix * model_matrix);
        }
    }

};


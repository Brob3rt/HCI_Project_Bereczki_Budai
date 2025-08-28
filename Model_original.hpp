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
    glm::vec3 origin{};
    glm::vec3 orientation{};  //rotation by x,y,z axis, in radians
    glm::vec3 scale{};
    glm::mat4 local_model_matrix = glm::identity<glm::mat4>(); //for complex transformations
    glm::mat3 normal_matrix = glm::identity<glm::mat3>(); //for normals calculation
    GLuint texture_id{0};
    ShaderProgram shader;
    std::vector<vertex> vertices{};
    
    Model(const std::filesystem::path & filename, ShaderProgram shader, GLuint const texture_id = 0) {
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

        Mesh Mesh(GL_TRIANGLES, shader, vertices, indices ,origin, orientation,texture_id);
        /* Mesh mesh( primitive type, shader to use, vertex list, index list,
        origin for this mesh (relative to model), orientation for this mesh (relative to model));*/

        meshes.push_back(std::move(Mesh));
        

    }

    // update position etc. based on running time
    void update(const float delta_t) {
        // origin += glm::vec3(3,0,0) * delta_t; // s = s0 + v*dt
    }
    
    void draw(glm::vec3 const & offset = glm::vec3(0.0f),
              glm::vec3 const & rotation = glm::vec3(0.0f),
              glm::vec3 const & scale_change = glm::vec3(1.0f)){

        // compute complete transformation
        /*
        glm::mat4 t = glm::translate(glm::mat4(1.0f), origin);
        glm::mat4 rx = glm::rotate(glm::mat4(1.0f), orientation.x, glm::vec3(1.0f, 0.0f, 0.0f));
        glm::mat4 ry = glm::rotate(glm::mat4(1.0f), orientation.y, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 rz = glm::rotate(glm::mat4(1.0f), orientation.z, glm::vec3(0.0f, 0.0f, 1.0f));
        glm::mat4 s = glm::scale(glm::mat4(1.0f), scale);*/

        glm::mat4 m_off = glm::translate(glm::mat4(1.0f), offset);
        glm::mat4 m_rx = glm::rotate(glm::mat4(1.0f), rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
        glm::mat4 m_ry = glm::rotate(glm::mat4(1.0f), rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 m_rz = glm::rotate(glm::mat4(1.0f), rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
        glm::mat4 m_s = glm::scale(glm::mat4(1.0f), scale_change);
        /*
        glm::mat4 mv_m = glm::identity<glm::mat4>();
        glm::mat4 local_model_matrix = mv_m * s * rz * ry * rx * t;*/
        glm::mat4 model_matrix = local_model_matrix * m_s * m_rz * m_ry * m_rx * m_off;
        normal_matrix = glm::mat3(glm::inverseTranspose(model_matrix));

        /*
        for (int i = 0; i < 4; ++i) {
            std::cout << "Local Model matrix (" << i << ")" << local_model_matrix[0][i] << " " << local_model_matrix[1][i] << " "
                << local_model_matrix[2][i] << " " << local_model_matrix[3][i] << "\n";
        }

        for (int i = 0; i < 4; ++i) {
            std::cout << "Model matrix (" << i << ")" << model_matrix[0][i] << " " << model_matrix[1][i] << " "
                << model_matrix[2][i] << " " << model_matrix[3][i] << "\n";
        }*/

        // call draw() on mesh (all meshes)
        for (auto & mesh : meshes) {
            mesh.draw(model_matrix);
        }
    }
    
    void draw(glm::mat4 const& model_matrix) {
        for (auto & mesh : meshes) {
            mesh.draw(local_model_matrix * model_matrix); 
        }
    }

};


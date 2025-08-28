#pragma once

#include <iostream> 
#include <filesystem>
#include <string>
#include <vector> 
#include <glm/glm.hpp> 
#include <algorithm>

#include "assets.hpp"
#include "Mesh.hpp"
#include "ShaderProgram.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

class Heightmap {
public:
    std::vector<Mesh> meshes;
    std::string name;
    glm::vec3 origin{0.0};
    glm::vec3 orientation{0.0};  //rotation by x,y,z axis, in radians
    glm::vec3 scale{1.0};
    glm::mat4 local_model_matrix = glm::identity<glm::mat4>();
    glm::mat3 normal_matrix = glm::identity<glm::mat3>(); //for normals calculation
    GLuint texture_id{ 0 };
    ShaderProgram shader;
    std::vector<vertex> vertices{};
    GLuint NUM_STRIPS = 0;
    GLuint NUM_VERTS_PER_STRIP = 0;
    int width = 1;
    int height = 1;
    std::vector<std::vector<float>> heightmap{};

    Heightmap() 
        :origin(0.0f), 
        orientation(0.0f), 
        scale(1.0f), 
        local_model_matrix(glm::identity<glm::mat4>()), 
        normal_matrix(glm::identity<glm::mat3>()),
        texture_id(0),
        NUM_STRIPS(0),
        NUM_VERTS_PER_STRIP(0),
        width(1),
        height(1)
    {}

    Heightmap(const std::filesystem::path& filename, ShaderProgram shader, GLuint const texture_id = 0) {
        
        // 1. load height map texture
        int nChannels;
        //stbi_set_flip_vertically_on_load(true);
        unsigned char* data = stbi_load(filename.string().c_str(), &width, &height, &nChannels, 0);
        if (!data) {
            std::cerr << "Failed to load heightmap named: " << filename << std::endl;
            return;
        }
        std::cout << "Loaded heightmap named: " << filename << std::endl;
        // 2. vertex generation
        std::vector<std::vector<float>> getheightmap(height, std::vector<float>(width)); // Needed for collision detection
        float yScale = 64.0f / 255.0f, yShift = 16.0f, xzScale = 1.0f;;  // apply a scale+shift to the height data

        for (int i = 0; i < height; i++)
        {
            for (int j = 0; j < width; j++)
            {                
                // computing normals from height differences
                int xm = std::max(i - 1, 0);
                int xp = std::min(i + 1, (int)height - 1);
                int zm = std::max(j - 1, 0);
                int zp = std::min(j + 1, (int)width - 1);

                // Heights
                auto getHeight = [&](int row, int col) {
                    row = glm::clamp(row, 0, height - 1);
                    col = glm::clamp(col, 0, width - 1);
                    unsigned char* texel = data + (col + width * row) * nChannels;
                    return static_cast<float>(texel[0]) * yScale - yShift;
                    };

                float h = getHeight(i,j);
                // store for later collision queries
                getheightmap[i][j] = h;
                float heightscale = 1.0f;
                glm::vec3 pL = glm::vec3(-height / 2.0f + (i - 1), getHeight(i - 1, j) * heightscale, -width / 2.0f + j);
                glm::vec3 pR = glm::vec3(-height / 2.0f + (i + 1), getHeight(i + 1, j) * heightscale, -width / 2.0f + j);
                glm::vec3 pD = glm::vec3(-height / 2.0f + i, getHeight(i, j - 1) * heightscale, -width / 2.0f + (j - 1));
                glm::vec3 pU = glm::vec3(-height / 2.0f + i, getHeight(i, j + 1) * heightscale, -width / 2.0f + (j + 1));
                // Build tangent vectors
                glm::vec3 dX = pR - pL;
                glm::vec3 dZ = pU - pD;
                // Normal is perpendicular to both
                glm::vec3 N = glm::normalize(glm::cross(dZ, dX));

                // vertex
                vertex v;
                v.position = glm::vec3(-height / 2.0f + i, h, -width / 2.0f + j);
                v.normal = N;
                v.texcoord = glm::vec2(static_cast<float>(j) / (width - 1), static_cast<float>(i) / (height - 1));
                vertices.push_back(v);               
            }
        }
        heightmap = getheightmap;
          
        stbi_image_free(data);

        // 3. index generation
        std::vector<GLuint> indices;
        for (GLuint i = 0; i < height - 1; i++)       // for each row a.k.a. each strip
        {
            for (GLuint j = 0; j < width; j++)      // for each column
            {
                for (GLuint k = 0; k < 2; k++)      // for each side of the strip
                {
                    indices.push_back(j + width * (i + k));
                }
            }
        }

        NUM_STRIPS = height - 1;
        NUM_VERTS_PER_STRIP = width * 2;

        Mesh Mesh(GL_TRIANGLE_STRIP, shader, vertices, indices, origin, orientation, texture_id, NUM_STRIPS, NUM_VERTS_PER_STRIP);
        /* Mesh mesh( primitive type, shader to use, vertex list, index list,
        origin for this mesh (relative to model), orientation for this mesh (relative to model));*/

        meshes.push_back(std::move(Mesh));

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
        glm::mat4 local_model_matrix = mv_m * s * rz * ry * rx * t;
        glm::mat4 model_matrix = local_model_matrix * m_s * m_rz * m_ry * m_rx * m_off;
        normal_matrix = glm::mat3(glm::inverseTranspose(model_matrix));

        // call draw() on mesh (all meshes)
        for (auto& mesh : meshes) {
            mesh.draw(model_matrix);
        }
    }

    void draw() {
        for (auto& mesh : meshes) {
            mesh.draw(local_model_matrix);
        }
    }

};

#pragma once

#include <iostream> 
#include <string>
#include <vector>

#include <glm/glm.hpp> 
#include <glm/ext.hpp>

#include "assets.hpp"
#include "ShaderProgram.hpp"
                         
class Mesh {
public:
    // mesh data
    glm::vec3 origin{};
    glm::vec3 orientation{};
    glm::mat4 model_matrix{};
    GLuint texture_id{0}; // texture id=0  means no texture
    GLenum primitive_type = GL_POINT;
    ShaderProgram shader;
    std::vector<vertex> vertices{};
    std::vector<GLuint> indices{};
    // For heightmap generation
    GLuint NUM_STRIPS = 0;
    GLuint NUM_VERTS_PER_STRIP = 0;

    // mesh material
    glm::vec4 ambient_material{1.0f}; //white, non-transparent 
    glm::vec4 diffuse_material{1.0f}; //white, non-transparent 
    glm::vec4 specular_material{1.0f}; //white, non-transparent
    float reflectivity{1.0f}; 
    
    // indirect (indexed) draw 
    Mesh(GLenum primitive_type, ShaderProgram shader, std::vector<vertex> const& vertices, std::vector<GLuint> const& indices, glm::vec3 const& origin, glm::vec3 const& orientation, GLuint const texture_id = 0, GLuint NUM_STRIPS = 0, GLuint NUM_VERTS_PER_STRIP = 0) :
        primitive_type(primitive_type),
        shader(shader),
        vertices(vertices),
        indices(indices),
        origin(origin),
        orientation(orientation),
        texture_id(texture_id),
        NUM_STRIPS(NUM_STRIPS),
        NUM_VERTS_PER_STRIP(NUM_VERTS_PER_STRIP)
    {
        
        // TODO: create and initialize VAO, VBO, EBO and parameters
        // Create the VAO and VBO
        glCreateVertexArrays(1, &VAO);
        glObjectLabel(GL_VERTEX_ARRAY, VAO, -1, "MyMeshVAO");
        // Set Vertex Attribute to explain OpenGL how to interpret the data
        GLint position_attrib_location = glGetAttribLocation(shader.getID(), "aPos");
        glVertexArrayAttribFormat(VAO, position_attrib_location, 3, GL_FLOAT, GL_FALSE, offsetof(vertex, position));
        glVertexArrayAttribBinding(VAO, position_attrib_location, 0);
        glEnableVertexArrayAttrib(VAO, position_attrib_location);
        // Set and enable Vertex Attribute for Texture Coordinates
        GLint normal_attrib_location = glGetAttribLocation(shader.getID(), "aNorm");
        glVertexArrayAttribFormat(VAO, normal_attrib_location, 3, GL_FLOAT, GL_FALSE, offsetof(vertex, normal));
        glVertexArrayAttribBinding(VAO, normal_attrib_location, 0);
        glEnableVertexArrayAttrib(VAO, normal_attrib_location);
        // Set and enable Vertex Attribute for Texture Coordinates
        GLint texture_attrib_location = glGetAttribLocation(shader.getID(), "aTex");
        glVertexArrayAttribFormat(VAO, texture_attrib_location, 2, GL_FLOAT, GL_FALSE, offsetof(vertex, texcoord));
        glVertexArrayAttribBinding(VAO, texture_attrib_location, 0);
        glEnableVertexArrayAttrib(VAO, texture_attrib_location);
        // Create and fill data
        glCreateBuffers(1, &VBO); // Vertex Buffer Object
        glObjectLabel(GL_BUFFER, VBO, -1, "MyMeshVBO");
        glCreateBuffers(1, &EBO); // Element Buffer Object
        glObjectLabel(GL_BUFFER, EBO, -1, "MyMeshEBO");
        glNamedBufferData(VBO, vertices.size() * sizeof(vertex),
                    vertices.data(), GL_STATIC_DRAW);
        glNamedBufferData(EBO, indices.size() * sizeof(GLuint),
                    indices.data(), GL_STATIC_DRAW);
        //Connect together
        glVertexArrayVertexBuffer(VAO, 0, VBO, 0, sizeof(vertex));
        glVertexArrayElementBuffer(VAO, EBO);

    };

    void draw(glm::mat4 const& model_matrix){
 		if (VAO == 0) {
			std::cerr << "VAO not initialized!\n";
			return;
		}
        
        shader.activate();
        glObjectLabel(GL_PROGRAM, shader.getID(), -1, "MyMeshShader");
        shader.setUniform("uM_m", model_matrix); //set model matrix


        //if textures are used (texID !=0 for single texture, std::vector<GLuint> textures.count() > 0 for multitexturing), set texture unit
            // - use in for loop for multitexturing, set all textures and bind to different texture units and shader variable names
        if (texture_id > 0) {
            int i = 0;
            
            glBindTextureUnit(i, texture_id);
            shader.setUniform("tex0", i);   //send texture unit number to FS            
        }

        //TODO: draw mesh: bind vertex array object, draw all elements with selected primitive type 

        glBindVertexArray(VAO);

        if (primitive_type == GL_TRIANGLE_STRIP) {
            for (GLuint strip = 0; strip < NUM_STRIPS; ++strip)
            {
                glDrawElements(GL_TRIANGLE_STRIP, NUM_VERTS_PER_STRIP, GL_UNSIGNED_INT, (void*)(sizeof(unsigned int)* NUM_VERTS_PER_STRIP* strip));
            }  
        }
        else {
            glDrawElements(primitive_type, indices.size(), GL_UNSIGNED_INT, 0);
        }
    }

	void clear(void) {

        if (texture_id) {   // or all textures in vector
            glDeleteTextures(1, &texture_id);
            texture_id = 0;
        }
        primitive_type = GL_POINT;
        // TODO: clear rest of the member variables to safe default
        ambient_material = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f); //white, non-transparent 
        diffuse_material = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f); //white, non-transparent 
        specular_material = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f); //white, non-transparent
        reflectivity = 1.0f;
        vertices.clear();
        indices.clear();
        origin = glm::vec3(0.0f);
        orientation = glm::vec3(0.0f);

        // TODO: delete all allocations 
        glDeleteBuffers(1, &VBO);
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &EBO);

    };

private:
    // OpenGL buffer IDs
    // ID = 0 is reserved (i.e. uninitalized)
     unsigned int VAO{0}, VBO{0}, EBO{0};
};
  



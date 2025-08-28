#version 460 core

in vec3 aPos; // Positions/Coordinates
in vec3 aNorm;// Normals
in vec2 aTex; // Texture Coordinates

uniform mat4 uP_m = mat4(1.0);	//Projection matrix - 
uniform mat4 uM_m = mat4(1.0);	//Model matrix - 
uniform mat4 uV_m = mat4(1.0);	//View matrix -

uniform vec4 my_color = vec4(1.0);			//Uniform to change the color of the shader

// Light properties
uniform vec3 light_position = vec3(0.0f);	//Default to origo will be changed in FS
uniform mat3 N_matrix = mat3(0.0f);

out VS_OUT {
vec4 color;		// Outputs color for FS
vec2 texCoord;	// Outputs texture coordinates for FS
vec3 N;			// normal in view space
vec3 L;			//view-space light vector
vec3 V;			//view vector (negative of the view-space position)
} vs_out;

void main() {

// Create Model-View matrix
mat4 mv_m = uV_m * uM_m;
// Calculate view-space coordinate - in P point
// we are computing the color
vec4 P = mv_m * vec4(aPos,1.0f);
vec3 P_world = vec3(uM_m * vec4(aPos,1.0));
// Calculate normal in view space
vec3 Normal = N_matrix * aNorm;
vs_out.N = mat3(mv_m) * Normal;
// Calculate view-space light vector
vs_out.L = light_position - P_world;
// Calculate view vector (negative of the view-space position)
vs_out.V = -P.xyz;
// Calculate the clip-space position of each vertex
gl_Position = uP_m * P;
//gl_Position = vec4(aPos * 0.001, 1.0); // shrink and center

// Assigns the colors somehow
vs_out.color = my_color;
// Pass the texture coordinates to "texCoord" for FS
vs_out.texCoord = aTex;

}
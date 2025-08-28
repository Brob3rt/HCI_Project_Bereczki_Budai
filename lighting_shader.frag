#version 460 core

#define MAX_LIGHTS 4

struct s_lights {
	vec4 position;	//position of the light  (w == 0.0 if Directional light 1.0 if Pointlight or Spotlight)
	vec3 ambientM;	// \
	float pad1;     //  \    -->  4 bytes (padding for alignment)
    vec3 diffuseM;	//   > Material properties they can change the color of the light (needed for ALL LIGHTS)
	float pad2;     //  /    -->  4 bytes (padding for alignment)
    vec3 specularM;	// /
	float pad3;		//       -->  4 bytes (padding for alignment)
	float consAttenuation;	// \
	float pad4;				//  \    -->  4 bytes (padding for alignment)
	float linAttenuation;	//   > Light properties for realistic light calculations (needed for SPOTLIGHTS and POINTLIGHTS)
	float quadAttenuation;	//  /
	float cutoff;     // cos(cutoff angle) (180 deg if Pointlight else Spotlight)
	vec3 direction;	//Direction the light is pointing at (only needed for SPOTLIGHTS)
    float exponent;   // spotlight edge falloff	(only needed for SPOTLIGHTS)
};
uniform s_lights lights[MAX_LIGHTS];


uniform vec3 ambient_intensity;	
// Material properties
uniform vec3 diffuse_intensity;		// \
									//  \
	//	 > The intensity of the componenets of the material properties
									//	/			
uniform vec3 specular_intensity;	// /
uniform float specular_shinines;	// Shininess of the object
//------ ------

in VS_OUT {
vec4 color;		// color for FS
vec2 texCoord;	// texture coordinates for FS
vec3 N;			// normal in view space
vec3 L;			// view-space light vector
vec3 V;			// view vector (negative of the view-space position)
} fs_in;

uniform sampler2D tex0;					// texture unit from C++
uniform vec2 tileOffset = vec2(0.0);	// the offset of one tile in a texture atlas
uniform float tileSize = 1.0;			// size of one tile from a texture atlas
out vec4 FragColor; 					// Final output


//------ Lighting calculations (using Phong lighting model) ------
vec4 DirectionalLight(int i){
	// Normalize the incoming N, L and V vectors
	vec3 L_raw = fs_in.L + lights[i].position.xyz;
	vec3 N = normalize(fs_in.N);
	vec3 L = normalize(L_raw);
	vec3 V = normalize(fs_in.V);
	// Calculate R by reflecting -L around the plane defined by N
	vec3 R = reflect(-L, N);
	// Calculate the ambient, diffuse and specular contributions
	vec3 ambient = lights[i].ambientM * ambient_intensity;
	vec3 diffuse = max(dot(N, L), 0.0) * lights[i].diffuseM * diffuse_intensity;
	vec3 specular = pow(max(dot(R, V), 0.0), specular_shinines) * lights[i].specularM * specular_intensity;
	
	return vec4(ambient + diffuse + specular, 1.0);
}
vec4 PointLight(int i){
	// Normalize the incoming N, L and V vectors
	vec3 L_raw = fs_in.L + lights[i].position.xyz;
	vec3 N = normalize(fs_in.N);
	vec3 V = normalize(fs_in.V);
	float d = length(L_raw);	// vector to light source
	vec3 L = normalize(L_raw);

	float dist_attenuation = 1.0 / (lights[i].consAttenuation + lights[i].linAttenuation * d + lights[i].quadAttenuation * d * d);

	// Calculate R by reflecting -L around the plane defined by N
	vec3 R = reflect(-L, N);
	// Calculate the ambient, diffuse and specular contributions
	vec3 ambient = lights[i].ambientM * ambient_intensity;
	vec3 diffuse = max(dot(N, L), 0.0) * lights[i].diffuseM * diffuse_intensity;
	vec3 specular = pow(max(dot(R, V), 0.0), specular_shinines) * lights[i].specularM * specular_intensity;

	return vec4(ambient + dist_attenuation * (diffuse + specular), 1.0);
}

vec4 SpotLight(int i){
	// Normalize the incoming N, L and V vectors
	vec3 L_raw = fs_in.L + lights[i].position.xyz;
	vec3 N = normalize(fs_in.N);
	vec3 V = normalize(fs_in.V);
	float d = length(L_raw);	// vector to light source
	vec3 L = normalize(L_raw);

	float dist_attenuation = 1.0 / (lights[i].consAttenuation + lights[i].linAttenuation * d + lights[i].quadAttenuation * d * d);
	float full_attenuation = 0.0; //out of cone

	float spotEffect = dot(normalize(lights[i].direction), -L);
	if (spotEffect > cos(radians(lights[i].cutoff)))
		full_attenuation = dist_attenuation * pow(spotEffect, lights[i].exponent);

	// Calculate R by reflecting -L around the plane defined by N
	vec3 R = reflect(-L, N);
	// Calculate the ambient, diffuse and specular contributions
	vec3 ambient = lights[i].ambientM * ambient_intensity;
	vec3 diffuse = max(dot(N, L), 0.0) * lights[i].diffuseM * diffuse_intensity;
	vec3 specular = pow(max(dot(R, V), 0.0), specular_shinines) * lights[i].specularM * specular_intensity;

	return vec4(ambient + full_attenuation * (diffuse + specular), 1.0);
}

//------ Lighting calculations end ------

//------ For fog calculations ------
uniform vec4 fog_color = vec4(vec3(0.85f), 1.0f); // black, non-transparent = night
uniform float near = 0.1f;
uniform float far = 300.0f;
float log_depth(float depth, float steepness, float offset){
float linear_depth = (2.0 * near * far) / (far + near - (depth * 2.0 - 1.0) * (far - near));
return (1 / (1 + exp(steepness * (linear_depth - offset))));
}

void main() {

vec2 tileUV = fs_in.texCoord * tileSize + tileOffset; //remapping the texturev coordinates for one tile from the texture atlas
vec4 light_result = vec4(0.0, 0.0, 0.0, 0.0);
vec4 additional_lights = vec4(1.0, 1.0, 1.0, 1.0);
// Calculating the lighting based on the number and type of lights in the s_lights structure 
for (int i = 0; i < MAX_LIGHTS; ++i) {
	bool isDirectional = lights[i].position.w == 0.0;
	bool isPoint = lights[i].cutoff == 180.0; 
	if (isDirectional == true)
		additional_lights = DirectionalLight(i);
	else if (isPoint == true)
		additional_lights = PointLight(i);
	else
		additional_lights = SpotLight(i);

	light_result += additional_lights; 
}
//FragColor = fs_in.color * texture(tex0, tileUV) * light_result;	//Final output of FS
vec4 PreFogColor = fs_in.color * texture(tex0, tileUV) * light_result;

float depth = log_depth(gl_FragCoord.z, 0.02f, 200.0f);
FragColor = mix(fog_color, PreFogColor, depth); // linear interpolation
/*
// Debug visualization of normals
    vec3 normalColor = normalize(fs_in.N) * 0.5 + 0.5;
    FragColor = vec4(normalColor, 1.0);*/
}

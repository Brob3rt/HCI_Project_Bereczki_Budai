// This whole program was imported from the drive -> updated so it handles quads
#include <string>
#include <GL/glew.h> 
#include <glm/glm.hpp>

#include "OBJloader.hpp"

#define MAX_LINE_SIZE 255

bool loadOBJ(const char * path, std::vector < glm::vec3 > & out_vertices, std::vector < glm::vec2 > & out_uvs, std::vector < glm::vec3 > & out_normals)
{
	std::vector< unsigned int > vertexIndices, uvIndices, normalIndices;
	std::vector< glm::vec3 > temp_vertices;
	std::vector< glm::vec2 > temp_uvs;
	std::vector< glm::vec3 > temp_normals;

	out_vertices.clear();
	out_uvs.clear();
	out_normals.clear();

	FILE * file;
	fopen_s(&file, path, "r");
	if (file == NULL) {
		printf("Impossible to open the file !\n");
		return false;
	}

	while (1) {

		char lineHeader[MAX_LINE_SIZE];
		int res = fscanf_s(file, "%s", lineHeader, MAX_LINE_SIZE);
		if (res == EOF) {
			break;
		}

		if (strcmp(lineHeader, "v") == 0) {
			glm::vec3 vertex;
			fscanf_s(file, "%f %f %f\n", &vertex.x, &vertex.y, &vertex.z);
			temp_vertices.push_back(vertex);
		}
		else if (strcmp(lineHeader, "vt") == 0) {
			glm::vec2 uv;
			fscanf_s(file, "%f %f\n", &uv.x, &uv.y);
			// Flip the V coordinate for OpenGL
			uv.y = 1.0f - uv.y;
			temp_uvs.push_back(uv);
		}
		else if (strcmp(lineHeader, "vn") == 0) {
			glm::vec3 normal;
			fscanf_s(file, "%f %f %f\n", &normal.x, &normal.y, &normal.z);
			temp_normals.push_back(normal);
		}
		else if (strcmp(lineHeader, "f") == 0) {
			unsigned int v[4], t[4], n[4];
			int count = fscanf_s(file, "%d/%d/%d %d/%d/%d %d/%d/%d %d/%d/%d\n",
				&v[0], &t[0], &n[0],
				&v[1], &t[1], &n[1],
				&v[2], &t[2], &n[2],
				&v[3], &t[3], &n[3]);

			if (count == 9) {
				// It's a triangle
				vertexIndices.push_back(v[0]); uvIndices.push_back(t[0]); normalIndices.push_back(n[0]);
				vertexIndices.push_back(v[1]); uvIndices.push_back(t[1]); normalIndices.push_back(n[1]);
				vertexIndices.push_back(v[2]); uvIndices.push_back(t[2]); normalIndices.push_back(n[2]);
			}
			else if (count == 12) {
				// It's a quad: triangulate into two triangles (0,1,2) and (0,2,3)
				vertexIndices.push_back(v[0]); uvIndices.push_back(t[0]); normalIndices.push_back(n[0]);
				vertexIndices.push_back(v[1]); uvIndices.push_back(t[1]); normalIndices.push_back(n[1]);
				vertexIndices.push_back(v[2]); uvIndices.push_back(t[2]); normalIndices.push_back(n[2]);

				vertexIndices.push_back(v[0]); uvIndices.push_back(t[0]); normalIndices.push_back(n[0]);
				vertexIndices.push_back(v[2]); uvIndices.push_back(t[2]); normalIndices.push_back(n[2]);
				vertexIndices.push_back(v[3]); uvIndices.push_back(t[3]); normalIndices.push_back(n[3]);
			}
			else {
				printf("Face format not supported (line: %s)\n", lineHeader);
				return false;
			}
		}
	}

	// unroll from indirect to direct vertex specification
	// sometimes not necessary, definitely not optimal
	for (unsigned int i = 0; i < vertexIndices.size(); i++) {
		unsigned int vertexIndex = vertexIndices[i];
		unsigned int uvIndex = uvIndices[i];
		unsigned int normalIndex = normalIndices[i];

		glm::vec3 vertex = temp_vertices[vertexIndex - 1];
		glm::vec2 uv = temp_uvs[uvIndex - 1];
		glm::vec3 normal = temp_normals[normalIndex - 1];

		out_vertices.push_back(vertex);
		out_uvs.push_back(uv);
		out_normals.push_back(normal);
	}

	fclose(file);
	return true;
}

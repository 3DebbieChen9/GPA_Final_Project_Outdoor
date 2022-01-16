#include "src\basic\SceneRenderer.h"
#include <GLFW\glfw3.h>
#include "SceneSetting.h"

#include "assimp/scene.h"
#include "assimp/cimport.h"
#include "assimp/postprocess.h"
#include "../externals/include/stb_image.h"

#pragma comment (lib, "lib-vc2015\\glfw3.lib")
#pragma comment (lib, "assimp\\assimp-vc140-mtd.lib")

#define TEXTURE_FINAL 5
#define TEXTURE_DIFFUSE 6
#define TEXTURE_AMBIENT 7
#define TEXTURE_SPECULAR 8
#define TEXTURE_WS_POSITION 9
#define TEXTURE_WS_NORMAL 10
#define TEXTURE_WS_TANGENT 11
#define TEXTURE_PHONG 12
#define TEXTURE_PHONGSHADOW 13
#define TEXTURE_BLOOMHDR 14


using namespace glm;
using namespace std;

int FRAME_WIDTH = 1024;
int FRAME_HEIGHT = 768;

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void mouseScrollCallback(GLFWwindow *window, double xoffset, double yoffset);
void cursorPosCallback(GLFWwindow* window, double x, double y);
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

void initializeGL();
void resizeGL(GLFWwindow *window, int w, int h);
void paintGL();
void updateState();
void adjustCameraPositionWithTerrain();
void updateAirplane(const glm::mat4 &viewMatrix);
void initScene();

bool m_leftButtonPressed = false;
bool m_leftButtonPressedFirst = false;
bool m_rightButtonPressed = false;
double cursorPos[2];

SceneRenderer *m_renderer = nullptr;
glm::vec3 m_lookAtCenter;
glm::vec3 m_eye;

mat4 m_cameraProjection(1.0f);
mat4 m_cameraView(1.0f);
#pragma region Trackball
vec3 m_lookDirection = vec3(0.0f, 0.0f, 0.0f);
vec3 m_upDirection = vec3(0.0f, 1.0f, 0.0f);
vec3 m_uAxis = vec3(1.0f, 0.0f, 0.0f);
vec3 m_vAxis = vec3(0.0f, 1.0f, 0.0f);
float m_cameraMoveSpeed = 2.0f; //0.05f;
vec2 m_trackball_initial = vec2(0, 0);
vec2 m_oldDistance = vec2(0, 0);
vec2 m_rotateAngle = vec2(0.0f, 0.0f);
#pragma endregion

// vec3 lightPosition = vec3(0.2f, 0.6f, 0.5f);
vec3 lightPosition = vec3(636.48, 100.79, 495.98);
void vsyncEnabled(GLFWwindow *window);
void vsyncDisabled(GLFWwindow *window);

PlantManager *m_plantManager = nullptr;
Terrain *m_terrain = nullptr;

// the airplane's transformation has been handled
glm::vec3 m_airplanePosition;
glm::mat4 m_airplaneRotMat;

#pragma region Structure
struct Shape
{
	GLuint vao;
	GLuint vbo_position;
	GLuint vbo_normal;
	GLuint vbo_texcoord;
	GLuint vbo_tangents;
	GLuint vbo_bittangents;
	GLuint ibo;
	int drawCount;
	int materialID;
};

struct Material
{
	GLuint diffuse_tex;
	glm::vec4 ka;
	glm::vec4 kd;
	glm::vec4 ks;
};

typedef struct TextureData
{
	TextureData() : width(0), height(0), data(0) {}
	int width;
	int height;
	unsigned char* data;
} TextureData;
#pragma endregion
void printGLError() {
	GLenum code = glGetError();
	switch (code)
	{
	case GL_NO_ERROR:
		std::cout << "GL_NO_ERROR" << std::endl;
		break;
	case GL_INVALID_ENUM:
		std::cout << "GL_INVALID_ENUM" << std::endl;
		break;
	case GL_INVALID_VALUE:
		std::cout << "GL_INVALID_VALUE" << std::endl;
		break;
	case GL_INVALID_OPERATION:
		std::cout << "GL_INVALID_OPERATION" << std::endl;
		break;
	case GL_INVALID_FRAMEBUFFER_OPERATION:
		std::cout << "GL_INVALID_FRAMEBUFFER_OPERATION" << std::endl;
		break;
	case GL_OUT_OF_MEMORY:
		std::cout << "GL_OUT_OF_MEMORY" << std::endl;
		break;
	case GL_STACK_UNDERFLOW:
		std::cout << "GL_STACK_UNDERFLOW" << std::endl;
		break;
	case GL_STACK_OVERFLOW:
		std::cout << "GL_STACK_OVERFLOW" << std::endl;
		break;
	default:
		std::cout << "GL_ERROR" << std::endl;
	}
}

#pragma region Default Loader
// load a png image and return a TextureData structure with raw data
// not limited to png format. works with any image format that is RGBA-32bit
TextureData loadImg(const char* path)
{
	TextureData texture;
	int n;
	stbi_set_flip_vertically_on_load(true);
	stbi_uc *data = stbi_load(path, &texture.width, &texture.height, &n, 4);
	if (data != NULL)
	{
		texture.data = new unsigned char[texture.width * texture.height * 4 * sizeof(unsigned char)];
		memcpy(texture.data, data, texture.width * texture.height * 4 * sizeof(unsigned char));
		cout << "Load Texture: " << path << " | width:" << texture.width << " | height: " << texture.height << endl;
		stbi_image_free(data);
	}
	return texture;
}
#pragma endregion

struct {
	GLuint diffuse;
	GLuint ambient;
	GLuint specular;

	GLuint ws_position;
	GLuint ws_normal;
	GLuint ws_tangent;

	GLuint phongColor;
	GLuint bloomHDR;
	GLuint phongShadow;

	GLuint dirDepth;

	GLuint finalTex;
} frameBufferTexture;

int CURRENT_TEX = TEXTURE_FINAL; // Default as FINAL

Shader *m_objectShader = nullptr;
struct {
	struct {
		mat4 model = mat4(1.0f);
		mat4 scale = mat4(1.0f);
		mat4 rotate = mat4(1.0f);
	} matrix;

	GLuint normalTexture;

	vector<Shape> shapes;
	vector<Material> materials;
	bool hasNormalMap;
	bool useNormalMap;
} m_airplane;
struct {
	struct {
		mat4 model = mat4(1.0f);
		mat4 scale = mat4(1.0f);
		mat4 rotate = mat4(1.0f);
	} matrixA;
	struct {
		mat4 model = mat4(1.0f);
		mat4 scale = mat4(1.0f);
		mat4 rotate = mat4(1.0f);
	} matrixB;

	GLuint normalTexture;

	vector<Shape> shapes;
	vector<Material> materials;

	bool hasNormalMap;
	bool useNormalMap;
} m_houses;
struct {
	mat4 model = mat4(1.0f);
	vector<Shape> shapes;
	GLuint whiteTexture;
	bool hasNormalMap = false;
	bool useNormalMap = false;
} m_sphere;

struct {
	Shader *shader;
	GLuint depthFBO;
	int shadowSize = 2048;
	float near = 0.0f, far = 1000.0f;
	vec3 LightPos = vec3(636.48, 134.79, 495.98);
	vec3 LightDir = vec3(0.2, 0.6, 0.5);
	mat4 LightVP = mat4(1.0f);

	// vec3 dir_light_pos = vec3(-1.14f, 3.0f, 0.77f);
	// vec3 dir_light = vec3(-2.51449f, 0.477241f, -1.21263f);
	// // vec3 dir_light = vec3(-2.51449f, -0.477241f, -1.21263f);
	// float dir_shadow_near=0.0f, dir_shadow_far=10.0f;
	// mat4 dir_light_vp;
} m_dirShadow;
struct {
	Shader* shader;
	GLuint vao;
	GLuint vbo;
} m_windowFrame;
struct {
	GLuint fbo;
	GLuint depthRBO;
} m_genTexture;
struct {
	Shader *shader;
	GLuint fbo;
	GLuint depthRBO;
} m_deferred;

void m_airplane_loadModel() {
	char* filepath = ".\\models\\airplane.obj";
	const aiScene* scene = aiImportFile(filepath, aiProcessPreset_TargetRealtime_MaxQuality);

	// Material
	for (unsigned int i = 0; i < scene->mNumMaterials; ++i) {
		aiMaterial* material = scene->mMaterials[i];
		Material Material;
		aiString texturePath;
		aiColor3D color;
		if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == aiReturn_SUCCESS) {
			cout << texturePath.C_Str() << endl;
			TextureData tex = loadImg(texturePath.C_Str());
			glGenTextures(1, &Material.diffuse_tex);
			glBindTexture(GL_TEXTURE_2D, Material.diffuse_tex);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, tex.width, tex.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex.data);
			glGenerateMipmap(GL_TEXTURE_2D);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}
		else {
			cout << "no texture or mtl loading failed" << endl;
		}
		if (material->Get(AI_MATKEY_COLOR_AMBIENT, color) == aiReturn_SUCCESS) {
			Material.ka = glm::vec4(color.r, color.g, color.b, 1.0f);
		}
		else {
			cout << "no ambient" << endl;
		}
		if (material->Get(AI_MATKEY_COLOR_DIFFUSE, color) == aiReturn_SUCCESS) {
			Material.kd = glm::vec4(color.r, color.g, color.b, 1.0f);
		}
		else {
			cout << "no diffuse" << endl;
		}
		if (material->Get(AI_MATKEY_COLOR_SPECULAR, color) == aiReturn_SUCCESS) {
			Material.ks = glm::vec4(color.r, color.g, color.b, 1.0f);
		}
		else {
			cout << "no specular" << endl;
		}
		// save material
		m_airplane.materials.push_back(Material);
	}
	// Geometry
	for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
	{
		aiMesh* mesh = scene->mMeshes[i];
		Shape shape;
		glGenVertexArrays(1, &shape.vao);
		glBindVertexArray(shape.vao);

		vector<float> position;
		vector<float> normal;
		vector<float> texcoord;
		vector<float> tangent;
		vector<float> bittangent;

		for (unsigned int v = 0; v < mesh->mNumVertices; ++v)
		{
			// mesh->mVertices[v][0~2] => position
			position.push_back(mesh->mVertices[v][0]);
			position.push_back(mesh->mVertices[v][1]);
			position.push_back(mesh->mVertices[v][2]);
			// mesh->mTextureCoords[0][v][0~1] => texcoord
			texcoord.push_back(mesh->mTextureCoords[0][v][0]);
			texcoord.push_back(mesh->mTextureCoords[0][v][1]);
			// mesh->mNormals[v][0~2] => normal
			normal.push_back(mesh->mNormals[v][0]);
			normal.push_back(mesh->mNormals[v][1]);
			normal.push_back(mesh->mNormals[v][2]);

			if (mesh->HasTangentsAndBitangents()) {
				// mesh->mTangents[v][0~2] => tangent
				tangent.push_back(mesh->mTangents[v][0]);
				tangent.push_back(mesh->mTangents[v][1]);
				tangent.push_back(mesh->mTangents[v][2]);
				// mesh->mBitangents[v][0~2] => bittangent
				bittangent.push_back(mesh->mBitangents[v][0]);
				bittangent.push_back(mesh->mBitangents[v][1]);
				bittangent.push_back(mesh->mBitangents[v][2]);
			}
			else {
				cout << "no tanagent" << endl; 
			}
		}

		// create 3 vbos to hold data
		glGenBuffers(1, &shape.vbo_position);
		glGenBuffers(1, &shape.vbo_texcoord);
		glGenBuffers(1, &shape.vbo_normal);
		glGenBuffers(1, &shape.vbo_tangents);
		glGenBuffers(1, &shape.vbo_bittangents);

		// position
		glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_position);
		glBufferData(GL_ARRAY_BUFFER, mesh->mNumVertices * 3 * sizeof(float), &position[0], GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(0);

		// texcoord
		glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_texcoord);
		glBufferData(GL_ARRAY_BUFFER, mesh->mNumVertices * 2 * sizeof(float), &texcoord[0], GL_STATIC_DRAW);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(1);

		// normal
		glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_normal);
		glBufferData(GL_ARRAY_BUFFER, mesh->mNumVertices * 3 * sizeof(float), &normal[0], GL_STATIC_DRAW);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(2);

		// tangent
		glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_tangents);
		glBufferData(GL_ARRAY_BUFFER, mesh->mNumVertices * 3 * sizeof(float), &tangent[0], GL_STATIC_DRAW);
		glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(3);

		// bittangent
		glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_bittangents);
		glBufferData(GL_ARRAY_BUFFER, mesh->mNumVertices * 3 * sizeof(float), &bittangent[0], GL_STATIC_DRAW);
		glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(4);

		vector<unsigned int> face;
		for (unsigned int f = 0; f < mesh->mNumFaces; ++f)
		{
			// mesh->mFaces[f].mIndices[0~2] => index
			face.push_back(mesh->mFaces[f].mIndices[0]);
			face.push_back(mesh->mFaces[f].mIndices[1]);
			face.push_back(mesh->mFaces[f].mIndices[2]);
		}
		// create 1 ibo to hold data
		glGenBuffers(1, &shape.ibo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, shape.ibo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->mNumFaces * 3 * sizeof(unsigned int), &face[0], GL_STATIC_DRAW);

		shape.materialID = mesh->mMaterialIndex;
		shape.drawCount = mesh->mNumFaces * 3;
		// save shape
		m_airplane.shapes.push_back(shape);

		position.clear();
		position.shrink_to_fit();
		texcoord.clear();
		texcoord.shrink_to_fit();
		normal.clear();
		normal.shrink_to_fit();
		tangent.clear();
		tangent.shrink_to_fit();
		face.clear();
		face.shrink_to_fit();
	}

	aiReleaseImport(scene);
	cout << "Load airplane.obj done" << endl;
}
void m_house_loadModel() {
	char* filepath = ".\\models\\medievalHouse.obj";
	const aiScene* scene = aiImportFile(filepath, aiProcessPreset_TargetRealtime_MaxQuality);

	// Material
	for (unsigned int i = 0; i < scene->mNumMaterials; ++i) {
		aiMaterial* material = scene->mMaterials[i];
		Material Material;
		aiString texturePath;
		aiColor3D color;
		if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == aiReturn_SUCCESS) {

			cout << texturePath.C_Str() << endl;
			TextureData tex = loadImg(texturePath.C_Str());
			glGenTextures(1, &Material.diffuse_tex);
			glBindTexture(GL_TEXTURE_2D, Material.diffuse_tex);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, tex.width, tex.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex.data);
			glGenerateMipmap(GL_TEXTURE_2D);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}
		else {
			cout << "no texture or mtl loading failed" << endl;
		}
		if (material->Get(AI_MATKEY_COLOR_AMBIENT, color) == aiReturn_SUCCESS) {
			Material.ka = glm::vec4(color.r, color.g, color.b, 1.0f);
		}
		else {
			cout << "no ambient" << endl;
		}
		if (material->Get(AI_MATKEY_COLOR_DIFFUSE, color) == aiReturn_SUCCESS) {
			Material.kd = glm::vec4(color.r, color.g, color.b, 1.0f);
		}
		else {
			cout << "no diffuse" << endl;
		}
		if (material->Get(AI_MATKEY_COLOR_SPECULAR, color) == aiReturn_SUCCESS) {
			Material.ks = glm::vec4(color.r, color.g, color.b, 1.0f);
		}
		else {
			cout << "no specular" << endl;
		}
		// save material
		m_houses.materials.push_back(Material);
	}
	// Geometry
	for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
	{
		aiMesh* mesh = scene->mMeshes[i];
		Shape shape;
		glGenVertexArrays(1, &shape.vao);
		glBindVertexArray(shape.vao);

		vector<float> position;
		vector<float> normal;
		vector<float> texcoord;
		vector<float> tangent;
		vector<float> bittangent;

		for (unsigned int v = 0; v < mesh->mNumVertices; ++v)
		{
			// mesh->mVertices[v][0~2] => position
			position.push_back(mesh->mVertices[v][0]);
			position.push_back(mesh->mVertices[v][1]);
			position.push_back(mesh->mVertices[v][2]);
			// mesh->mTextureCoords[0][v][0~1] => texcoord
			texcoord.push_back(mesh->mTextureCoords[0][v][0]);
			texcoord.push_back(mesh->mTextureCoords[0][v][1]);
			// mesh->mNormals[v][0~2] => normal
			normal.push_back(mesh->mNormals[v][0]);
			normal.push_back(mesh->mNormals[v][1]);
			normal.push_back(mesh->mNormals[v][2]);

			if (mesh->HasTangentsAndBitangents()) {
				// mesh->mTangents[v][0~2] => tangent
				tangent.push_back(mesh->mTangents[v][0]);
				tangent.push_back(mesh->mTangents[v][1]);
				tangent.push_back(mesh->mTangents[v][2]);
				// mesh->mBitangents[v][0~2] => bittangent
				bittangent.push_back(mesh->mBitangents[v][0]);
				bittangent.push_back(mesh->mBitangents[v][1]);
				bittangent.push_back(mesh->mBitangents[v][2]);
			}
			else {
				cout << "no tangent" << endl;
			}
		}

		// create vbos to hold data
		glGenBuffers(1, &shape.vbo_position);
		glGenBuffers(1, &shape.vbo_texcoord);
		glGenBuffers(1, &shape.vbo_normal);
		glGenBuffers(1, &shape.vbo_tangents);
		glGenBuffers(1, &shape.vbo_bittangents);

		// position
		glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_position);
		glBufferData(GL_ARRAY_BUFFER, mesh->mNumVertices * 3 * sizeof(float), &position[0], GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(0);

		// texcoord
		glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_texcoord);
		glBufferData(GL_ARRAY_BUFFER, mesh->mNumVertices * 2 * sizeof(float), &texcoord[0], GL_STATIC_DRAW);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(1);

		// normal
		glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_normal);
		glBufferData(GL_ARRAY_BUFFER, mesh->mNumVertices * 3 * sizeof(float), &normal[0], GL_STATIC_DRAW);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(2);

		// tangent
		glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_tangents);
		glBufferData(GL_ARRAY_BUFFER, mesh->mNumVertices * 3 * sizeof(float), &tangent[0], GL_STATIC_DRAW);
		glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(3);

		// bittangent
		glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_bittangents);
		glBufferData(GL_ARRAY_BUFFER, mesh->mNumVertices * 3 * sizeof(float), &bittangent[0], GL_STATIC_DRAW);
		glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(4);

		vector<unsigned int> face;
		for (unsigned int f = 0; f < mesh->mNumFaces; ++f)
		{
			// mesh->mFaces[f].mIndices[0~2] => index
			face.push_back(mesh->mFaces[f].mIndices[0]);
			face.push_back(mesh->mFaces[f].mIndices[1]);
			face.push_back(mesh->mFaces[f].mIndices[2]);
		}
		// create 1 ibo to hold data
		glGenBuffers(1, &shape.ibo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, shape.ibo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->mNumFaces * 3 * sizeof(unsigned int), &face[0], GL_STATIC_DRAW);

		shape.materialID = mesh->mMaterialIndex;
		shape.drawCount = mesh->mNumFaces * 3;
		// save shape
		m_houses.shapes.push_back(shape);

		position.clear();
		position.shrink_to_fit();
		texcoord.clear();
		texcoord.shrink_to_fit();
		normal.clear();
		normal.shrink_to_fit();
		tangent.clear();
		tangent.shrink_to_fit();
		face.clear();
		face.shrink_to_fit();
	}

	aiReleaseImport(scene);

	TextureData normalImg = loadImg(".\\models\\textures\\Medieval_house_Normal.png");
	glGenTextures(1, &m_houses.normalTexture);
	glBindTexture(GL_TEXTURE_2D, m_houses.normalTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, normalImg.width, normalImg.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, normalImg.data);
	glGenerateMipmap(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	cout << "Load medievalHouse.obj done" << endl;
}
void m_sphere_loadModel() {
	char* filepath = ".\\models\\Sphere.obj";
	const aiScene* scene = aiImportFile(filepath, aiProcessPreset_TargetRealtime_MaxQuality);
	// Geometry
	for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
	{
		aiMesh* mesh = scene->mMeshes[i];
		Shape shape;
		glGenVertexArrays(1, &shape.vao);
		glBindVertexArray(shape.vao);

		vector<float> position;
		vector<float> normal;
		vector<float> texcoord;
		vector<float> tangent;
		vector<float> bittangent;

		for (unsigned int v = 0; v < mesh->mNumVertices; ++v)
		{
			// mesh->mVertices[v][0~2] => position
			position.push_back(mesh->mVertices[v][0]);
			position.push_back(mesh->mVertices[v][1]);
			position.push_back(mesh->mVertices[v][2]);
			// mesh->mTextureCoords[0][v][0~1] => texcoord
			texcoord.push_back(mesh->mTextureCoords[0][v][0]);
			texcoord.push_back(mesh->mTextureCoords[0][v][1]);
			// mesh->mNormals[v][0~2] => normal
			normal.push_back(mesh->mNormals[v][0]);
			normal.push_back(mesh->mNormals[v][1]);
			normal.push_back(mesh->mNormals[v][2]);

			if (mesh->HasTangentsAndBitangents()) {
				// mesh->mTangents[v][0~2] => tangent
				tangent.push_back(mesh->mTangents[v][0]);
				tangent.push_back(mesh->mTangents[v][1]);
				tangent.push_back(mesh->mTangents[v][2]);
				// mesh->mBitangents[v][0~2] => bittangent
				bittangent.push_back(mesh->mBitangents[v][0]);
				bittangent.push_back(mesh->mBitangents[v][1]);
				bittangent.push_back(mesh->mBitangents[v][2]);
			}
			else {
				cout << "no tangent" << endl;
			}
		}

		// create vbos to hold data
		glGenBuffers(1, &shape.vbo_position);
		glGenBuffers(1, &shape.vbo_texcoord);
		glGenBuffers(1, &shape.vbo_normal);
		glGenBuffers(1, &shape.vbo_tangents);

		// position
		glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_position);
		glBufferData(GL_ARRAY_BUFFER, mesh->mNumVertices * 3 * sizeof(float), &position[0], GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(0);

		// texcoord
		glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_texcoord);
		glBufferData(GL_ARRAY_BUFFER, mesh->mNumVertices * 2 * sizeof(float), &texcoord[0], GL_STATIC_DRAW);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(1);

		// normal
		glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_normal);
		glBufferData(GL_ARRAY_BUFFER, mesh->mNumVertices * 3 * sizeof(float), &normal[0], GL_STATIC_DRAW);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(2);

		// tangent
		glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_tangents);
		glBufferData(GL_ARRAY_BUFFER, mesh->mNumVertices * 3 * sizeof(float), &tangent[0], GL_STATIC_DRAW);
		glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(3);

		// bittangent
		glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_bittangents);
		glBufferData(GL_ARRAY_BUFFER, mesh->mNumVertices * 3 * sizeof(float), &bittangent[0], GL_STATIC_DRAW);
		glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(4);

		vector<unsigned int> face;
		for (unsigned int f = 0; f < mesh->mNumFaces; ++f)
		{
			// mesh->mFaces[f].mIndices[0~2] => index
			face.push_back(mesh->mFaces[f].mIndices[0]);
			face.push_back(mesh->mFaces[f].mIndices[1]);
			face.push_back(mesh->mFaces[f].mIndices[2]);
		}
		// create 1 ibo to hold data
		glGenBuffers(1, &shape.ibo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, shape.ibo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->mNumFaces * 3 * sizeof(unsigned int), &face[0], GL_STATIC_DRAW);

		shape.materialID = mesh->mMaterialIndex;
		shape.drawCount = mesh->mNumFaces * 3;
		// save shape
		m_sphere.shapes.push_back(shape);

		position.clear();
		position.shrink_to_fit();
		texcoord.clear();
		texcoord.shrink_to_fit();
		normal.clear();
		normal.shrink_to_fit();
		tangent.clear();
		tangent.shrink_to_fit();
		face.clear();
		face.shrink_to_fit();
	}

	aiReleaseImport(scene);

	TextureData normalImg = loadImg(".\\models\\textures\\defaultTexture.png");
	glGenTextures(1, &m_sphere.whiteTexture);
	glBindTexture(GL_TEXTURE_2D, m_sphere.whiteTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, normalImg.width, normalImg.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, normalImg.data);
	glGenerateMipmap(GL_TEXTURE_2D);
	cout << "Load Sphere.obj done" << endl;
}
void m_objects_init() {
	m_airplane_loadModel();
	m_airplane.matrix.rotate = m_airplaneRotMat;
	m_airplane.matrix.model = translate(mat4(1.0f), m_airplanePosition);
	m_airplane.hasNormalMap = false;
	m_airplane.useNormalMap = false;

	m_house_loadModel();
	m_houses.matrixA.rotate = rotate(mat4(1.0f), radians(60.0f), vec3(0.0f, 1.0f, 0.0f));
	m_houses.matrixA.model = translate(mat4(1.0f), vec3(631.0f, 130.0f, 468.0f));
	m_houses.matrixB.rotate = rotate(mat4(1.0f), radians(15.0f), vec3(0.0f, 1.0f, 0.0f));
	m_houses.matrixB.model = translate(mat4(1.0f), vec3(656.0f, 135.0f, 483.0f));
	m_houses.hasNormalMap = true;
	m_houses.useNormalMap = true;

	m_sphere_loadModel();
	m_sphere.model = translate(mat4(1.0f), vec3(636.48f, 134.79f, 495.98f));
	m_sphere.hasNormalMap = false;
	m_sphere.useNormalMap = false;

	m_objectShader = new Shader("assets\\GenTextures_vs.vs.glsl", "assets\\GenTextures_fs.fs.glsl");
	m_objectShader->useShader();
	const GLuint programID = m_objectShader->getProgramID();
	cout << "Objects programID: " << programID << endl;
	glUniform1i(glGetUniformLocation(programID, "diffuseTexture"), 3);
	glUniform1i(glGetUniformLocation(programID, "normalTexture"), 4);
	m_objectShader->disableShader();
}
void m_objects_shadowRender() {
	
	m_dirShadow.shader->useShader();
	const GLuint programID = m_dirShadow.shader->getProgramID();

	glUniformMatrix4fv(glGetUniformLocation(programID, "light_vp"), 1, GL_FALSE, value_ptr(m_dirShadow.LightVP));

	// Draw Airplane
	m_airplane.matrix.rotate = m_airplaneRotMat;
	m_airplane.matrix.model = translate(mat4(1.0f), m_airplanePosition);
	glUniformMatrix4fv(glGetUniformLocation(programID, "um4m"), 1, GL_FALSE, value_ptr(m_airplane.matrix.model * m_airplane.matrix.rotate));
	for (int i = 0; i < m_airplane.shapes.size(); i++) {
		int materialID = m_airplane.shapes[i].materialID;

		glBindVertexArray(m_airplane.shapes[i].vao);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_airplane.shapes[i].ibo);

		glDrawElements(GL_TRIANGLES, m_airplane.shapes[i].drawCount, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	// Draw HouseA
	glUniformMatrix4fv(glGetUniformLocation(programID, "um4m"), 1, GL_FALSE, value_ptr(m_houses.matrixA.model * m_houses.matrixA.rotate));
	for (int i = 0; i < m_houses.shapes.size(); i++) {
		int materialID = m_houses.shapes[i].materialID;

		glBindVertexArray(m_houses.shapes[i].vao);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_houses.shapes[i].ibo);

		glDrawElements(GL_TRIANGLES, m_houses.shapes[i].drawCount, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	// Draw HouseB
	glUniformMatrix4fv(glGetUniformLocation(programID, "um4m"), 1, GL_FALSE, value_ptr(m_houses.matrixB.model * m_houses.matrixB.rotate));
	for (int i = 0; i < m_houses.shapes.size(); i++) {
		int materialID = m_houses.shapes[i].materialID;

		glBindVertexArray(m_houses.shapes[i].vao);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_houses.shapes[i].ibo);

		glDrawElements(GL_TRIANGLES, m_houses.shapes[i].drawCount, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	m_renderer->m_terrain->updateShadowMap(programID);
	
	m_dirShadow.shader->disableShader();
}
void m_objects_render() {
	m_airplane.matrix.rotate = m_airplaneRotMat;
	m_airplane.matrix.model = translate(mat4(1.0f), m_airplanePosition);

	m_objectShader->useShader();
	const GLuint programID = m_objectShader->getProgramID();

	glUniformMatrix4fv(glGetUniformLocation(programID, "um4v"), 1, GL_FALSE, value_ptr(m_cameraView));
	glUniformMatrix4fv(glGetUniformLocation(programID, "um4p"), 1, GL_FALSE, value_ptr(m_cameraProjection));
	glUniform3fv(glGetUniformLocation(programID, "uv3LightPos"), 1, value_ptr(lightPosition));

	// airplane	
	glUniformMatrix4fv(glGetUniformLocation(programID, "um4m"), 1, GL_FALSE, value_ptr(m_airplane.matrix.model * m_airplane.matrix.rotate));
	glUniform1i(glGetUniformLocation(programID, "ubHasNormalMap"), (m_airplane.hasNormalMap) ? 1 : 0);
	glUniform1i(glGetUniformLocation(programID, "ubUseNormalMap"), (m_airplane.useNormalMap) ? 1 : 0);
	for (int i = 0; i < m_airplane.shapes.size(); i++) {
		int materialID = m_airplane.shapes[i].materialID;
		glUniform3fv(glGetUniformLocation(programID, "uv3Ambient"), 1, value_ptr(m_airplane.materials[materialID].ka));
		glUniform3fv(glGetUniformLocation(programID, "uv3Specular"), 1, value_ptr(m_airplane.materials[materialID].ks));
		glUniform3fv(glGetUniformLocation(programID, "uv3Diffuse"), 1, value_ptr(m_airplane.materials[materialID].kd));

		glBindVertexArray(m_airplane.shapes[i].vao);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_airplane.shapes[i].ibo);

		glUniform1i(glGetUniformLocation(programID, "diffuseTexture"), 3);
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, m_airplane.materials[materialID].diffuse_tex);
		glUniform1i(glGetUniformLocation(programID, "normalTexture"), 4);
		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_2D, m_airplane.materials[materialID].diffuse_tex);

		glDrawElements(GL_TRIANGLES, m_airplane.shapes[i].drawCount, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	// houses
	glUniform1i(glGetUniformLocation(programID, "ubHasNormalMap"), (m_houses.hasNormalMap) ? 1 : 0);
	glUniform1i(glGetUniformLocation(programID, "ubUseNormalMap"), (m_houses.useNormalMap) ? 1 : 0);
	// house A
	glUniformMatrix4fv(glGetUniformLocation(programID, "um4m"), 1, GL_FALSE, value_ptr(m_houses.matrixA.model * m_houses.matrixA.rotate));
	for (int i = 0; i < m_houses.shapes.size(); i++) {
		int materialID = m_houses.shapes[i].materialID;
		glUniform3fv(glGetUniformLocation(programID, "uv3Ambient"), 1, value_ptr(m_houses.materials[materialID].ka));
		glUniform3fv(glGetUniformLocation(programID, "uv3Specular"), 1, value_ptr(m_houses.materials[materialID].ks));
		glUniform3fv(glGetUniformLocation(programID, "uv3Diffuse"), 1, value_ptr(m_houses.materials[materialID].kd));

		glBindVertexArray(m_houses.shapes[i].vao);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_houses.shapes[i].ibo);

		glUniform1i(glGetUniformLocation(programID, "diffuseTexture"), 3);
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, m_houses.materials[materialID].diffuse_tex);

		glUniform1i(glGetUniformLocation(programID, "normalTexture"), 4);
		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_2D, m_houses.normalTexture);

		glDrawElements(GL_TRIANGLES, m_houses.shapes[i].drawCount, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	// house B
	glUniformMatrix4fv(glGetUniformLocation(programID, "um4m"), 1, GL_FALSE, value_ptr(m_houses.matrixB.model * m_houses.matrixB.rotate));
	for (int i = 0; i < m_houses.shapes.size(); i++) {
		int materialID = m_houses.shapes[i].materialID;
		glUniform3fv(glGetUniformLocation(programID, "uv3Ambient"), 1, value_ptr(m_houses.materials[materialID].ka));
		glUniform3fv(glGetUniformLocation(programID, "uv3Specular"), 1, value_ptr(m_houses.materials[materialID].ks));
		glUniform3fv(glGetUniformLocation(programID, "uv3Diffuse"), 1, value_ptr(m_houses.materials[materialID].kd));

		glBindVertexArray(m_houses.shapes[i].vao);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_houses.shapes[i].ibo);

		glUniform1i(glGetUniformLocation(programID, "diffuseTexture"), 3);
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, m_houses.materials[materialID].diffuse_tex);

		glUniform1i(glGetUniformLocation(programID, "normalTexture"), 4);
		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_2D, m_houses.normalTexture);

		glDrawElements(GL_TRIANGLES, m_houses.shapes[i].drawCount, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	m_objectShader->disableShader();
}
void spherer_render() {
	m_objectShader->useShader();
	const GLuint programID = m_objectShader->getProgramID();
	// Sphere
	glUniform1i(glGetUniformLocation(programID, "ubHasNormalMap"), (m_sphere.hasNormalMap) ? 1 : 0);
	glUniform1i(glGetUniformLocation(programID, "ubUseNormalMap"), (m_sphere.useNormalMap) ? 1 : 0);
	glUniformMatrix4fv(glGetUniformLocation(programID, "um4m"), 1, GL_FALSE, value_ptr(m_sphere.model));
	for (int i = 0; i < m_sphere.shapes.size(); i++) {
		int materialID = m_sphere.shapes[i].materialID;
		glUniform3fv(glGetUniformLocation(programID, "uv3Ambient"), 1, value_ptr(vec3(1.0f)));
		glUniform3fv(glGetUniformLocation(programID, "uv3Specular"), 1, value_ptr(vec3(1.0f)));
		glUniform3fv(glGetUniformLocation(programID, "uv3Diffuse"), 1, value_ptr(vec3(1.0f)));

		glBindVertexArray(m_sphere.shapes[i].vao);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_sphere.shapes[i].ibo);

		glUniform1i(glGetUniformLocation(programID, "diffuseTexture"), 3);
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, m_sphere.whiteTexture);
		glUniform1i(glGetUniformLocation(programID, "normalTexture"), 4);
		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_2D, m_sphere.whiteTexture);

		glDrawElements(GL_TRIANGLES, m_sphere.shapes[i].drawCount, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	m_objectShader->disableShader();
}



float window_positions[16] = {
		 1.0f, -1.0f,  1.0f,  0.0f,
		-1.0f, -1.0f,  0.0f,  0.0f,
		-1.0f,  1.0f,  0.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,  1.0f
};
void m_window_init() {
	glGenVertexArrays(1, &m_windowFrame.vao);
	glBindVertexArray(m_windowFrame.vao);

	glGenBuffers(1, &m_windowFrame.vbo);
	glBindBuffer(GL_ARRAY_BUFFER, m_windowFrame.vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(window_positions), window_positions, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4, 0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4, (const GLvoid*)(sizeof(GLfloat) * 2));
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	
	m_windowFrame.shader = new Shader("assets\\Window_vs.vs.glsl", "assets\\Window_fs.fs.glsl");
	m_windowFrame.shader->useShader();
	const GLuint programID = m_windowFrame.shader->getProgramID();
	cout << "Window programID: " << programID << endl;
	glUniform1i(glGetUniformLocation(programID, "currentTex"), CURRENT_TEX);

	glUniform1i(glGetUniformLocation(programID, "tex_diffuse"), 6);
	glActiveTexture(GL_TEXTURE6);
	glUniform1i(glGetUniformLocation(programID, "tex_ambient"), 7);
	glActiveTexture(GL_TEXTURE7);
	glUniform1i(glGetUniformLocation(programID, "tex_specular"), 8);
	glActiveTexture(GL_TEXTURE8);
	glUniform1i(glGetUniformLocation(programID, "tex_ws_position"), 9);
	glActiveTexture(GL_TEXTURE9);
	glUniform1i(glGetUniformLocation(programID, "tex_ws_normal"), 10);
	glActiveTexture(GL_TEXTURE10);
	glUniform1i(glGetUniformLocation(programID, "tex_ws_tangent"), 11);
	glActiveTexture(GL_TEXTURE11);
	glUniform1i(glGetUniformLocation(programID, "tex_phong"), 12);
	glActiveTexture(GL_TEXTURE12);
	glUniform1i(glGetUniformLocation(programID, "tex_bloomHDR"), 13);
	glActiveTexture(GL_TEXTURE13);
	glUniform1i(glGetUniformLocation(programID, "tex_phongShadow"), 14);
	glActiveTexture(GL_TEXTURE14);
	m_windowFrame.shader->disableShader();
}
void m_window_render() {
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	m_windowFrame.shader->useShader();
	const GLuint programID = m_windowFrame.shader->getProgramID();

	glUniform1i(glGetUniformLocation(programID, "currentTex"), CURRENT_TEX);

	glUniform1i(glGetUniformLocation(programID, "tex_diffuse"), 6);
	glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_2D, frameBufferTexture.diffuse);

	glUniform1i(glGetUniformLocation(programID, "tex_ambient"), 7);
	glActiveTexture(GL_TEXTURE7);
	glBindTexture(GL_TEXTURE_2D, frameBufferTexture.ambient);

	glUniform1i(glGetUniformLocation(programID, "tex_specular"), 8);
	glActiveTexture(GL_TEXTURE8);
	glBindTexture(GL_TEXTURE_2D, frameBufferTexture.specular);

	glUniform1i(glGetUniformLocation(programID, "tex_ws_position"), 9);
	glActiveTexture(GL_TEXTURE9);
	glBindTexture(GL_TEXTURE_2D, frameBufferTexture.ws_position);

	glUniform1i(glGetUniformLocation(programID, "tex_ws_normal"), 10);
	glActiveTexture(GL_TEXTURE10);
	glBindTexture(GL_TEXTURE_2D, frameBufferTexture.ws_normal);

	glUniform1i(glGetUniformLocation(programID, "tex_ws_tangent"), 11);
	glActiveTexture(GL_TEXTURE11);
	glBindTexture(GL_TEXTURE_2D, frameBufferTexture.ws_tangent);

	glUniform1i(glGetUniformLocation(programID, "tex_phong"), 12);
	glActiveTexture(GL_TEXTURE12);
	glBindTexture(GL_TEXTURE_2D, frameBufferTexture.phongColor);

	glUniform1i(glGetUniformLocation(programID, "tex_bloomHDR"), 13);
	glActiveTexture(GL_TEXTURE13);
	glBindTexture(GL_TEXTURE_2D, frameBufferTexture.bloomHDR);

	glUniform1i(glGetUniformLocation(programID, "tex_phongShadow"), 14);
	glActiveTexture(GL_TEXTURE14);
	glBindTexture(GL_TEXTURE_2D, frameBufferTexture.phongShadow);

	glBindVertexArray(m_windowFrame.vao);

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	m_windowFrame.shader->disableShader();
}



void genTexture_setBuffer() {
	glDeleteRenderbuffers(1, &m_genTexture.depthRBO);
	glDeleteTextures(1, &frameBufferTexture.diffuse);
	glDeleteTextures(1, &frameBufferTexture.ambient);
	glDeleteTextures(1, &frameBufferTexture.specular);
	glDeleteTextures(1, &frameBufferTexture.ws_position);
	glDeleteTextures(1, &frameBufferTexture.ws_normal);
	glDeleteTextures(1, &frameBufferTexture.ws_tangent);

	// Render Buffer
	glGenRenderbuffers(1, &m_genTexture.depthRBO);
	glBindRenderbuffer(GL_RENDERBUFFER, m_genTexture.depthRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32, FRAME_WIDTH, FRAME_HEIGHT);

	// Frame Buffer
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_genTexture.fbo);
	glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_genTexture.depthRBO);

	//// Texture | 0: diffuse 1: ambient 2: specular 3: ws_position 4: ws_normal 5: ws_tangent
	// diffuse
	glGenTextures(1, &frameBufferTexture.diffuse);
	glBindTexture(GL_TEXTURE_2D, frameBufferTexture.diffuse);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, FRAME_WIDTH, FRAME_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, frameBufferTexture.diffuse, 0);
	// ambient
	glGenTextures(1, &frameBufferTexture.ambient);
	glBindTexture(GL_TEXTURE_2D, frameBufferTexture.ambient);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, FRAME_WIDTH, FRAME_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, frameBufferTexture.ambient, 0);
	// specular
	glGenTextures(1, &frameBufferTexture.specular);
	glBindTexture(GL_TEXTURE_2D, frameBufferTexture.specular);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, FRAME_WIDTH, FRAME_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, frameBufferTexture.specular, 0);
	// ws_position
	glGenTextures(1, &frameBufferTexture.ws_position);
	glBindTexture(GL_TEXTURE_2D, frameBufferTexture.ws_position);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, FRAME_WIDTH, FRAME_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, frameBufferTexture.ws_position, 0);
	// ws_normal
	glGenTextures(1, &frameBufferTexture.ws_normal);
	glBindTexture(GL_TEXTURE_2D, frameBufferTexture.ws_normal);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, FRAME_WIDTH, FRAME_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_2D, frameBufferTexture.ws_normal, 0);
	// ws_tangent
	glGenTextures(1, &frameBufferTexture.ws_tangent);
	glBindTexture(GL_TEXTURE_2D, frameBufferTexture.ws_tangent);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, FRAME_WIDTH, FRAME_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT5, GL_TEXTURE_2D, frameBufferTexture.ws_tangent, 0);
	
	// finally check if framebuffer is complete
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "Framebuffer not complete!" << std::endl;
}
void genTexture_bindFrameBuffer() {
	//glBindTexture(GL_TEXTURE_2D, frameBufferTexture.dColor);
	const GLenum colorBuffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5 };
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_genTexture.fbo);
	glDrawBuffers(6, colorBuffers);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	static const GLfloat green[] = { 0.0f, 0.25f, 0.0f, 1.0f };
	static const GLfloat one = 1.0f;

	glClearBufferfv(GL_COLOR, 0, green);
	glClearBufferfv(GL_DEPTH, 0, &one);
}
void genTexture_init() {
	glGenFramebuffers(1, &m_genTexture.fbo);
	genTexture_setBuffer();
}


void dirShadow_setBuffer() {
	glDeleteTextures(1, &frameBufferTexture.dirDepth);

	glBindFramebuffer(GL_FRAMEBUFFER, m_dirShadow.depthFBO);

	// depth texture
	glGenTextures(1, &frameBufferTexture.dirDepth);
	glBindTexture(GL_TEXTURE_2D, frameBufferTexture.dirDepth);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, m_dirShadow.shadowSize, m_dirShadow.shadowSize, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	GLfloat borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, frameBufferTexture.dirDepth, 0);
	
	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE){
		cout << "direct light framebuffer error" << endl;
	}
	// // back to default framebuffer
	// glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
void dirShadow_init() {
	// depth FBO
	glGenFramebuffers(1, &m_dirShadow.depthFBO);
	dirShadow_setBuffer();

	// Shader Init
	m_dirShadow.shader = new Shader("assets\\DirectionalShadow_vs.vs.glsl", "assets\\DirectionalShadow_fs.fs.glsl");
	m_dirShadow.shader->useShader();
	const GLuint programID = m_dirShadow.shader->getProgramID();
	cout << "m_dirShadow programID: " << programID << endl;
	glUniformMatrix4fv(glGetUniformLocation(programID, "light_vp"), 1, GL_FALSE, value_ptr(m_dirShadow.LightVP));
	glUniformMatrix4fv(glGetUniformLocation(programID, "um4m"), 1, GL_FALSE, value_ptr(mat4(1.0f)));
	m_dirShadow.shader->disableShader();
	
}
void dirShadow_bindFrameBuffer() {
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	static const GLfloat blue[] = { 0.0f, 0.0f, 0.5f, 1.0f };
	static const GLfloat one = 1.0f;

	glClearBufferfv(GL_COLOR, 0, blue);
	glClearBufferfv(GL_DEPTH, 0, &one);
}
void dirShadow_render() {	

	// global state
	glEnable(GL_DEPTH_TEST);

	// render depth map
	glBindFramebuffer(GL_FRAMEBUFFER, m_dirShadow.depthFBO);

	glViewport(0, 0, m_dirShadow.shadowSize, m_dirShadow.shadowSize);

	mat4 light_p, light_v;
	// float range = 5.0f;
	float range = 20.0f;
	
	light_p = ortho(-range, range, -range, range, m_dirShadow.near, m_dirShadow.far);
	light_v = lookAt(m_dirShadow.LightPos, m_dirShadow.LightPos - m_dirShadow.LightDir, vec3(0.0f, 1.0f, 0.0));
	m_dirShadow.LightVP = light_p * light_v;

	m_objects_shadowRender();

	glViewport(0, 0, FRAME_WIDTH, FRAME_HEIGHT);
	// glDisable(GL_DEPTH_TEST);
	// glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}




void deferred_setBuffer() {
	glDeleteRenderbuffers(1, &m_deferred.depthRBO);
	glDeleteTextures(1, &frameBufferTexture.phongColor);
	glDeleteTextures(1, &frameBufferTexture.bloomHDR);
	glDeleteTextures(1, &frameBufferTexture.phongShadow);

	// Render Buffer
	glGenRenderbuffers(1, &m_deferred.depthRBO);
	glBindRenderbuffer(GL_RENDERBUFFER, m_deferred.depthRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32, FRAME_WIDTH, FRAME_HEIGHT);

	// Frame Buffer
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_deferred.fbo);
	glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_deferred.depthRBO);

	//// Texture | 0: phongColor 1: bloomHDR 2: phongShadow
	// phongColor
	glGenTextures(1, &frameBufferTexture.phongColor);
	glBindTexture(GL_TEXTURE_2D, frameBufferTexture.phongColor);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, FRAME_WIDTH, FRAME_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, frameBufferTexture.phongColor, 0);

	// bloomHDR
	glGenTextures(1, &frameBufferTexture.bloomHDR);
	glBindTexture(GL_TEXTURE_2D, frameBufferTexture.bloomHDR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, FRAME_WIDTH, FRAME_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, frameBufferTexture.bloomHDR, 0);

	// phongShadow
	glGenTextures(1, &frameBufferTexture.phongShadow);
	glBindTexture(GL_TEXTURE_2D, frameBufferTexture.phongShadow);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, FRAME_WIDTH, FRAME_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, frameBufferTexture.phongShadow, 0);

	// finally check if framebuffer is complete
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "Framebuffer not complete!" << std::endl;
}
void deferred_bindFrameBuffer() {
	const GLenum colorBuffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_deferred.fbo);
	glDrawBuffers(3, colorBuffers);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	static const GLfloat blue[] = { 0.0f, 0.0f, 0.5f, 1.0f };
	static const GLfloat one = 1.0f;

	glClearBufferfv(GL_COLOR, 0, blue);
	glClearBufferfv(GL_DEPTH, 0, &one);
}
void deferred_init() {
	glGenFramebuffers(1, &m_deferred.fbo);
	deferred_setBuffer();

	m_deferred.shader = new Shader("assets\\Deferred_vs.vs.glsl", "assets\\Deferred_fs.fs.glsl");
	m_deferred.shader->useShader();
	const GLuint programID = m_deferred.shader->getProgramID();
	cout << "m_deferred programID: " << programID << endl;
	glUniform3fv(glGetUniformLocation(programID, "uv3LightPos"), 1, value_ptr(lightPosition));
	glUniform3fv(glGetUniformLocation(programID, "uv3eyePos"), 1, value_ptr(m_eye));
	glUniformMatrix4fv(glGetUniformLocation(programID, "um4LightVP"), 1, GL_FALSE, value_ptr(m_dirShadow.LightVP));
	
	glUniform1i(glGetUniformLocation(programID, "tex_diffuse"), 6);
	glActiveTexture(GL_TEXTURE6);
	glUniform1i(glGetUniformLocation(programID, "tex_ambient"), 7);
	glActiveTexture(GL_TEXTURE7);
	glUniform1i(glGetUniformLocation(programID, "tex_specular"), 8);
	glActiveTexture(GL_TEXTURE8);
	glUniform1i(glGetUniformLocation(programID, "tex_ws_position"), 9);
	glActiveTexture(GL_TEXTURE9);
	glUniform1i(glGetUniformLocation(programID, "tex_ws_normal"), 10);
	glActiveTexture(GL_TEXTURE10);
	glUniform1i(glGetUniformLocation(programID, "tex_ws_tangent"), 11);
	glActiveTexture(GL_TEXTURE11);
	glUniform1i(glGetUniformLocation(programID, "tex_dirDepth"), 15);
	glActiveTexture(GL_TEXTURE15);

	m_deferred.shader->disableShader();
}
void deferred_render() {
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	m_deferred.shader->useShader();

	const GLuint programID = m_deferred.shader->getProgramID();
	glUniform3fv(glGetUniformLocation(programID, "uv3LightPos"), 1, value_ptr(lightPosition));
	glUniform3fv(glGetUniformLocation(programID, "uv3eyePos"), 1, value_ptr(m_eye));
	glUniformMatrix4fv(glGetUniformLocation(programID, "um4LightVP"), 1, GL_FALSE, value_ptr(m_dirShadow.LightVP));

	glUniform1i(glGetUniformLocation(programID, "tex_diffuse"), 6);
	glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_2D, frameBufferTexture.diffuse);

	glUniform1i(glGetUniformLocation(programID, "tex_ambient"), 7);
	glActiveTexture(GL_TEXTURE7);
	glBindTexture(GL_TEXTURE_2D, frameBufferTexture.ambient);

	glUniform1i(glGetUniformLocation(programID, "tex_specular"), 8);
	glActiveTexture(GL_TEXTURE8);
	glBindTexture(GL_TEXTURE_2D, frameBufferTexture.specular);

	glUniform1i(glGetUniformLocation(programID, "tex_ws_position"), 9);
	glActiveTexture(GL_TEXTURE9);
	glBindTexture(GL_TEXTURE_2D, frameBufferTexture.ws_position);

	glUniform1i(glGetUniformLocation(programID, "tex_ws_normal"), 10);
	glActiveTexture(GL_TEXTURE10);
	glBindTexture(GL_TEXTURE_2D, frameBufferTexture.ws_normal);

	glUniform1i(glGetUniformLocation(programID, "tex_ws_tangent"), 11);
	glActiveTexture(GL_TEXTURE11);
	glBindTexture(GL_TEXTURE_2D, frameBufferTexture.ws_tangent);

	glUniform1i(glGetUniformLocation(programID, "tex_dirDepth"), 15);
	glActiveTexture(GL_TEXTURE15);
	glBindTexture(GL_TEXTURE_2D, frameBufferTexture.dirDepth);

	glBindVertexArray(m_windowFrame.vao);

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	m_deferred.shader->disableShader();
}


int main() {
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);


	GLFWwindow *window = glfwCreateWindow(FRAME_WIDTH, FRAME_HEIGHT, "rendering", nullptr, nullptr);
	if (window == nullptr) {
		std::cout << "failed to create GLFW window\n";
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	// load OpenGL function pointer
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}



	glfwSetKeyCallback(window, keyCallback);
	glfwSetScrollCallback(window, mouseScrollCallback);
	glfwSetMouseButtonCallback(window, mouseButtonCallback);
	glfwSetCursorPosCallback(window, cursorPosCallback);
	glfwSetFramebufferSizeCallback(window, resizeGL);

	initializeGL();

	//vsyncEnabled(window);

	// be careful v-sync issue
	glfwSwapInterval(0);
	vsyncDisabled(window);

	glfwTerminate();
	return 0;
}
void vsyncDisabled(GLFWwindow *window) {
	float periodForEvent = 1.0 / 60.0;
	float accumulatedEventPeriod = 0.0;
	double previousTime = glfwGetTime();
	double previousTimeForFPS = glfwGetTime();
	int frameCount = 0;
	while (!glfwWindowShouldClose(window)) {
		// measure speed
		double currentTime = glfwGetTime();
		float deltaTime = currentTime - previousTime;
		frameCount = frameCount + 1;
		if (currentTime - previousTimeForFPS >= 1.0) {
			std::cout << "\rFPS: " << frameCount;
			frameCount = 0;
			previousTimeForFPS = currentTime;
		}
		previousTime = currentTime;

		// game loop
		accumulatedEventPeriod = accumulatedEventPeriod + deltaTime;
		if (accumulatedEventPeriod > periodForEvent) {
			updateState();
			accumulatedEventPeriod = accumulatedEventPeriod - periodForEvent;
		}

		paintGL();
		glfwSwapBuffers(window);
		glfwPollEvents();
	}
}
void vsyncEnabled(GLFWwindow *window) {
	double previousTimeForFPS = glfwGetTime();
	int frameCount = 0;
	while (!glfwWindowShouldClose(window)) {
		// measure speed
		double currentTime = glfwGetTime();
		frameCount = frameCount + 1;
		if (currentTime - previousTimeForFPS >= 1.0) {
			std::cout << "\rFPS: " << frameCount;
			frameCount = 0;
			previousTimeForFPS = currentTime;
		}

		updateState();
		paintGL();

		glfwSwapBuffers(window);
		glfwPollEvents();
	}
}

void initializeGL() {
	m_renderer = new SceneRenderer();
	m_renderer->initialize(FRAME_WIDTH, FRAME_HEIGHT);

	m_eye = glm::vec3(512.0, 10.0, 512.0);
	// m_eye = glm::vec3(512.0, 10.0, 512.0);
	m_lookAtCenter = glm::vec3(512.0, 0.0, 500.0);
	m_lookDirection = m_lookAtCenter - m_eye;

	initScene();
	m_objects_init();
	m_window_init();
	
	genTexture_init();
	dirShadow_init();
	deferred_init();
	
	m_cameraProjection = perspective(glm::radians(60.0f), FRAME_WIDTH * 1.0f / FRAME_HEIGHT, 0.1f, 1000.0f);
	m_renderer->setProjection(m_cameraProjection);
}
void resizeGL(GLFWwindow *window, int w, int h) {
	FRAME_WIDTH = w;
	FRAME_HEIGHT = h;
	glViewport(0, 0, w, h);
	m_renderer->resize(w, h);
	genTexture_setBuffer();
	dirShadow_setBuffer();
	deferred_setBuffer();
	m_cameraProjection = perspective(glm::radians(60.0f), w * 1.0f / h, 0.1f, 1000.0f);
	m_renderer->setProjection(m_cameraProjection);
}

void updateState() {
	// [TODO] update your eye position and look-at center here
	// m_eye = ... ;
	// m_lookAtCenter = ... ;
	//m_lookAtCenter.z = m_lookAtCenter.z + 1;
	//m_eye.z = m_eye.z + 1;

	m_uAxis = cross(m_lookDirection, m_upDirection);
	m_vAxis = cross(m_uAxis, m_lookDirection);
	m_lookAtCenter = m_eye + m_lookDirection;

	// adjust camera position with terrain
	adjustCameraPositionWithTerrain();

	// calculate camera matrix
	m_cameraView = lookAt(m_eye, m_lookAtCenter, m_upDirection);

	// [IMPORTANT] set camera information to renderer
	m_renderer->setView(m_cameraView, m_eye);

	// update airplane
	updateAirplane(m_cameraView);
}

void paintGL() {

	genTexture_bindFrameBuffer();
	// render terrain
	m_renderer->renderPass();
	// [TODO] implement your rendering function here
	// shader set flag 0
	m_objects_render();
	// shader set flag 1
	spherer_render();
	// light pass: shadow
	dirShadow_bindFrameBuffer();
	dirShadow_render(); // shadow map

	// geometry pass: pos, normal
	deferred_bindFrameBuffer();
	deferred_render(); // bind shadow map

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	m_window_render();
}

////////////////////////////////////////////////
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
#pragma region Trackball
	if (action == GLFW_PRESS)
	{
		if (button == GLFW_MOUSE_BUTTON_LEFT) {
			m_leftButtonPressed = true;
			m_leftButtonPressedFirst = true;
		}
	}
	else if (action == GLFW_RELEASE)
	{
		if (button == GLFW_MOUSE_BUTTON_LEFT) {
			m_leftButtonPressed = false;
			m_trackball_initial = vec2(0, 0);
		}
	}
#pragma endregion
}
void mouseScrollCallback(GLFWwindow *window, double xoffset, double yoffset) {}
void cursorPosCallback(GLFWwindow* window, double x, double y) {
	cursorPos[0] = x;
	cursorPos[1] = y;
#pragma region Trackball
	if (m_leftButtonPressed) {
		if (m_leftButtonPressedFirst) {
			m_trackball_initial = vec2(cursorPos[0], cursorPos[1]);
			m_leftButtonPressedFirst = false;
			cout << "Cursor Pressed at (" << x << ", " << y << ")" << endl;
		}
		// Update trackball_initial
		float disX = abs(x - m_trackball_initial.x);
		if (disX > m_oldDistance.x) {
			m_oldDistance.x = disX;
		}
		else {
			m_oldDistance.x = 0.0f;
			m_trackball_initial.x = x;
		}
		float disY = abs(y - m_trackball_initial.y);
		if (disY > m_oldDistance.y) {
			m_oldDistance.y = disY;
		}
		else {
			m_oldDistance.y = 0.0f;
			m_trackball_initial.y = y;
		}
		// limit the up angle
		float limitCos = dot(normalize(m_lookDirection), normalize(m_upDirection));
		if (limitCos > 0.8 || limitCos < -0.8) {
			m_rotateAngle.x = (x - m_trackball_initial.x) / 70.0f;
			float rotateRadians_x = radians(m_rotateAngle.x);
			mat4 rotationX = rotate(mat4(1.0f), rotateRadians_x, m_vAxis);
			m_lookDirection = vec3(rotationX * vec4(m_lookDirection, 1.0));

			m_rotateAngle.y = (y - m_trackball_initial.y) / 100.0f;
			if (limitCos > 0.8 && m_rotateAngle.y < 0) {
				float rotateRadians_y = radians(m_rotateAngle.y);
				mat4 rotationY = rotate(mat4(1.0f), rotateRadians_y, m_uAxis);
				m_lookDirection = vec3(rotationY * vec4(m_lookDirection, 1.0));
			}
			else if (limitCos < -0.8 && m_rotateAngle.y > 0) {
				float rotateRadians_y = radians(m_rotateAngle.y);
				mat4 rotationY = rotate(mat4(1.0f), rotateRadians_y, m_uAxis);
				m_lookDirection = vec3(rotationY * vec4(m_lookDirection, 1.0));
			}
		}
		else {
			m_rotateAngle.x = (x - m_trackball_initial.x) / 70.0f;
			float rotateRadians_x = radians(m_rotateAngle.x);
			mat4 rotationX = rotate(mat4(1.0f), rotateRadians_x, m_vAxis);
			m_lookDirection = vec3(rotationX * vec4(m_lookDirection, 1.0));

			m_rotateAngle.y = (y - m_trackball_initial.y) / 100.0f;
			float rotateRadians_y = radians(m_rotateAngle.y);
			mat4 rotationY = rotate(mat4(1.0f), rotateRadians_y, m_uAxis);
			m_lookDirection = vec3(rotationY * vec4(m_lookDirection, 1.0));
		}
	}
#pragma endregion
}
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (GLFW_PRESS == action) {
		switch (key)
		{
		case GLFW_KEY_Z:
			m_houses.useNormalMap = !m_houses.useNormalMap;
			cout << "House use/unuse normal texture" << endl;
			break;
		case GLFW_KEY_X:
			cout << endl << "Input eye (split with space): " << endl;
			scanf("%f %f %f", &m_eye[0], &m_eye[1], &m_eye[2]);
			cout << "Current eye at (" << m_eye[0] << ", " << m_eye[1] << ", " << m_eye[2] << ")" << endl;
			break;
		case GLFW_KEY_C:
			cout << endl << "Input look-at center (split with space): " << endl;
			scanf("%f %f %f", &m_lookAtCenter[0], &m_lookAtCenter[1], &m_lookAtCenter[2]);
			cout << endl << "Current look-at center at (" << m_lookAtCenter[0] << ", " << m_lookAtCenter[1] << ", " << m_lookAtCenter[2] << ")" << endl;
			break;
		case GLFW_KEY_1:
			CURRENT_TEX = TEXTURE_FINAL;
			cout << "Texture is [FINAL]" << endl;
			break;
		case GLFW_KEY_2:
			CURRENT_TEX = TEXTURE_DIFFUSE;
			cout << "Texture is [DIFFUSE]" << endl;
			break;
		case GLFW_KEY_3:
			CURRENT_TEX = TEXTURE_AMBIENT;
			cout << "Texture is [AMBIENT]" << endl;
			break;
		case GLFW_KEY_4:
			CURRENT_TEX = TEXTURE_SPECULAR;
			cout << "Texture is [SPECULAR]" << endl;
			break;
		case GLFW_KEY_5:
			CURRENT_TEX = TEXTURE_WS_POSITION;
			cout << "Texture is [WS_POSITION]" << endl;
			break;
		case GLFW_KEY_6:
			CURRENT_TEX = TEXTURE_WS_NORMAL;
			cout << "Texture is [WS_NORMAL]" << endl;
			break;
		case GLFW_KEY_7:
			CURRENT_TEX = TEXTURE_WS_TANGENT;
			cout << "Texture is [TANGENT]" << endl;
			break;
		case GLFW_KEY_8:
			CURRENT_TEX = TEXTURE_PHONG;
			cout << "Texture is [PHONG]" << endl;
			break;
		case GLFW_KEY_9:
			CURRENT_TEX = TEXTURE_BLOOMHDR;
			cout << "Texture is [BLOOMHDR]" << endl;
			break;
		case GLFW_KEY_0:
			CURRENT_TEX = TEXTURE_PHONGSHADOW;
			cout << "Texture is [PHONG with SHADOW]" << endl;
			break;
		default:
			break;
		}
	}
	#pragma region Trackball
	switch(key) {	
	case GLFW_KEY_W:
		m_eye += normalize(m_lookDirection) * m_cameraMoveSpeed;
		break;
	case GLFW_KEY_S:
		m_eye -= normalize(m_lookDirection) * m_cameraMoveSpeed;
		break;
	case GLFW_KEY_D:
		m_eye += normalize(m_uAxis) * m_cameraMoveSpeed;
		break;
	case GLFW_KEY_A:
		m_eye -= normalize(m_uAxis) * m_cameraMoveSpeed;
		break;
	case GLFW_KEY_Q:
		m_eye -= normalize(m_vAxis) * m_cameraMoveSpeed;
		break;
	case GLFW_KEY_E:
		m_eye += normalize(m_vAxis) * m_cameraMoveSpeed;
		break;
	default:
		break;
	}
	#pragma endregion
}
////////////////////////////////////////////////
// The following functions don't need to change
void initScene() {
	m_plantManager = new PlantManager();
	m_terrain = new Terrain();
	m_renderer->appendTerrain(m_terrain);
}
void updateAirplane(const glm::mat4 &viewMatrix) {
	// apply yaw
	glm::mat4 rm = viewMatrix;
	rm = glm::transpose(rm);
	glm::vec3 tangent(-1.0 * rm[2].x, 0.0, -1.0 * rm[2].z);
	tangent = glm::normalize(tangent);
	glm::vec3 bitangent = glm::normalize(glm::cross(glm::vec3(0.0, 1.0, 0.0), tangent));
	glm::vec3 normal = glm::normalize(glm::cross(tangent, bitangent));

	m_airplaneRotMat[0] = glm::vec4(bitangent, 0.0);
	m_airplaneRotMat[1] = glm::vec4(normal, 0.0);
	m_airplaneRotMat[2] = glm::vec4(tangent, 0.0);
	m_airplaneRotMat[3] = glm::vec4(0.0, 0.0, 0.0, 1.0);

	m_airplanePosition = m_lookAtCenter;
}

// adjust camera position and look-at center with terrain
void adjustCameraPositionWithTerrain() {
	// adjust camera height
	glm::vec3 cp = m_lookAtCenter;
	float ty = m_terrain->getHeight(cp.x, cp.z);
	if (cp.y < ty + 3.0) {
		m_lookAtCenter.y = ty + 3.0;
		m_eye.y = m_eye.y + (ty + 3.0 - cp.y);
	}
	cp = m_eye;
	ty = m_terrain->getHeight(cp.x, cp.z);
	if (cp.y < ty + 3.0) {
		m_lookAtCenter.y = ty + 3.0;
		m_eye.y = m_eye.y + (ty + 3.0 - cp.y);
	}
}

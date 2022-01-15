#include "src\basic\SceneRenderer.h"
#include <GLFW\glfw3.h>
#include "SceneSetting.h"

#include "assimp/scene.h"
#include "assimp/cimport.h"
#include "assimp/postprocess.h"
#include "../externals/include/stb_image.h"

#pragma comment (lib, "lib-vc2015\\glfw3.lib")
#pragma comment (lib, "assimp\\assimp-vc140-mtd.lib")

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

#pragma region Light Source
mat4 lightView(1.0f);
mat4 lightProjection(1.0f);
vec3 lightPosition = vec3(0.2f, 0.6f, 0.5f);
// Shadow
//mat4 scale_bias_matrix = translate(mat4(1.0f), vec3(0.5f, 0.5f, 0.5f)) * scale(mat4(1.0f), vec3(0.5f, 0.5f, 0.5f));
#pragma endregion

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

char** loadShaderSource(const char* file)
{
	FILE* fp = fopen(file, "rb");
	fseek(fp, 0, SEEK_END);
	long sz = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	char *src = new char[sz + 1];
	fread(src, sizeof(char), sz, fp);
	src[sz] = '\0';
	char **srcp = new char*[1];
	srcp[0] = src;
	return srcp;
}

void freeShaderSource(char** srcp)
{
	delete srcp[0];
	delete srcp;
}

void shaderLog(GLuint shader)
{
	GLint isCompiled = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
	if (isCompiled == GL_FALSE)
	{
		GLint maxLength = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

		// The maxLength includes the NULL character
		GLchar* errorLog = new GLchar[maxLength];
		glGetShaderInfoLog(shader, maxLength, &maxLength, &errorLog[0]);

		printf("%s\n", errorLog);
		delete[] errorLog;
	}
}
#pragma endregion

#pragma region Airplane
struct {
	Shader *shader = nullptr;
	struct {
		mat4 model = mat4(1.0f);
		mat4 scale = mat4(1.0f);
		mat4 rotate = mat4(1.0f);
	} matrix;

	vector<Shape> shapes;
	vector<Material> materials;
	bool hasNormalMap;
	bool useNormalMap;
} m_airplane;
void plane_loadModel() {
	char* filepath = ".\\models\\airplane.obj";
	const aiScene* scene = aiImportFile(filepath, aiProcessPreset_TargetRealtime_MaxQuality);

	// Material
	for (unsigned int i = 0; i < scene->mNumMaterials; ++i) {
		aiMaterial* material = scene->mMaterials[i];
		Material Material;
		aiString texturePath;
		aiColor3D color;
		if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == aiReturn_SUCCESS) {
			// load width, height and data from texturePath.C_Str();
			//string p = ".\\models\\"; /*..\\Assets\\*/
			//texturePath = p + texturePath.C_Str();
			cout << texturePath.C_Str() << endl;
			TextureData tex = loadImg(texturePath.C_Str());
			glGenTextures(1, &Material.diffuse_tex);
			glBindTexture(GL_TEXTURE_2D, Material.diffuse_tex);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, tex.width, tex.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex.data);
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
		}

		// create 3 vbos to hold data
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
void plane_init(mat4 _rotateMatrix, vec3 _position) {
	m_airplane.matrix.rotate = _rotateMatrix;
	m_airplane.matrix.model = translate(mat4(1.0f), _position);

	plane_loadModel();
	m_airplane.hasNormalMap = false;
	m_airplane.useNormalMap = false;

	m_airplane.shader = new Shader("assets\\GenTextures_vs.vs.glsl", "assets\\GenTextures_fs.fs.glsl");
	m_airplane.shader->useShader();

	const GLuint programId = m_airplane.shader->getProgramID();
	glUniform1i(glGetUniformLocation(programId, "tex_diffuse"), 3);
	m_airplane.shader->disableShader();
}
void plane_render() {
	m_airplane.matrix.rotate = m_airplaneRotMat;
	m_airplane.matrix.model = translate(mat4(1.0f), m_airplanePosition);

	m_airplane.shader->useShader();
	const GLuint programId = m_airplane.shader->getProgramID();
	// vs
	glUniformMatrix4fv(glGetUniformLocation(programId, "um4m"), 1, GL_FALSE, value_ptr(m_airplane.matrix.model * m_airplane.matrix.rotate));
	glUniformMatrix4fv(glGetUniformLocation(programId, "um4v"), 1, GL_FALSE, value_ptr(m_cameraView));
	glUniformMatrix4fv(glGetUniformLocation(programId, "um4p"), 1, GL_FALSE, value_ptr(m_cameraProjection));
	glUniform3fv(glGetUniformLocation(programId, "uv3LightPos"), 1, value_ptr(lightPosition));
	// fs
	//glUniform1i(m_airplane.uniformID.ubPhongFlag, (m_airplane.blinnPhongFlag) ? 1 : 0);
	//glUniform1i(m_airplane.uniformID.ubNormalFlag, (m_airplane.normalMappingFlag) ? 1 : 0);
	glUniform1i(glGetUniformLocation(programId, "ubHasNormalMap"), (m_airplane.hasNormalMap) ? 1 : 0);
	glUniform1i(glGetUniformLocation(programId, "ubUseNormalMap"), (m_airplane.useNormalMap) ? 1 : 0);

	for (int i = 0; i < m_airplane.shapes.size(); i++) {
		int materialID = m_airplane.shapes[i].materialID;
		glUniform3fv(glGetUniformLocation(programId, "uv3Ambient"), 1, value_ptr(m_airplane.materials[materialID].ka));
		glUniform3fv(glGetUniformLocation(programId, "uv3Specular"), 1, value_ptr(m_airplane.materials[materialID].ks));
		glUniform3fv(glGetUniformLocation(programId, "uv3Diffuse"), 1, value_ptr(m_airplane.materials[materialID].kd));

		glBindVertexArray(m_airplane.shapes[i].vao);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_airplane.shapes[i].ibo);

		glUniform1i(glGetUniformLocation(programId, "diffuseTexture"), 3);
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, m_airplane.materials[materialID].diffuse_tex);

		glDrawElements(GL_TRIANGLES, m_airplane.shapes[i].drawCount, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	m_airplane.shader->disableShader();
}
#pragma endregion

#pragma region House
struct {
	Shader *shaderA = nullptr;
	Shader *shaderB = nullptr;
	struct {
		mat4 model = mat4(1.0f);
		mat4 scale = mat4(1.0f);
		mat4 rotate = mat4(1.0f);
	} houseA_matrix;
	struct {
		mat4 model = mat4(1.0f);
		mat4 scale = mat4(1.0f);
		mat4 rotate = mat4(1.0f);
	} houseB_matrix;
	struct {
		GLenum diffuse;
		GLenum normal;
	} texUnit;

	GLuint normalTexture;

	vector<Shape> shapes;
	vector<Material> materials;

	bool hasNormalMap;
	bool useNormalMap;
} m_houses;

void house_loadModel() {
	char* filepath = ".\\models\\medievalHouse.obj";
	const aiScene* scene = aiImportFile(filepath, aiProcessPreset_TargetRealtime_MaxQuality);

	// Material
	for (unsigned int i = 0; i < scene->mNumMaterials; ++i) {
		aiMaterial* material = scene->mMaterials[i];
		Material Material;
		aiString texturePath;
		aiColor3D color;
		if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == aiReturn_SUCCESS) {
			// load width, height and data from texturePath.C_Str();
			//string p = ".\\models\\"; /*..\\Assets\\*/
			//texturePath = p + texturePath.C_Str();
			cout << texturePath.C_Str() << endl;
			TextureData tex = loadImg(texturePath.C_Str());
			glGenTextures(1, &Material.diffuse_tex);
			glBindTexture(GL_TEXTURE_2D, Material.diffuse_tex);
			//glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, tex.width, tex.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex.data);
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
	//glGenerateMipmap(GL_TEXTURE_2D);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	cout << "Load medievalHouse.obj done" << endl;
}
void houses_init(float a_rotateAngle, vec3 a_position, float b_rotateAngle,  vec3 b_position) {
	m_houses.houseA_matrix.rotate = rotate(mat4(1.0f), radians(a_rotateAngle), vec3(0.0f, 1.0f, 0.0f));
	m_houses.houseA_matrix.model = translate(mat4(1.0f), a_position);
	m_houses.houseB_matrix.rotate = rotate(mat4(1.0f), radians(b_rotateAngle), vec3(0.0f, 1.0f, 0.0f));
	m_houses.houseB_matrix.model = translate(mat4(1.0f), b_position);

	house_loadModel();
	m_houses.hasNormalMap = true;
	m_houses.useNormalMap = false;

	// House A
	m_houses.shaderA = new Shader("assets\\GenTextures_vs.vs.glsl", "assets\\GenTextures_fs.fs.glsl");
	m_houses.shaderA->useShader();

	const GLuint programId_A = m_houses.shaderA->getProgramID();
	m_houses.texUnit.diffuse = GL_TEXTURE3;
	glUniform1i(glGetUniformLocation(programId_A, "diffuseTexture"), 3);
	m_houses.texUnit.normal = GL_TEXTURE4;
	glUniform1i(glGetUniformLocation(programId_A, "normalTexture"), 4);
	m_houses.shaderA->disableShader();

	// House B
	m_houses.shaderB = new Shader("assets\\GenTextures_vs.vs.glsl", "assets\\GenTextures_fs.fs.glsl");
	m_houses.shaderB->useShader();

	const GLuint programId_B = m_houses.shaderB->getProgramID();
	m_houses.texUnit.diffuse = GL_TEXTURE3;
	glUniform1i(glGetUniformLocation(programId_B, "diffuseTexture"), 3);
	m_houses.texUnit.normal = GL_TEXTURE4;
	glUniform1i(glGetUniformLocation(programId_B, "normalTexture"), 4);
	m_houses.shaderB->disableShader();
}
void houses_render() {
	// House A
	m_houses.shaderA->useShader();
	const GLuint programId_A = m_airplane.shader->getProgramID();
	// vs
	glUniformMatrix4fv(glGetUniformLocation(programId_A, "um4m"), 1, GL_FALSE, value_ptr(m_houses.houseA_matrix.model * m_houses.houseA_matrix.rotate));
	glUniformMatrix4fv(glGetUniformLocation(programId_A, "um4v"), 1, GL_FALSE, value_ptr(m_cameraView));
	glUniformMatrix4fv(glGetUniformLocation(programId_A, "um4p"), 1, GL_FALSE, value_ptr(m_cameraProjection));
	glUniform3fv(glGetUniformLocation(programId_A, "uv3LightPos"), 1, value_ptr(lightPosition));

	// fs
	//glUniform1i(this->uniformID.ubPhongFlag, (this->blinnPhongFlag) ? 1 : 0);
	//glUniform1i(this->uniformID.ubNormalFlag, (this->normalMappingFlag) ? 1 : 0);
	glUniform1i(glGetUniformLocation(programId_A, "ubHasNormalMap"), (m_houses.hasNormalMap) ? 1 : 0);
	glUniform1i(glGetUniformLocation(programId_A, "ubUseNormalMap"), (m_houses.useNormalMap) ? 1 : 0);

	for (int i = 0; i < m_houses.shapes.size(); i++) {
		int materialID = m_houses.shapes[i].materialID;
		glUniform3fv(glGetUniformLocation(programId_A, "uv3Ambient"), 1, value_ptr(m_houses.materials[materialID].ka));
		glUniform3fv(glGetUniformLocation(programId_A, "uv3Specular"), 1, value_ptr(m_houses.materials[materialID].ks));
		glUniform3fv(glGetUniformLocation(programId_A, "uv3Diffuse"), 1, value_ptr(m_houses.materials[materialID].kd));


		glBindVertexArray(m_houses.shapes[i].vao);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_houses.shapes[i].ibo);

		glUniform1i(glGetUniformLocation(programId_A, "diffuseTexture"), 3);
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, m_houses.materials[materialID].diffuse_tex);

		glUniform1i(glGetUniformLocation(programId_A, "normalTexture"), 4);
		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_2D, m_houses.normalTexture);

		glDrawElements(GL_TRIANGLES, m_houses.shapes[i].drawCount, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	m_houses.shaderA->disableShader();

	// House B
	m_houses.shaderB->useShader();
	const GLuint programId_B = m_airplane.shader->getProgramID();
	// vs
	glUniformMatrix4fv(glGetUniformLocation(programId_B, "um4m"), 1, GL_FALSE, value_ptr(m_houses.houseB_matrix.model * m_houses.houseB_matrix.rotate));
	glUniformMatrix4fv(glGetUniformLocation(programId_B, "um4v"), 1, GL_FALSE, value_ptr(m_cameraView));
	glUniformMatrix4fv(glGetUniformLocation(programId_B, "um4p"), 1, GL_FALSE, value_ptr(m_cameraProjection));
	glUniform3fv(glGetUniformLocation(programId_B, "uv3LightPos"), 1, value_ptr(lightPosition));

	// fs
	//glUniform1i(this->uniformID.ubPhongFlag, (this->blinnPhongFlag) ? 1 : 0);
	//glUniform1i(this->uniformID.ubNormalFlag, (this->normalMappingFlag) ? 1 : 0);
	glUniform1i(glGetUniformLocation(programId_B, "ubHasNormalMap"), (m_houses.hasNormalMap) ? 1 : 0);
	glUniform1i(glGetUniformLocation(programId_B, "ubUseNormalMap"), (m_houses.useNormalMap) ? 1 : 0);

	for (int i = 0; i < m_houses.shapes.size(); i++) {
		int materialID = m_houses.shapes[i].materialID;
		glUniform3fv(glGetUniformLocation(programId_B, "uv3Ambient"), 1, value_ptr(m_houses.materials[materialID].ka));
		glUniform3fv(glGetUniformLocation(programId_B, "uv3Specular"), 1, value_ptr(m_houses.materials[materialID].ks));
		glUniform3fv(glGetUniformLocation(programId_B, "uv3Diffuse"), 1, value_ptr(m_houses.materials[materialID].kd));


		glBindVertexArray(m_houses.shapes[i].vao);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_houses.shapes[i].ibo);

		glUniform1i(glGetUniformLocation(programId_B, "diffuseTexture"), 3);
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, m_houses.materials[materialID].diffuse_tex);

		glUniform1i(glGetUniformLocation(programId_B, "normalTexture"), 4);
		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_2D, m_houses.normalTexture);

		glDrawElements(GL_TRIANGLES, m_houses.shapes[i].drawCount, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	m_houses.shaderB->disableShader();

}
#pragma endregion


#pragma region WindowFrame
struct {
	Shader* shader;
	GLuint vao;
	GLuint vbo;
} m_windowFrame;
float window_positions[16] = {
		 1.0f, -1.0f,  1.0f,  0.0f,
		-1.0f, -1.0f,  0.0f,  0.0f,
		-1.0f,  1.0f,  0.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,  1.0f
};
void window_init() {
	m_windowFrame.shader = new Shader("assets\\windowFrame_vertex.vs.glsl", "assets\\windowFrame_fragment.fs.glsl");
	//m_windowFrame.shader->useShader();
	//const GLuint programId = m_windowFrame.shader->getProgramID();
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
	//m_windowFrame.shader->disableShader();
}
void window_render(GLuint texture) {
	glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	m_windowFrame.shader->useShader();
	glUniform1i(glGetUniformLocation(m_windowFrame.shader->getProgramID(), "tex"), 3);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, texture);
	glBindVertexArray(m_windowFrame.vao);

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	m_windowFrame.shader->disableShader();
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

	GLuint bloomColor;
} frameBufferTexture;

struct {
	GLuint fbo;
	GLuint depthRBO;
} m_genTexture;
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

struct {
	Shader *shader;
	GLuint fbo;
	GLuint depthRBO;
} m_deferred;

void deferred_setBuffer() {
	glDeleteRenderbuffers(1, &m_deferred.depthRBO);
	glDeleteTextures(1, &frameBufferTexture.phongColor);
	glDeleteTextures(1, &frameBufferTexture.bloomHDR);

	// Render Buffer
	glGenRenderbuffers(1, &m_deferred.depthRBO);
	glBindRenderbuffer(GL_RENDERBUFFER, m_deferred.depthRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32, FRAME_WIDTH, FRAME_HEIGHT);

	// Frame Buffer
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_deferred.fbo);
	glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_deferred.depthRBO);

	//// Texture | 0: phongColor 1: bloomHDR
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

	// finally check if framebuffer is complete
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "Framebuffer not complete!" << std::endl;
}
void deferred_bindFrameBuffer() {
	const GLenum colorBuffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_deferred.fbo);
	glDrawBuffers(2, colorBuffers);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	static const GLfloat blue[] = { 0.0f, 0.0f, 0.5f, 1.0f };
	static const GLfloat one = 1.0f;

	glClearBufferfv(GL_COLOR, 0, blue);
	glClearBufferfv(GL_DEPTH, 0, &one);
}
void deferred_init() {
	glGenFramebuffers(1, &m_deferred.fbo);
	deferred_setBuffer();
	cout << "m_deferred Init" << endl;
	m_deferred.shader = new Shader("assets\\Deferred_vs.vs.glsl", "assets\\Deferred_fs.fs.glsl");
}
void deferred_render() {
	glClearColor(0.5f, 0.0f, 0.5f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	m_deferred.shader->useShader();
	const GLuint programId = m_deferred.shader->getProgramID();
	glUniform3fv(glGetUniformLocation(programId, "uv3LightPos"), 1, value_ptr(lightPosition));

	glUniform1i(glGetUniformLocation(programId, "tex_diffuse"), 3);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, frameBufferTexture.diffuse);

	glUniform1i(glGetUniformLocation(programId, "tex_ambient"), 4);
	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D, frameBufferTexture.ambient);

	glUniform1i(glGetUniformLocation(programId, "tex_specular"), 5);
	glActiveTexture(GL_TEXTURE5);
	glBindTexture(GL_TEXTURE_2D, frameBufferTexture.specular);

	glUniform1i(glGetUniformLocation(programId, "tex_ws_position"), 6);
	glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_2D, frameBufferTexture.ws_position);

	glUniform1i(glGetUniformLocation(programId, "tex_ws_normal"), 7);
	glActiveTexture(GL_TEXTURE7);
	glBindTexture(GL_TEXTURE_2D, frameBufferTexture.ws_normal);

	glUniform1i(glGetUniformLocation(programId, "tex_ws_tangent"), 8);
	glActiveTexture(GL_TEXTURE8);
	glBindTexture(GL_TEXTURE_2D, frameBufferTexture.ws_tangent);

	glBindVertexArray(m_windowFrame.vao);

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	m_deferred.shader->disableShader();
}

struct {
	Shader *shader;
	GLuint fbo;
	GLuint depthRBO;
} m_bloom;

void bloom_setBuffer() {
	glDeleteRenderbuffers(1, &m_bloom.depthRBO);
	glDeleteTextures(1, &frameBufferTexture.bloomColor);
	// Render Buffer
	glGenRenderbuffers(1, &m_bloom.depthRBO);
	glBindRenderbuffer(GL_RENDERBUFFER, m_bloom.depthRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32, FRAME_WIDTH, FRAME_HEIGHT);

	// Frame Buffer
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_bloom.fbo);
	glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_bloom.depthRBO);

	//// Texture | 0: bloomColor
	// bloomColor
	glGenTextures(1, &frameBufferTexture.bloomColor);
	glBindTexture(GL_TEXTURE_2D, frameBufferTexture.bloomColor);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, FRAME_WIDTH, FRAME_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, frameBufferTexture.bloomColor, 0);

	// finally check if framebuffer is complete
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "Framebuffer not complete!" << std::endl;
}
void bloom_bindFrameBuffrer() {
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_bloom.fbo);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	static const GLfloat pink[] = { 1.0f, 1.0f, 0.0f, 1.0f };
	static const GLfloat one = 1.0f;

	glClearBufferfv(GL_COLOR, 0, pink);
	glClearBufferfv(GL_DEPTH, 0, &one);
}
void bloom_init() {
	glGenFramebuffers(1, &m_bloom.fbo);
	bloom_setBuffer();
	cout << "m_bloom Init" << endl;
	m_bloom.shader = new Shader("assets\\Bloom_vs.vs.glsl", "assets\\Bloom_fs.fs.glsl");
}
void bloom_render() {
	glClearColor(0.5f, 0.5f, 1.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	m_bloom.shader->useShader();
	const GLuint programId = m_bloom.shader->getProgramID();

	glUniform1i(glGetUniformLocation(programId, "tex_bloomHDR"), 3);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, frameBufferTexture.bloomHDR);
	glUniform1i(glGetUniformLocation(programId, "tex_phongColor"), 4);
	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D, frameBufferTexture.phongColor);

	glBindVertexArray(m_windowFrame.vao);

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	m_bloom.shader->disableShader();
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

GLuint currentTexture = frameBufferTexture.diffuse;
void initializeGL() {
	m_renderer = new SceneRenderer();
	m_renderer->initialize(FRAME_WIDTH, FRAME_HEIGHT);

	m_eye = glm::vec3(512.0, 10.0, 512.0);
	// m_eye = glm::vec3(512.0, 10.0, 512.0);
	m_lookAtCenter = glm::vec3(512.0, 0.0, 500.0);
	m_lookDirection = m_lookAtCenter - m_eye;

	initScene();
	plane_init(m_airplaneRotMat, m_airplanePosition);
	houses_init(60.0f, vec3(631.0f, 130.0f, 468.0f), 15.0f, vec3(656.0f, 135.0f, 483.0f));
	window_init();
	
	glGenFramebuffers(1, &m_genTexture.fbo);
	genTexture_setBuffer();
	deferred_init();
	bloom_init();

	currentTexture = frameBufferTexture.phongColor;

	m_cameraProjection = perspective(glm::radians(60.0f), FRAME_WIDTH * 1.0f / FRAME_HEIGHT, 0.1f, 1000.0f);
	m_renderer->setProjection(m_cameraProjection);
}
void resizeGL(GLFWwindow *window, int w, int h) {
	FRAME_WIDTH = w;
	FRAME_HEIGHT = h;
	glViewport(0, 0, w, h);
	m_renderer->resize(w, h);
	genTexture_setBuffer();
	deferred_setBuffer();
	bloom_setBuffer();
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
	plane_render();
	houses_render();
	
	deferred_bindFrameBuffer();
	deferred_render();

	bloom_bindFrameBuffrer();
	bloom_render();
	
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	window_render(currentTexture);
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
			cout << "House use normal texture" << endl;
			break;
		case GLFW_KEY_1:
			currentTexture = frameBufferTexture.diffuse;
			cout << "Texture is [Diffuse]" << endl;
			break;
		case GLFW_KEY_2:
			currentTexture = frameBufferTexture.ambient;
			cout << "Texture is [Ambient]" << endl;
			break;
		case GLFW_KEY_3:
			currentTexture = frameBufferTexture.specular;
			cout << "Texture is [Specular]" << endl;
			break;
		case GLFW_KEY_4:
			currentTexture = frameBufferTexture.ws_position;
			cout << "Texture is [ws_position]" << endl;
			break;
		case GLFW_KEY_5:
			currentTexture = frameBufferTexture.ws_normal;
			cout << "Texture is [ws_normal]" << endl;
			break;
		case GLFW_KEY_6:
			currentTexture = frameBufferTexture.phongColor;
			cout << "Texture is [phongColor]" << endl;
			break;
		case GLFW_KEY_7:
			currentTexture = frameBufferTexture.bloomColor;
			cout << "Texture is [bloomColor]" << endl;
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

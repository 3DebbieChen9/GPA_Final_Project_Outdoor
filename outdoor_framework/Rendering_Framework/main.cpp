#include "src\basic\SceneRenderer.h"
#include <GLFW\glfw3.h>
#include "SceneSetting.h"

#include "assimp/scene.h"
#include "assimp/cimport.h"
#include "assimp/postprocess.h"
#include "../externals/include/stb_image.h"
#include "../externals/include/AntTweakBar/AntTweakBar.h"
#include "../externals/include/FreeGLUT/freeglut.h"

#pragma comment (lib, "lib-vc2017\\glfw3.lib")
#pragma comment (lib, "assimp\\assimp-vc141-mt.lib")
#pragma comment (lib, "freeglut.lib")
#pragma comment (lib, "AntTweakBar.lib")
#pragma comment (lib, "AntTweakBar64.lib")


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

////void My_Resize(int width, int height);

bool m_leftButtonPressed = false;
bool m_rightButtonPressed = false;
double cursorPos[2];

SceneRenderer *m_renderer = nullptr;
glm::vec3 m_lookAtCenter;
glm::vec3 m_eye;


void vsyncEnabled(GLFWwindow *window);
void vsyncDisabled(GLFWwindow *window);

PlantManager *m_plantManager = nullptr;
Terrain *m_terrain = nullptr;

// the airplane's transformation has been handled
glm::vec3 m_airplanePosition;
glm::mat4 m_airplaneRotMat;

struct Shape
{
	GLuint vao;
	GLuint vbo_position;
	GLuint vbo_normal;
	GLuint vbo_texcoord;
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

#pragma region Default Loader
// define a simple data structure for storing texture image raw data
typedef struct TextureData
{
	TextureData() : width(0), height(0), data(0) {}
	int width;
	int height;
	unsigned char* data;
} TextureData;

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
#pragma endregion

GLuint TEST_TEXTURE;

#pragma region Light Source
mat4 lightView(1.0f);
mat4 lightProjection(1.0f);
vec3 lightPosition = vec3(0.2f, 0.6f, 0.5f);
// Shadow
mat4 scale_bias_matrix = translate(mat4(1.0f), vec3(0.5f, 0.5f, 0.5f)) * scale(mat4(1.0f), vec3(0.5f, 0.5f, 0.5f));
#pragma endregion

#pragma region Camera
mat4 cameraProjection(1.0f);
#pragma endregion


#pragma region Airplane
class class_airplane {
public:
	// Constructor
	class_airplane() {}

	// Struct
	typedef struct struct_uniformID {
		// vs
		GLint um4m;
		GLint um4v;
		GLint um4p; 
		GLint um4Lightmpv;
		// fs
		GLint texLoc;
		GLint uv3Ambient;
		GLint uv3Specular;
		GLint uv3Diffuse;
		GLint ubPhongFlag;
	};
	////typedef struct struct_buffer {
	////	GLuint vao;
	////	GLuint vbo;
	////};
	typedef struct struct_matrix {
		mat4 model = mat4(1.0f);
		mat4 scale = mat4(1.0f);
		mat4 rotate = mat4(1.0f);
	};

	// Variable
	GLuint program;
	struct_uniformID uniformID;
	////struct_buffer buffer;
	struct_matrix matrix;
	int vertexCount;
	vector<Shape> shapes;
	vector<Material> materials;

	// Setting Functions
	void linkProgram() {
		// Create Shader Program
		this->program = glCreateProgram();
		// Create customize shader by tell openGL specify shader type 
		GLuint vs = glCreateShader(GL_VERTEX_SHADER);
		GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
		// Load shader file
		char** vs_source = loadShaderSource("airplane_vertex.vs.glsl");
		char** fs_source = loadShaderSource("airplane_fragment.fs.glsl");
		// Assign content of these shader files to those shaders we created before
		glShaderSource(vs, 1, vs_source, NULL);
		glShaderSource(fs, 1, fs_source, NULL);
		// Free the shader file string(won't be used any more)
		freeShaderSource(vs_source);
		freeShaderSource(fs_source);
		// Compile these shaders
		glCompileShader(vs);
		glCompileShader(fs);
		// Assign the program we created before with these shaders
		glAttachShader(this->program, vs);
		glAttachShader(this->program, fs);
		glLinkProgram(this->program);
	}

	void uniformLocation() {
		// vs
		this->uniformID.um4m = glGetUniformLocation(this->program, "um4m");
		this->uniformID.um4v = glGetUniformLocation(this->program, "um4v");
		this->uniformID.um4p = glGetUniformLocation(this->program, "um4p");
		this->uniformID.um4Lightmpv = glGetUniformLocation(this->program, "um4Lightmpv");
		// fs
		this->uniformID.texLoc = glGetUniformLocation(this->program, "tex");
		this->uniformID.ubPhongFlag = glGetUniformLocation(this->program, "ubPhongFlag");
		this->uniformID.uv3Ambient = glGetUniformLocation(this->program, "uv3Ambient");
		this->uniformID.uv3Specular = glGetUniformLocation(this->program, "uv3Specular");
		this->uniformID.uv3Diffuse = glGetUniformLocation(this->program, "uv3Diffuse");
	}

	void loadModel() {
		char* filepath = ".\\models\\airplane.obj";
		const aiScene* scene = aiImportFile(filepath, aiProcessPreset_TargetRealtime_MaxQuality);
		
		// Material
		for (unsigned int i = 0; i < scene->mNumMaterials; ++i)
		{
			aiMaterial* material = scene->mMaterials[i];
			Material Material;
			aiString texturePath;
			aiColor3D color;
			if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == aiReturn_SUCCESS)
			{
				// load width, height and data from texturePath.C_Str();
				string p = ".\\models\\"; /*..\\Assets\\*/
				texturePath = p + texturePath.C_Str();
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
			material->Get(AI_MATKEY_COLOR_AMBIENT, color);
			Material.ka = glm::vec4(color.r, color.g, color.b, 1.0f);
			material->Get(AI_MATKEY_COLOR_DIFFUSE, color);
			Material.kd = glm::vec4(color.r, color.g, color.b, 1.0f);
			material->Get(AI_MATKEY_COLOR_SPECULAR, color);
			Material.ks = glm::vec4(color.r, color.g, color.b, 1.0f);
			// save material
			this->materials.push_back(Material);
		}
		// Geometry
		for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
		{
			aiMesh* mesh = scene->mMeshes[i];
			Shape shape;
			glGenVertexArrays(1, &shape.vao);
			glBindVertexArray(shape.vao);
		
			// create 3 vbos to hold data
			glGenBuffers(1, &shape.vbo_position);
			glGenBuffers(1, &shape.vbo_texcoord);
			glGenBuffers(1, &shape.vbo_normal);
		
			vector<float> position;
			vector<float> normal;
			vector<float> texcoord;
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
			}
		
			// position
			glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_position);
			glBufferData(GL_ARRAY_BUFFER, mesh->mNumVertices * 3 * sizeof(GL_FLOAT), &position[0], GL_STATIC_DRAW);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
			glEnableVertexAttribArray(0);
		
			// texcoord
			glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_texcoord);
			glBufferData(GL_ARRAY_BUFFER, mesh->mNumVertices * 2 * sizeof(GL_FLOAT), &texcoord[0], GL_STATIC_DRAW);
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);
			glEnableVertexAttribArray(1);
		
			// normal
			glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_normal);
			glBufferData(GL_ARRAY_BUFFER, mesh->mNumVertices * 3 * sizeof(GL_FLOAT), &normal[0], GL_STATIC_DRAW);
			glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, 0);
			glEnableVertexAttribArray(2);
		
			// create 1 ibo to hold data
			glGenBuffers(1, &shape.ibo);
			vector<unsigned int> face;
			for (unsigned int f = 0; f < mesh->mNumFaces; ++f)
			{
				// mesh->mFaces[f].mIndices[0~2] => index
				face.push_back(mesh->mFaces[f].mIndices[0]);
				face.push_back(mesh->mFaces[f].mIndices[1]);
				face.push_back(mesh->mFaces[f].mIndices[2]);
			}
		
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, shape.ibo);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->mNumFaces * 3 * sizeof(unsigned int), &face[0], GL_STATIC_DRAW);
		
			shape.materialID = mesh->mMaterialIndex;
			shape.drawCount = mesh->mNumFaces * 3;
			// save shape
			this->shapes.push_back(shape);
		
			position.clear();
			position.shrink_to_fit();
			texcoord.clear();
			texcoord.shrink_to_fit();
			normal.clear();
			normal.shrink_to_fit();
			face.clear();
			face.shrink_to_fit();
		}
		
		aiReleaseImport(scene);

		this->matrix.model = translate(mat4(1.0f), m_airplanePosition);
		this->matrix.rotate = m_airplaneRotMat;
		cout << "Load airplane.obj | " << this->vertexCount << " vertices" << endl;
	}

	void passUniformData(int blinnPhongFlag) {
		// vs
		glUniformMatrix4fv(this->uniformID.um4m, 1, GL_FALSE, value_ptr(this->matrix.model));
		glUniformMatrix4fv(this->uniformID.um4v, 1, GL_FALSE, value_ptr(lookAt(m_eye, m_lookAtCenter, glm::vec3(0.0, 1.0, 0.0))));
		glUniformMatrix4fv(this->uniformID.um4p, 1, GL_FALSE, value_ptr(cameraProjection));
		glUniformMatrix4fv(this->uniformID.um4Lightmpv, 1, GL_FALSE, value_ptr(scale_bias_matrix * lightProjection * lightView * this->matrix.model * this->matrix.scale * this->matrix.rotate));
		// fs
		glUniform1i(this->uniformID.texLoc, 3);
		for (int i = 0; i < this->shapes.size(); i++) {
			int materialID = this->shapes[i].materialID;
			glUniform3fv(this->uniformID.uv3Ambient, 1, value_ptr(this->materials[materialID].ka));
			glUniform3fv(this->uniformID.uv3Specular, 1, value_ptr(this->materials[materialID].ks));
			glUniform3fv(this->uniformID.uv3Diffuse, 1, value_ptr(this->materials[materialID].kd));
		}
		glUniform1i(this->uniformID.ubPhongFlag, blinnPhongFlag);
	}

	void render(bool blinnPhongFlag) {
		if (blinnPhongFlag) {
			this->passUniformData(1);
		}
		else {
			this->passUniformData(0);
		}
	}
};

////vector<Shape> m_airplaneShapes;
////vector<Material> m_airplaneMaterials;
////GLuint m_airplaneProgram;
////// uniform
////GLint m_airplane_um4mv;
////GLint m_airplane_um4p;
////GLint m_airplane_ubPhongFlag;
////GLuint m_airplane_texLoc;
////GLuint m_airplane_ambient;
////GLuint m_airplane_specular;
////GLuint m_airplane_diffuse;
////
////bool m_airplane_PhongFlag;
////float viewportAspect = (float)FRAME_WIDTH / (float)FRAME_HEIGHT;
////glm::mat4 m_airplane_projection = perspective(glm::radians(60.0f), viewportAspect, 0.1f, 1000.0f);
////glm::mat4 m_airplane_model;
////glm::mat4 m_airplane_view = lookAt(vec3(-10.0f, 5.0f, 0.0f), vec3(1.0f, 1.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
////
////void airplane_loadModel() {
////	char* filepath = ".\\models\\airplane.obj";
////	const aiScene* scene = aiImportFile(filepath, aiProcessPreset_TargetRealtime_MaxQuality);
////
////	// Material
////	for (unsigned int i = 0; i < scene->mNumMaterials; ++i)
////	{
////		aiMaterial* material = scene->mMaterials[i];
////		Material Material;
////		aiString texturePath;
////		aiColor3D color;
////		if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == aiReturn_SUCCESS)
////		{
////			// load width, height and data from texturePath.C_Str();
////			string p = ".\\models\\"; /*..\\Assets\\*/
////			texturePath = p + texturePath.C_Str();
////			cout << texturePath.C_Str() << endl;
////			TextureData tex = loadImg(texturePath.C_Str());
////			glGenTextures(1, &Material.diffuse_tex);
////			glBindTexture(GL_TEXTURE_2D, Material.diffuse_tex);
////			//glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, tex.width, tex.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex.data);
////			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, tex.width, tex.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex.data);
////			glGenerateMipmap(GL_TEXTURE_2D);
////			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
////			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
////			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
////			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
////		}
////		material->Get(AI_MATKEY_COLOR_AMBIENT, color);
////		Material.ka = glm::vec4(color.r, color.g, color.b, 1.0f);
////		material->Get(AI_MATKEY_COLOR_DIFFUSE, color);
////		Material.kd = glm::vec4(color.r, color.g, color.b, 1.0f);
////		material->Get(AI_MATKEY_COLOR_SPECULAR, color);
////		Material.ks = glm::vec4(color.r, color.g, color.b, 1.0f);
////		// save material
////		m_airplaneMaterials.push_back(Material);
////	}
////	// Geometry
////	for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
////	{
////		aiMesh* mesh = scene->mMeshes[i];
////		Shape shape;
////		glGenVertexArrays(1, &shape.vao);
////		glBindVertexArray(shape.vao);
////
////		// create 3 vbos to hold data
////		glGenBuffers(1, &shape.vbo_position);
////		glGenBuffers(1, &shape.vbo_texcoord);
////		glGenBuffers(1, &shape.vbo_normal);
////
////		vector<float> position;
////		vector<float> normal;
////		vector<float> texcoord;
////		for (unsigned int v = 0; v < mesh->mNumVertices; ++v)
////		{
////			// mesh->mVertices[v][0~2] => position
////			position.push_back(mesh->mVertices[v][0]);
////			position.push_back(mesh->mVertices[v][1]);
////			position.push_back(mesh->mVertices[v][2]);
////			// mesh->mTextureCoords[0][v][0~1] => texcoord
////			texcoord.push_back(mesh->mTextureCoords[0][v][0]);
////			texcoord.push_back(mesh->mTextureCoords[0][v][1]);
////			// mesh->mNormals[v][0~2] => normal
////			normal.push_back(mesh->mNormals[v][0]);
////			normal.push_back(mesh->mNormals[v][1]);
////			normal.push_back(mesh->mNormals[v][2]);
////		}
////
////		// position
////		glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_position);
////		glBufferData(GL_ARRAY_BUFFER, mesh->mNumVertices * 3 * sizeof(GL_FLOAT), &position[0], GL_STATIC_DRAW);
////		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
////		glEnableVertexAttribArray(0);
////
////		// texcoord
////		glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_texcoord);
////		glBufferData(GL_ARRAY_BUFFER, mesh->mNumVertices * 2 * sizeof(GL_FLOAT), &texcoord[0], GL_STATIC_DRAW);
////		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);
////		glEnableVertexAttribArray(1);
////
////		// normal
////		glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_normal);
////		glBufferData(GL_ARRAY_BUFFER, mesh->mNumVertices * 3 * sizeof(GL_FLOAT), &normal[0], GL_STATIC_DRAW);
////		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, 0);
////		glEnableVertexAttribArray(2);
////
////		// create 1 ibo to hold data
////		glGenBuffers(1, &shape.ibo);
////		vector<unsigned int> face;
////		for (unsigned int f = 0; f < mesh->mNumFaces; ++f)
////		{
////			// mesh->mFaces[f].mIndices[0~2] => index
////			face.push_back(mesh->mFaces[f].mIndices[0]);
////			face.push_back(mesh->mFaces[f].mIndices[1]);
////			face.push_back(mesh->mFaces[f].mIndices[2]);
////		}
////
////		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, shape.ibo);
////		glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->mNumFaces * 3 * sizeof(unsigned int), &face[0], GL_STATIC_DRAW);
////
////		shape.materialID = mesh->mMaterialIndex;
////		shape.drawCount = mesh->mNumFaces * 3;
////		// save shape
////		m_airplaneShapes.push_back(shape);
////
////		position.clear();
////		position.shrink_to_fit();
////		texcoord.clear();
////		texcoord.shrink_to_fit();
////		normal.clear();
////		normal.shrink_to_fit();
////		face.clear();
////		face.shrink_to_fit();
////	}
////
////	aiReleaseImport(scene);
////}
////
////void airplane_program() {
////	m_airplaneProgram = glCreateProgram();
////
////	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
////	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
////
////	char** vertexShaderSource = loadShaderSource(".\\assets\\airplane_vertex.vs.glsl");
////	char** fragmentShaderSource = loadShaderSource(".\\assets\\airplane_fragment.fs.glsl");
////
////	glShaderSource(vertexShader, 1, vertexShaderSource, NULL);
////	glShaderSource(fragmentShader, 1, fragmentShaderSource, NULL);
////
////	// Free the shader file string(won't be used any more)
////	freeShaderSource(vertexShaderSource);
////	freeShaderSource(fragmentShaderSource);
////
////	// Compile these shaders
////	glCompileShader(vertexShader);
////	glCompileShader(fragmentShader);
////
////	glAttachShader(m_airplaneProgram, vertexShader);
////	glAttachShader(m_airplaneProgram, fragmentShader);
////	glLinkProgram(m_airplaneProgram);
////
////	m_airplane_um4mv = glGetUniformLocation(m_airplaneProgram, "um4mv");
////	m_airplane_um4p = glGetUniformLocation(m_airplaneProgram, "um4p");
////	m_airplane_ubPhongFlag = glGetUniformLocation(m_airplaneProgram, "ubPhongFlag");
////	m_airplane_texLoc = glGetUniformLocation(m_airplaneProgram, "tex");
////	m_airplane_ambient = glGetUniformLocation(m_airplaneProgram, "ambient");
////	m_airplane_specular = glGetUniformLocation(m_airplaneProgram, "specular");
////	m_airplane_diffuse = glGetUniformLocation(m_airplaneProgram, "diffuse");
////}
////
////void airplane_render() {
////
////	GLfloat move = 20.0;
////	m_airplane_model = glm::rotate(glm::mat4(1.0f), glm::radians(move), m_airplanePosition) * m_airplaneRotMat;
////	//m_airplane_model = translate(mat4(1.0f), m_airplanePosition) * m_airplaneRotMat;
////	glUseProgram(m_airplaneProgram);
////	//cout << "m_airplaneProgram " << m_airplaneProgram << endl;
////	glUniformMatrix4fv(m_airplane_um4mv, 1, GL_FALSE, glm::value_ptr(m_airplane_view * m_airplane_model));
////	glUniformMatrix4fv(m_airplane_um4p, 1, GL_FALSE, glm::value_ptr(m_airplane_projection));
////	glUniform1i(m_airplane_ubPhongFlag, m_airplane_PhongFlag);
////	glActiveTexture(GL_TEXTURE3);
////	glUniform1i(m_airplane_texLoc, 3);
////	for (int i = 0; i < m_airplaneShapes.size(); i++) {
////		glBindVertexArray(m_airplaneShapes[i].vao);
////		int materialID = m_airplaneShapes[i].materialID;
////		glUniform3fv(m_airplane_ambient, 1, value_ptr(m_airplaneMaterials[materialID].ka));
////		glUniform3fv(m_airplane_specular, 1, value_ptr(m_airplaneMaterials[materialID].ks));
////		glUniform3fv(m_airplane_diffuse, 1, value_ptr(m_airplaneMaterials[materialID].kd));
////		glEnableVertexAttribArray(0);
////		glEnableVertexAttribArray(1);
////		glEnableVertexAttribArray(2);
////		glBindTexture(GL_TEXTURE_2D, m_airplaneMaterials[materialID].diffuse_tex);
////		glDrawElements(GL_TRIANGLES, m_airplaneShapes[i].drawCount, GL_UNSIGNED_INT, 0);
////	}
////	glBindVertexArray(0);
////	glBindTexture(GL_TEXTURE_2D, 0);
////	glUseProgram(0);
////
////}
////
////void airplane_init() {
////	airplane_loadModel();
////	airplane_program();
////	m_airplane_PhongFlag = true;
////}
#pragma endregion

#pragma region Window Frame
class class_windowFrame {
public:
	// Constructor
	class_windowFrame() {}

	// Struct
	typedef struct struct_uniformID {
		GLuint tex;
	};
	typedef struct struct_buffer {
		GLuint vao;
		GLuint vbo;
	};

	// Variable
	GLuint program;
	struct_uniformID unifromID;
	struct_buffer buffer;

	float window_positions[16] = {
		 1.0f, -1.0f,  1.0f,  0.0f,
		-1.0f, -1.0f,  0.0f,  0.0f,
		-1.0f,  1.0f,  0.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,  1.0f
	};

	// Setting Functions
	void linkProgram() {
		// Create Shader Program
		this->program = glCreateProgram();
		// Create customize shader by tell openGL specify shader type 
		GLuint vs = glCreateShader(GL_VERTEX_SHADER);
		GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
		// Load shader file
		char** vs_source = loadShaderSource("windowFrame_vertex.vs.glsl");
		char** fs_source = loadShaderSource("windowFrame_fragment.fs.glsl");
		// Assign content of these shader files to those shaders we created before
		glShaderSource(vs, 1, vs_source, NULL);
		glShaderSource(fs, 1, fs_source, NULL);
		// Free the shader file string(won't be used any more)
		freeShaderSource(vs_source);
		freeShaderSource(fs_source);
		// Compile these shaders
		glCompileShader(vs);
		glCompileShader(fs);
		// Assign the program we created before with these shaders
		glAttachShader(this->program, vs);
		glAttachShader(this->program, fs);
		glLinkProgram(this->program);
	}

	void uniformLocation() {
		this->unifromID.tex = glGetUniformLocation(this->program, "text");
	}

	void setBuffer() {
		glGenVertexArrays(1, &this->buffer.vao);
		glBindVertexArray(this->buffer.vao);

		glGenBuffers(1, &this->buffer.vbo);
		glBindBuffer(GL_ARRAY_BUFFER, this->buffer.vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(this->window_positions), this->window_positions, GL_STATIC_DRAW);

		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT) * 4, 0);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT) * 4, (const GLvoid*)(sizeof(GL_FLOAT) * 2));
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);

		glBindVertexArray(0);
		glBindBuffer(GL_VERTEX_ARRAY, 0);
	}

	void passUniformData() {
		glUniform1i(this->unifromID.tex, 3);
	}

	void render() {
		this->passUniformData();
		glBindVertexArray(this->buffer.vao);
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, TEST_TEXTURE);

		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	}

	void initial() {
		this->linkProgram();
		this->uniformLocation();
		this->setBuffer();
	}

	void useProgram() {
		glUseProgram(this->program);
		this->render();
	}
};
class_windowFrame m_windowFrame;

//GLuint m_windowProgram;
//GLuint m_windowVAO;
//GLuint m_windowVBO;
////GLuint m_windowDepthRBO;
////GLuint m_windowFBO;
////GLuint m_windowFBODataTexture;
//// uniform
//GLuint m_window_texLoc;
//
//static const GLfloat m_windowPosition[] =
//{
//	 1.0f, -1.0f,  1.0f,  0.0f,
//	-1.0f, -1.0f,  0.0f,  0.0f,
//	-1.0f,  1.0f,  0.0f,  1.0f,
//	 1.0f,  1.0f,  1.0f,  1.0f
//};

//void frameBuffer_program() {
//	m_frameProgram = glCreateProgram();
//
//	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
//	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
//
//	char** vertexShaderSource = loadShaderSource(".\\assets\\frameBuffer_vertex.vs.glsl");
//	char** fragmentShaderSource = loadShaderSource(".\\assets\\frameBuffer_fragment.fs.glsl");
//
//	glShaderSource(vertexShader, 1, vertexShaderSource, NULL);
//	glShaderSource(fragmentShader, 1, fragmentShaderSource, NULL);
//
//	// Free the shader file string(won't be used any more)
//	freeShaderSource(vertexShaderSource);
//	freeShaderSource(fragmentShaderSource);
//
//	// Compile these shaders
//	glCompileShader(vertexShader);
//	glCompileShader(fragmentShader);
//
//	glAttachShader(m_frameProgram, vertexShader);
//	glAttachShader(m_frameProgram, fragmentShader);
//	glLinkProgram(m_frameProgram);
//
//	glGenVertexArrays(1, &m_frameVAO);
//	glBindVertexArray(m_frameVAO);
//
//	glGenBuffers(1, &m_frameVBO);
//	glBindBuffer(GL_ARRAY_BUFFER, m_frameVBO);
//	glBufferData(GL_ARRAY_BUFFER, sizeof(m_framePosition), m_framePosition, GL_STATIC_DRAW);
//
//	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT) * 4, 0);
//	glEnableVertexAttribArray(0);
//	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT) * 4, (const GLvoid*)(sizeof(GL_FLOAT) * 2));
//	glEnableVertexAttribArray(1);
//
//	glGenFramebuffers(1, &m_frameFBO);
//	m_frame_texLoc = glGetUniformLocation(m_frameProgram, "tex");
//
//	My_Resize(FRAME_WIDTH, FRAME_HEIGHT);
//}
//
//void frameBuffer_render() {
//	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
//	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//	glClearColor(1.0f, 0.5f, 0.5f, 0.5f);
//	
//	glUseProgram(m_frameProgram);
//
//	glUniform1i(m_frame_texLoc, 0);
//
//	glBindVertexArray(m_frameVAO);
//	glActiveTexture(GL_TEXTURE0);
//	glBindTexture(GL_TEXTURE_2D, m_frameFBODataTexture);
//
//	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
//}
#pragma endregion




#pragma region GUI
//TwBar* bar;
//
////void TW_CALL SSAO_Switch(void* clientData)
////{
////	ssaoEffect.ssaoSwitch = !ssaoEffect.ssaoSwitch;
////
////}
//void TW_CALL blinnPhongChange(void* clientData) {
//	m_airplane_PhongFlag = !m_airplane_PhongFlag;
//}
//
//void setupGUI() {
//	// Initialize AntTweakBar
//	//TwDefine(" GLOBAL fontscaling=2 ");
//#ifdef _MSC_VER
//	TwInit(TW_OPENGL, NULL);
//#else
//	TwInit(TW_OPENGL_CORE, NULL);
//#endif
//	//TwGLUTModifiersFunc(glutGetModifiers);
//	bar = TwNewBar("Properties");
//	TwDefine(" Properties size='300 220' ");
//	TwDefine(" Properties fontsize='3' color='0 0 0' alpha=180 ");
//
//	//TwAddButton(bar, "SSAO_SWITH", SSAO_Switch, NULL, " label='SSAO_SWITCH' ");
//	/*TwAddVarRW(bar, "LightPosition_x", TW_TYPE_FLOAT, &(light_position.x), "label='Light Position.x'");
//	TwAddVarRW(bar, "LightPosition_y", TW_TYPE_FLOAT, &(light_position.y), "label='Light Position.y'");
//	TwAddVarRW(bar, "LightPosition_z", TW_TYPE_FLOAT, &(light_position.z), "label='Light Position.z'");*/
//	TwAddButton(bar, "BlinnPhong", blinnPhongChange, NULL, " label='BlinnPhong' ");/*
//	TwAddButton(bar, "particle", particleChange, NULL, "label = 'Particle'");*/
//}
#pragma endregion

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
	m_lookAtCenter = glm::vec3(512.0, 0.0, 500.0);


	initScene();
	//setupGUI();
	//airplane_init();

	m_renderer->setProjection(glm::perspective(glm::radians(60.0f), FRAME_WIDTH * 1.0f / FRAME_HEIGHT, 0.1f, 1000.0f));
}
////void My_Resize(int width, int height) {
////	// If the windows is reshaped, we need to reset some settings of framebuffer
////	glDeleteRenderbuffers(1, &m_frameDepthRBO);
////	glDeleteTextures(1, &m_frameFBODataTexture);
////	glGenRenderbuffers(1, &m_frameDepthRBO);
////	glBindRenderbuffer(GL_RENDERBUFFER, m_frameDepthRBO);
////	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32, width, height);
////
////	glGenTextures(1, &m_frameFBODataTexture);
////	glBindTexture(GL_TEXTURE_2D, m_frameFBODataTexture);
////	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
////	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
////	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
////	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
////	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
////
////	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_frameFBO);
////	glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_frameDepthRBO);
////	glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_frameFBODataTexture, 0);
////}
void resizeGL(GLFWwindow *window, int w, int h) {
	FRAME_WIDTH = w;
	FRAME_HEIGHT = h;
	m_renderer->resize(w, h);
	m_renderer->setProjection(glm::perspective(glm::radians(60.0f), w * 1.0f / h, 0.1f, 1000.0f));
	cameraProjection = perspective(glm::radians(60.0f), w * 1.0f / h, 0.1f, 1000.0f);

	////My_Resize(FRAME_WIDTH, FRAME_HEIGHT);
}

void updateState() {
	// [TODO] update your eye position and look-at center here
	// m_eye = ... ;
	// m_lookAtCenter = ... ;
	m_lookAtCenter.z = m_lookAtCenter.z + 1;
	m_eye.z = m_eye.z + 1;

	// adjust camera position with terrain
	adjustCameraPositionWithTerrain();

	// calculate camera matrix
	glm::mat4 vm = glm::lookAt(m_eye, m_lookAtCenter, glm::vec3(0.0, 1.0, 0.0));

	// [IMPORTANT] set camera information to renderer
	m_renderer->setView(vm, m_eye);

	// update airplane
	updateAirplane(vm);
}
void paintGL() {
	////glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_frameFBO);
	////glDrawBuffer(GL_COLOR_ATTACHMENT0);
	////glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// render terrain
	m_renderer->renderPass();

	// [TODO] implement your rendering function here
	////airplane_render();
	////frameBuffer_render();
	////TwDraw();
	////glutSwapBuffers();
}

////////////////////////////////////////////////
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {}
void mouseScrollCallback(GLFWwindow *window, double xoffset, double yoffset) {}
void cursorPosCallback(GLFWwindow* window, double x, double y) {
	cursorPos[0] = x;
	cursorPos[1] = y;
	//cout << "Cursor Click at (" << x << ", " << y << ")" << endl;
}
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	printf("\nKey %d is pressed ", key);
	switch (key)
	{
	case GLFW_KEY_A:
		/*m_airplane_PhongFlag = !m_airplane_PhongFlag;
		if (m_airplane_PhongFlag) {
			cout << "True" << endl;
		}
		else {
			cout << "False" << endl;
		}*/
		break;
	}
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
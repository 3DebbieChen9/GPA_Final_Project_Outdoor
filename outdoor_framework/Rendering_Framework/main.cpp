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

bool m_leftButtonPressed = false;
bool m_leftButtonPressedFlag = false;
bool m_rightButtonPressed = false;

double cursorPos[2];

SceneRenderer *m_renderer = nullptr;
glm::vec3 m_lookAtCenter = vec3(0.0f, 0.0f, 0.0f);
glm::vec3 m_eye = vec3(0.0f, 0.0f, 0.0f); // Eye Position / Camera Position
mat4 m_cameraProjection(1.0f);
#pragma region Trackball
vec3 m_lookDirection = vec3(0.0f, 0.0f, 0.0f);
vec3 m_upDirection = vec3(0.0f, 1.0f, 0.0f);
vec3 m_uAxis = vec3(1.0f, 0.0f, 0.0f);
vec3 m_vAxis = vec3(0.0f, 1.0f, 0.0f);
float m_cameraMoveSpeed = 2.0f;//0.05f;
vec2 m_trackball_initial = vec2(0, 0);
vec2 m_oldDistance = vec2(0, 0);
vec2 m_rotateAngle = vec2(0.0f, 0.0f);
#pragma endregion



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

#pragma region Light Source
mat4 lightView(1.0f);
mat4 lightProjection(1.0f);
vec3 lightPosition = vec3(0.2f, 0.6f, 0.5f);
// Shadow
mat4 scale_bias_matrix = translate(mat4(1.0f), vec3(0.5f, 0.5f, 0.5f)) * scale(mat4(1.0f), vec3(0.5f, 0.5f, 0.5f));
#pragma endregion



#pragma region Airplane
class class_airplane {
public:
	// Constructor
	class_airplane() {}
	// Struct
	typedef struct struct_uniformID {
		// vs
		GLuint um4m;
		GLuint um4v;
		GLuint um4p;
		GLuint um4Lightmvp;
		// fs
		GLuint texLoc;
		GLuint uv3Ambient;
		GLuint uv3Specular;
		GLuint uv3Diffuse;
		GLuint ubPhongFlag;
	};
	typedef struct struct_matrix {
		mat4 model = mat4(1.0f);
		mat4 scale = mat4(1.0f);
		mat4 rotate = mat4(1.0f);
	};
	// Variables
	GLuint program;
	struct_uniformID uniformID;
	struct_matrix matrix;
	vector<Shape> shapes;
	vector<Material> materials;

	bool blinnPhongFlag;

	// Setting Functions
	void linkProgram() {
		// Create Shader Program
		this->program = glCreateProgram();
		// Create customize shader by tell openGL specify shader type 
		GLuint vs = glCreateShader(GL_VERTEX_SHADER);
		GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
		// Load shader file
		char** vs_source = loadShaderSource(".\\assets\\airplane_vertex.vs.glsl");
		char** fs_source = loadShaderSource(".\\assets\\airplane_fragment.fs.glsl");
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
		this->uniformID.um4Lightmvp = glGetUniformLocation(this->program, "um4Lightmvp");
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
		
			// create 3 vbos to hold data
			glGenBuffers(1, &shape.vbo_position);
			glGenBuffers(1, &shape.vbo_texcoord);
			glGenBuffers(1, &shape.vbo_normal);

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
		cout << "Load airplane.obj done" << endl;
	}

	void passUniformData(int blinnPhongFlag) {
		// vs
		glUniformMatrix4fv(this->uniformID.um4m, 1, GL_FALSE, value_ptr(this->matrix.model * this->matrix.rotate));
		glUniformMatrix4fv(this->uniformID.um4v, 1, GL_FALSE, value_ptr(lookAt(m_eye, m_lookAtCenter, m_upDirection)));
		glUniformMatrix4fv(this->uniformID.um4p, 1, GL_FALSE, value_ptr(m_cameraProjection));
		glUniformMatrix4fv(this->uniformID.um4Lightmvp, 1, GL_FALSE, value_ptr(scale_bias_matrix * lightProjection * lightView * this->matrix.model * this->matrix.scale * this->matrix.rotate));
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

	void initial() {
		GLfloat move = 20.0;
		//this->matrix.model = rotate(mat4(1.0f), radians(move), m_airplanePosition);
		//this->matrix.model = translate(mat4(1.0f), vec3(0.0, 0.0, 0.0));
		//this->matrix.model = translate(mat4(1.0f), vec3(200.0, 70.0, 100.0));
		//this->matrix.model = mat4(1.0f);
		
		this->matrix.rotate = m_airplaneRotMat;
		this->matrix.model = translate(mat4(1.0f), m_airplanePosition);// *this->matrix.rotate;
		this->blinnPhongFlag = false;

		this->loadModel();
		this->linkProgram();
		cout << "program " << this->program << endl;
		this->uniformLocation();
	}

	void render() {
		//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		//glClearColor(1.0f, 0.5f, 1.0f, 1.0f);
		glUseProgram(this->program);
		this->matrix.rotate = m_airplaneRotMat;
		this->matrix.model = translate(mat4(1.0f), m_airplanePosition);
		if (this->blinnPhongFlag) {
			this->passUniformData(1);
		}
		else {
			this->passUniformData(0);
		}
		/*glActiveTexture(GL_TEXTURE3);*/
		for (int i = 0; i < this->shapes.size(); i++) {
			glBindVertexArray(this->shapes[i].vao);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->shapes[i].ibo);
			int materialID = this->shapes[i].materialID;
			glActiveTexture(GL_TEXTURE3);
			glBindTexture(GL_TEXTURE_2D, this->materials[materialID].diffuse_tex);
			glDrawElements(GL_TRIANGLES, this->shapes[i].drawCount, GL_UNSIGNED_INT, 0);
		}
		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D, 0);
		//glUseProgram(0);
	}
};
class_airplane m_airplane;
#pragma endregion

#pragma region House
class class_house {
public:
	// Constructor
	class_house() {}
	// Struct
	typedef struct struct_uniformID {
		// vs
		GLuint um4m;
		GLuint um4v;
		GLuint um4p;
		GLuint um4Lightmvp;
		// fs
		GLuint texLoc;
		GLuint uv3Ambient;
		GLuint uv3Specular;
		GLuint uv3Diffuse;
		GLuint ubPhongFlag;
	};
	typedef struct struct_matrix {
		mat4 model = mat4(1.0f);
		mat4 scale = mat4(1.0f);
		mat4 rotate = mat4(1.0f);
	};
	// Variables
	GLuint program;
	struct_uniformID uniformID;
	struct_matrix matrix;
	vector<Shape> shapes;
	vector<Material> materials;

	bool blinnPhongFlag;
	float rotateAngle;
	vec3 position;

	// Setting Functions
	void linkProgram() {
		// Create Shader Program
		this->program = glCreateProgram();
		// Create customize shader by tell openGL specify shader type 
		GLuint vs = glCreateShader(GL_VERTEX_SHADER);
		GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
		// Load shader file
		char** vs_source = loadShaderSource(".\\assets\\house_vertex.vs.glsl");
		char** fs_source = loadShaderSource(".\\assets\\house_fragment.fs.glsl");
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
		this->uniformID.um4Lightmvp = glGetUniformLocation(this->program, "um4Lightmvp");
		// fs
		this->uniformID.texLoc = glGetUniformLocation(this->program, "tex");
		this->uniformID.ubPhongFlag = glGetUniformLocation(this->program, "ubPhongFlag");
		this->uniformID.uv3Ambient = glGetUniformLocation(this->program, "uv3Ambient");
		this->uniformID.uv3Specular = glGetUniformLocation(this->program, "uv3Specular");
		this->uniformID.uv3Diffuse = glGetUniformLocation(this->program, "uv3Diffuse");
	}
	void loadModel() {
		char* filepath = ".\\models\\medievalHouse.obj";
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

			// create 3 vbos to hold data
			glGenBuffers(1, &shape.vbo_position);
			glGenBuffers(1, &shape.vbo_texcoord);
			glGenBuffers(1, &shape.vbo_normal);

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
		cout << "Load medievalHouse.obj done" << endl;
	}

	void passUniformData(int blinnPhongFlag) {
		// vs
		glUniformMatrix4fv(this->uniformID.um4m, 1, GL_FALSE, value_ptr(this->matrix.model * this->matrix.rotate));
		glUniformMatrix4fv(this->uniformID.um4v, 1, GL_FALSE, value_ptr(lookAt(m_eye, m_lookAtCenter, m_upDirection)));
		glUniformMatrix4fv(this->uniformID.um4p, 1, GL_FALSE, value_ptr(m_cameraProjection));
		glUniformMatrix4fv(this->uniformID.um4Lightmvp, 1, GL_FALSE, value_ptr(scale_bias_matrix * lightProjection * lightView * this->matrix.model * this->matrix.scale * this->matrix.rotate));
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

	void initial(float _rotateAngle, vec3 _position) {
		this->rotateAngle = _rotateAngle;
		this->position = _position;
		this->matrix.rotate = rotate(mat4(1.0f), radians(this->rotateAngle), vec3(0.0f, 1.0f, 0.0f));
		this->matrix.model = translate(mat4(1.0f), this->position);
		this->blinnPhongFlag = false;
		
		this->loadModel();
		this->linkProgram();
		cout << "program " << this->program << endl;
		this->uniformLocation();
	}

	void render() {
		//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		//glClearColor(1.0f, 0.5f, 1.0f, 1.0f);
		glUseProgram(this->program);
		if (this->blinnPhongFlag) {
			this->passUniformData(1);
		}
		else {
			this->passUniformData(0);
		}
		/*glActiveTexture(GL_TEXTURE3);*/
		for (int i = 0; i < this->shapes.size(); i++) {
			glBindVertexArray(this->shapes[i].vao);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->shapes[i].ibo);
			int materialID = this->shapes[i].materialID;
			glActiveTexture(GL_TEXTURE3);
			glBindTexture(GL_TEXTURE_2D, this->materials[materialID].diffuse_tex);
			glDrawElements(GL_TRIANGLES, this->shapes[i].drawCount, GL_UNSIGNED_INT, 0);
		}
		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D, 0);
		//glUseProgram(0);
	}
};
class_house m_houseA;
class_house m_houseB;
#pragma endregion


#pragma region Combination
class class_combination {
public:
	// Constructor
	class_combination() {}
	// Struct
	typedef struct struct_buffer {
		GLuint fbo;
		GLuint depthRBO;
	};
	// Variables
	struct_buffer buffer;
	GLuint texture;

	// Functions
	void setBuffer() {
		// FBO
		glDeleteFramebuffers(1, &this->buffer.fbo);
		glGenFramebuffers(1, &this->buffer.fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, this->buffer.fbo);

		glDeleteTextures(1, &this->texture);
		glGenTextures(1, &this->texture);
		glBindTexture(GL_TEXTURE_2D, this->texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, FRAME_WIDTH, FRAME_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, this->texture, 0);

		// DepthRBO
		glDeleteRenderbuffers(1, &this->buffer.depthRBO);
		glGenRenderbuffers(1, &this->buffer.depthRBO);
		glBindRenderbuffer(GL_RENDERBUFFER, this->buffer.depthRBO);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, FRAME_WIDTH, FRAME_HEIGHT);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, this->buffer.depthRBO);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glBindTexture(GL_TEXTURE_2D, 0);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);
	}

	void framebufferRender() {
		glBindFramebuffer(GL_FRAMEBUFFER, this->buffer.fbo);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		glClearColor(0.0f, 1.0f, 0.0f, 1.0f);

		// Enable Depth Test
		glEnable(GL_DEPTH_TEST);
		glDisable(GL_STENCIL_TEST);

		//m_renderer->renderPass();
		m_airplane.render();
	}
};
class_combination m_combination;
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
		char** vs_source = loadShaderSource(".\\assets\\windowFrame_vertex.vs.glsl");
		char** fs_source = loadShaderSource(".\\assets\\windowFrame_fragment.fs.glsl");
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
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT) * 4, (const GLvoid*)(sizeof(GL_FLOAT) * 2));
		glEnableVertexAttribArray(1);

		glBindVertexArray(0);
		glBindBuffer(GL_VERTEX_ARRAY, 0);
	}

	void passUniformData() {
		glUniform1i(this->unifromID.tex, 3);
	}

	void initial() {
		this->linkProgram();
		this->uniformLocation();
		this->setBuffer();
	}

	void render() {
		glUseProgram(this->program);
		this->passUniformData();
		glBindVertexArray(this->buffer.vao);
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, m_combination.texture);

		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	}
};
class_windowFrame m_windowFrame;
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
	glfwSetCursorPosCallback(window, cursorPosCallback);
	glfwSetMouseButtonCallback(window, mouseButtonCallback);
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
	m_lookDirection = m_lookAtCenter - m_eye;

	initScene();
	m_airplane.initial();
	m_houseA.initial(60.0f, vec3(631.0f, 130.0f, 468.0f));
	m_houseB.initial(15.0f, vec3(656.0f, 135.0f, 483.0f));
	//m_windowFrame.initial();
	//m_combination.setBuffer();
	m_renderer->setProjection(glm::perspective(glm::radians(60.0f), FRAME_WIDTH * 1.0f / FRAME_HEIGHT, 0.1f, 1000.0f));
	m_cameraProjection = perspective(glm::radians(60.0f), FRAME_WIDTH * 1.0f / FRAME_HEIGHT, 0.1f, 1000.0f);
}

void resizeGL(GLFWwindow *window, int w, int h) {
	FRAME_WIDTH = w;
	FRAME_HEIGHT = h;
	m_renderer->resize(w, h);
	m_renderer->setProjection(glm::perspective(glm::radians(60.0f), w * 1.0f / h, 0.1f, 1000.0f));
	m_cameraProjection = perspective(glm::radians(60.0f), w * 1.0f / h, 0.1f, 1000.0f);
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
	//glm::mat4 vm = glm::lookAt(m_eye, m_lookAtCenter, glm::vec3(0.0, 1.0, 0.0));
	mat4 vm = lookAt(m_eye, m_lookAtCenter, m_upDirection);

	// [IMPORTANT] set camera information to renderer
	m_renderer->setView(vm, m_eye);

	// update airplane
	updateAirplane(vm);
}
void paintGL() {
	// render terrain
	m_renderer->renderPass();

	// [TODO] implement your rendering function here
	//m_combination.framebufferRender();
	//glBindFramebuffer(GL_FRAMEBUFFER, 0);
	//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	//glClearColor(0.0f, 0.0f, 1.0f, 1.0f);

	//m_windowFrame.render();
	m_airplane.render();
	m_houseA.render();
	m_houseB.render();
	
}

////////////////////////////////////////////////
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
#pragma region Trackball
	if (action == GLFW_PRESS)
	{
		if (button == GLFW_MOUSE_BUTTON_LEFT) {
			m_leftButtonPressed = true;
			m_leftButtonPressedFlag = true;
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
	//cout << "Cursor at (" << x << ", " << y << ")" << endl;
#pragma region Trackball
	if (m_leftButtonPressed) {
		if (m_leftButtonPressedFlag) {
			m_trackball_initial = vec2(cursorPos[0], cursorPos[1]);
			m_leftButtonPressedFlag = false;
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
	printf("\nKey %d is pressed ", key);
	switch (key)
	{
#pragma region Trackball
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
#pragma endregion

	case GLFW_KEY_Z:
		m_airplane.blinnPhongFlag = !m_airplane.blinnPhongFlag;
		if (m_airplane.blinnPhongFlag) {
			cout << "True" << endl;
		}
		else {
			cout << "False" << endl;
		}
		break;
	
	default:
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
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
mat4 scale_bias_matrix = translate(mat4(1.0f), vec3(0.5f, 0.5f, 0.5f)) * scale(mat4(1.0f), vec3(0.5f, 0.5f, 0.5f));
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
	GLuint ambient_tex;
	GLuint specular_tex;
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

#pragma region Object
class ObjectClass {
public:
	// Constructor / Destructor
	ObjectClass() {};
	~ObjectClass() {};


};
#pragma endregion


#pragma region Airplane Class
class AirplaneClass {
public:
	// Constructor / Destructor
	 AirplaneClass() {};
    ~AirplaneClass() {};

	// Struct
	typedef struct struct_uniformID {
		// vs
		GLint um4m;
		GLint um4v;
		GLint um4p;
		GLint um4Lightmvp;
		GLint uv3LightPos;
		// fs
		//GLint texLoc_diffuse;
		//GLint texLoc_ambient;
		//GLint texLoc_specular;
		GLint texLoc;
		GLint uv3Ambient;
		GLint uv3Specular;
		GLint uv3Diffuse;
		GLint ubPhongFlag;
	};
	typedef struct struct_matrix {
		mat4 model = mat4(1.0f);
		mat4 scale = mat4(1.0f);
		mat4 rotate = mat4(1.0f);
	};

private:
    GLuint programID;
    struct_matrix matrix;
    vector<Shape> shapes;
    vector<Material> materials;

public:
    bool blinnPhongFlag;
	struct_uniformID uniformID;
	//struct
	//{
	//	GLint diffuse;
	//	GLint ambient;
	//	GLint specular;
	//	GLint worldVertex;
	//	GLint worldNormal;
	//} texture;

private:
    void loadModel();
    void getUniformLocation();
	void linkProgram();

public:
	void initial(mat4 _rotateMatrix, vec3 _position);
    void render();
};

void AirplaneClass::loadModel() {
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
        if(material->Get(AI_MATKEY_COLOR_AMBIENT, color) == aiReturn_SUCCESS){
			Material.ka = glm::vec4(color.r, color.g, color.b, 1.0f);
		}
		else {
			cout << "no ambient" << endl;
		}
		if(material->Get(AI_MATKEY_COLOR_DIFFUSE, color) == aiReturn_SUCCESS){
			Material.kd = glm::vec4(color.r, color.g, color.b, 1.0f);
		}
		else {
			cout << "no diffuse" << endl;
		}
		if(material->Get(AI_MATKEY_COLOR_SPECULAR, color) == aiReturn_SUCCESS){
			Material.ks = glm::vec4(color.r, color.g, color.b, 1.0f);
		}
		else {
			cout << "no specular" << endl;
		}
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
        this->shapes.push_back(shape);
    
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

void AirplaneClass::getUniformLocation() {
    // vs
	this->uniformID.um4m = glGetUniformLocation(this->programID, "um4m");
	this->uniformID.um4v = glGetUniformLocation(this->programID, "um4v");
	this->uniformID.um4p = glGetUniformLocation(this->programID, "um4p");
	this->uniformID.um4Lightmvp = glGetUniformLocation(this->programID, "um4Lightmvp");
	this->uniformID.uv3LightPos = glGetUniformLocation(this->programID, "uv3LightPos");
	// fs
	//this->uniformID.texLoc_diffuse = glGetUniformLocation(this->programID, "tex_diffuse");
	//this->uniformID.texLoc_ambient = glGetUniformLocation(this->programID, "tex_ambient");
	//this->uniformID.texLoc_specular = glGetUniformLocation(this->programID, "tex_specular");
	this->uniformID.texLoc = glGetUniformLocation(this->programID, "tex");
	this->uniformID.ubPhongFlag = glGetUniformLocation(this->programID, "ubPhongFlag");
	this->uniformID.uv3Ambient = glGetUniformLocation(this->programID, "uv3Ambient");
	this->uniformID.uv3Specular = glGetUniformLocation(this->programID, "uv3Specular");
	this->uniformID.uv3Diffuse = glGetUniformLocation(this->programID, "uv3Diffuse");
}

void AirplaneClass::linkProgram() {
	// Create Shader Program
	this->programID = glCreateProgram();
	// Create customize shader by tell openGL specify shader type 
	GLuint vs = glCreateShader(GL_VERTEX_SHADER);
	GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
	// Load shader file
	//char** vs_source = loadShaderSource(".\\assets\\geometry_vs.vs.glsl");
	//char** fs_source = loadShaderSource(".\\assets\\geometry_fs.fs.glsl");
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
	// Logging
	shaderLog(vs);
	shaderLog(fs);
	// Assign the program we created before with these shaders
	glAttachShader(this->programID, vs);
	glAttachShader(this->programID, fs);
	glLinkProgram(this->programID);
	// It is not required to hande the shader in memory
   	glDeleteShader(vs);
   	glDeleteShader(fs);
}

void AirplaneClass::initial(mat4 _rotateMatrix, vec3 _position) {
	this->matrix.rotate = _rotateMatrix;
    this->matrix.model = translate(mat4(1.0f), _position);

    this->blinnPhongFlag = false;
	
	this->linkProgram();
	this->getUniformLocation();
    this->loadModel();
}

void AirplaneClass::render() {
   	this->matrix.rotate = m_airplaneRotMat;
	this->matrix.model = translate(mat4(1.0f), m_airplanePosition);

	glUseProgram(this->programID);
	// vs
	glUniformMatrix4fv(this->uniformID.um4m, 1, GL_FALSE, value_ptr(this->matrix.model * this->matrix.rotate));
	glUniformMatrix4fv(this->uniformID.um4v, 1, GL_FALSE, value_ptr(m_cameraView));
	glUniformMatrix4fv(this->uniformID.um4p, 1, GL_FALSE, value_ptr(m_cameraProjection));
	glUniformMatrix4fv(this->uniformID.um4Lightmvp, 1, GL_FALSE, value_ptr(scale_bias_matrix * lightProjection * lightView * this->matrix.model * this->matrix.scale * this->matrix.rotate));
	glUniform3fv(this->uniformID.uv3LightPos, 1, value_ptr(vec3(0.2f, 0.6f, 0.5f)));
	// fs
	glUniform1i(this->uniformID.ubPhongFlag, (this->blinnPhongFlag) ? 1 : 0);
	
	for (int i = 0; i < this->shapes.size(); i++) {
		int materialID = this->shapes[i].materialID;
		glUniform3fv(this->uniformID.uv3Ambient, 1, value_ptr(this->materials[materialID].ka));
		glUniform3fv(this->uniformID.uv3Specular, 1, value_ptr(this->materials[materialID].ks));
		glUniform3fv(this->uniformID.uv3Diffuse, 1, value_ptr(this->materials[materialID].kd));
		
		glBindVertexArray(this->shapes[i].vao);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->shapes[i].ibo);

		glUniform1i(this->uniformID.texLoc, 3);
		glActiveTexture(GL_TEXTURE3);
		//glUniform1i(this->uniformID.texLoc_diffuse, 3);
		//glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, this->materials[materialID].diffuse_tex);
		
		glDrawElements(GL_TRIANGLES, this->shapes[i].drawCount, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	glUseProgram(0);
	glBindVertexArray(0);
}
#pragma endregion

#pragma region House
class HouseClass {
public:
	// Constructor / Destructor
	 HouseClass() {};
    ~HouseClass() {};

	// Struct
	typedef struct struct_uniformID {
		// vs
		GLint um4m;
		GLint um4v;
		GLint um4p;
		GLint um4Lightmvp;
		GLint uv3LightPos;
		// fs
		GLint texLoc;
		GLint texLoc_normal;
		GLint uv3Ambient;
		GLint uv3Specular;
		GLint uv3Diffuse;
		GLint ubPhongFlag;
		GLint ubNormalFlag;
	};
	typedef struct struct_matrix {
		mat4 model = mat4(1.0f);
		mat4 scale = mat4(1.0f);
		mat4 rotate = mat4(1.0f);
	};

private:
    GLuint programID;
    struct_uniformID uniformID;
    struct_matrix matrix;
    vector<Shape> shapes;
    vector<Material> materials;
	GLuint normalTexture;

public:
    bool blinnPhongFlag;
	bool normalMappingFlag;

private:
    void loadModel();
    void getUniformLocation();
	void linkProgram();

public:
	void initial(float _rotateAngle, vec3 _position);
    void render();
};

void HouseClass::loadModel() {
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
        if(material->Get(AI_MATKEY_COLOR_AMBIENT, color) == aiReturn_SUCCESS){
			Material.ka = glm::vec4(color.r, color.g, color.b, 1.0f);
		}
		else {
			cout << "no ambient" << endl;
		}
		if(material->Get(AI_MATKEY_COLOR_DIFFUSE, color) == aiReturn_SUCCESS){
			Material.kd = glm::vec4(color.r, color.g, color.b, 1.0f);
		}
		else {
			cout << "no diffuse" << endl;
		}
		if(material->Get(AI_MATKEY_COLOR_SPECULAR, color) == aiReturn_SUCCESS){
			Material.ks = glm::vec4(color.r, color.g, color.b, 1.0f);
		}
		else {
			cout << "no specular" << endl;
		}
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
        this->shapes.push_back(shape);
    
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
	glGenTextures(1, &this->normalTexture);
	glBindTexture(GL_TEXTURE_2D, this->normalTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, normalImg.width, normalImg.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, normalImg.data);
	glGenerateMipmap(GL_TEXTURE_2D);
	//glGenerateMipmap(GL_TEXTURE_2D);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    cout << "Load medievalHouse.obj done" << endl;
}

void HouseClass::getUniformLocation() {
	// vs
	this->uniformID.um4m = glGetUniformLocation(this->programID, "um4m");
	this->uniformID.um4v = glGetUniformLocation(this->programID, "um4v");
	this->uniformID.um4p = glGetUniformLocation(this->programID, "um4p");
	this->uniformID.um4Lightmvp = glGetUniformLocation(this->programID, "um4Lightmvp");
	this->uniformID.uv3LightPos = glGetUniformLocation(this->programID, "uv3LightPos");
	// fs
	this->uniformID.texLoc = glGetUniformLocation(this->programID, "tex");
	this->uniformID.texLoc_normal = glGetUniformLocation(this->programID, "texNormal");
	this->uniformID.ubPhongFlag = glGetUniformLocation(this->programID, "ubPhongFlag");
	this->uniformID.ubNormalFlag = glGetUniformLocation(this->programID, "ubNormalFlag");
	this->uniformID.uv3Ambient = glGetUniformLocation(this->programID, "uv3Ambient");
	this->uniformID.uv3Specular = glGetUniformLocation(this->programID, "uv3Specular");
	this->uniformID.uv3Diffuse = glGetUniformLocation(this->programID, "uv3Diffuse");
}

void HouseClass::linkProgram() {
	// Create Shader Program
	this->programID = glCreateProgram();
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
	// Logging
	shaderLog(vs);
	shaderLog(fs);
	// Assign the program we created before with these shaders
	glAttachShader(this->programID, vs);
	glAttachShader(this->programID, fs);
	glLinkProgram(this->programID);
	// It is not required to hande the shader in memory
   	glDeleteShader(vs);
   	glDeleteShader(fs);
}

void HouseClass::initial(float _rotateAngle, vec3 _position) {
	this->matrix.rotate = rotate(mat4(1.0f), radians(_rotateAngle), vec3(0.0f, 1.0f, 0.0f));
	this->matrix.model = translate(mat4(1.0f), _position);
	
	this->blinnPhongFlag = false;
	this->normalMappingFlag = false;
	
	this->linkProgram();
	this->getUniformLocation();
    this->loadModel();
}

void HouseClass::render() {
	glUseProgram(this->programID);
	// vs
	glUniformMatrix4fv(this->uniformID.um4m, 1, GL_FALSE, value_ptr(this->matrix.model * this->matrix.rotate));
	glUniformMatrix4fv(this->uniformID.um4v, 1, GL_FALSE, value_ptr(m_cameraView));
	glUniformMatrix4fv(this->uniformID.um4p, 1, GL_FALSE, value_ptr(m_cameraProjection));
	glUniformMatrix4fv(this->uniformID.um4Lightmvp, 1, GL_FALSE, value_ptr(scale_bias_matrix * lightProjection * lightView * this->matrix.model * this->matrix.scale * this->matrix.rotate));
	glUniform3fv(this->uniformID.uv3LightPos, 1, value_ptr(vec3(0.2f, 0.6f, 0.5f)));
	// fs
	glUniform1i(this->uniformID.ubPhongFlag, (this->blinnPhongFlag) ? 1 : 0);
	glUniform1i(this->uniformID.ubNormalFlag, (this->normalMappingFlag) ? 1 : 0);

	for (int i = 0; i < this->shapes.size(); i++) {
		int materialID = this->shapes[i].materialID;
		glUniform3fv(this->uniformID.uv3Ambient, 1, value_ptr(this->materials[materialID].ka));
		glUniform3fv(this->uniformID.uv3Specular, 1, value_ptr(this->materials[materialID].ks));
		glUniform3fv(this->uniformID.uv3Diffuse, 1, value_ptr(this->materials[materialID].kd));
		
		glBindVertexArray(this->shapes[i].vao);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->shapes[i].ibo);
		
		glUniform1i(this->uniformID.texLoc, 3);
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, this->materials[materialID].diffuse_tex);
		
		glUniform1i(this->uniformID.texLoc_normal, 4);
		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_2D, this->normalTexture);
		
		glDrawElements(GL_TRIANGLES, this->shapes[i].drawCount, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	

	glUseProgram(0);
	glBindVertexArray(0);
}

#pragma endregion

#pragma region G-buffer
class GBufferClass {
public :
	// Constructor / Destructor
	GBufferClass() {};
	~GBufferClass() {};

	GLuint gbuffer;
	GLuint tex_vertex;
	GLuint tex_normal;
	GLuint tex_ambient;
	GLuint tex_diffuse;
	GLuint tex_specular;

	void setupGBuffer();
	void GBufferRender();
	void GBufferRenderEnd();
	
};

void GBufferClass::setupGBuffer() {
	glGenFramebuffers(1, &this->gbuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, this->gbuffer);

	// Diffuse
	glGenTextures(1, &this->tex_diffuse);
	glBindTexture(GL_TEXTURE_2D, this->tex_diffuse);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, FRAME_WIDTH, FRAME_HEIGHT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// Ambient
	glGenTextures(1, &this->tex_ambient);
	glBindTexture(GL_TEXTURE_2D, this->tex_ambient);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, FRAME_WIDTH, FRAME_HEIGHT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// Specular
	glGenTextures(1, &this->tex_specular);
	glBindTexture(GL_TEXTURE_2D, this->tex_specular);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, FRAME_WIDTH, FRAME_HEIGHT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// Vertex
	glGenTextures(1, &this->tex_vertex);
	glBindTexture(GL_TEXTURE_2D, this->tex_vertex);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, FRAME_WIDTH, FRAME_HEIGHT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// Normal
	glGenTextures(1, &this->tex_normal);
	glBindTexture(GL_TEXTURE_2D, this->tex_normal);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, FRAME_WIDTH, FRAME_HEIGHT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, this->tex_diffuse, 0);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, this->tex_ambient, 0);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, this->tex_specular, 0);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, this->tex_vertex, 0);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, this->tex_normal, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GBufferClass::GBufferRender() {
	glBindFramebuffer(GL_FRAMEBUFFER, this->gbuffer);
	const GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4 };
	glDrawBuffers(3, drawBuffers);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	static const GLfloat blue[] = { 0.0f, 0.0f, 0.25f, 1.0f };
	static const GLfloat one = 1.0f;

	glClearBufferfv(GL_COLOR, 0, blue);
	glClearBufferfv(GL_DEPTH, 0, &one);
}

void GBufferClass::GBufferRenderEnd() {
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
#pragma endregion


#pragma region FrameBuffer
class FrameBufferClass {
public: 
	// Constructor / Destructor
	 FrameBufferClass() {};
    ~FrameBufferClass() {};

	GLuint FBO;
	GLuint depthRBO;
	GLuint FBODataTexture;

	void framebufferRender();
	void framebufferRenderEnd();
	void setBuffer();
	void initial();
};

void FrameBufferClass::setBuffer() { // Need to be called in Reshape
	glDeleteRenderbuffers(1, &this->depthRBO);
	glDeleteTextures(1, &this->FBODataTexture);
	// glDeleteFramebuffers(1, &this->FBO);

	// Render Buffer
	glGenRenderbuffers(1, &this->depthRBO);
	glBindRenderbuffer(GL_RENDERBUFFER, this->depthRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32, FRAME_WIDTH, FRAME_HEIGHT);

	// FBO Data Texture
	glGenTextures(1, &this->FBODataTexture);
	glBindTexture(GL_TEXTURE_2D, this->FBODataTexture);
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, FRAME_WIDTH, FRAME_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

	// Frame Buffer
	//glGenFramebuffers(1, &this->FBO);
	glBindFramebuffer( GL_DRAW_FRAMEBUFFER, this->FBO );
	glFramebufferRenderbuffer( GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, this->depthRBO );
	glFramebufferTexture2D( GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, this->FBODataTexture, 0 );
}

void FrameBufferClass::framebufferRender() {
	glBindTexture(GL_TEXTURE_2D, this->FBODataTexture);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, this->FBO);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	static const GLfloat green[] = { 0.0f, 0.25f, 0.0f, 1.0f };
	static const GLfloat one = 1.0f;

	glClearBufferfv(GL_COLOR, 0, green);
	glClearBufferfv(GL_DEPTH, 0, &one);
}

void FrameBufferClass::framebufferRenderEnd() {
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

void FrameBufferClass::initial() {
	glGenFramebuffers(1, &this->FBO);
	this->setBuffer();
}
#pragma endregion

#pragma region Window Frame
class WindowFrameClass {
public: 
	// Constructor / Destructor
	 WindowFrameClass() {};
    ~WindowFrameClass() {};

private:
    GLuint programID;
	GLuint texLoc;
	GLuint vao;
	GLuint vbo;

	float window_positions[16] = {
		 1.0f, -1.0f,  1.0f,  0.0f,
		-1.0f, -1.0f,  0.0f,  0.0f,
		-1.0f,  1.0f,  0.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,  1.0f
	};

	void linkProgram();
	void setBuffer();

public:
	void initial();
	void render(GLuint texture);
};

void WindowFrameClass::linkProgram() {
	// Create Shader Program
	this->programID = glCreateProgram();
	// Create customize shader by tell openGL specify shader type 
	GLuint vs = glCreateShader(GL_VERTEX_SHADER);
	GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
	// Load shader file
	char** vs_source = loadShaderSource(".\\assets\\windowFrame_vertex.vs.glsl");
	char** fs_source = loadShaderSource(".\\assets\\windowFrame_fragment.fs.glsl");
	// Assign content of these shader files to those shaders we created before
	glShaderSource(vs, 1, vs_source, NULL);
	glShaderSource(fs, 1, fs_source, NULL);
	
	// Compile these shaders
	glCompileShader(vs);
	glCompileShader(fs);
	// Logging
	shaderLog(vs);
	shaderLog(fs);
	// Assign the program we created before with these shaders
	glAttachShader(this->programID, vs);
	glAttachShader(this->programID, fs);
	glLinkProgram(this->programID);
	// It is not required to hande the shader in memory
   	glDeleteShader(vs);
   	glDeleteShader(fs);

	// Free the shader file string(won't be used any more)
	freeShaderSource(vs_source);
	freeShaderSource(fs_source);

	this->texLoc = glGetUniformLocation(this->programID, "tex");
}

void WindowFrameClass::setBuffer() {
	glGenVertexArrays(1, &this->vao);
	glBindVertexArray(this->vao);

	glGenBuffers(1, &this->vbo);
	glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(this->window_positions), this->window_positions, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4, 0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4, (const GLvoid*)(sizeof(GLfloat) * 2));
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void WindowFrameClass::initial() {
	this->linkProgram();
	cout << this->programID << endl;
	this->setBuffer();
}

void WindowFrameClass::render(GLuint texture) {
	glClearColor( 1.0f, 0.0f, 0.0f, 1.0f );
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	glUniform1i(this->texLoc, 3);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, texture);
	glBindVertexArray(this->vao);
	glUseProgram(this->programID);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	glUseProgram(0);
}

#pragma endregion


AirplaneClass m_airplane;
HouseClass m_houseA;
HouseClass m_houseB;
FrameBufferClass m_frameBuffer_test;
WindowFrameClass m_window;


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
	m_airplane.initial(m_airplaneRotMat, m_airplanePosition);
	m_houseA.initial(60.0f, vec3(631.0f, 130.0f, 468.0f));
	m_houseB.initial(15.0f, vec3(656.0f, 135.0f, 483.0f));
	m_window.initial();
	m_frameBuffer_test.initial();

	m_cameraProjection = perspective(glm::radians(60.0f), FRAME_WIDTH * 1.0f / FRAME_HEIGHT, 0.1f, 1000.0f);
	m_renderer->setProjection(m_cameraProjection);
}
void resizeGL(GLFWwindow *window, int w, int h) {
	FRAME_WIDTH = w;
	FRAME_HEIGHT = h;
	m_frameBuffer_test.setBuffer();
	m_renderer->resize(w, h);
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

bool m_terrainOn = false;

void paintGL() {
	// render terrain
	// m_renderer->renderPass();

	// [TODO] implement your rendering function here
	// glViewport(0, 0, FRAME_WIDTH, FRAME_HEIGHT);

	m_frameBuffer_test.framebufferRender();

	//if (m_terrainOn) {
	//	m_renderer->renderPass();
	//}
	//m_renderer->renderPass();
	m_airplane.render();
	m_houseA.render();
	m_houseB.render();

	m_frameBuffer_test.framebufferRenderEnd();
	m_window.render(m_frameBuffer_test.FBODataTexture);

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
			m_airplane.blinnPhongFlag = !m_airplane.blinnPhongFlag;
			m_houseA.blinnPhongFlag = !m_houseA.blinnPhongFlag;
			m_houseB.blinnPhongFlag = !m_houseB.blinnPhongFlag;
			cout << "Phong Shading: " << m_airplane.blinnPhongFlag << endl;
			break;
		case GLFW_KEY_X:
			m_houseA.normalMappingFlag = !m_houseA.normalMappingFlag;
			m_houseB.normalMappingFlag = !m_houseB.normalMappingFlag;
			cout << "Normal Mapping: " << m_houseA.normalMappingFlag << endl;
			break;
		case GLFW_KEY_C:
			m_terrainOn = !m_terrainOn;
			cout << "Draw Terrain: " << m_terrainOn << endl;
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
#include "src\basic\SceneRenderer.h"
#include <GLFW\glfw3.h>
#include "SceneSetting.h"

#include "assimp/scene.h"
#include "assimp/cimport.h"
#include "assimp/postprocess.h"
#include "../externals/include/stb_image.h"

#pragma comment (lib, "lib-vc2017\\glfw3.lib")
#pragma comment (lib, "assimp\\assimp-vc141-mt.lib")

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

vector<Shape> shapes;
vector<Material> materials;

#pragma region Airplane
void airplaneModel_load() {
	char* filepath = "..\\models\\airplane.obj";//..\\Assets\\sponza.obj
	const aiScene* scene = aiImportFile(filepath, aiProcessPreset_TargetRealtime_MaxQuality);

	for (unsigned int i = 0; i < scene->mNumMaterials; ++i)
	{

		aiMaterial* material = scene->mMaterials[i];
		Material Material;
		aiString texturePath;
		aiColor3D color;
		if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) ==
			aiReturn_SUCCESS)
		{
			// load width, height and data from texturePath.C_Str();
			string p = "..\\models\\"; /*..\\Assets\\*/
			texturePath = p + texturePath.C_Str();
			cout << texturePath.C_Str() << endl;
			TextureData tex = loadImg(texturePath.C_Str());
			glGenTextures(1, &Material.diffuse_tex);
			glBindTexture(GL_TEXTURE_2D, Material.diffuse_tex);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, tex.width, tex.height, 0,
				GL_RGBA, GL_UNSIGNED_BYTE, tex.data);
			glGenerateMipmap(GL_TEXTURE_2D);
		}
		material->Get(AI_MATKEY_COLOR_AMBIENT, color);
		Material.ka = glm::vec4(color.r, color.g, color.b, 1.0f);
		material->Get(AI_MATKEY_COLOR_DIFFUSE, color);
		Material.kd = glm::vec4(color.r, color.g, color.b, 1.0f);
		material->Get(AI_MATKEY_COLOR_SPECULAR, color);
		Material.ks = glm::vec4(color.r, color.g, color.b, 1.0f);
		// save material�K
		materials.push_back(Material);
	}
	for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
	{
		aiMesh* mesh = scene->mMeshes[i];
		Shape shape;
		glGenVertexArrays(1, &shape.vao);
		glBindVertexArray(shape.vao);
		// create 3 vbos to hold data
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
		// create 1 ibo to hold data
		vector<unsigned int> face;
		for (unsigned int f = 0; f < mesh->mNumFaces; ++f)
		{
			// mesh->mFaces[f].mIndices[0~2] => index
			face.push_back(mesh->mFaces[f].mIndices[0]);
			face.push_back(mesh->mFaces[f].mIndices[1]);
			face.push_back(mesh->mFaces[f].mIndices[2]);
		}
		// glVertexAttribPointer / glEnableVertexArray calls�K

		glGenBuffers(1, &shape.vbo_position);
		glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_position);
		glBufferData(GL_ARRAY_BUFFER, mesh->mNumVertices * 3 * sizeof(float), &position[0], GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(0);

		glGenBuffers(1, &shape.vbo_texcoord);
		glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_texcoord);
		glBufferData(GL_ARRAY_BUFFER, mesh->mNumVertices * 2 * sizeof(float), &texcoord[0], GL_STATIC_DRAW);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(1);

		glGenBuffers(1, &shape.vbo_normal);
		glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_normal);
		glBufferData(GL_ARRAY_BUFFER, mesh->mNumVertices * 3 * sizeof(float), &normal[0], GL_STATIC_DRAW);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(2);

		glGenBuffers(1, &shape.ibo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, shape.ibo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->mNumFaces * 3 * sizeof(unsigned int), &face[0], GL_STATIC_DRAW);


		shape.materialID = mesh->mMaterialIndex;
		shape.drawCount = mesh->mNumFaces * 3;
		// save shape
		shapes.push_back(shape);

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
}

void airplaneModel_program() {

}
#pragma endregion





int main(){
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	

	GLFWwindow *window = glfwCreateWindow(FRAME_WIDTH, FRAME_HEIGHT, "rendering", nullptr, nullptr);
	if (window == nullptr){
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
void vsyncDisabled(GLFWwindow *window){
	float periodForEvent = 1.0 / 60.0;
	float accumulatedEventPeriod = 0.0;
	double previousTime = glfwGetTime();
	double previousTimeForFPS = glfwGetTime();
	int frameCount = 0;
	while (!glfwWindowShouldClose(window)){
		// measure speed
		double currentTime = glfwGetTime();
		float deltaTime = currentTime - previousTime;
		frameCount = frameCount + 1;
		if (currentTime - previousTimeForFPS >= 1.0){
			std::cout << "\rFPS: " << frameCount;
			frameCount = 0;
			previousTimeForFPS = currentTime;
		}
		previousTime = currentTime;

		// game loop
		accumulatedEventPeriod = accumulatedEventPeriod + deltaTime;
		if (accumulatedEventPeriod > periodForEvent){
			updateState();
			accumulatedEventPeriod = accumulatedEventPeriod - periodForEvent;
		}

		paintGL();
		glfwSwapBuffers(window);
		glfwPollEvents();
	}
}
void vsyncEnabled(GLFWwindow *window){
	double previousTimeForFPS = glfwGetTime();
	int frameCount = 0;
	while (!glfwWindowShouldClose(window)){
		// measure speed
		double currentTime = glfwGetTime();
		frameCount = frameCount + 1;
		if (currentTime - previousTimeForFPS >= 1.0){
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

void initializeGL(){
	m_renderer = new SceneRenderer();
	m_renderer->initialize(FRAME_WIDTH, FRAME_HEIGHT);	

	m_eye = glm::vec3(512.0, 10.0, 512.0);
	m_lookAtCenter = glm::vec3(512.0, 0.0, 500.0);
	
	initScene();

	m_renderer->setProjection(glm::perspective(glm::radians(60.0f), FRAME_WIDTH * 1.0f / FRAME_HEIGHT, 0.1f, 1000.0f));
}
void resizeGL(GLFWwindow *window, int w, int h){
	FRAME_WIDTH = w;
	FRAME_HEIGHT = h;
	m_renderer->resize(w, h);
	m_renderer->setProjection(glm::perspective(glm::radians(60.0f), w * 1.0f / h, 0.1f, 1000.0f));
}


void updateState(){	
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
void paintGL(){
	// render terrain
	m_renderer->renderPass();

	// [TODO] implement your rendering function here
}

////////////////////////////////////////////////
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods){}
void mouseScrollCallback(GLFWwindow *window, double xoffset, double yoffset) {}
void cursorPosCallback(GLFWwindow* window, double x, double y){
	cursorPos[0] = x;
	cursorPos[1] = y;
}
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods){}
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
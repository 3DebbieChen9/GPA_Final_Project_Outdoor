#include "Terrain.h"



glm::vec3 Terrain::worldVToHeightMapUV(float x, float z) const {
	glm::vec4 uv = this->m_worldToHeightUVMat * glm::vec4(x, 0, z, 1.0);
	for (int i = 0; i < 3; i += 2) {
		float n = uv[i];
		int z = floor(n);
		float f = n - floor(n);
		uv[i] = f;
	}

	return glm::vec3(uv.x, 0.0, uv.z);
}
float Terrain::getHeight(const float x, const float z) const {
	glm::vec3 uv = this->worldVToHeightMapUV(x, z);

	float fx = uv.x * (this->m_width - 1);
	float fz = uv.z * (this->m_height - 1);

	int corners[] = {
		(int)floor(fx),
		(int)floor(fx) + 1,
		(int)floor(fz),
		(int)floor(fz) + 1,
	};

	int cornerIdxes[] = {
		0, 2,
		0, 3,
		1, 2,
		1, 3
	};
	float h[4];

	for (int i = 0; i < 4; i++) {
		int mx = corners[cornerIdxes[i * 2 + 0]];
		int mz = corners[cornerIdxes[i * 2 + 1]];
		h[i] = this->m_elevationMap[mz * this->m_width + mx] * this->m_heightScale;
	}

	float ch = h[0] * (fx - corners[0]) * (fz - corners[2]) +
		h[1] * (fx - corners[0]) * (corners[3] - fz) +
		h[2] * (corners[1] - fx) * (fz - corners[2]) +
		h[3] * (corners[1] - fx) * (corners[3] - fz);

	return ch;
}


Terrain::~Terrain()
{

}

void Terrain::updateShadowMap(GLuint programID) {
	// Bind Buffer
	glBindVertexArray(m_vao);	
	for (int i = 0; i < 4; i++) {		
		glUniformMatrix4fv(glGetUniformLocation(programID, "um4m"), 1, false, glm::value_ptr(this->m_chunkRotMat[i]));
		int indicesPointer = this->m_elevationTex->m_vertexStart * 4;
		glDrawElements(GL_TRIANGLES, this->m_elevationTex->m_vertexCount, GL_UNSIGNED_INT, (GLvoid*)(indicesPointer));
	}	
}

void Terrain::update() {
	const SceneManager *manager = SceneManager::Instance();

	// Bind Buffer
	glBindVertexArray(m_vao);	
	glUniformMatrix4fv(manager->m_vToHeightUVMatHandle, 1, false, glm::value_ptr(this->m_worldToHeightUVMat));

	this->m_elevationTex->bind();
	this->m_normalTex->bind(); 
	this->m_colorTex->bind();

	for (int i = 0; i < 4; i++) {		
		glUniformMatrix4fv(manager->m_modelMatHandle, 1, false, glm::value_ptr(this->m_chunkRotMat[i]));

		int indicesPointer = this->m_elevationTex->m_vertexStart * 4;
		glDrawElements(this->m_elevationTex->m_mode, this->m_elevationTex->m_vertexCount, GL_UNSIGNED_INT, (GLvoid*)(indicesPointer));
	}	
}
void Terrain::setCameraPosition(const glm::vec3 &pos) {
	for (int i = 0; i < 4; i++) {
		this->m_chunkRotMat[i][3] = glm::vec4(pos, 1.0);
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
Terrain::Terrain() {
	this->m_heightScale = 300.0;
	m_transform = new Transformation();
	const SceneManager *manager = SceneManager::Instance();

	std::ifstream input("assets\\terrain.chunkdata", std::ios::binary); 
	int NUM_VERTEX = -1;
	input.read((char*)(&NUM_VERTEX), sizeof(int));
	if (NUM_VERTEX <= 0) {
		input.close();
		return;
	}
	float *vertices = new float[NUM_VERTEX * 3];
	input.read((char*)vertices, sizeof(float) * NUM_VERTEX * 3);
	int NUM_INDEX = -1;
	input.read((char*)(&NUM_INDEX), sizeof(int));
	if(NUM_INDEX <= 0) {
		input.close();
		return;
	}
	unsigned int *indices = new unsigned int[NUM_INDEX];	
	input.read((char*)indices, sizeof(unsigned int) * NUM_INDEX);



	// create buffer
	// Create Geometry Data Buffer
	glCreateBuffers(1, &(m_dataBuffer));
	glNamedBufferData(m_dataBuffer, NUM_VERTEX * 12, vertices, GL_STATIC_DRAW);

	// Create Indices Buffer
	glCreateBuffers(1, &m_indexBuffer);
	glNamedBufferData(m_indexBuffer, NUM_INDEX * 4, indices, GL_STATIC_DRAW);

	// create VAO
	glGenVertexArrays(1, &(m_vao));
	glBindVertexArray(m_vao);
	glBindBuffer(GL_ARRAY_BUFFER, m_dataBuffer);
	glVertexAttribPointer(manager->m_vertexHandle, 3, GL_FLOAT, false, 0, nullptr);
	// The normal and uv is generated by shader
	glVertexAttribPointer(manager->m_normalHandle, 3, GL_FLOAT, false, 0, nullptr);
	glVertexAttribPointer(manager->m_uvHandle, 3, GL_FLOAT, false, 0, nullptr);
	glEnableVertexAttribArray(manager->m_vertexHandle);	
	glEnableVertexAttribArray(manager->m_normalHandle);	
	glEnableVertexAttribArray(manager->m_uvHandle);	
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);
	glBindVertexArray(0);

	// create material
	this->fromMYTD("assets\\elevationMap_0.mytd");
	
	this->m_elevationTex = new TextureMaterial(this->m_elevationMap, 1, this->m_width, this->m_height, GL_R16, GL_RED, GL_FLOAT);
	this->m_elevationTex->setTextureUnit(manager->m_elevationMapTexUnit);

	this->m_normalTex = new TextureMaterial(this->m_normalMap, 3, this->m_width, this->m_height, GL_RGBA32F, GL_RGB, GL_FLOAT);
	this->m_normalTex->setTextureUnit(manager->m_normalMapTexUnit);

	this->m_colorTex = new TextureMaterial(this->m_colorMap, 3, this->m_width, this->m_height, GL_RGBA32F, GL_RGB, GL_FLOAT);
	this->m_colorTex->setTextureUnit(manager->m_diffuseTexUnit);	

	this->m_worldToHeightUVMat = glm::scale(glm::vec3(1.0 / this->m_width, 1.0, 1.0 / this->m_height));
	this->m_worldToHeightUVMat[3] = glm::vec4(0.0, 0.0, 0.0, 1.0);

	this->m_elevationTex->m_vertexStart = 0;
	this->m_elevationTex->m_vertexCount = NUM_INDEX;

	for (int i = 0; i < 4; i++) {
		glm::quat quaternion = glm::quat(glm::radians(glm::vec3(0.0, 90.0 * i, 0.0)));
		this->m_chunkRotMat[i] = glm::toMat4(quaternion);
	}

	delete[] vertices;
	delete[] indices;
}
void Terrain::fromMYTD(const std::string &filename) {
	std::ifstream input(filename, std::ios::binary);
	int sizeInfo[] = { 0, 0 };
	input.read((char*)sizeInfo, sizeof(int) * 2);

	this->m_elevationMap = new float[sizeInfo[0] * sizeInfo[1] * 1];
	input.read((char*)(this->m_elevationMap), sizeof(float) * (sizeInfo[1] * sizeInfo[0]));
	
	this->m_normalMap = new float[sizeInfo[0] * sizeInfo[1] * 3];
	input.read((char*)(this->m_normalMap), sizeof(float) * (sizeInfo[1] * sizeInfo[0]) * 3);

	this->m_colorMap = new float[sizeInfo[0] * sizeInfo[1] * 3];
	input.read((char*)(this->m_colorMap), sizeof(float) * (sizeInfo[1] * sizeInfo[0]) * 3);
	
	input.close();

	this->m_width = sizeInfo[0];
	this->m_height = sizeInfo[1];
}
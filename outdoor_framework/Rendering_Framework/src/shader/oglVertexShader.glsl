#version 430 core

in vec3 v_vertex;
in vec3 v_normal ;
in vec3 v_uv ;

out vec3 f_viewVertex ;
out vec3 f_uv;

uniform mat4 projMat ;
uniform mat4 viewMat ;
uniform mat4 modelMat ;

// terrain
uniform mat4 vToHeightUVMat;
uniform sampler2D elevationMap;
uniform sampler2D normalMap;

vec3 getTerrainVertex(vec4 worldV){
	float mHeight = 300.0 ;
	vec4 uvForHeight = vToHeightUVMat * worldV;
	float h = texture(elevationMap, vec2(uvForHeight.x, uvForHeight.z)).r ;
	float y =  h * mHeight ;	
	return vec3(worldV.x, y, worldV.z) ;		
}

void renderTerrain(){	
	vec4 worldV = modelMat * vec4(v_vertex, 1.0) ;
	worldV.w = 1.0;
	vec3 cVertex = getTerrainVertex(worldV) ;	
	
	// get normal
	vec4 uvForNormal = vToHeightUVMat * worldV;
	vec4 normalTex = texture(normalMap, vec2(uvForNormal.x, uvForNormal.z)) ;
	// [0, 1] -> [-1, 1]
	normalTex = normalTex * 2.0 - 1.0 ;
		
	vec4 viewVertex = viewMat * vec4(cVertex, 1) ;
	vec4 viewNormal = viewMat * vec4(normalTex.rgb, 0) ;
	
	f_viewVertex = viewVertex.xyz ;
	
	vec4 uvForDiffuse = vToHeightUVMat * worldV;
	f_uv = vec3(uvForDiffuse.x, uvForDiffuse.z, 1.0) ;
	
	gl_Position = projMat * viewVertex ;
}

void main(){	
	renderTerrain() ;	
}
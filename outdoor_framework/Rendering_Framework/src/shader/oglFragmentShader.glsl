#version 430 core

in vec3 f_viewVertex ;
in vec3 f_uv ;
in vec3 f_wsPosition;
in vec4 f_wsNormal;

layout (location = 0) out vec4 diffuse_color;
layout (location = 1) out vec4 ambient_color;
layout (location = 2) out vec4 specular_color;
layout (location = 3) out vec4 ws_position;
layout (location = 4) out vec4 ws_normal;

uniform sampler2D texture0 ;

vec4 withFog(vec4 color){
	const vec4 FOG_COLOR = vec4(1.0, 1.0, 1.0, 1) ;
	const float MAX_DIST = 1000.0 ;
	const float MIN_DIST = 600.0 ;
	
	float dis = length(f_viewVertex) ;
	float fogFactor = (MAX_DIST - dis) / (MAX_DIST - MIN_DIST) ;
	fogFactor = clamp(fogFactor, 0.0, 1.0) ;
	fogFactor = fogFactor * fogFactor ;
	
	vec4 colorWithFog = mix(FOG_COLOR, color, fogFactor) ;
	return colorWithFog ;
}

void renderTerrain(){
	// get terrain color
	vec4 terrainColor = texture(texture0, f_uv.rg) ;				
	// apply fog
	// vec4 fColor = withFog(terrainColor) ;
	
	vec4 fColor = terrainColor;
	fColor.a = 1.0 ;
	diffuse_color = fColor ;
	ambient_color = fColor;
	specular_color = vec4(0.0f);
	ws_position = vec4(f_wsPosition, 1.0f);
	ws_normal = f_wsNormal;
}

void main(){	
	renderTerrain() ;
}
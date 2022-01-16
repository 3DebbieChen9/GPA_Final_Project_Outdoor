#version 430 core

layout(location = 0) out vec4 phongColor; // Phong-Shading Result
layout(location = 1) out vec4 bloomHDR;
layout(location = 2) out vec4 phongShadow;

in VS_OUT
{
	vec2 texcoord;
} fs_in;

uniform vec3 uv3LightPos;
uniform vec3 uv3eyePos;
uniform mat4 um4LightVP;

uniform sampler2D tex_diffuse;
uniform sampler2D tex_ambient;
uniform sampler2D tex_specular;
uniform sampler2D tex_ws_position;
uniform sampler2D tex_ws_normal;
uniform sampler2D tex_ws_tangent;
uniform sampler2D tex_dirDepth;

float CalcDirShadow(vec3 normal){
    float shadow = 0.0;

    // transform frag to light space
    // vec4 fragInLight = light_vp * vec4(vertexData.FragPos, 1.0);
    vec3 fragPos = texelFetch(tex_ws_position, ivec2(gl_FragCoord.xy), 0).xyz;
	vec4 fragInLight = um4LightVP * vec4(fragPos, 1.0);

    // normalize to -1~1
    vec3 proj_coord = fragInLight.xyz / fragInLight.w;
    proj_coord = proj_coord * 0.5 + 0.5;

    float closestDepth = texture(tex_dirDepth, proj_coord.xy).r;  // depth from light's view
    float currentDepth = proj_coord.z;  // depth of current fragment
    
    // acne shadow
	int dir_flag = 1;
    if(dir_flag == 1){
        shadow = currentDepth > closestDepth ? 1.0 : 0.0;
        return shadow;
    }

    // adaptive bias
    // vec3 normal = normalize(vertexData.N);
    // vec3 normal = ??????
    vec3 light_dir = normalize(uv3LightPos - fragPos);
    // float bias = max(0.05 * (1.0 - dot(normal, light_dir)), 0.005);
    float bias = 0.0005;

    // PCF
    vec2 tex_size = 1.0 / textureSize(tex_dirDepth ,0);
    for(int x =-1; x <= 1; x++){
        for(int y=-1; y <=1; y++){
            float pcf_depth = texture(tex_dirDepth, proj_coord.xy + vec2(x, y) * tex_size).r;
            shadow += currentDepth - bias > pcf_depth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;

    if(proj_coord.z > 1.0)  // outside far plane
        shadow = 0.0;

    return shadow;
}

vec3 phongShading() {

	// Eye space to tangent space TBN
	// -1 ~ 1
	vec3 vs_N = normalize(texelFetch(tex_ws_normal, ivec2(gl_FragCoord.xy), 0).xyz * 2.0 - vec3(1.0)); // out
	// 	vec3 vs_N = normalize(texelFetch(tex_ws_normal, ivec2(gl_FragCoord.xy), 0).xyz);

	// 	vec3 vs_T = normalize(texelFetch(tex_ws_tangent, ivec2(gl_FragCoord.xy), 0).xyz);
	// 	vec3 vs_B = cross(vs_N, vs_T);

	// 	// TBN * normal
	// 	mat3 TBN = mat3(vs_T, vs_B, vs_N);
	// 	vs_N = vs_N * 2.0 - 1.0;
	// 	vs_N = normalize(TBN * vs_N);

	vec4 vs_P = vec4(texelFetch(tex_ws_position, ivec2(gl_FragCoord.xy), 0).xyz, 1.0);
	
	// N =TBN * normal
	// light * N

	// Parallel Light
	vec3 vs_L = 1000.0f * normalize(vec3(0.2f, 0.6f, 0.5f)); 
	//vec3 vs_L = uv3LightPos; //- vs_P.xyz;
	vec3 lightDir = vs_L;
	// vec3 lightDir = normalize(vec3(dot(vs_L, vs_T), dot(vs_L, vs_B), dot(vs_L, vs_N))); /// out
	vec3 vs_V = uv3eyePos - vs_P.xyz;
	vec3 eyeDir = vs_V;
	// vec3 eyeDir = normalize(vec3(dot(vs_V, vs_T), dot(vs_V, vs_B), dot(vs_V, vs_N))); /// out
	
	vec3 fs_V = normalize(eyeDir);
	vec3 fs_L = normalize(lightDir);
	vec3 fs_N = normalize(vs_N);

	vec3 fs_R = reflect(-fs_L, fs_N);

	vec3 diffuse_albedo = vec3(2.0) * texture(tex_diffuse, fs_in.texcoord).rgb;
	vec3 diffuse = max(dot(fs_N, fs_L), 0.0) * diffuse_albedo;

	vec3 specular_albedo = texture(tex_specular, fs_in.texcoord).rgb;
	vec3 specular = max(pow(dot(fs_R, fs_V), 64.0), 0.0) * specular_albedo;

	vec3 ambient = texture(tex_ambient, fs_in.texcoord).rgb;

	vec3 color = ambient * vec3(0.1) + diffuse * vec3(0.8) + specular * vec3(0.1);

	// Shadow
	float shadow = CalcDirShadow(fs_N);
	// phongShadow = vec4((vec3(0.1) * ambient + (1.0 - shadow)) *  (vec3(0.8) * diffuse + vec3(0.1) * specular), 1.0f);

	vec3 org_diffuse = texture(tex_diffuse, fs_in.texcoord).rgb;
	vec3 org_ambient = texture(tex_ambient, fs_in.texcoord).rgb;
	vec3 org_specular = texture(tex_specular, fs_in.texcoord).rgb;
	phongShadow = vec4((vec3(0.1) * org_ambient + (1.0 - shadow)) *  (vec3(0.8) * org_diffuse + vec3(0.1) * org_specular), 1.0f);



	// Point Light
	vec3 pointLightPos = vec3(636.48, 134.79, 495.98);
	vec3 point_L = pointLightPos - vs_P.xyz;
	// lightDir = normalize(vec3(dot(point_L, vs_T), dot(point_L, vs_B), dot(point_L, vs_N)));
	lightDir = point_L;
	fs_L = normalize(lightDir);
	fs_R = reflect(-fs_L, fs_N);

	diffuse = max(dot(fs_N, fs_L), 0.0) * diffuse_albedo;
	specular = max(pow(dot(fs_R, fs_V), 900.0), 0.0) * specular_albedo;

	float dist = distance(pointLightPos, vs_P.xyz);
	
	// Bloom Sphere
	if(dist <= 2.0f) {
		color = vec3(1.0f) * vec3(0.7f) + diffuse * vec3(0.3);
		phongShadow = vec4(vec3(1.0f) * vec3(0.7f) + diffuse * vec3(0.3), 1.0f);
	}
	float attenuation = 50.0f / (pow(dist, 2) + 0.5);
	// float attenuation = 1.0f /  (pow(dist, 2) + 1.0);

	color += ((diffuse * vec3(0.8) + specular * vec3(0.2)) * attenuation);

	// phongColor = vec4(color, 1.0);
	return color;
}

void genBloomHDR(vec3 _color) {
	
	float brightness = dot(_color, vec3(0.8));
	float dist = distance(texelFetch(tex_ws_position, ivec2(gl_FragCoord.xy), 0).xyz, vec3(636.48, 134.79, 495.98));
	bloomHDR = vec4(0.0f);
	if (brightness > 1 && dist <= 3.0f) {
		bloomHDR = vec4(_color, 1.0);
	}
	else {
		bloomHDR = vec4(vec3(0.0f), 1.0);
	}

}

void main()
{
	vec3 deferredColor = phongShading();
	genBloomHDR(deferredColor);
	phongColor = vec4(deferredColor, 1.0f);
}
out vec4 FragColor;

in VertexData
{
    vec3 N; // model space normal
    vec2 texcoord;
    vec3 FragPos;

    vec3 ambient_color;
    vec3 specular_color;
    vec3 diffuse_color; // diffuse color from mtl

	vec3 tangent;
	mat3 TBN;
} vertexData;

in vec4 FragPosLightSpace;


uniform bool tex_flag;
layout (binding = 0) uniform sampler2D diffuseTexture;
uniform bool normal_flag;
layout (binding = 1) uniform sampler2D normMap;


// shadow
layout (binding = 2) uniform sampler2D shadow_map;
uniform mat4 light_vp;
uniform vec3 light_pos;
uniform bool shadow_flag;
uniform float near_plane, far_plane;

// event
uniform int dir_flag;

// phong shading
uniform vec3 view_pos;

vec3 compute_normal(){ // handle normal mapping info
    vec3 normal;

    if(!normal_flag){
        normal = normalize(vertexData.N);
    }
    else if(normal_flag){   // normal mapping
        // load normal from normal map
        normal = texture(normMap, vertexData.texcoord).rgb;   // read from normal map

        // 0 ~ 1 -> -1 ~ 1
        normal = normal * 2.0 - 1.0; 

        // compute tangent space normal
        normal = normalize(vertexData.TBN * normal);
    }
    
    return normal;
}

float CalcDirShadow(){
    float shadow = 0.0;

    // transform frag to light space
    // vec4 fragInLight = light_vp * vec4(vertexData.FragPos, 1.0);
    vec4 fragInLight = FragPosLightSpace;

    // normalize to -1~1
    vec3 proj_coord = fragInLight.xyz / fragInLight.w;
    proj_coord = proj_coord * 0.5 + 0.5;

    float closestDepth = texture(shadow_map, proj_coord.xy).r;  // depth from light's view
    float currentDepth = proj_coord.z;  // depth of current fragment
    
    // acne shadow
    if(dir_flag == 1){
        shadow = currentDepth > closestDepth ? 1.0 : 0.0;
        return shadow;
    }

    // adaptive bias
    // vec3 normal = normalize(vertexData.N);
    vec3 normal = compute_normal();
    vec3 light_dir = normalize(light_pos - vertexData.FragPos);
    // float bias = max(0.05 * (1.0 - dot(normal, light_dir)), 0.005);
    float bias = 0.0005;

    // PCF
    vec2 tex_size = 1.0 / textureSize(shadow_map ,0);
    for(int x =-1; x <= 1; x++){
        for(int y=-1; y <=1; y++){
            float pcf_depth = texture(shadow_map, proj_coord.xy + vec2(x, y) * tex_size).r;
            shadow += currentDepth - bias > pcf_depth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;

    if(proj_coord.z > 1.0)  // outside far plane
        shadow = 0.0;

    // if(dir_flag == 2){ // vis depth
    //     FragColor = vec4(vec3(closestDepth) ,1.0);
    // }

    return shadow;
}

vec3 dir_light_shadow(){
    // retrieve data
    vec3 diffuse_color;
    vec3 ambient_color = vertexData.ambient_color;
    vec3 specular_color = vertexData.specular_color;
    
    // vec3 color = texture(diffuseTexture, vertexData.texcoord).rgb;
    if(!tex_flag){
        diffuse_color = vertexData.diffuse_color;
    }
    else{
        diffuse_color = texture(diffuseTexture, vertexData.texcoord).rgb;
    }
    
    vec3 normal = compute_normal();
    vec3 light_dir = normalize(light_pos - vertexData.FragPos);
    vec3 view_dir = view_pos - vertexData.FragPos;
    vec3 half_dir = normalize(light_dir + view_dir);

    // diffuse
    float diff = max(dot(normal, light_dir), 0.0);

    // specular
    float spec = pow(max(dot(normal, half_dir), 0.0), 64.0);

    // vec3 light_color = (0.2 * ambient_color + 0.7 * diff * diffuse_color + 0.1 * spec * specular_color);
    // ka to kd
    float shadow = shadow_flag ? CalcDirShadow() : 0.0;
    // float shadow = 0.0;
    vec3 light_color = (0.2 * diffuse_color + (1.0 - shadow) * (0.7 * diff * diffuse_color + 0.1 * spec * specular_color));
    return light_color;
    // return diffuse_color;
}

vec3 vis_depth(){
    // transform frag to light space
    // vec4 fragInLight = light_vp * vec4(vertexData.FragPos, 1.0);
    vec4 fragInLight = FragPosLightSpace;

    // normalize to -1~1
    vec3 proj_coord = fragInLight.xyz / fragInLight.w;
    proj_coord = proj_coord * 0.5 + 0.5;

    float depth = texture(shadow_map, proj_coord.xy).z;
    // ortho
    return vec3(depth);

    // Back to NDC
    // depth = depth * 2 - 1.0;
    // vec3 depth_result = vec3((2.0 * near_plane * far_plane) / (far_plane + near_plane - depth * (far_plane - near_plane)));
    // return depth_result / far_plane;
}

void main()
{   
    vec3 result; 

    // cal
    result = dir_light_shadow();

    // vis depth
    if(dir_flag == 2){
        result = vis_depth();
    }

    FragColor = vec4(result, 1.0);
}
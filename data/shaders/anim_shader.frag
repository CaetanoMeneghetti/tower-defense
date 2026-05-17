#version 330 core

out vec4 fragColor;

in vec2 TexCoords;
in vec3 Normal;
in vec3 FragPos;
in mat3 TBN;
in float vHasTangent;


uniform sampler2D tex;
uniform sampler2D normalMap;
uniform vec3 viewPos;
// 0..1: intensidade do flash vermelho (ex: hit do inimigo).
uniform float hitFlash;

// Luz direcional (sol/lua)
struct DirLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};
uniform DirLight light;

// Point lights (lanternas)
struct PointLight {
    vec3 position;
    vec3 color;
};
const int MAX_POINT_LIGHTS = 20;
uniform int numPointLights;
uniform PointLight pointLights[MAX_POINT_LIGHTS];

const float SHININESS = 32.0;

const float PL_KC = 1.0;
const float PL_KL = 0.22;
const float PL_KQ = 0.20;

vec3 calcDirLight(vec3 norm, vec3 viewDir, vec3 matColor) {
    vec3 lDir     = normalize(light.direction);
    vec3 ambient  = light.ambient * matColor;
    float diff    = max(dot(norm, lDir), 0.0);
    vec3 diffuse  = light.diffuse * diff * matColor;
    vec3 halfDir  = normalize(lDir + viewDir);
    float spec    = pow(max(dot(norm, halfDir), 0.0), SHININESS);
    vec3 specular = light.specular * spec;
    return ambient + diffuse + specular;
}

vec3 calcPointLights(vec3 norm, vec3 fragPos, vec3 viewDir, vec3 matColor) {
    vec3 result = vec3(0.0);
    for (int i = 0; i < numPointLights; ++i) {
        vec3 toLight = pointLights[i].position - fragPos;
        float dist = length(toLight);
        vec3 lDir  = toLight / max(dist, 0.0001);
        float att  = 1.0 / (PL_KC + PL_KL * dist + PL_KQ * dist * dist);
        float diff = max(dot(norm, lDir), 0.0);
        vec3 halfDir = normalize(lDir + viewDir);
        float spec = pow(max(dot(norm, halfDir), 0.0), SHININESS);
        result += (diff * matColor + spec * vec3(0.25)) * pointLights[i].color * att;
    }
    return result;
}

void main() {
    vec4 texColor = texture(tex, TexCoords);
    if (texColor.a < 0.1) discard;
    
    vec3 norm;
    if (vHasTangent > 0.5) {
        vec3 mapNormal = texture(normalMap, TexCoords).rgb;
        mapNormal = mapNormal * 2.0 - 1.0; 
        
        

        // --- Gram-Schmidt Per-Pixel ---
        
        vec3 T = TBN[0];
        vec3 B = TBN[1];
        vec3 N = TBN[2];

        
        T = normalize(T - dot(T, N) * N);
        
        
        vec3 B_ideal = cross(N, T);
        
        
        float espelhado = (dot(B_ideal, B) < 0.0) ? -1.0 : 1.0;
        B = B_ideal * espelhado;

        
        mat3 perfectTBN = mat3(T, B, N);

         
        // para diminuir a profundidade do normal map tlvz
        mapNormal.xy *= 1;

        norm = normalize(perfectTBN * mapNormal);
    } else {
        norm = normalize(Normal);
    }
    
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 lit = calcDirLight(norm, viewDir, texColor.rgb)
             + calcPointLights(norm, FragPos, viewDir, texColor.rgb);

    lit = mix(lit, vec3(1.0, 0.0, 0.0), clamp(hitFlash, 0.0, 1.0));

    fragColor = vec4(lit, texColor.a);
}
#version 300 es
precision mediump float;


in vec2 vTexCoords;
in vec3 vViewSpaceNormal;  //N
in vec3 vViewSpacePosition;

uniform vec3 uLightDirection; //L
uniform vec3 uLightIntensity;

uniform float uMetallicFactor;
uniform float uRoughnessFactor;
uniform vec4 uBaseColorFactor;
uniform sampler2D uBaseColorTexture;
uniform sampler2D uMetallicRoughnessTexture;

out vec3 fColor;

// Constants
const float GAMMA = 2.2;
const float INV_GAMMA = 1. / GAMMA;
const float M_PI = 3.141592653589793;
const float M_1_PI = 1.0 / M_PI;

const vec3 dieletricSpecular = vec3(0.04, 0.04, 0.04);

// We need some simple tone mapping functions
// Basic gamma = 2.2 implementation
// Stolen here: https://github.com/KhronosGroup/glTF-Sample-Viewer/blob/master/src/shaders/tonemapping.glsl

// linear to sRGB approximation
// see http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
vec3 LINEARtoSRGB(vec3 color)
{
    return pow(color, vec3(INV_GAMMA));
}

// sRGB to linear approximation
// see http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
vec4 SRGBtoLINEAR(vec4 srgbIn)
{
    return vec4(pow(srgbIn.xyz, vec3(GAMMA)), srgbIn.w);
}

void main()
{
    

    vec3 V = normalize(-vViewSpacePosition);
    vec3 N = normalize(vViewSpaceNormal);
    vec3 L = uLightDirection;
    vec3 H = normalize(L + V);
    const vec3 black = vec3(0.f, 0.f, 0.f);
    
    
    vec4 baseColorFromTexture = SRGBtoLINEAR(texture(uBaseColorTexture, vTexCoords));
    vec4 metallicRougnessFromTexture = texture(uMetallicRoughnessTexture, vTexCoords);
    vec4 baseColor = baseColorFromTexture * uBaseColorFactor;
    
    vec3 metallic = vec3(uMetallicFactor * metallicRougnessFromTexture.b); // metalic
    vec3 c_diff = mix(baseColor.rgb * (1. - dieletricSpecular.r), black, metallic);
    vec3 F0 = mix(dieletricSpecular, baseColor.rgb, metallic);

    float roughness = uRoughnessFactor * metallicRougnessFromTexture.g; //roughness green
    float alpha = roughness * roughness;

    float NdotL = clamp(dot(N, L), 0., 1.);
    float NdotV = clamp(dot(N, V), 0., 1.);
    float NdotH = clamp(dot(N, H), 0., 1.);
    float VdotH = clamp(dot(V, H), 0., 1.);

    float Vis;
    float alpha2 = alpha * alpha;
    float dVis = ((NdotL)   *    sqrt((NdotV) * (NdotV) * (1. - alpha2) + alpha2)   +    (NdotV)    *    sqrt((NdotL * NdotL) *  (1. - alpha2) + alpha2)
    );   
    if(dVis > 0.){
        Vis = 0.5 / dVis;
    }
    
    float D = 0.;
    float dD = M_PI * ((NdotH * NdotH) * (alpha2 -1.) +1.);
    if(dD > 0.){
        D = alpha2 / dD;
    }

    
    vec3 F = F0 + (1. - F0) - (1. - VdotH); //Fresnel Schlick
    vec3 diffuse = baseColor.rgb * M_1_PI;
    vec3 f_specular = (F * Vis * D);
    vec3 f_diffuse = (1. - F) * diffuse;
    vec3 f = f_diffuse + f_specular;

    float shlickFactor  = F * F;
    shlickFactor *= shlickFactor;
    shlickFactor *= F;

    //fColor = LINEARtoSRGB(diffuse * uLightIntensity * NdotL);
    fColor = LINEARtoSRGB((f_diffuse + f_specular) * uLightIntensity * NdotL);
}
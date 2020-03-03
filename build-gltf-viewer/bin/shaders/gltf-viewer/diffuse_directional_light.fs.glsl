#version 300 es
precision mediump float;

in vec3 vViewSpacePosition;
in vec3 vViewSpaceNormal;
in vec2 vTexCoords;


out vec3 fColor;


const float PI = 3.14;

uniform vec3 uLightDir;
uniform vec3 uRadiance;

vec3 light(){
    return vec3(1.f/PI) * uRadiance * dot(normalize(vViewSpaceNormal), uLightDir);
}

void main(){
    fColor = light();
}
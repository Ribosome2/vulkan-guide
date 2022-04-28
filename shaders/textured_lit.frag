#version 450

//shader input
layout (location = 0) in vec3 inColor;

//output write
layout (location = 0) out vec4 outFragColor;

layout(set=0,binding =1) uniform SceneData{
    vec4 fogColor;
    vec4 fogDistance;
    vec4 ambientColor;
    vec4 sunlightDirection;
    vec4 sunlightColor;
} sceneData;

void main()
{
    //return color
    outFragColor = vec4(inColor+sceneData.ambientColor.xyz,1.0f);
}
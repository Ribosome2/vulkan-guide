#version 450
layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec3 vColor;
layout (location = 3) in vec2 vTexCoord;

layout (location = 0) out vec3 outColor;
layout (location = 1) out vec2 texCoord;

//uniforms and ssbos
layout(set=0,binding =1) uniform SceneData{
    vec4 objects;
    vec4 fogDistance;
    vec4 ambientColor;
    vec4 sunlightDirection;
    vec4 sunlightColor;
} objectBuffer;

void main()
{
    mat4 modelMatrix = objectBuffer.objects[gl_InstanceIndex].model;
    mat4 transformMatrix = (cameraData.viewproj * modelMatrix);
    gl_Position = transformMatrix * vec4(vPosition, 1.0f);
    outColor = vColor;
    texCoord = vTexCoord;
}
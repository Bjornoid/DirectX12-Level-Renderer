// an ultra simple hlsl pixel shader
struct OBJ_ATTRIBUTES
{
    float3 Kd;
    float d;
    float3 Ks;
    float Ns;
    float3 Ka;
    float sharpness;
    float3 Tf;
    float Ni;
    float3 Ke;
    unsigned int illum;
};

cbuffer SCENE_DATA : register(b0, space0)
{
    float4 sunDirection, sunColor, sunAmbient, camPos;
    matrix viewProjection;
};

cbuffer MESH_DATA : register(b1, space0)
{
    unsigned int materialIndex;
    unsigned int transformIndexStart;
};

StructuredBuffer<OBJ_ATTRIBUTES> materials : register(t0, space0);

float4 main(float4 posH : SV_POSITION, float3 posW : WORLD, float3 normW : NORMAL) : SV_TARGET
{
    float4 surfaceNormal = normalize(vector(normW, 0));
    float4 dirToLight = -(normalize(sunDirection));

    float ratio = saturate(dot(dirToLight, surfaceNormal));
    float lightRed = sunColor.r * saturate(ratio + sunAmbient.r);
    float lightGreen = sunColor.g * saturate(ratio + sunAmbient.g);
    float lightBlue = sunColor.b * saturate(ratio + sunAmbient.b);
    float3 lambertian = float3(lightRed, lightGreen, lightBlue);

    float3 viewDir = normalize(camPos - posW);
    float3 halfVector = normalize(dirToLight + viewDir);
    float base = saturate(dot(surfaceNormal, halfVector));
    float intensity = max(pow(base, materials[materialIndex].Ns + 0.000001f), 0);
    float3 specular = sunColor.rgb * materials[materialIndex].Ks * intensity;

    float4 outColor = float4(lambertian * materials[materialIndex].Kd.rgb + specular + materials[materialIndex].Ke, 1);
    
	return float4(outColor);     
}
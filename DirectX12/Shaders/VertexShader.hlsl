//#pragma pack_matrix( row_major )

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

StructuredBuffer<matrix> transforms : register(t0, space0);

struct OutputToRasterizer
{
    float4 posH : SV_POSITION;
    float3 posW : WORLD;
    float3 normW : NORMAL;
};

OutputToRasterizer main(float3 inputPos : POSITION, float3 inputUVW : UVW, float3 inputNorm : NORMAL, unsigned int instanceID : SV_InstanceID)
{
    float4 outPosH = float4(inputPos, 1);
    float4 outPosW = float4(inputPos, 1);    
    float4 outNormW = float4(inputNorm, 0);
    
    outPosH = mul(transforms[transformIndexStart + instanceID], outPosH);
    outPosH = mul(viewProjection, outPosH);
    
    outPosW = mul(transforms[transformIndexStart + instanceID], outPosW);
    outNormW = mul(transforms[transformIndexStart + instanceID], outNormW);
    
    OutputToRasterizer output = (OutputToRasterizer) 0;
    output.posH = outPosH;
    output.posW = outPosW;
    output.normW = outNormW;
    
	return output;
}
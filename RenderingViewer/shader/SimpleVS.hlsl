//-------------------------------------------------------------------------------------------------
// File : SimpleVS.hlsl
// Desc : Simple Vertex Shader.
// Copyright(c) Project Asura. All right reserved.
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
// Includes
//-------------------------------------------------------------------------------------------------
#include "SimpleDef.hlsli"

//-------------------------------------------------------------------------------------------------
//      頂点シェーダのメインエントリーポイントです.
//-------------------------------------------------------------------------------------------------
VSOutput VSMain(const VSInput input)
{
    VSOutput output = (VSOutput)0;

    float4 localPos = float4(input.Position, 1.0f);

    float4 worldPos = mul(World, localPos);
    float4 viewPos  = mul(View,  worldPos);
    float4 projPos  = mul(Proj,  viewPos);

    float3 worldNormal = mul( (float3x3)World, input.Normal );

    output.Position = projPos;
    output.Normal   = worldNormal;
    output.TexCoord = input.TexCoord;
    output.Color    = input.Color;

    return output;
}

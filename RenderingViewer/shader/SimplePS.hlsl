//------------------------------------------------------------------- ------------------------------
// File : SimplePS.hlsl
// Desc : Simple Pixel Shader
// Copyright(c) Project Asura. All right reserved.
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
// Includes
//-------------------------------------------------------------------------------------------------
#include "SimpleDef.hlsli"

float3 Phong( float4 pos, float3 normal )
{
    float3 l = -normalize( LightDirection );

    float4 viewNormal = mul( View, float4(normal, 1.0) );
    float3 n = normalize( float3(viewNormal.x, viewNormal.y, viewNormal.z) );
    float3 r = reflect( -normalize( l ), n );
    float3 v = -pos;

    float3 ka = { 0.0, 1.0, 0.0 };
    float3 kd = { 0.5, 0.5, 0.5 };
    float3 ks = { 0.0, 0.0, 0.0 };
    float shininess = 100.0;

    return kd * max( dot( l, n ), 0.0 );
}

//-------------------------------------------------------------------------------------------------
//      ピクセルシェーダのメインエントリーポイントです.
//-------------------------------------------------------------------------------------------------
PSOutput PSMain( const VSOutput input )
{
    PSOutput output = (PSOutput)0;

    //output.Color = input.Color;
    output.Color = float4(Phong( input.Position, input.Normal ), 1.0);

    return output;
}


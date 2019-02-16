//------------------------------------------------------------------- ------------------------------
// File : SimplePS.hlsl
// Desc : Simple Pixel Shader
// Copyright(c) Project Asura. All right reserved.
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
// Includes
//-------------------------------------------------------------------------------------------------
#include "SimpleDef.hlsli"

float4 Phong( float4 pos, float3 normal )
{
    float3 n = normal;

    float3 p = (float3)normalize( mul( World, pos ) );
    float3 v = -p;

    float3 l = -normalize( Direction );

    float3 h = normalize( v + l );

    float3 ambient = Ka;
    float3 diffuse = Kd * max( dot( l, n ), 0.0 );
    float3 specular = Ks * pow(max( dot( l, n ), 0.0 ), Shininess );

    return float4(ambient + diffuse + specular, 1.0);
}

//-------------------------------------------------------------------------------------------------
//      ピクセルシェーダのメインエントリーポイントです.
//-------------------------------------------------------------------------------------------------
PSOutput PSMain( const VSOutput input )
{
    PSOutput output = (PSOutput)0;

    output.Color = Phong( input.Position, input.Normal );

    return output;
}

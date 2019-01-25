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
    float3 l = -normalize( Direction );

    float4 viewNormal = mul( View, float4(normal, 1.0) );
    float3 n = normalize( float3(viewNormal.x, viewNormal.y, viewNormal.z) );
    float3 r = reflect( l, n );
    //float3 v = -pos;

    float3 ambient  = Ka;
    float3 diffuse  = Kd * max( dot( l, n ), 0.0 );
    float3 specular = Ks * pow( max( dot( l, r ), 0.0 ), Shininess );

    return float4(ambient + diffuse, 1.0);
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


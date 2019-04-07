#include "inputDef.hlsli"

//-------------------------------------------------------------------------------------------------
//      Vertex shader entry point
//-------------------------------------------------------------------------------------------------
VSOutput VSMain( const VSInput input )
{
    VSOutput output = (VSOutput)0;

    float4 localPos = float4(input.Position, 1.0f);

    float4 worldPos = mul( World, localPos );
    float4 viewPos = mul( View, worldPos );
    float4 projPos = mul( Proj, viewPos );

    float3 worldNormal = mul( (float3x3)World, input.Normal );

    output.Position = projPos;
    output.WorldPos = worldPos;
    output.Normal = worldNormal;
    output.TexCoord = input.TexCoord;
    output.Color = input.Color;

    return output;
}


float4 Phong( float4 pos, float3 normal )
{
    float3 n = normal;

    float3 p = (float3)normalize( mul( World, pos ) );
    float3 v = -p;

    float3 l = -normalize( Direction );

    float3 h = normalize( v + l );

    float3 ambient = Ka;
    float3 diffuse = Kd * max( dot( l, n ), 0.0 );
    float3 specular = Ks * pow( max( dot( l, n ), 0.0 ), Shininess );

    return float4(ambient + diffuse + specular, 1.0);
}

//-------------------------------------------------------------------------------------------------
//      Pixel shader entry point
//-------------------------------------------------------------------------------------------------
PSOutput PSMain( const VSOutput input )
{
    PSOutput output = (PSOutput)0;

    float4 color = Phong( input.Position, input.Normal );

    float4 lightSpacePos = input.WorldPos;
    lightSpacePos = mul( LightView, lightSpacePos );
    lightSpacePos = mul( LightProj, lightSpacePos );

    lightSpacePos.xyz /= lightSpacePos.w;

    float depth = lightSpacePos.z - 0.00005f;

    float2 shadowTexCoord = 0.5f * (lightSpacePos.xy + 1.0f);
    shadowTexCoord.y = 1.0f - shadowTexCoord.y;

    float4 shadowFactor = float4(1.0, 1.0, 1.0, 1.0);
    if (ShadowMap.Sample( ShadowSmp, shadowTexCoord ).x < depth )
        shadowFactor = float4(0.25, 0.25, 0.25, 1.0);

    output.Color = color * shadowFactor;
    return output;
}

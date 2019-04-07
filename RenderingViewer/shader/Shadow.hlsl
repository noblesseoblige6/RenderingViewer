#include "inputDef.hlsli"

//-------------------------------------------------------------------------------------------------
//      Vertex shader entry point
//-------------------------------------------------------------------------------------------------
VSOutput VSMain( const VSInput input )
{
    VSOutput output = (VSOutput)0;

    float4 localPos = float4(input.Position, 1.0f);

    float4 viewPos = mul( LightView, localPos );
    float4 projPos = mul( LightProj, viewPos );

    output.Position = projPos;

    return output;
}

//-------------------------------------------------------------------------------------------------
//      Pixel shader entry point
//-------------------------------------------------------------------------------------------------
PSOutput PSMain( const VSOutput input )
{
    PSOutput output = (PSOutput)0;

    output.Color = float4(0.0, 1.0, 0.0, 1.0);

    return output;
}
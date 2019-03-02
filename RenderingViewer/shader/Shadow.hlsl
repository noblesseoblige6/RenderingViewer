#include "inputDef.hlsli"

//-------------------------------------------------------------------------------------------------
//      Vertex shader entry point
//-------------------------------------------------------------------------------------------------
VSOutput VSMain(const VSInput input)
{
    VSOutput output = (VSOutput)0;

    float4 localPos = float4(input.Position, 1.0f);

    float4 worldPos = mul(World, localPos);
    float4 viewPos  = mul(View,  worldPos);
    float4 projPos  = mul(Proj,  viewPos);

    output.Position = projPos;

    return output;
}

//-------------------------------------------------------------------------------------------------
//      Pixel shader entry point
//-------------------------------------------------------------------------------------------------
PSOutput PSMain( const VSOutput input )
{
    PSOutput output = (PSOutput)0;

    output.Color = float4(1.0, 1.0, 1.0, 1.0);

    return output;
}
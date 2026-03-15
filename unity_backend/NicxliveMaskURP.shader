Shader "Nicxlive/URP Mask"
{
    Properties
    {
        _StencilRef("Stencil Ref", Float) = 1
        _StencilComp("Stencil Comp", Float) = 8
        _StencilPass("Stencil Pass", Float) = 2
        _ColorMask("Color Mask", Float) = 0
    }

    SubShader
    {
        Tags { "RenderType"="Transparent" "Queue"="Transparent" "RenderPipeline"="UniversalPipeline" }
        Pass
        {
            Blend One Zero
            ZWrite Off
            ZTest Always
            Cull Off
            ColorMask [_ColorMask]

            Stencil
            {
                Ref [_StencilRef]
                Comp [_StencilComp]
                Pass [_StencilPass]
            }

            HLSLPROGRAM
            #pragma vertex Vert
            #pragma fragment Frag

            struct Attributes
            {
                float3 positionOS : POSITION;
            };

            struct Varyings
            {
                float4 positionCS : SV_POSITION;
            };

            Varyings Vert(Attributes input)
            {
                Varyings output;
                output.positionCS = float4(input.positionOS.x, -input.positionOS.y, 0.0, 1.0);
                return output;
            }

            half4 Frag(Varyings input) : SV_Target
            {
                return half4(1.0, 1.0, 1.0, 1.0);
            }
            ENDHLSL
        }
    }
}

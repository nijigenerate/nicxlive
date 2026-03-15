Shader "Hidden/Nicxlive/Present"
{
    Properties
    {
        _MainTex("Main Tex", 2D) = "black" {}
    }

    SubShader
    {
        Tags { "RenderType"="Transparent" "Queue"="Transparent" "RenderPipeline"="UniversalPipeline" }
        Pass
        {
            Blend One OneMinusSrcAlpha
            BlendOp Add
            ZWrite Off
            ZTest Always
            Cull Off

            HLSLPROGRAM
            #pragma vertex Vert
            #pragma fragment Frag

            Texture2D _MainTex;
            SamplerState sampler_MainTex;

            struct Attributes
            {
                float3 positionOS : POSITION;
                float2 uv : TEXCOORD0;
            };

            struct Varyings
            {
                float4 positionCS : SV_POSITION;
                float2 uv : TEXCOORD0;
            };

            half NicxSrgbToLinearChannel(half c)
            {
                return c <= 0.04045h
                    ? (c / 12.92h)
                    : pow((c + 0.055h) / 1.055h, 2.4h);
            }

            half3 NicxSrgbToLinear(half3 c)
            {
                return half3(
                    NicxSrgbToLinearChannel(c.r),
                    NicxSrgbToLinearChannel(c.g),
                    NicxSrgbToLinearChannel(c.b));
            }

            Varyings Vert(Attributes input)
            {
                Varyings output;
                output.positionCS = float4(input.positionOS.xy, 0.0, 1.0);
                output.uv = input.uv;
                return output;
            }

            half4 Frag(Varyings input) : SV_Target
            {
                half4 color = _MainTex.Sample(sampler_MainTex, input.uv);
#ifndef UNITY_COLORSPACE_GAMMA
                color.rgb = NicxSrgbToLinear(color.rgb);
#endif
                return color;
            }
            ENDHLSL
        }
    }
}

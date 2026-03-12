Shader "Nicxlive/URP Part"
{
    Properties
    {
        _MainTex("Main Tex", 2D) = "white" {}
        _EmissionTex("Emission Tex", 2D) = "black" {}
        _Opacity("Opacity", Range(0, 1)) = 1
        _DebugForceOpaque("Debug Force Opaque", Float) = 1
        _DebugFlipY("Debug Flip Y", Float) = 0
        _DebugFlipV("Debug Flip V", Float) = 0
        _DebugShowAlbedo("Debug Show Albedo", Float) = 0
        _MultColor("Multiply", Color) = (1,1,1,1)
        _ScreenColor("Screen", Color) = (0,0,0,0)
        _EmissionStrength("Emission Strength", Float) = 1
        _StencilRef("Stencil Ref", Float) = 1
        _StencilComp("Stencil Comp", Float) = 8
        _StencilPass("Stencil Pass", Float) = 0
        _ColorMask("Color Mask", Float) = 15
    }

    SubShader
    {
        Tags { "RenderType"="Transparent" "Queue"="Transparent" "RenderPipeline"="UniversalPipeline" }
        Pass
        {
            Blend SrcAlpha OneMinusSrcAlpha
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

            #include "Packages/com.unity.render-pipelines.core/ShaderLibrary/Common.hlsl"

            TEXTURE2D(_MainTex);
            SAMPLER(sampler_MainTex);
            TEXTURE2D(_EmissionTex);
            SAMPLER(sampler_EmissionTex);

            CBUFFER_START(UnityPerMaterial)
                float4 _MainTex_ST;
                float4 _MultColor;
                float4 _ScreenColor;
                float _Opacity;
                float _DebugForceOpaque;
                float _DebugFlipY;
                float _DebugFlipV;
                float _DebugShowAlbedo;
                float _EmissionStrength;
            CBUFFER_END

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

            Varyings Vert(Attributes input)
            {
                Varyings output;
                float y = (_DebugFlipY > 0.5) ? -input.positionOS.y : input.positionOS.y;
                output.positionCS = float4(input.positionOS.x, y, 0.0, 1.0);
                output.uv = input.uv;
                return output;
            }

            half4 Frag(Varyings input) : SV_Target
            {
                float2 sampleUv = input.uv;
                if (_DebugFlipV > 0.5)
                {
                    sampleUv.y = 1.0 - sampleUv.y;
                }
                half4 texColor = SAMPLE_TEXTURE2D(_MainTex, sampler_MainTex, sampleUv);
                half3 emission = SAMPLE_TEXTURE2D(_EmissionTex, sampler_EmissionTex, sampleUv).rgb * _EmissionStrength;
                if (_DebugShowAlbedo > 0.5)
                {
                    return half4(texColor.rgb, texColor.a);
                }
                half3 rgb = texColor.rgb * _MultColor.rgb;
                rgb = rgb + _ScreenColor.rgb - (rgb * _ScreenColor.rgb);
                rgb += emission;
                half alpha = saturate(texColor.a * _Opacity);
                if (_DebugForceOpaque > 0.5)
                {
                    if (alpha <= 0.001h)
                    {
                        return half4(sampleUv.x, sampleUv.y, 1.0h, 1.0h);
                    }
                    return half4(max(rgb, half3(0.05h, 0.05h, 0.05h)), 1.0h);
                }
                return half4(rgb, alpha);
            }
            ENDHLSL
        }
    }
}


Shader "Nicxlive/URP Part"
{
    Properties
    {
        _MainTex("Main Tex", 2D) = "white" {}
        _EmissionTex("Emission Tex", 2D) = "black" {}
        _BumpMap("Bump Map", 2D) = "black" {}
        _Opacity("Opacity", Range(0, 1)) = 1
        _MaskThreshold("Mask Threshold", Range(0, 1)) = 0
        _ShaderStage("Shader Stage", Float) = 2
        _BlendMode("Blend Mode", Float) = 0
        _LegacyBlendOnly("Legacy Blend Only", Float) = 1
        _AdvancedBlend("Advanced Blend", Float) = 0
        _DebugForceOpaque("Debug Force Opaque", Float) = 1
        _DebugFlipY("Debug Flip Y", Float) = 0
        _DebugFlipV("Debug Flip V", Float) = 0
        _DebugShowAlbedo("Debug Show Albedo", Float) = 0
        _WrapMainTex("Main Wrap", Float) = 0
        _WrapEmissionTex("Emission Wrap", Float) = 0
        _WrapBumpTex("Bump Wrap", Float) = 0
        _SrcBlend("Src Blend", Float) = 1
        _DstBlend("Dst Blend", Float) = 10
        _SrcBlendAlpha("Src Blend Alpha", Float) = 1
        _DstBlendAlpha("Dst Blend Alpha", Float) = 10
        _BlendOp("Blend Op", Float) = 0
        _BlendOpAlpha("Blend Op Alpha", Float) = 0
        _MultColor("Multiply", Color) = (1,1,1,1)
        _ScreenColor("Screen", Color) = (0,0,0,0)
        _EmissionStrength("Emission Strength", Float) = 1
        _IsMaskPass("Is Mask Pass", Float) = 0
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
            Blend [_SrcBlend] [_DstBlend], [_SrcBlendAlpha] [_DstBlendAlpha]
            BlendOp [_BlendOp], [_BlendOpAlpha]
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

            Texture2D _MainTex;
            SamplerState sampler_MainTex;
            Texture2D _EmissionTex;
            SamplerState sampler_EmissionTex;
            Texture2D _BumpMap;
            SamplerState sampler_BumpMap;

            cbuffer UnityPerMaterial
            {
                float4 _MainTex_ST;
                float4 _MultColor;
                float4 _ScreenColor;
                float _Opacity;
                float _MaskThreshold;
                float _ShaderStage;
                float _BlendMode;
                float _LegacyBlendOnly;
                float _AdvancedBlend;
                float _DebugForceOpaque;
                float _DebugFlipY;
                float _DebugFlipV;
                float _DebugShowAlbedo;
                float _WrapMainTex;
                float _WrapEmissionTex;
                float _WrapBumpTex;
                float _SrcBlend;
                float _DstBlend;
                float _SrcBlendAlpha;
                float _DstBlendAlpha;
                float _BlendOp;
                float _BlendOpAlpha;
                float _EmissionStrength;
                float _IsMaskPass;
            };

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

            struct FragOutputs
            {
                half4 outAlbedo : SV_Target0;
                half4 outEmissive : SV_Target1;
                half4 outBump : SV_Target2;
            };

            half4 SampleWrapMain(float2 uv)
            {
                if ((int)round(_WrapMainTex) == 0)
                {
                    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
                    {
                        return half4(0.0h, 0.0h, 0.0h, 0.0h);
                    }
                }
                return _MainTex.Sample(sampler_MainTex, uv);
            }

            half4 SampleWrapEmission(float2 uv)
            {
                if ((int)round(_WrapEmissionTex) == 0)
                {
                    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
                    {
                        return half4(0.0h, 0.0h, 0.0h, 0.0h);
                    }
                }
                return _EmissionTex.Sample(sampler_EmissionTex, uv);
            }

            half4 SampleWrapBump(float2 uv)
            {
                if ((int)round(_WrapBumpTex) == 0)
                {
                    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
                    {
                        return half4(0.0h, 0.0h, 0.0h, 0.0h);
                    }
                }
                return _BumpMap.Sample(sampler_BumpMap, uv);
            }

            half4 ScreenColorize(half3 texRgb, half alpha)
            {
                return half4(
                    half3(1.0h, 1.0h, 1.0h) -
                    ((half3(1.0h, 1.0h, 1.0h) - texRgb) * (half3(1.0h, 1.0h, 1.0h) - (_ScreenColor.rgb * alpha))),
                    alpha
                );
            }

            Varyings Vert(Attributes input)
            {
                Varyings output;
                float y = (_DebugFlipY > 0.5) ? -input.positionOS.y : input.positionOS.y;
                output.positionCS = float4(input.positionOS.x, y, 0.0, 1.0);
                output.uv = input.uv;
                return output;
            }

            FragOutputs Frag(Varyings input)
            {
                FragOutputs output;
                output.outAlbedo = 0;
                output.outEmissive = 0;
                output.outBump = 0;

                float2 sampleUv = input.uv;
                if (_DebugFlipV > 0.5)
                {
                    sampleUv.y = 1.0 - sampleUv.y;
                }
                half4 texColor = SampleWrapMain(sampleUv);
                half4 emissionTex = SampleWrapEmission(sampleUv);
                half4 bumpColor = SampleWrapBump(sampleUv);
                if (_IsMaskPass > 0.5)
                {
                    clip(texColor.a - max(0.001h, (half)_MaskThreshold));
                    output.outAlbedo = half4(1.0h, 1.0h, 1.0h, 1.0h);
                    return output;
                }
                if (_DebugShowAlbedo > 0.5)
                {
                    half alpha = saturate(texColor.a * _Opacity);
                    clip(alpha - max(0.001h, (half)_MaskThreshold));
                    output.outAlbedo = half4(texColor.rgb, texColor.a) * _Opacity;
                    return output;
                }
                half4 mult = half4(_MultColor.rgb, 1.0h);
                half4 albedoOut = ScreenColorize(texColor.rgb, texColor.a) * mult;
                half4 emissionOut = ScreenColorize(emissionTex.rgb, texColor.a) * mult * _EmissionStrength;
                half alpha = saturate(texColor.a * _Opacity);
                clip(alpha - max(0.001h, (half)_MaskThreshold));
                if (_DebugForceOpaque > 0.5)
                {
                    if (alpha <= 0.001h)
                    {
                        output.outAlbedo = half4(sampleUv.x, sampleUv.y, 1.0h, 1.0h);
                        return output;
                    }
                    output.outAlbedo = half4(max(albedoOut.rgb + (emissionTex.rgb * _EmissionStrength), half3(0.05h, 0.05h, 0.05h)), 1.0h);
                    return output;
                }

                int shaderStage = (int)round(_ShaderStage);
                if (shaderStage == 0)
                {
                    output.outAlbedo = albedoOut * _Opacity;
                    return output;
                }

                if (shaderStage == 1)
                {
                    output.outEmissive = emissionOut * texColor.a;
                    output.outBump = bumpColor * texColor.a;
                    return output;
                }

                output.outAlbedo = albedoOut * _Opacity;
                output.outEmissive = emissionOut * output.outAlbedo.a;
                output.outBump = bumpColor * output.outAlbedo.a;
                return output;
            }
            ENDHLSL
        }
    }
}

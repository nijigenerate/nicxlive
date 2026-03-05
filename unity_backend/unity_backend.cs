using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using Nicxlive.UnityBackend.Interop;
using UnityEngine;
using UnityEngine.Rendering;

namespace System.Runtime.CompilerServices
{
    internal static class IsExternalInit
    {
    }
}

namespace Nicxlive.UnityBackend.Interop
{
    public static class NicxliveNative
    {
        private const string DllName = "nicxlive";

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate nuint CreateTextureDelegate(
            int width,
            int height,
            int channels,
            int mipLevels,
            int format,
            [MarshalAs(UnmanagedType.I1)] bool renderTarget,
            [MarshalAs(UnmanagedType.I1)] bool stencil,
            IntPtr userData);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void UpdateTextureDelegate(
            nuint handle,
            IntPtr data,
            nuint dataLen,
            int width,
            int height,
            int channels,
            IntPtr userData);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void ReleaseTextureDelegate(nuint handle, IntPtr userData);

        public enum NjgResult : int
        {
            Ok = 0,
            InvalidArgument = 1,
            Failure = 2,
        }

        public enum NjgRenderCommandKind : uint
        {
            DrawPart,
            BeginDynamicComposite,
            EndDynamicComposite,
            BeginMask,
            ApplyMask,
            BeginMaskContent,
            EndMask,
        }

        public enum MaskDrawableKind : uint
        {
            Part = 0,
            Mask = 1,
        }

        public enum BlendMode : int
        {
            Normal = 0,
            Multiply = 1,
            Screen = 2,
            Overlay = 3,
            Darken = 4,
            Lighten = 5,
            ColorDodge = 6,
            LinearDodge = 7,
            AddGlow = 8,
            ColorBurn = 9,
            HardLight = 10,
            SoftLight = 11,
            Difference = 12,
            Exclusion = 13,
            Subtract = 14,
            Inverse = 15,
            DestinationIn = 16,
            ClipToLower = 17,
            SliceFromLower = 18,
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct UnityRendererConfig
        {
            public int ViewportWidth;
            public int ViewportHeight;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct FrameConfig
        {
            public int ViewportWidth;
            public int ViewportHeight;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct Vec2
        {
            public float X;
            public float Y;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct Vec3
        {
            public float X;
            public float Y;
            public float Z;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct Mat4
        {
            public float M11; public float M12; public float M13; public float M14;
            public float M21; public float M22; public float M23; public float M24;
            public float M31; public float M32; public float M33; public float M34;
            public float M41; public float M42; public float M43; public float M44;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct UnityResourceCallbacks
        {
            public IntPtr UserData;
            public IntPtr CreateTexture;
            public IntPtr UpdateTexture;
            public IntPtr ReleaseTexture;
        }

        [StructLayout(LayoutKind.Sequential)]
        public unsafe struct NjgPartDrawPacket
        {
            [MarshalAs(UnmanagedType.I1)] public bool IsMask;
            [MarshalAs(UnmanagedType.I1)] public bool Renderable;
            public Mat4 ModelMatrix;
            public Mat4 RenderMatrix;
            public float RenderRotation;
            public Vec3 ClampedTint;
            public Vec3 ClampedScreen;
            public float Opacity;
            public float EmissionStrength;
            public float MaskThreshold;
            public int BlendingMode;
            [MarshalAs(UnmanagedType.I1)] public bool UseMultistageBlend;
            [MarshalAs(UnmanagedType.I1)] public bool HasEmissionOrBumpmap;
            public nuint TextureHandle0;
            public nuint TextureHandle1;
            public nuint TextureHandle2;
            public nuint TextureCount;
            public Vec2 Origin;
            public nuint VertexOffset;
            public nuint VertexAtlasStride;
            public nuint UvOffset;
            public nuint UvAtlasStride;
            public nuint DeformOffset;
            public nuint DeformAtlasStride;
            public nuint IndexHandle;
            public ushort* Indices;
            public nuint IndexCount;
            public nuint VertexCount;
        }

        [StructLayout(LayoutKind.Sequential)]
        public unsafe struct NjgMaskDrawPacket
        {
            public Mat4 ModelMatrix;
            public Mat4 Mvp;
            public Vec2 Origin;
            public nuint VertexOffset;
            public nuint VertexAtlasStride;
            public nuint DeformOffset;
            public nuint DeformAtlasStride;
            public nuint IndexHandle;
            public ushort* Indices;
            public nuint IndexCount;
            public nuint VertexCount;
        }

        [StructLayout(LayoutKind.Sequential)]
        public unsafe struct NjgMaskApplyPacket
        {
            public MaskDrawableKind Kind;
            [MarshalAs(UnmanagedType.I1)] public bool IsDodge;
            public NjgPartDrawPacket PartPacket;
            public NjgMaskDrawPacket MaskPacket;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct NjgDynamicCompositePass
        {
            public nuint Texture0;
            public nuint Texture1;
            public nuint Texture2;
            public nuint TextureCount;
            public nuint Stencil;
            public Vec2 Scale;
            public float RotationZ;
            [MarshalAs(UnmanagedType.I1)] public bool AutoScaled;
            public nuint OrigBuffer;
            public int OrigViewport0;
            public int OrigViewport1;
            public int OrigViewport2;
            public int OrigViewport3;
            public int DrawBufferCount;
            [MarshalAs(UnmanagedType.I1)] public bool HasStencil;
        }

        [StructLayout(LayoutKind.Sequential)]
        public unsafe struct NjgQueuedCommand
        {
            public NjgRenderCommandKind Kind;
            public NjgPartDrawPacket PartPacket;
            public NjgMaskApplyPacket MaskApplyPacket;
            public NjgDynamicCompositePass DynamicPass;
            [MarshalAs(UnmanagedType.I1)] public bool UsesStencil;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct CommandQueueView
        {
            public IntPtr Commands;
            public nuint Count;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct NjgBufferSlice
        {
            public IntPtr Data;
            public nuint Length;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct SharedBufferSnapshot
        {
            public NjgBufferSlice Vertices;
            public NjgBufferSlice Uvs;
            public NjgBufferSlice Deform;
            public nuint VertexCount;
            public nuint UvCount;
            public nuint DeformCount;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct NjgWasmLayout
        {
            public uint SizeQueued;
            public uint OffQueuedPart;
            public uint OffQueuedMaskApply;
            public uint OffQueuedDynamic;
            public uint OffQueuedUsesStencil;

            public uint SizePart;
            public uint OffPartTextureHandles;
            public uint OffPartTextureCount;
            public uint OffPartOrigin;
            public uint OffPartVertexOffset;
            public uint OffPartVertexStride;
            public uint OffPartUvOffset;
            public uint OffPartUvStride;
            public uint OffPartDeformOffset;
            public uint OffPartDeformStride;
            public uint OffPartIndexHandle;
            public uint OffPartIndicesPtr;
            public uint OffPartIndexCount;
            public uint OffPartVertexCount;

            public uint SizeMaskDraw;
            public uint OffMaskDrawIndicesPtr;
            public uint OffMaskDrawIndexCount;
            public uint OffMaskDrawVertexOffset;
            public uint OffMaskDrawVertexStride;
            public uint OffMaskDrawDeformOffset;
            public uint OffMaskDrawDeformStride;
            public uint OffMaskDrawIndexHandle;
            public uint OffMaskDrawVertexCount;

            public uint SizeMaskApply;
            public uint OffMaskKind;
            public uint OffMaskIsDodge;
            public uint OffMaskPartPacket;
            public uint OffMaskMaskPacket;

            public uint SizeDynamicPass;
            public uint OffDynTextures;
            public uint OffDynTextureCount;
            public uint OffDynStencil;
            public uint OffDynScale;
            public uint OffDynRotationZ;
            public uint OffDynAutoScaled;
            public uint OffDynOrigBuffer;
            public uint OffDynOrigViewport;
            public uint OffDynDrawBufferCount;
            public uint OffDynHasStencil;
        }

        [DllImport(DllName, EntryPoint = "njgCreateRenderer", CallingConvention = CallingConvention.Cdecl)]
        public static extern NjgResult CreateRenderer(ref UnityRendererConfig config, ref UnityResourceCallbacks callbacks, out IntPtr renderer);

        [DllImport(DllName, EntryPoint = "njgDestroyRenderer", CallingConvention = CallingConvention.Cdecl)]
        public static extern void DestroyRenderer(IntPtr renderer);

        [DllImport(DllName, EntryPoint = "njgLoadPuppet", CallingConvention = CallingConvention.Cdecl)]
        public static extern NjgResult LoadPuppet(IntPtr renderer, [MarshalAs(UnmanagedType.LPUTF8Str)] string path, out IntPtr puppet);

        [DllImport(DllName, EntryPoint = "njgUnloadPuppet", CallingConvention = CallingConvention.Cdecl)]
        public static extern NjgResult UnloadPuppet(IntPtr renderer, IntPtr puppet);

        [DllImport(DllName, EntryPoint = "njgBeginFrame", CallingConvention = CallingConvention.Cdecl)]
        public static extern NjgResult BeginFrame(IntPtr renderer, ref FrameConfig config);

        [DllImport(DllName, EntryPoint = "njgTickPuppet", CallingConvention = CallingConvention.Cdecl)]
        public static extern NjgResult TickPuppet(IntPtr puppet, double deltaSeconds);

        [DllImport(DllName, EntryPoint = "njgEmitCommands", CallingConvention = CallingConvention.Cdecl)]
        public static extern NjgResult EmitCommands(IntPtr renderer, out CommandQueueView view);

        [DllImport(DllName, EntryPoint = "njgGetSharedBuffers", CallingConvention = CallingConvention.Cdecl)]
        public static extern NjgResult GetSharedBuffers(IntPtr renderer, out SharedBufferSnapshot snapshot);

        [DllImport(DllName, EntryPoint = "njgGetWasmLayout", CallingConvention = CallingConvention.Cdecl)]
        public static extern NjgResult GetWasmLayout(out NjgWasmLayout layout);
    }
}



namespace Nicxlive.UnityBackend.Managed
{
    internal static class AbiValidator
    {
        private static bool _validated;

        public static void ValidateOrThrow()
        {
            if (_validated)
            {
                return;
            }

            var result = NicxliveNative.GetWasmLayout(out var native);
            if (result != NicxliveNative.NjgResult.Ok)
            {
                throw new InvalidOperationException($"njgGetWasmLayout failed: {result}");
            }

            RequireEqual("sizeof(NjgQueuedCommand)", (uint)Marshal.SizeOf<NicxliveNative.NjgQueuedCommand>(), native.SizeQueued);
            RequireEqual("offsetof(NjgQueuedCommand.PartPacket)", Offset<NicxliveNative.NjgQueuedCommand>(nameof(NicxliveNative.NjgQueuedCommand.PartPacket)), native.OffQueuedPart);
            RequireEqual("offsetof(NjgQueuedCommand.MaskApplyPacket)", Offset<NicxliveNative.NjgQueuedCommand>(nameof(NicxliveNative.NjgQueuedCommand.MaskApplyPacket)), native.OffQueuedMaskApply);
            RequireEqual("offsetof(NjgQueuedCommand.DynamicPass)", Offset<NicxliveNative.NjgQueuedCommand>(nameof(NicxliveNative.NjgQueuedCommand.DynamicPass)), native.OffQueuedDynamic);
            RequireEqual("offsetof(NjgQueuedCommand.UsesStencil)", Offset<NicxliveNative.NjgQueuedCommand>(nameof(NicxliveNative.NjgQueuedCommand.UsesStencil)), native.OffQueuedUsesStencil);

            RequireEqual("sizeof(NjgPartDrawPacket)", (uint)Marshal.SizeOf<NicxliveNative.NjgPartDrawPacket>(), native.SizePart);
            RequireEqual("offsetof(NjgPartDrawPacket.TextureHandle0)", Offset<NicxliveNative.NjgPartDrawPacket>(nameof(NicxliveNative.NjgPartDrawPacket.TextureHandle0)), native.OffPartTextureHandles);
            RequireEqual("offsetof(NjgPartDrawPacket.TextureCount)", Offset<NicxliveNative.NjgPartDrawPacket>(nameof(NicxliveNative.NjgPartDrawPacket.TextureCount)), native.OffPartTextureCount);
            RequireEqual("offsetof(NjgPartDrawPacket.Origin)", Offset<NicxliveNative.NjgPartDrawPacket>(nameof(NicxliveNative.NjgPartDrawPacket.Origin)), native.OffPartOrigin);
            RequireEqual("offsetof(NjgPartDrawPacket.VertexOffset)", Offset<NicxliveNative.NjgPartDrawPacket>(nameof(NicxliveNative.NjgPartDrawPacket.VertexOffset)), native.OffPartVertexOffset);
            RequireEqual("offsetof(NjgPartDrawPacket.VertexAtlasStride)", Offset<NicxliveNative.NjgPartDrawPacket>(nameof(NicxliveNative.NjgPartDrawPacket.VertexAtlasStride)), native.OffPartVertexStride);
            RequireEqual("offsetof(NjgPartDrawPacket.UvOffset)", Offset<NicxliveNative.NjgPartDrawPacket>(nameof(NicxliveNative.NjgPartDrawPacket.UvOffset)), native.OffPartUvOffset);
            RequireEqual("offsetof(NjgPartDrawPacket.UvAtlasStride)", Offset<NicxliveNative.NjgPartDrawPacket>(nameof(NicxliveNative.NjgPartDrawPacket.UvAtlasStride)), native.OffPartUvStride);
            RequireEqual("offsetof(NjgPartDrawPacket.DeformOffset)", Offset<NicxliveNative.NjgPartDrawPacket>(nameof(NicxliveNative.NjgPartDrawPacket.DeformOffset)), native.OffPartDeformOffset);
            RequireEqual("offsetof(NjgPartDrawPacket.DeformAtlasStride)", Offset<NicxliveNative.NjgPartDrawPacket>(nameof(NicxliveNative.NjgPartDrawPacket.DeformAtlasStride)), native.OffPartDeformStride);
            RequireEqual("offsetof(NjgPartDrawPacket.IndexHandle)", Offset<NicxliveNative.NjgPartDrawPacket>(nameof(NicxliveNative.NjgPartDrawPacket.IndexHandle)), native.OffPartIndexHandle);
            RequireEqual("offsetof(NjgPartDrawPacket.Indices)", Offset<NicxliveNative.NjgPartDrawPacket>(nameof(NicxliveNative.NjgPartDrawPacket.Indices)), native.OffPartIndicesPtr);
            RequireEqual("offsetof(NjgPartDrawPacket.IndexCount)", Offset<NicxliveNative.NjgPartDrawPacket>(nameof(NicxliveNative.NjgPartDrawPacket.IndexCount)), native.OffPartIndexCount);
            RequireEqual("offsetof(NjgPartDrawPacket.VertexCount)", Offset<NicxliveNative.NjgPartDrawPacket>(nameof(NicxliveNative.NjgPartDrawPacket.VertexCount)), native.OffPartVertexCount);

            RequireEqual("sizeof(NjgMaskDrawPacket)", (uint)Marshal.SizeOf<NicxliveNative.NjgMaskDrawPacket>(), native.SizeMaskDraw);
            RequireEqual("offsetof(NjgMaskDrawPacket.Indices)", Offset<NicxliveNative.NjgMaskDrawPacket>(nameof(NicxliveNative.NjgMaskDrawPacket.Indices)), native.OffMaskDrawIndicesPtr);
            RequireEqual("offsetof(NjgMaskDrawPacket.IndexCount)", Offset<NicxliveNative.NjgMaskDrawPacket>(nameof(NicxliveNative.NjgMaskDrawPacket.IndexCount)), native.OffMaskDrawIndexCount);
            RequireEqual("offsetof(NjgMaskDrawPacket.VertexOffset)", Offset<NicxliveNative.NjgMaskDrawPacket>(nameof(NicxliveNative.NjgMaskDrawPacket.VertexOffset)), native.OffMaskDrawVertexOffset);
            RequireEqual("offsetof(NjgMaskDrawPacket.VertexAtlasStride)", Offset<NicxliveNative.NjgMaskDrawPacket>(nameof(NicxliveNative.NjgMaskDrawPacket.VertexAtlasStride)), native.OffMaskDrawVertexStride);
            RequireEqual("offsetof(NjgMaskDrawPacket.DeformOffset)", Offset<NicxliveNative.NjgMaskDrawPacket>(nameof(NicxliveNative.NjgMaskDrawPacket.DeformOffset)), native.OffMaskDrawDeformOffset);
            RequireEqual("offsetof(NjgMaskDrawPacket.DeformAtlasStride)", Offset<NicxliveNative.NjgMaskDrawPacket>(nameof(NicxliveNative.NjgMaskDrawPacket.DeformAtlasStride)), native.OffMaskDrawDeformStride);
            RequireEqual("offsetof(NjgMaskDrawPacket.IndexHandle)", Offset<NicxliveNative.NjgMaskDrawPacket>(nameof(NicxliveNative.NjgMaskDrawPacket.IndexHandle)), native.OffMaskDrawIndexHandle);
            RequireEqual("offsetof(NjgMaskDrawPacket.VertexCount)", Offset<NicxliveNative.NjgMaskDrawPacket>(nameof(NicxliveNative.NjgMaskDrawPacket.VertexCount)), native.OffMaskDrawVertexCount);

            RequireEqual("sizeof(NjgMaskApplyPacket)", (uint)Marshal.SizeOf<NicxliveNative.NjgMaskApplyPacket>(), native.SizeMaskApply);
            RequireEqual("offsetof(NjgMaskApplyPacket.Kind)", Offset<NicxliveNative.NjgMaskApplyPacket>(nameof(NicxliveNative.NjgMaskApplyPacket.Kind)), native.OffMaskKind);
            RequireEqual("offsetof(NjgMaskApplyPacket.IsDodge)", Offset<NicxliveNative.NjgMaskApplyPacket>(nameof(NicxliveNative.NjgMaskApplyPacket.IsDodge)), native.OffMaskIsDodge);
            RequireEqual("offsetof(NjgMaskApplyPacket.PartPacket)", Offset<NicxliveNative.NjgMaskApplyPacket>(nameof(NicxliveNative.NjgMaskApplyPacket.PartPacket)), native.OffMaskPartPacket);
            RequireEqual("offsetof(NjgMaskApplyPacket.MaskPacket)", Offset<NicxliveNative.NjgMaskApplyPacket>(nameof(NicxliveNative.NjgMaskApplyPacket.MaskPacket)), native.OffMaskMaskPacket);

            RequireEqual("sizeof(NjgDynamicCompositePass)", (uint)Marshal.SizeOf<NicxliveNative.NjgDynamicCompositePass>(), native.SizeDynamicPass);
            RequireEqual("offsetof(NjgDynamicCompositePass.Texture0)", Offset<NicxliveNative.NjgDynamicCompositePass>(nameof(NicxliveNative.NjgDynamicCompositePass.Texture0)), native.OffDynTextures);
            RequireEqual("offsetof(NjgDynamicCompositePass.TextureCount)", Offset<NicxliveNative.NjgDynamicCompositePass>(nameof(NicxliveNative.NjgDynamicCompositePass.TextureCount)), native.OffDynTextureCount);
            RequireEqual("offsetof(NjgDynamicCompositePass.Stencil)", Offset<NicxliveNative.NjgDynamicCompositePass>(nameof(NicxliveNative.NjgDynamicCompositePass.Stencil)), native.OffDynStencil);
            RequireEqual("offsetof(NjgDynamicCompositePass.Scale)", Offset<NicxliveNative.NjgDynamicCompositePass>(nameof(NicxliveNative.NjgDynamicCompositePass.Scale)), native.OffDynScale);
            RequireEqual("offsetof(NjgDynamicCompositePass.RotationZ)", Offset<NicxliveNative.NjgDynamicCompositePass>(nameof(NicxliveNative.NjgDynamicCompositePass.RotationZ)), native.OffDynRotationZ);
            RequireEqual("offsetof(NjgDynamicCompositePass.AutoScaled)", Offset<NicxliveNative.NjgDynamicCompositePass>(nameof(NicxliveNative.NjgDynamicCompositePass.AutoScaled)), native.OffDynAutoScaled);
            RequireEqual("offsetof(NjgDynamicCompositePass.OrigBuffer)", Offset<NicxliveNative.NjgDynamicCompositePass>(nameof(NicxliveNative.NjgDynamicCompositePass.OrigBuffer)), native.OffDynOrigBuffer);
            RequireEqual("offsetof(NjgDynamicCompositePass.OrigViewport0)", Offset<NicxliveNative.NjgDynamicCompositePass>(nameof(NicxliveNative.NjgDynamicCompositePass.OrigViewport0)), native.OffDynOrigViewport);
            RequireEqual("offsetof(NjgDynamicCompositePass.DrawBufferCount)", Offset<NicxliveNative.NjgDynamicCompositePass>(nameof(NicxliveNative.NjgDynamicCompositePass.DrawBufferCount)), native.OffDynDrawBufferCount);
            RequireEqual("offsetof(NjgDynamicCompositePass.HasStencil)", Offset<NicxliveNative.NjgDynamicCompositePass>(nameof(NicxliveNative.NjgDynamicCompositePass.HasStencil)), native.OffDynHasStencil);

            _validated = true;
        }

        private static uint Offset<T>(string fieldName) where T : struct
        {
            return (uint)Marshal.OffsetOf<T>(fieldName).ToInt64();
        }

        private static void RequireEqual(string label, uint managed, uint native)
        {
            if (managed != native)
            {
                throw new InvalidOperationException($"{label} mismatch managed={managed} native={native}");
            }
        }
    }

    public sealed class DecodedCommand
    {
        public NicxliveNative.NjgRenderCommandKind Kind;
        public NicxliveNative.NjgPartDrawPacket PartPacket;
        public NicxliveNative.NjgMaskApplyPacket MaskApplyPacket;
        public NicxliveNative.NjgDynamicCompositePass DynamicPass;
        public bool UsesStencil;
    }

    public static class CommandStream
    {
        public static unsafe List<DecodedCommand> Decode(NicxliveNative.CommandQueueView view)
        {
            var commands = new List<DecodedCommand>((int)view.Count);
            if (view.Commands == IntPtr.Zero || view.Count == 0)
            {
                return commands;
            }

            var basePtr = (NicxliveNative.NjgQueuedCommand*)view.Commands;
            for (nuint i = 0; i < view.Count; i++)
            {
                var src = basePtr[i];
                commands.Add(new DecodedCommand
                {
                    Kind = src.Kind,
                    PartPacket = src.PartPacket,
                    MaskApplyPacket = src.MaskApplyPacket,
                    DynamicPass = src.DynamicPass,
                    UsesStencil = src.UsesStencil,
                });
            }

            return commands;
        }
    }
}



namespace Nicxlive.UnityBackend.Managed
{
    public sealed class SharedBuffers
    {
        public float[] Vertices = Array.Empty<float>();
        public float[] Uvs = Array.Empty<float>();
        public float[] Deform = Array.Empty<float>();
        public int VertexCount;
        public int UvCount;
        public int DeformCount;
    }

    public static class SharedBufferUploader
    {
        public static SharedBuffers CopyFromNative(NicxliveNative.SharedBufferSnapshot snapshot)
        {
            var shared = new SharedBuffers
            {
                VertexCount = checked((int)snapshot.VertexCount),
                UvCount = checked((int)snapshot.UvCount),
                DeformCount = checked((int)snapshot.DeformCount),
                Vertices = CopySlice(snapshot.Vertices),
                Uvs = CopySlice(snapshot.Uvs),
                Deform = CopySlice(snapshot.Deform),
            };
            return shared;
        }

        private static float[] CopySlice(NicxliveNative.NjgBufferSlice slice)
        {
            if (slice.Data == IntPtr.Zero || slice.Length == 0)
            {
                return Array.Empty<float>();
            }

            var dst = new float[checked((int)slice.Length)];
            Marshal.Copy(slice.Data, dst, 0, dst.Length);
            return dst;
        }
    }
}



namespace Nicxlive.UnityBackend.Managed
{
    public enum Filtering
    {
        Nearest = 0,
        Linear = 1,
        Bilinear = 2,
        Trilinear = 3,
    }

    public enum Wrapping
    {
        Repeat = 0,
        MirroredRepeat = 1,
        ClampToEdge = 2,
        ClampToBorder = 3,
    }

    public sealed class ShaderHandle
    {
        public ulong HandleId;
        public Material? Material;
    }

    public sealed class TextureRegistry
    {
        private readonly Dictionary<ulong, Texture> _textures = new Dictionary<ulong, Texture>();
        private readonly Dictionary<ulong, ShaderHandle> _shaders = new Dictionary<ulong, ShaderHandle>();
        private readonly Dictionary<ulong, Dictionary<string, int>> _shaderUniformIds = new Dictionary<ulong, Dictionary<string, int>>();
        private ulong _nextHandle = 1;
        private ulong _nextShaderHandle = 1;

        public ulong CreateTexture(int width, int height, int channels, bool renderTarget, bool stencil)
        {
            var handle = _nextHandle++;
            if (renderTarget)
            {
                var depth = stencil ? 24 : 0;
                var rt = new RenderTexture(Mathf.Max(1, width), Mathf.Max(1, height), depth, RenderTextureFormat.ARGB32);
                rt.name = $"nicx_rt_{handle}";
                rt.useMipMap = false;
                rt.autoGenerateMips = false;
                rt.Create();
                _textures[handle] = rt;
            }
            else
            {
                var format = channels switch
                {
                    1 => TextureFormat.R8,
                    2 => TextureFormat.RG16,
                    3 => TextureFormat.RGB24,
                    _ => TextureFormat.RGBA32,
                };
                var tex = new Texture2D(Mathf.Max(1, width), Mathf.Max(1, height), format, false, false);
                tex.name = $"nicx_tex_{handle}";
                _textures[handle] = tex;
            }

            return handle;
        }

        public unsafe void UpdateTexture(ulong handle, IntPtr data, nuint dataLen, int width, int height, int channels)
        {
            if (!_textures.TryGetValue(handle, out var texture))
            {
                return;
            }

            if (!(texture is Texture2D tex2D))
            {
                return;
            }

            if (data == IntPtr.Zero || dataLen == 0)
            {
                return;
            }

            var expected = checked(Mathf.Max(1, width) * Mathf.Max(1, height) * Mathf.Max(1, channels));
            var copyLen = Math.Min(expected, checked((int)dataLen));
            var bytes = new byte[copyLen];
            Marshal.Copy(data, bytes, 0, copyLen);

            if (tex2D.width != width || tex2D.height != height)
            {
                tex2D.Reinitialize(width, height);
            }

            tex2D.LoadRawTextureData(bytes);
            tex2D.Apply(false, false);
        }

        public void ReleaseTexture(ulong handle)
        {
            if (!_textures.TryGetValue(handle, out var texture))
            {
                return;
            }

            _textures.Remove(handle);
            if (texture != null)
            {
                UnityEngine.Object.Destroy(texture);
            }
        }

        public Texture? TryGet(ulong handle)
        {
            _textures.TryGetValue(handle, out var texture);
            return texture;
        }

        public RenderTexture? TryGetRenderTexture(ulong handle)
        {
            return TryGet(handle) as RenderTexture;
        }

        public ulong createTextureHandle()
        {
            return CreateTexture(1, 1, 4, false, false);
        }

        public void destroyTextureHandle(ulong handle)
        {
            ReleaseTexture(handle);
        }

        public void bindTextureHandle(ulong handle, int unit)
        {
            if (!_textures.TryGetValue(handle, out var texture) || texture == null)
            {
                Shader.SetGlobalTexture($"_NicxTex{unit}", _whiteFallback());
                return;
            }
            Shader.SetGlobalTexture($"_NicxTex{unit}", texture);
        }

        public void uploadTextureData(ulong handle, int width, int height, int channels, byte[] data)
        {
            if (data == null || data.Length == 0)
            {
                return;
            }
            var pinned = GCHandle.Alloc(data, GCHandleType.Pinned);
            try
            {
                UpdateTexture(handle, pinned.AddrOfPinnedObject(), (nuint)data.Length, width, height, channels);
            }
            finally
            {
                pinned.Free();
            }
        }

        public void generateTextureMipmap(ulong handle)
        {
            if (!_textures.TryGetValue(handle, out var texture) || texture == null)
            {
                return;
            }
            if (texture is Texture2D tex2D)
            {
                tex2D.Apply(true, false);
            }
            else if (texture is RenderTexture rt)
            {
                rt.useMipMap = true;
                rt.autoGenerateMips = true;
                rt.GenerateMips();
            }
        }

        public void applyTextureFiltering(ulong handle, Filtering filtering, bool useMipmaps = true)
        {
            if (!_textures.TryGetValue(handle, out var texture) || texture == null)
            {
                return;
            }
            texture.filterMode = filtering switch
            {
                Filtering.Nearest => FilterMode.Point,
                Filtering.Linear => FilterMode.Bilinear,
                Filtering.Bilinear => FilterMode.Bilinear,
                Filtering.Trilinear => FilterMode.Trilinear,
                _ => FilterMode.Bilinear,
            };
            if (texture is Texture2D tex2D)
            {
                tex2D.Apply(useMipmaps, false);
            }
        }

        public void applyTextureWrapping(ulong handle, Wrapping wrapping)
        {
            if (!_textures.TryGetValue(handle, out var texture) || texture == null)
            {
                return;
            }
            texture.wrapMode = wrapping switch
            {
                Wrapping.Repeat => TextureWrapMode.Repeat,
                Wrapping.MirroredRepeat => TextureWrapMode.Mirror,
                Wrapping.ClampToEdge => TextureWrapMode.Clamp,
                Wrapping.ClampToBorder => TextureWrapMode.Clamp,
                _ => TextureWrapMode.Clamp,
            };
        }

        public void applyTextureAnisotropy(ulong handle, float value)
        {
            if (!_textures.TryGetValue(handle, out var texture) || texture == null)
            {
                return;
            }
            texture.anisoLevel = Mathf.Clamp(Mathf.RoundToInt(value), 1, 16);
        }

        public byte[] readTextureData(ulong handle, int channels, bool stencil)
        {
            _ = stencil;
            if (!_textures.TryGetValue(handle, out var texture) || texture == null)
            {
                return Array.Empty<byte>();
            }

            if (texture is Texture2D tex2D)
            {
                return tex2D.GetRawTextureData();
            }

            if (texture is RenderTexture rt)
            {
                var prev = RenderTexture.active;
                RenderTexture.active = rt;
                var copy = new Texture2D(rt.width, rt.height, channels == 3 ? TextureFormat.RGB24 : TextureFormat.RGBA32, false, false);
                copy.ReadPixels(new Rect(0, 0, rt.width, rt.height), 0, 0);
                copy.Apply(false, false);
                var data = copy.GetRawTextureData();
                UnityEngine.Object.Destroy(copy);
                RenderTexture.active = prev;
                return data;
            }

            return Array.Empty<byte>();
        }

        public ulong textureNativeHandle(ulong handle)
        {
            if (!_textures.TryGetValue(handle, out var texture) || texture == null)
            {
                return 0;
            }
            return (ulong)texture.GetNativeTexturePtr().ToInt64();
        }

        public ulong createShader(Shader shader)
        {
            var handle = _nextShaderHandle++;
            _shaders[handle] = new ShaderHandle
            {
                HandleId = handle,
                Material = shader != null ? new Material(shader) : null,
            };
            _shaderUniformIds[handle] = new Dictionary<string, int>(StringComparer.Ordinal);
            return handle;
        }

        public void destroyShader(ulong handle)
        {
            if (_shaders.TryGetValue(handle, out var shaderHandle))
            {
                if (shaderHandle.Material != null)
                {
                    UnityEngine.Object.Destroy(shaderHandle.Material);
                }
                _shaders.Remove(handle);
            }
            _shaderUniformIds.Remove(handle);
        }

        public Material? useShader(ulong handle)
        {
            if (_shaders.TryGetValue(handle, out var shaderHandle))
            {
                return shaderHandle.Material;
            }
            return null;
        }

        public int getShaderUniformLocation(ulong handle, string name)
        {
            if (!_shaderUniformIds.TryGetValue(handle, out var map))
            {
                return -1;
            }
            if (!map.TryGetValue(name, out var id))
            {
                id = Shader.PropertyToID(name);
                map[name] = id;
            }
            return id;
        }

        public void setShaderUniform(ulong handle, int location, bool value)
        {
            if (_shaders.TryGetValue(handle, out var shaderHandle) && shaderHandle.Material != null)
            {
                shaderHandle.Material.SetFloat(location, value ? 1.0f : 0.0f);
            }
        }

        public void setShaderUniform(ulong handle, int location, int value)
        {
            if (_shaders.TryGetValue(handle, out var shaderHandle) && shaderHandle.Material != null)
            {
                shaderHandle.Material.SetInt(location, value);
            }
        }

        public void setShaderUniform(ulong handle, int location, float value)
        {
            if (_shaders.TryGetValue(handle, out var shaderHandle) && shaderHandle.Material != null)
            {
                shaderHandle.Material.SetFloat(location, value);
            }
        }

        public void setShaderUniform(ulong handle, int location, Vector2 value)
        {
            if (_shaders.TryGetValue(handle, out var shaderHandle) && shaderHandle.Material != null)
            {
                shaderHandle.Material.SetVector(location, value);
            }
        }

        public void setShaderUniform(ulong handle, int location, Vector3 value)
        {
            if (_shaders.TryGetValue(handle, out var shaderHandle) && shaderHandle.Material != null)
            {
                shaderHandle.Material.SetVector(location, value);
            }
        }

        public void setShaderUniform(ulong handle, int location, Vector4 value)
        {
            if (_shaders.TryGetValue(handle, out var shaderHandle) && shaderHandle.Material != null)
            {
                shaderHandle.Material.SetVector(location, value);
            }
        }

        public void setShaderUniform(ulong handle, int location, Matrix4x4 value)
        {
            if (_shaders.TryGetValue(handle, out var shaderHandle) && shaderHandle.Material != null)
            {
                shaderHandle.Material.SetMatrix(location, value);
            }
        }

        private Texture _whiteFallback()
        {
            return Texture2D.whiteTexture;
        }
    }
}

namespace Nicxlive.UnityBackend.Compat
{
    using Nicxlive.UnityBackend.Managed;
    using Nicxlive.UnityBackend.Interop;

    public enum RenderCommandKind { DrawPart, BeginDynamicComposite, EndDynamicComposite, BeginMask, ApplyMask, BeginMaskContent, EndMask }
    public enum RenderPassKind { Root, DynamicComposite }
    public enum MaskDrawableKind { Part, Mask }
    public enum BlendMode
    {
        Normal, Multiply, Screen, Overlay, Darken, Lighten, ColorDodge, LinearDodge, AddGlow, ColorBurn, HardLight, SoftLight, Difference, Exclusion, Subtract, Inverse, DestinationIn, ClipToLower, SliceFromLower
    }

    public struct ShaderProgramHandle { public int Program; }
    public struct GLShaderHandle { public ulong Handle; }
    public struct GLTextureHandle { public ulong Handle; }
    public struct ShaderStageSource { public string Vertex; public string Fragment; }
    public struct ShaderAsset { public ShaderStageSource Stage; }
    public struct PartDrawPacket { }
    public struct MaskDrawPacket { }
    public struct MaskApplyPacket { }
    public struct DynamicCompositeSurface { }
    public struct DynamicCompositePass { }
    public struct OpenGLBackendInit { public int Width; public int Height; }
    public struct DirectXBackendInit { public int Width; public int Height; }
    public struct Vertex { public float X; public float Y; public float U; public float V; }
    public struct Vec2f { public float X; public float Y; }
    public struct Vec3f { public float X; public float Y; public float Z; }
    public struct Vec4f { public float X; public float Y; public float Z; public float W; }
    public struct DrawSpan { public int FirstIndex; public int IndexCount; }
    public struct CompositeState { public bool Valid; }
    public struct VSInput { public Vector4 Position; }
    public struct VSOutput { public Vector4 Position; }
    public enum StencilMode { None, Write, TestEqual }
    public struct DirectXRuntimeOptions { public bool SkipDraw; public bool SkipPresent; }
    public struct PostProcessingShader { public Material Material; }
    public struct NSOpenGLCPSurfaceOpacity { public int Value; }

    public sealed class Texture
    {
        public int WidthValue;
        public int HeightValue;
        public int Channels;
        public bool Stencil;
        public bool RenderTarget;
        public ulong BackendHandleValue;
        private byte[] _data = Array.Empty<byte>();
        private uint _boundUnit;
        private bool _hasMipmap;

        public int width() => WidthValue;
        public int height() => HeightValue;
        public int maxAnisotropy() => 16;
        public int channelFormat() => Channels;
        public void setFiltering(Filtering filtering)
        {
            BackendRegistry.currentRenderBackend().applyTextureFiltering(BackendHandleValue, filtering);
        }
        public void setWrapping(Wrapping wrapping)
        {
            BackendRegistry.currentRenderBackend().applyTextureWrapping(BackendHandleValue, wrapping);
        }
        public void setAnisotropy(float value)
        {
            var backend = BackendRegistry.currentRenderBackend();
            backend.applyTextureAnisotropy(BackendHandleValue, Mathf.Clamp(value, 1f, backend.maxTextureAnisotropy()));
        }
        public void setData(byte[] data)
        {
            _data = data != null ? (byte[])data.Clone() : Array.Empty<byte>();
            if (_data.Length == 0)
            {
                return;
            }
            BackendRegistry.currentRenderBackend().uploadTextureData(BackendHandleValue, WidthValue, HeightValue, Channels, _data);
            genMipmap();
        }
        public void genMipmap()
        {
            _hasMipmap = true;
            BackendRegistry.currentRenderBackend().generateTextureMipmap(BackendHandleValue);
        }
        public void bind(uint unit)
        {
            _boundUnit = unit;
            BackendRegistry.currentRenderBackend().bindTextureHandle(BackendHandleValue, unit);
        }
        public ulong backendHandle() => BackendHandleValue;
        public byte[] getTextureData()
        {
            var data = BackendRegistry.currentRenderBackend().readTextureData(BackendHandleValue, Channels, Stencil);
            return BackendRegistry.unPremultiplyRgba(data);
        }
        public ulong getTextureId()
        {
            var backend = BackendRegistry.tryRenderBackend();
            return backend != null ? backend.textureNativeHandle(BackendHandleValue) : 0;
        }
        public void dispose()
        {
            var backend = BackendRegistry.tryRenderBackend();
            if (backend != null)
            {
                backend.destroyTextureHandle(BackendHandleValue);
            }
        }
        public bool opEquals(Texture rhs) => ReferenceEquals(this, rhs);
    }

    public sealed class Shader
    {
        private readonly Material? _material;
        public ulong Handle;
        public Shader(ShaderAsset source)
        {
            _ = source;
            _material = new Material(UnityEngine.Shader.Find("Unlit/Texture"));
            Handle = BackendRegistry.currentRenderBackend().createShader(UnityEngine.Shader.Find("Unlit/Texture")).Handle;
        }
        public static Shader fromOpenGLSource(string vertex, string fragment) => new Shader(new ShaderAsset { Stage = new ShaderStageSource { Vertex = vertex, Fragment = fragment } });
        public void use()
        {
            BackendRegistry.currentRenderBackend().useShader(new GLShaderHandle { Handle = Handle });
            _material?.SetPass(0);
        }
        public int getUniformLocation(string name)
        {
            return BackendRegistry.currentRenderBackend().getUniformLocation(new GLShaderHandle { Handle = Handle }, name);
        }
        public bool hasUniform(string name) => getUniformLocation(name) >= 0;
        public int getUniform(string name) => getUniformLocation(name);
        public void setUniform(int location, bool value)
        {
            BackendRegistry.currentRenderBackend().setShaderUniform(new GLShaderHandle { Handle = Handle }, location, value);
            _material?.SetFloat(location, value ? 1f : 0f);
        }
        public void setUniform(int location, int value)
        {
            BackendRegistry.currentRenderBackend().setShaderUniform(new GLShaderHandle { Handle = Handle }, location, value);
            _material?.SetInt(location, value);
        }
        public void setUniform(int location, float value)
        {
            BackendRegistry.currentRenderBackend().setShaderUniform(new GLShaderHandle { Handle = Handle }, location, value);
            _material?.SetFloat(location, value);
        }
        public void setUniform(int location, Vector2 value)
        {
            BackendRegistry.currentRenderBackend().setShaderUniform(new GLShaderHandle { Handle = Handle }, location, value);
            _material?.SetVector(location, value);
        }
        public void setUniform(int location, Vector3 value)
        {
            BackendRegistry.currentRenderBackend().setShaderUniform(new GLShaderHandle { Handle = Handle }, location, value);
            _material?.SetVector(location, value);
        }
        public void setUniform(int location, Vector4 value)
        {
            BackendRegistry.currentRenderBackend().setShaderUniform(new GLShaderHandle { Handle = Handle }, location, value);
            _material?.SetVector(location, value);
        }
        public void setUniform(int location, Matrix4x4 value)
        {
            BackendRegistry.currentRenderBackend().setShaderUniform(new GLShaderHandle { Handle = Handle }, location, value);
            _material?.SetMatrix(location, value);
        }
        public void destroyShader()
        {
            BackendRegistry.currentRenderBackend().destroyShader(new GLShaderHandle { Handle = Handle });
            if (_material != null)
            {
                UnityEngine.Object.Destroy(_material);
            }
        }
    }

    public sealed class RenderingBackend
    {
        public readonly CommandExecutor Executor = new CommandExecutor();
        public readonly TextureRegistry Textures = new TextureRegistry();

        private readonly Dictionary<ulong, ushort[]> _ibo = new Dictionary<ulong, ushort[]>();
        private readonly HashSet<ulong> _dynamicFb = new HashSet<ulong>();
        private readonly List<DecodedCommand> _immediateCommands = new List<DecodedCommand>();
        private SharedBuffers _sharedSnapshot = new SharedBuffers();
        private CommandBuffer? _commandBuffer;
        private Camera? _boundCamera;
        private Material? _partMaterial;
        private Material? _maskMaterial;
        private readonly CommandExecutor.PropertyConfig _propertyConfig = new CommandExecutor.PropertyConfig();
        private bool _sceneOpen;
        private bool _attachedToCamera;
        private int _viewportWidth = 1280;
        private int _viewportHeight = 720;

        public void initializeRenderer() => Executor.initializeRenderer();
        public void initializePartBackendResources() => Executor.initializePartBackendResources();
        public void initializeMaskBackend() => Executor.initializeMaskBackend();
        public void bindPartShader() => Executor.bindPartShader();
        public void bindDrawableVao() => Executor.bindDrawableVao();
        public void beginScene(CommandBuffer buffer, int width, int height)
        {
            EnsurePipelineObjects();
            _commandBuffer = buffer;
            _viewportWidth = Mathf.Max(1, width);
            _viewportHeight = Mathf.Max(1, height);
            _immediateCommands.Clear();
            _sceneOpen = true;
            Executor.setViewport(_viewportWidth, _viewportHeight);
            if (EnsureRuntimeReady())
            {
                Executor.beginScene(_commandBuffer!, _viewportWidth, _viewportHeight);
            }
        }

        public void endScene()
        {
            if (!_sceneOpen)
            {
                return;
            }
            if (EnsureRuntimeReady())
            {
                Executor.endScene(_commandBuffer!);
            }
            _sceneOpen = false;
        }

        public void postProcessScene()
        {
            if (EnsureRuntimeReady())
            {
                Executor.postProcessScene(_commandBuffer!);
            }
        }

        public void presentSceneToBackbuffer(int width, int height)
        {
            _viewportWidth = Mathf.Max(1, width);
            _viewportHeight = Mathf.Max(1, height);
            if (EnsureRuntimeReady())
            {
                Executor.ensurePresentProgram();
                rebindActiveTargets();
                Executor.setViewport(_viewportWidth, _viewportHeight);
                Executor.presentSceneToBackbuffer(_commandBuffer!, _viewportWidth, _viewportHeight);
            }
        }
        public void setViewport(int width, int height) => Executor.setViewport(width, height);
        public ulong framebufferHandle() => Executor.framebufferHandle();
        public void rebindActiveTargets()
        {
            if (EnsureRuntimeReady())
            {
                Executor.rebindActiveTargets(_commandBuffer!);
            }
        }

        public void beginDynamicComposite(NicxliveNative.NjgDynamicCompositePass pass)
        {
            if (!EnsureRuntimeReady())
            {
                return;
            }
            Executor.beginDynamicComposite(_commandBuffer!, Textures, pass);
        }

        public void endDynamicComposite(NicxliveNative.NjgDynamicCompositePass pass)
        {
            if (!EnsureRuntimeReady())
            {
                return;
            }
            Executor.endDynamicComposite(_commandBuffer!, pass);
        }
        public NicxliveNative.NjgDynamicCompositePass createDynamicCompositePass(NicxliveNative.NjgDynamicCompositePass pass, Dictionary<ulong, Texture> texturesByHandle)
        {
            _ = texturesByHandle;
            var count = Mathf.Min(3, Mathf.Max(0, pass.TextureCount));
            var result = pass;
            result.TextureCount = (uint)count;
            return result;
        }
        public void beginMask(bool useStencil)
        {
            if (!EnsureRuntimeReady())
            {
                return;
            }
            Executor.beginMask(_commandBuffer!, useStencil);
        }

        public void applyMask(NicxliveNative.NjgMaskApplyPacket packet)
        {
            if (!EnsureRuntimeReady())
            {
                return;
            }
            initializeMaskBackend();
            switch (packet.Kind)
            {
                case NicxliveNative.MaskDrawableKind.Part:
                    drawPartPacket(packet.PartPacket, new Dictionary<ulong, Texture>());
                    break;
                case NicxliveNative.MaskDrawableKind.Mask:
                    executeMaskPacket(packet.MaskPacket);
                    break;
                default:
                    Executor.applyMask(_commandBuffer!, _sharedSnapshot, Textures, _partMaterial!, _maskMaterial!, _propertyConfig, packet);
                    break;
            }
        }

        public void beginMaskContent()
        {
            Executor.beginMaskContent();
        }

        public void endMask()
        {
            Executor.endMask();
        }

        public void drawPartPacket(NicxliveNative.NjgPartDrawPacket packet, Dictionary<ulong, Texture> texturesByHandle)
        {
            _ = texturesByHandle;
            if (!EnsureRuntimeReady())
            {
                return;
            }
            if (!packet.Renderable || packet.TextureCount == 0)
            {
                return;
            }

            bindDrawableVao();

            if (packet.IsMask)
            {
                renderStage(packet, false);
                return;
            }

            if (packet.UseMultistageBlend)
            {
                setupShaderStage(packet, 0, Matrix4x4.identity, Matrix4x4.identity);
                renderStage(packet, true);
                if (packet.HasEmissionOrBumpmap)
                {
                    setupShaderStage(packet, 1, Matrix4x4.identity, Matrix4x4.identity);
                    renderStage(packet, false);
                }
            }
            else
            {
                setupShaderStage(packet, 2, Matrix4x4.identity, Matrix4x4.identity);
                renderStage(packet, false);
            }
        }

        public void executeMaskPacket(NicxliveNative.NjgMaskDrawPacket packet)
        {
            if (!EnsureRuntimeReady())
            {
                return;
            }
            initializeMaskBackend();
            bindDrawableVao();
            if (_maskMaterial != null)
            {
                _maskMaterial.SetPass(0);
                _maskMaterial.SetFloat(_propertyConfig.MaskThreshold, packet.Threshold);
            }
            _ = sharedVertexBufferHandle();
            _ = sharedDeformBufferHandle();
            Executor.executeMaskPacket(_commandBuffer!, _sharedSnapshot, _maskMaterial!, _propertyConfig, packet, false, 1);
        }
        private NicxliveNative.NjgPartDrawPacket _stagedPacket;
        private int _stagedShaderStage;

        public void setupShaderStage(NicxliveNative.NjgPartDrawPacket packet, int stage, Matrix4x4 matrix, Matrix4x4 renderMatrix)
        {
            _ = (matrix, renderMatrix);
            bindPartShader();
            _partMaterial?.SetFloat(_propertyConfig.ShaderStage, stage);
            applyBlendMode((BlendMode)packet.BlendingMode, stage == 0);
            _stagedPacket = packet;
            _stagedShaderStage = stage;
            Executor.setupShaderStage(_propertyConfig, packet, stage);
        }

        public void renderStage(NicxliveNative.NjgPartDrawPacket packet, bool advanced)
        {
            if (!EnsureRuntimeReady())
            {
                return;
            }
            if (packet.VertexCount == 0 && _stagedPacket.VertexCount > 0)
            {
                packet = _stagedPacket;
            }
            Executor.renderStage(_commandBuffer!, _sharedSnapshot, Textures, _partMaterial!, _propertyConfig, packet, advanced);
            _ = _stagedShaderStage;
        }
        public void setLegacyBlendMode(BlendMode mode)
        {
            Executor.setLegacyBlendMode(_propertyConfig, (NicxliveNative.BlendMode)mode);
        }

        public void setAdvancedBlendEquation(BlendMode mode)
        {
            setLegacyBlendMode(mode);
        }

        public void applyBlendMode(BlendMode mode, bool legacyOnly = false)
        {
            Executor.applyBlendMode(_propertyConfig, (NicxliveNative.BlendMode)mode, legacyOnly);
        }

        public void blendModeBarrier(BlendMode mode)
        {
            if (supportsAdvancedBlend() && !supportsAdvancedBlendCoherent() && isAdvancedBlendMode(mode))
            {
                issueBlendBarrier();
            }
        }

        public void issueBlendBarrier()
        {
            // Fallback backend: no explicit advanced blend barrier.
        }

        public bool supportsAdvancedBlend() => false;
        public bool supportsAdvancedBlendCoherent() => false;
        public bool isAdvancedBlendMode(BlendMode mode) => mode is BlendMode.Multiply or BlendMode.Screen or BlendMode.Overlay or BlendMode.Darken or BlendMode.Lighten or BlendMode.ColorDodge or BlendMode.ColorBurn or BlendMode.HardLight or BlendMode.SoftLight or BlendMode.Difference or BlendMode.Exclusion;
        public int maxTextureAnisotropy() => 16;
        public int sharedVertexBufferHandle() => 1;
        public int sharedUvBufferHandle() => 2;
        public int sharedDeformBufferHandle() => 3;
        public ulong textureId(Texture texture) => texture?.backendHandle() ?? 0;
        public bool dynamicFramebufferKeyUsesHandle(ulong handle) => _dynamicFb.Contains(handle);
        public ulong acquireDynamicFramebuffer(ulong t0, ulong t1, ulong t2, ulong stencil)
        {
            var k = makeDynamicFramebufferKey(t0, t1, t2, stencil);
            _dynamicFb.Add(k);
            return k;
        }
        private void glDeleteFramebuffers(ulong fb) { _ = fb; }
        public void releaseDynamicFramebuffersForTextureHandle(ulong handle)
        {
            if (dynamicFramebufferKeyUsesHandle(handle))
            {
                glDeleteFramebuffers(handle);
            }
            _dynamicFb.RemoveWhere(x => x == handle);
        }
        public ulong makeDynamicFramebufferKey(ulong t0, ulong t1, ulong t2, ulong stencil) => toHash(t0, t1, t2, stencil);
        public static ulong toHash(params ulong[] values)
        {
            unchecked
            {
                ulong h = 1469598103934665603UL;
                foreach (var v in values)
                {
                    h ^= v;
                    h *= 1099511628211UL;
                }
                return h;
            }
        }
        public ulong quickIndexSignature(ushort[] indices)
        {
            if (indices == null || indices.Length == 0) return 0;
            unchecked
            {
                ulong h = 2166136261;
                for (int i = 0; i < indices.Length; i += Math.Max(1, indices.Length / 16))
                {
                    h ^= indices[i];
                    h *= 16777619;
                }
                h ^= (ulong)indices.Length;
                h *= 16777619;
                return h;
            }
        }
        public ulong getOrCreateIbo(ushort[] indices)
        {
            var sig = quickIndexSignature(indices);
            if (!_ibo.ContainsKey(sig))
            {
                createDrawableBuffers();
                uploadDrawableIndices(sig, indices);
            }
            return sig;
        }
        public ulong getOrCreateIboByHandle(ulong handle, ushort[] indices)
        {
            if (!_ibo.ContainsKey(handle))
            {
                createDrawableBuffers();
                uploadDrawableIndices(handle, indices);
            }
            return handle;
        }
        public void uploadDrawableIndices(ulong ibo, ushort[] indices) => _ibo[ibo] = (ushort[])indices.Clone();
        public void createDrawableBuffers() => Executor.createDrawableBuffers();
        public void drawDrawableElements(ulong ibo, nuint indexCount)
        {
            if (!_ibo.TryGetValue(ibo, out var indices) || indices.Length == 0 || indexCount == 0)
            {
                return;
            }
        }
        public void ensureDebugRendererInitialized() => Executor.initializeRenderer();
        public ulong currentRtvHandle() => framebufferHandle();
        public ulong offscreenRtvHandle() => framebufferHandle();
        public ulong offscreenRtvHandleAt(int index) { _ = index; return 0; }
        public ulong dsvHandle() => framebufferHandle();
        public ulong offscreenDsvHandle() => framebufferHandle();
        public ulong descriptorHeapCpuStart() => framebufferHandle();
        public ulong descriptorHeapGpuStart() => framebufferHandle();
        public void enqueueSpan(DrawSpan span) { _ = span; }
        public void drawMaskPacket(NicxliveNative.NjgMaskDrawPacket packet) => executeMaskPacket(packet);
        public void drawUploadedGeometry(List<DrawSpan> spans, Texture fallbackTexture)
        {
            _ = fallbackTexture;
            if (spans == null)
            {
                return;
            }
            foreach (var span in spans)
            {
                _ = span;
            }
        }
        public void renderScene(Vector4 area, PostProcessingShader shaderToUse, ulong albedo, ulong emissive, ulong bump)
        {
            _ = (area, albedo, emissive, bump);
            if (shaderToUse.Material == null)
            {
                return;
            }
            ExecuteQueuedCommands(_immediateCommands);
            _immediateCommands.Clear();
        }
        public void applyBlendingCapabilities(bool advancedEnabled)
        {
            _ = advancedEnabled;
            var coherent = supportsAdvancedBlend() && supportsAdvancedBlendCoherent();
            setAdvancedBlendCoherent(coherent);
        }
        public void setAdvancedBlendCoherent(bool enabled) => Executor.setAdvancedBlendCoherent(enabled);

        public void setSharedSnapshot(NicxliveNative.SharedBufferSnapshot snapshot)
        {
            _sharedSnapshot = SharedBufferUploader.CopyFromNative(snapshot);
        }

        public void renderCommands(NicxliveNative.SharedBufferSnapshot snapshot, NicxliveNative.CommandQueueView view, int width, int height)
        {
            _viewportWidth = Mathf.Max(1, width);
            _viewportHeight = Mathf.Max(1, height);
            setSharedSnapshot(snapshot);
            var decoded = CommandStream.Decode(view);
            ExecuteQueuedCommands(decoded);
        }

        public ulong createTextureHandle() => Textures.createTextureHandle();
        public void destroyTextureHandle(ulong texture) => Textures.destroyTextureHandle(texture);
        public void bindTextureHandle(ulong texture, uint unit) => Textures.bindTextureHandle(texture, (int)unit);
        public void uploadTextureData(ulong texture, int width, int height, int channels, byte[] data) => Textures.uploadTextureData(texture, width, height, channels, data);
        public void generateTextureMipmap(ulong texture) => Textures.generateTextureMipmap(texture);
        public void applyTextureFiltering(ulong texture, Filtering filtering, bool useMipmaps = true) => Textures.applyTextureFiltering(texture, filtering, useMipmaps);
        public void applyTextureWrapping(ulong texture, Wrapping wrapping) => Textures.applyTextureWrapping(texture, wrapping);
        public void applyTextureAnisotropy(ulong texture, float value) => Textures.applyTextureAnisotropy(texture, value);
        public byte[] readTextureData(ulong texture, int channels, bool stencil) => Textures.readTextureData(texture, channels, stencil);
        public ulong textureNativeHandle(ulong texture) => Textures.textureNativeHandle(texture);

        public GLShaderHandle createShader(string vertexSource, string fragmentSource)
        {
            var shader = UnityEngine.Shader.Find("Unlit/Texture");
            var h = Textures.createShader(shader);
            return new GLShaderHandle { Handle = h };
        }
        public void useShader(GLShaderHandle shader) { _ = Textures.useShader(shader.Handle); }
        public void destroyShader(GLShaderHandle shader) => Textures.destroyShader(shader.Handle);
        public int glGetUniformLocation(GLShaderHandle shader, string name) => Textures.getShaderUniformLocation(shader.Handle, name);
        public int getUniformLocation(GLShaderHandle shader, string name) => glGetUniformLocation(shader, name);
        public void setShaderUniform(GLShaderHandle shader, int location, bool value) => Textures.setShaderUniform(shader.Handle, location, value);
        public void setShaderUniform(GLShaderHandle shader, int location, int value) => Textures.setShaderUniform(shader.Handle, location, value);
        public void setShaderUniform(GLShaderHandle shader, int location, float value) => Textures.setShaderUniform(shader.Handle, location, value);
        public void setShaderUniform(GLShaderHandle shader, int location, Vector2 value) => Textures.setShaderUniform(shader.Handle, location, value);
        public void setShaderUniform(GLShaderHandle shader, int location, Vector3 value) => Textures.setShaderUniform(shader.Handle, location, value);
        public void setShaderUniform(GLShaderHandle shader, int location, Vector4 value) => Textures.setShaderUniform(shader.Handle, location, value);
        public void setShaderUniform(GLShaderHandle shader, int location, Matrix4x4 value) => Textures.setShaderUniform(shader.Handle, location, value);

        private void EnsurePipelineObjects()
        {
            if (_boundCamera == null)
            {
                _boundCamera = Camera.main;
            }
            if (_commandBuffer == null)
            {
                _commandBuffer = new CommandBuffer { name = "nicxlive_compat_backend" };
            }
            if (_partMaterial == null)
            {
                _partMaterial = new Material(UnityEngine.Shader.Find("Unlit/Texture"));
            }
            if (_maskMaterial == null)
            {
                _maskMaterial = new Material(UnityEngine.Shader.Find("Unlit/Color"));
            }
            if (_boundCamera != null && !_attachedToCamera)
            {
                _boundCamera.AddCommandBuffer(CameraEvent.BeforeImageEffects, _commandBuffer);
                _attachedToCamera = true;
            }
        }

        private void ExecuteQueuedCommands(List<DecodedCommand> commands)
        {
            EnsurePipelineObjects();
            if (_boundCamera == null || _commandBuffer == null || _partMaterial == null || _maskMaterial == null)
            {
                return;
            }
            Executor.setViewport(_viewportWidth, _viewportHeight);
            Executor.Execute(_commandBuffer, _boundCamera, commands, _sharedSnapshot, Textures, _partMaterial, _maskMaterial, _propertyConfig);
        }

        private bool EnsureRuntimeReady()
        {
            EnsurePipelineObjects();
            return _commandBuffer != null && _boundCamera != null && _partMaterial != null && _maskMaterial != null;
        }
    }

    public static class BackendRegistry
    {
        private static RenderingBackend? _cachedRenderBackend;
        public static void ensureRenderBackend() { _cachedRenderBackend ??= new RenderingBackend(); }
        public static void inSetRenderBackend(RenderingBackend backend) { _cachedRenderBackend = backend; }
        public static RenderingBackend? tryRenderBackend() { ensureRenderBackend(); return _cachedRenderBackend; }
        public static void enforce(bool condition, string message)
        {
            if (!condition)
            {
                throw new InvalidOperationException(message);
            }
        }
        public static RenderingBackend requireRenderBackend()
        {
            var backend = tryRenderBackend();
            enforce(backend != null, "Render backend is required.");
            return backend!;
        }
        public static RenderingBackend currentRenderBackend() => requireRenderBackend();
        public static string sdlError(string message) => $"SDL error: {message}";
        public static byte[] unPremultiplyRgba(byte[] rgba)
        {
            if (rgba == null || rgba.Length == 0)
            {
                return Array.Empty<byte>();
            }
            var outData = new byte[rgba.Length];
            Array.Copy(rgba, outData, rgba.Length);
            for (var i = 0; i + 3 < outData.Length; i += 4)
            {
                var a = outData[i + 3];
                if (a == 0)
                {
                    outData[i] = 0;
                    outData[i + 1] = 0;
                    outData[i + 2] = 0;
                    continue;
                }
                var scale = 255.0f / a;
                outData[i] = (byte)Mathf.Clamp(Mathf.RoundToInt(outData[i] * scale), 0, 255);
                outData[i + 1] = (byte)Mathf.Clamp(Mathf.RoundToInt(outData[i + 1] * scale), 0, 255);
                outData[i + 2] = (byte)Mathf.Clamp(Mathf.RoundToInt(outData[i + 2] * scale), 0, 255);
            }
            return outData;
        }
        public static void glGetShaderiv(int shader) { _ = shader; }
        public static void glGetShaderInfoLog(int shader) { _ = shader; }
        public static void checkShader(int shader)
        {
            glGetShaderiv(shader);
            glGetShaderiv(shader);
            glGetShaderInfoLog(shader);
        }
        public static void glDeleteShader(int shader) { _ = shader; }
        public static void glGetProgramiv(int program) { _ = program; }
        public static void glGetProgramInfoLog(int program) { _ = program; }
        public static void checkProgram(int program)
        {
            glGetProgramiv(program);
            glGetProgramiv(program);
            glGetProgramInfoLog(program);
        }
        public static IntPtr dlopen(string path) { _ = path; return IntPtr.Zero; }
        public static IntPtr dlsym(IntPtr handle, string symbol) { _ = (handle, symbol); return IntPtr.Zero; }
        public static void configureMacOpenGLSurfaceOpacity(IntPtr glContext)
        {
            var h = dlopen("AppKit");
            _ = dlsym(h, "NSOpenGLCPSurfaceOpacity");
            _ = dlsym(h, "setValues");
            _ = glContext;
        }
        public static ShaderAsset shaderAsset(string vertexPath, string fragmentPath) => new ShaderAsset { Stage = new ShaderStageSource { Vertex = vertexPath, Fragment = fragmentPath } };
        public static bool loadSDL() => true;
        public static int SDL_Init() => 0;
        public static int SDL_GL_SetAttribute(int key, int value) { _ = (key, value); return 0; }
        public static OpenGLBackendInit initOpenGLBackend(int width, int height, bool isTest)
        {
            _ = isTest;
            var loaded = loadSDL();
            enforce(loaded, "loadSDL failed");
            enforce(SDL_Init() == 0, sdlError("SDL_Init"));
            _ = SDL_GL_SetAttribute(0, 0);
            _ = SDL_GL_SetAttribute(1, 0);
            inSetRenderBackend(new RenderingBackend());
            currentRenderBackend().setViewport(width, height);
            return new OpenGLBackendInit { Width = width, Height = height };
        }
        public static void renderCommands(OpenGLBackendInit gl, NicxliveNative.SharedBufferSnapshot snapshot, NicxliveNative.CommandQueueView view)
        {
            var backend = requireRenderBackend();
            backend.renderCommands(snapshot, view, gl.Width, gl.Height);
        }
        public static OpenGLBackendInit OpenGLBackendInit(int width, int height, bool isTest) => initOpenGLBackend(width, height, isTest);

        // DirectX compatibility surface
        public static bool dxSucceeded(int hr) => hr >= 0;
        public static bool dxFailed(int hr) => hr < 0;
        public static bool isDxTraceEnabled() => false;
        public static void dxTrace(string message)
        {
            if (!isDxTraceEnabled())
            {
                return;
            }
            Console.WriteLine(message);
            Console.Out.Flush();
        }
        public static void enforceHr(int hr, string context)
        {
            enforce(dxSucceeded(hr), $"{context}: hr={hr}");
        }
        public static bool isDeviceLossHr(int hr) => hr == unchecked((int)0x887A0005) || hr == unchecked((int)0x887A0006);
        public static ulong alignUp(ulong value, ulong alignment) => alignment == 0 ? value : ((value + alignment - 1) / alignment) * alignment;
        public static int version() => 1;
        public static DirectXBackendInit initDirectXBackend(int width, int height, bool isTest)
        {
            _ = isTest;
            dxTrace("initDirectXBackend");
            enforce(loadSDL(), "loadSDL failed");
            _ = version();
            enforce(SDL_Init() == 0, sdlError("SDL_Init"));
            return new DirectXBackendInit { Width = width, Height = height };
        }
        public static DirectXBackendInit DirectXBackendInit(int width, int height, bool isTest) => initDirectXBackend(width, height, isTest);
        public static void renderCommands(DirectXBackendInit dx, NicxliveNative.SharedBufferSnapshot snapshot, NicxliveNative.CommandQueueView view)
        {
            var backend = requireRenderBackend();
            backend.renderCommands(snapshot, view, dx.Width, dx.Height);
        }
        public static void shutdownDirectXBackend(ref DirectXBackendInit dx)
        {
            _cachedRenderBackend = null;
            dx = default;
        }
        public static string fromStringz(IntPtr ptr) => ptr == IntPtr.Zero ? string.Empty : Marshal.PtrToStringAnsi(ptr) ?? string.Empty;
        public static (int width, int height) queryWindowPixelSize(IntPtr hwnd) { _ = hwnd; return (0, 0); }
        public static void queryWindowPixelSize(IntPtr hwnd, out int width, out int height)
        {
            _ = hwnd;
            width = 0;
            height = 0;
        }
        public static string sdlError() => "sdl error";
        public static IntPtr requireWindowHandle() => IntPtr.Zero;
        public static CompositeState defaultCompositeState() => new CompositeState { Valid = true };
        public static Vector4 float4(float x, float y, float z, float w) => new Vector4(x, y, z, w);
        public static Matrix4x4 mulMat4(Matrix4x4 a, Matrix4x4 b) => a * b;
        public static Vector4 mulMat4Vec4(Matrix4x4 m, float x, float y, float z, float w) => m * new Vector4(x, y, z, w);
        public static bool rangeInBounds(ulong offset, ulong count, ulong length) => offset <= length && count <= (length - offset);
        public static Vector4 screenBlend(Vector4 src, Vector4 dst) => src + dst - Vector4.Scale(src, dst);
        public static Vector4 sampleTex(Texture2D tex, Vector2 uv) { _ = uv; return tex != null ? Vector4.one : Vector4.zero; }
        public static Vector4 psMain(Vector4 input) => input;
        public static Vector4 vsMain(Vector4 input) => input;
        public static int sanitizeBlendMode(int mode) => Mathf.Clamp(mode, 0, 18);
        public static object buildBlendDesc(int mode) => sanitizeBlendMode(mode);
        public static void setDirectXRuntimeOptions(DirectXRuntimeOptions opts) { _ = opts; }

        public static IntPtr WinD3D12CreateDevice() => IntPtr.Zero;
        public static IntPtr WinD3D12GetDebugInterface() => IntPtr.Zero;
        public static IntPtr WinD3D12SerializeRootSignature() => IntPtr.Zero;
        public static IntPtr D3D12_ROOT_DESCRIPTOR_TABLE() => IntPtr.Zero;
    }

    public sealed class DirectXRuntime
    {
        private readonly RenderingBackend _backend = BackendRegistry.requireRenderBackend();
        private NicxliveNative.SharedBufferSnapshot _snapshot;
        private NicxliveNative.CommandQueueView _pendingView;
        private bool _hasPendingView;
        private int _width = 1280;
        private int _height = 720;
        private bool _deviceResetRequested;
        private readonly Dictionary<ulong, int> _resourceStates = new Dictionary<ulong, int>();
        private bool _srvResourcesCreated;
        private byte[] _stagingUpload = Array.Empty<byte>();

        public bool consumeDeviceResetFlag()
        {
            var v = _deviceResetRequested;
            _deviceResetRequested = false;
            return v;
        }

        public void createDepthStencilTarget(int width, int height)
        {
            _width = Mathf.Max(1, width);
            _height = Mathf.Max(1, height);
            _backend.setViewport(_width, _height);
        }

        public void createPipelineState() => _backend.initializePartBackendResources();
        public void createRenderTargets(int width, int height)
        {
            releaseRenderTargets();
            createDepthStencilTarget(width, height);
        }
        public void createSrvResources()
        {
            _srvResourcesCreated = true;
            _ = _backend.descriptorHeapCpuStart();
            _ = _backend.descriptorHeapGpuStart();
        }
        public void createSwapChainAndTargets(IntPtr window, int width, int height)
        {
            _ = window;
            createDepthStencilTarget(width, height);
        }

        public void ensureOffscreenDepthStencilTarget(int width, int height) => createDepthStencilTarget(width, height);
        public void ensureTextureUploaded(ulong textureHandle) => _ = _backend.Textures.TryGet(textureHandle);
        public void ensureUploadBuffer(ulong byteSize) => _ = byteSize;
        public void initialize() => _backend.initializeRenderer();
        public void invalidateGpuObjects() => _deviceResetRequested = true;
        public void recoverFromDeviceLoss() => _deviceResetRequested = true;
        public void releaseDxResource()
        {
            _resourceStates.Clear();
            _stagingUpload = Array.Empty<byte>();
        }
        public void releaseRenderTargets()
        {
            _resourceStates.Clear();
        }
        public void resizeSwapChain(int width, int height)
        {
            releaseRenderTargets();
            createDepthStencilTarget(width, height);
        }
        public void transitionResource(ulong handle, int oldState, int newState)
        {
            if (handle == 0)
            {
                return;
            }
            if (_resourceStates.TryGetValue(handle, out var current) && current != oldState)
            {
                _deviceResetRequested = true;
            }
            _resourceStates[handle] = newState;
        }
        public void transitionTextureState(ulong handle, int oldState, int newState) => transitionResource(handle, oldState, newState);
        public void updateHeapSrvTexture2D(int index, ulong textureHandle)
        {
            if (!_srvResourcesCreated)
            {
                createSrvResources();
            }
            _ = index;
            _resourceStates[textureHandle] = _resourceStates.TryGetValue(textureHandle, out var state) ? state : 0;
        }
        public void uploadData(IntPtr dst, byte[] src)
        {
            _ = dst;
            ensureUploadBuffer((ulong)(src?.Length ?? 0));
            _stagingUpload = src != null ? (byte[])src.Clone() : Array.Empty<byte>();
        }
        public void uploadGeometry(List<Vertex> vertices, List<ushort> indices)
        {
            if (indices == null || indices.Count == 0)
            {
                return;
            }
            var arr = new ushort[indices.Count];
            indices.CopyTo(arr);
            _backend.getOrCreateIbo(arr);
            uploadData(IntPtr.Zero, new byte[arr.Length * sizeof(ushort)]);
            _ = vertices;
        }

        public void waitForGpu()
        {
            System.Threading.Thread.MemoryBarrier();
        }

        public void beginFrame(int width, int height)
        {
            initialize();
            _width = Mathf.Max(1, width);
            _height = Mathf.Max(1, height);
            if (BackendRegistry.isDxTraceEnabled())
            {
                BackendRegistry.dxTrace("beginFrame");
            }
            if (!_hasPendingView)
            {
                shutdown();
            }
        }

        public void dispose()
        {
            invalidateGpuObjects();
            shutdown();
        }

        public void shutdown()
        {
            _hasPendingView = false;
            _pendingView = default;
            _snapshot = default;
        }

        public void endFrame()
        {
            if (!_hasPendingView)
            {
                return;
            }
            _backend.renderCommands(_snapshot, _pendingView, _width, _height);
            _hasPendingView = false;
        }

        public void setSharedSnapshot(NicxliveNative.SharedBufferSnapshot snapshot)
        {
            _snapshot = snapshot;
            _backend.setSharedSnapshot(snapshot);
        }

        public void setPendingCommands(NicxliveNative.CommandQueueView view)
        {
            _pendingView = view;
            _hasPendingView = true;
        }
    }
}



namespace Nicxlive.UnityBackend.Managed
{
    public sealed class NicxliveRenderer : IDisposable
    {
        private IntPtr _renderer;
        private IntPtr _puppet;
        private bool _disposed;

        private readonly TextureRegistry _textureRegistry;
        private readonly NicxliveNative.CreateTextureDelegate _createTexture;
        private readonly NicxliveNative.UpdateTextureDelegate _updateTexture;
        private readonly NicxliveNative.ReleaseTextureDelegate _releaseTexture;

        public NicxliveRenderer(int viewportWidth, int viewportHeight, TextureRegistry textureRegistry)
        {
            AbiValidator.ValidateOrThrow();
            _textureRegistry = textureRegistry;

            _createTexture = CreateTexture;
            _updateTexture = UpdateTexture;
            _releaseTexture = ReleaseTexture;

            var callbacks = new NicxliveNative.UnityResourceCallbacks
            {
                UserData = IntPtr.Zero,
                CreateTexture = Marshal.GetFunctionPointerForDelegate(_createTexture),
                UpdateTexture = Marshal.GetFunctionPointerForDelegate(_updateTexture),
                ReleaseTexture = Marshal.GetFunctionPointerForDelegate(_releaseTexture),
            };

            var cfg = new NicxliveNative.UnityRendererConfig
            {
                ViewportWidth = viewportWidth,
                ViewportHeight = viewportHeight,
            };

            var result = NicxliveNative.CreateRenderer(ref cfg, ref callbacks, out _renderer);
            if (result != NicxliveNative.NjgResult.Ok || _renderer == IntPtr.Zero)
            {
                throw new InvalidOperationException($"njgCreateRenderer failed: {result}");
            }
        }

        public TextureRegistry TextureRegistry => _textureRegistry;

        public void LoadPuppet(string puppetPath)
        {
            var result = NicxliveNative.LoadPuppet(_renderer, puppetPath, out _puppet);
            if (result != NicxliveNative.NjgResult.Ok || _puppet == IntPtr.Zero)
            {
                throw new InvalidOperationException($"njgLoadPuppet failed: {result}, path={puppetPath}");
            }
        }

        public void BeginFrame(int width, int height)
        {
            var cfg = new NicxliveNative.FrameConfig
            {
                ViewportWidth = width,
                ViewportHeight = height,
            };

            var result = NicxliveNative.BeginFrame(_renderer, ref cfg);
            if (result != NicxliveNative.NjgResult.Ok)
            {
                throw new InvalidOperationException($"njgBeginFrame failed: {result}");
            }
        }

        public void Tick(double deltaSeconds)
        {
            if (_puppet == IntPtr.Zero)
            {
                return;
            }

            var result = NicxliveNative.TickPuppet(_puppet, deltaSeconds);
            if (result != NicxliveNative.NjgResult.Ok)
            {
                throw new InvalidOperationException($"njgTickPuppet failed: {result}");
            }
        }

        public NicxliveNative.CommandQueueView EmitCommands()
        {
            var result = NicxliveNative.EmitCommands(_renderer, out var view);
            if (result != NicxliveNative.NjgResult.Ok)
            {
                throw new InvalidOperationException($"njgEmitCommands failed: {result}");
            }
            return view;
        }

        public SharedBuffers GetSharedBuffers()
        {
            var result = NicxliveNative.GetSharedBuffers(_renderer, out var snapshot);
            if (result != NicxliveNative.NjgResult.Ok)
            {
                throw new InvalidOperationException($"njgGetSharedBuffers failed: {result}");
            }
            return SharedBufferUploader.CopyFromNative(snapshot);
        }

        public void Dispose()
        {
            if (_disposed)
            {
                return;
            }

            _disposed = true;

            if (_puppet != IntPtr.Zero && _renderer != IntPtr.Zero)
            {
                NicxliveNative.UnloadPuppet(_renderer, _puppet);
                _puppet = IntPtr.Zero;
            }

            if (_renderer != IntPtr.Zero)
            {
                NicxliveNative.DestroyRenderer(_renderer);
                _renderer = IntPtr.Zero;
            }
        }

        private nuint CreateTexture(int width, int height, int channels, int mipLevels, int format, bool renderTarget, bool stencil, IntPtr userData)
        {
            var handle = _textureRegistry.CreateTexture(width, height, channels, renderTarget, stencil);
            return (nuint)handle;
        }

        private void UpdateTexture(nuint handle, IntPtr data, nuint dataLen, int width, int height, int channels, IntPtr userData)
        {
            _textureRegistry.UpdateTexture((ulong)handle, data, dataLen, width, height, channels);
        }

        private void ReleaseTexture(nuint handle, IntPtr userData)
        {
            _textureRegistry.ReleaseTexture((ulong)handle);
        }
    }
}



namespace Nicxlive.UnityBackend.Managed
{
    public sealed class CommandExecutor
    {
        [Serializable]
        public sealed class PropertyConfig
        {
            public string MainTex = "_MainTex";
            public string EmissiveTex = "_EmissionTex";
            public string BumpTex = "_BumpMap";
            public string Opacity = "_Opacity";
            public string MultColor = "_MultColor";
            public string ScreenColor = "_ScreenColor";
            public string EmissionStrength = "_EmissionStrength";
            public string MaskThreshold = "_MaskThreshold";
            public string BlendMode = "_BlendMode";
            public string StencilComp = "_StencilComp";
            public string StencilRef = "_StencilRef";
            public string StencilPass = "_StencilPass";
            public string ColorMask = "_ColorMask";
            public string IsMaskPass = "_IsMaskPass";
            public string ShaderStage = "_ShaderStage";
            public string LegacyBlendOnly = "_LegacyBlendOnly";
            public string AdvancedBlend = "_AdvancedBlend";
        }

        private readonly Texture2D _whiteTexture;
        private readonly Stack<RenderTargetIdentifier> _renderTargetStack = new Stack<RenderTargetIdentifier>();
        private readonly Stack<Vector4> _viewportStack = new Stack<Vector4>();
        private readonly Stack<NicxliveNative.NjgDynamicCompositePass> _dynamicPassStack = new Stack<NicxliveNative.NjgDynamicCompositePass>();
        private readonly MaterialPropertyBlock _props = new MaterialPropertyBlock();

        private bool _inMaskPass;
        private bool _inMaskContent;
        private bool _maskUsesStencil;
        private bool _advancedBlending;
        private bool _advancedBlendingCoherent;
        private byte _maskContentStencilRef = 1;
        private int _dynamicCompositeDepth;
        private readonly List<Vector3> _debugVertices = new List<Vector3>();
        private readonly Dictionary<ulong, ushort[]> _sharedIndexRanges = new Dictionary<ulong, ushort[]>();
        private ulong _sharedIndexBufferHandle;
        private bool _rendererInitialized;
        private bool _partBackendInitialized;
        private bool _maskBackendInitialized;
        private bool _drawableBackendInitialized;
        private bool _drawableBound;
        private bool _partShaderBound;
        private bool _presentProgramInitialized;
        private int _viewportW = 1280;
        private int _viewportH = 720;
        private bool _resizingViewportTargets;
        private CommandBuffer? _activeBuffer;
        private readonly int _postSrcId = Shader.PropertyToID("_NicxPostSrc");
        private readonly int _postTmpAId = Shader.PropertyToID("_NicxPostTmpA");
        private readonly int _postTmpBId = Shader.PropertyToID("_NicxPostTmpB");
        private readonly int _drawBufferCountId = Shader.PropertyToID("_NicxDrawBufferCount");

        public CommandExecutor()
        {
            _whiteTexture = Texture2D.whiteTexture;
        }

        public void initializeRenderer()
        {
            _rendererInitialized = true;
            initializePartBackendResources();
            initializeMaskBackend();
            ensureDrawableBackendInitialized();
            ensurePresentProgram();
        }

        public void initializePartBackendResources()
        {
            if (_partBackendInitialized)
            {
                return;
            }
            _partBackendInitialized = true;
        }

        public void initializeMaskBackend()
        {
            ensureMaskBackendInitialized();
        }

        public void bindPartShader()
        {
            _partShaderBound = true;
        }

        public void bindDrawableVao()
        {
            _drawableBound = true;
        }

        private void ensureMaskBackendInitialized()
        {
            if (_maskBackendInitialized)
            {
                return;
            }
            _maskBackendInitialized = true;
        }

        private void ensureDrawableBackendInitialized()
        {
            if (_drawableBackendInitialized)
            {
                return;
            }
            _drawableBackendInitialized = true;
        }

        public void Execute(
            CommandBuffer buffer,
            Camera camera,
            List<DecodedCommand> commands,
            SharedBuffers shared,
            TextureRegistry textures,
            Material partMaterial,
            Material maskMaterial,
            PropertyConfig propertyConfig)
        {
            if (buffer == null || camera == null || partMaterial == null || maskMaterial == null)
            {
                return;
            }

            buffer.Clear();
            _activeBuffer = buffer;
            buffer.SetViewProjectionMatrices(Matrix4x4.identity, Matrix4x4.identity);
            buffer.SetGlobalVector("_SceneAmbientLight", Vector4.one);
            _renderTargetStack.Clear();
            _renderTargetStack.Push(BuiltinRenderTextureType.CameraTarget);
            _viewportStack.Clear();
            _dynamicPassStack.Clear();
            _inMaskPass = false;
            _inMaskContent = false;
            _maskUsesStencil = false;
            _maskContentStencilRef = 1;
            _dynamicCompositeDepth = 0;
            _advancedBlending = false;
            _advancedBlendingCoherent = false;

            if (!_rendererInitialized)
            {
                initializeRenderer();
            }
            bindDrawableVao();
            bindPartShader();

            rebindActiveTargets(buffer);
            beginScene(buffer, camera.pixelWidth, camera.pixelHeight);

            foreach (var command in commands)
            {
                switch (command.Kind)
                {
                    case NicxliveNative.NjgRenderCommandKind.DrawPart:
                        drawPartPacket(buffer, shared, textures, partMaterial, propertyConfig, command.PartPacket);
                        break;
                    case NicxliveNative.NjgRenderCommandKind.BeginMask:
                        beginMask(buffer, command.UsesStencil);
                        break;
                    case NicxliveNative.NjgRenderCommandKind.ApplyMask:
                        applyMask(buffer, shared, textures, partMaterial, maskMaterial, propertyConfig, command.MaskApplyPacket);
                        break;
                    case NicxliveNative.NjgRenderCommandKind.BeginMaskContent:
                        beginMaskContent();
                        break;
                    case NicxliveNative.NjgRenderCommandKind.EndMask:
                        endMask();
                        break;
                    case NicxliveNative.NjgRenderCommandKind.BeginDynamicComposite:
                        beginDynamicComposite(buffer, textures, command.DynamicPass);
                        break;
                    case NicxliveNative.NjgRenderCommandKind.EndDynamicComposite:
                        endDynamicComposite(buffer, command.DynamicPass);
                        break;
                }
            }

            postProcessScene(buffer);
            presentSceneToBackbuffer(buffer, camera.pixelWidth, camera.pixelHeight);
            endScene(buffer);
            buffer.SetViewProjectionMatrices(camera.worldToCameraMatrix, camera.projectionMatrix);
            _activeBuffer = null;
        }

        public void rebindActiveTargets(CommandBuffer buffer)
        {
            if (_renderTargetStack.Count == 0)
            {
                _renderTargetStack.Push(BuiltinRenderTextureType.CameraTarget);
            }
            buffer.SetRenderTarget(_renderTargetStack.Peek());
        }

        public void beginScene(CommandBuffer buffer, int width, int height)
        {
            setViewport(width, height);
            rebindActiveTargets(buffer);
            pushViewport(width, height);
            buffer.ClearRenderTarget(RTClearFlags.Color, Color.clear, 1.0f, 0);
        }

        public void endScene(CommandBuffer buffer)
        {
            popViewport();
            rebindActiveTargets(buffer);
        }

        public void postProcessScene(CommandBuffer buffer)
        {
            var current = _renderTargetStack.Count > 0 ? _renderTargetStack.Peek() : new RenderTargetIdentifier(BuiltinRenderTextureType.CameraTarget);
            buffer.GetTemporaryRT(_postTmpAId, _viewportW, _viewportH, 0, FilterMode.Bilinear, RenderTextureFormat.ARGB32);
            buffer.GetTemporaryRT(_postTmpBId, _viewportW, _viewportH, 0, FilterMode.Bilinear, RenderTextureFormat.ARGB32);
            buffer.Blit(current, _postTmpAId);
            buffer.Blit(_postTmpAId, _postTmpBId);
            buffer.Blit(_postTmpBId, current);
            buffer.ReleaseTemporaryRT(_postTmpAId);
            buffer.ReleaseTemporaryRT(_postTmpBId);
        }

        public void presentSceneToBackbuffer(CommandBuffer buffer, int width, int height)
        {
            rebindActiveTargets(buffer);
            ensurePresentProgram();
            var size = new Vector4(Mathf.Max(1, width), Mathf.Max(1, height), 0, 0);
            buffer.SetGlobalVector("_PresentFramebufferSize", size);
            var src = _renderTargetStack.Count > 0 ? _renderTargetStack.Peek() : new RenderTargetIdentifier(BuiltinRenderTextureType.CameraTarget);
            buffer.SetRenderTarget(BuiltinRenderTextureType.CameraTarget);
            buffer.Blit(src, BuiltinRenderTextureType.CameraTarget);
        }

        private void renderScene(CommandBuffer buffer, Material material, Mesh mesh, Matrix4x4 matrix)
        {
            if (buffer == null || material == null || mesh == null)
            {
                return;
            }
            buffer.DrawMesh(mesh, matrix, material, 0, 0);
        }

        private void pushViewport(int width, int height)
        {
            _viewportStack.Push(new Vector4(0, 0, Mathf.Max(1, width), Mathf.Max(1, height)));
        }

        private void popViewport()
        {
            if (_viewportStack.Count > 0)
            {
                _viewportStack.Pop();
            }
        }

        private Vector4 getViewport()
        {
            if (_viewportStack.Count == 0)
            {
                return Vector4.zero;
            }
            return _viewportStack.Peek();
        }

        public void setViewport(int width, int height)
        {
            _viewportW = Mathf.Max(1, width);
            _viewportH = Mathf.Max(1, height);
            if (!_resizingViewportTargets)
            {
                resizeViewportTargets(_viewportW, _viewportH);
            }
        }

        private void resizeViewportTargets(int width, int height)
        {
            _resizingViewportTargets = true;
            try
            {
                setViewport(width, height);
            }
            finally
            {
                _resizingViewportTargets = false;
            }
        }

        public ulong framebufferHandle()
        {
            if (_renderTargetStack.Count == 0)
            {
                return 0;
            }
            var current = _renderTargetStack.Peek();
            return (ulong)current.GetHashCode();
        }

        public void ensurePresentProgram()
        {
            if (_presentProgramInitialized)
            {
                return;
            }
            _presentProgramInitialized = true;
        }

        public void createDrawableBuffers()
        {
            ensureDrawableBackendInitialized();
            ensureSharedIndexBuffer();
        }

        public void uploadDrawableIndices(ulong ibo, ushort[] indices)
        {
            if (ibo == 0 || indices == null)
            {
                return;
            }
            _sharedIndexRanges[ibo] = (ushort[])indices.Clone();
            ensureSharedIndexBuffer();
        }

        private void ensureSharedIndexBuffer()
        {
            if (_sharedIndexBufferHandle != 0)
            {
                return;
            }
            _sharedIndexBufferHandle = 1;
        }

        public void beginMask(CommandBuffer buffer, bool usesStencil)
        {
            _inMaskPass = true;
            _inMaskContent = false;
            _maskUsesStencil = usesStencil;
            _maskContentStencilRef = 1;
            var clearStencil = usesStencil ? 0u : 1u;
            buffer.ClearRenderTarget(RTClearFlags.Stencil, Color.clear, 1.0f, clearStencil);
        }

        public void beginMaskContent()
        {
            _inMaskContent = true;
        }

        public void endMask()
        {
            _inMaskPass = false;
            _inMaskContent = false;
            _maskUsesStencil = false;
            _maskContentStencilRef = 1;
        }

        public void applyMask(
            CommandBuffer buffer,
            SharedBuffers shared,
            TextureRegistry textures,
            Material partMaterial,
            Material maskMaterial,
            PropertyConfig cfg,
            NicxliveNative.NjgMaskApplyPacket packet)
        {
            _maskContentStencilRef = packet.IsDodge ? (byte)0 : (byte)1;
            if (packet.Kind == NicxliveNative.MaskDrawableKind.Part)
            {
                drawPart(buffer, shared, textures, partMaterial, cfg, packet.PartPacket, true, _maskContentStencilRef, true);
            }
            else
            {
                executeMaskPacket(buffer, shared, maskMaterial, cfg, packet.MaskPacket, true, _maskContentStencilRef);
            }
        }

        public void beginDynamicComposite(
            CommandBuffer buffer,
            TextureRegistry textures,
            NicxliveNative.NjgDynamicCompositePass pass)
        {
            var tex0 = textures.TryGetRenderTexture((ulong)pass.Texture0);
            if (tex0 == null)
            {
                return;
            }

            _dynamicCompositeDepth++;
            _dynamicPassStack.Push(pass);

            var rt0 = new RenderTargetIdentifier(tex0);
            _renderTargetStack.Push(rt0);

            var tex1 = textures.TryGetRenderTexture((ulong)pass.Texture1);
            var tex2 = textures.TryGetRenderTexture((ulong)pass.Texture2);
            var depthStencil = textures.TryGetRenderTexture((ulong)pass.Stencil);

            if (tex1 != null && tex2 != null)
            {
                var colors = new[]
                {
                    new RenderTargetIdentifier(tex0),
                    new RenderTargetIdentifier(tex1),
                    new RenderTargetIdentifier(tex2),
                };
                var depth = depthStencil != null ? new RenderTargetIdentifier(depthStencil) : colors[0];
                buffer.SetRenderTarget(colors, depth);
            }
            else
            {
                if (depthStencil != null)
                {
                    buffer.SetRenderTarget(rt0, new RenderTargetIdentifier(depthStencil));
                }
                else
                {
                    buffer.SetRenderTarget(rt0);
                }
            }

            pushViewport(tex0.width, tex0.height);
            buffer.ClearRenderTarget(RTClearFlags.Color, Color.clear, 1.0f, 0);
        }

        public void endDynamicComposite(CommandBuffer buffer, NicxliveNative.NjgDynamicCompositePass pass)
        {
            _ = pass;
            if (_dynamicCompositeDepth > 0)
            {
                _dynamicCompositeDepth--;
            }
            if (_dynamicPassStack.Count > 0)
            {
                _dynamicPassStack.Pop();
            }
            if (_renderTargetStack.Count > 1)
            {
                _renderTargetStack.Pop();
            }
            popViewport();
            buffer.SetRenderTarget(_renderTargetStack.Peek());
        }

        public void drawPartPacket(
            CommandBuffer buffer,
            SharedBuffers shared,
            TextureRegistry textures,
            Material partMaterial,
            PropertyConfig cfg,
            NicxliveNative.NjgPartDrawPacket packet)
        {
            if (!packet.Renderable || packet.TextureCount == 0)
            {
                return;
            }

            if (packet.IsMask)
            {
                drawPart(buffer, shared, textures, partMaterial, cfg, packet, false, 1, false);
                return;
            }

            if (packet.UseMultistageBlend)
            {
                setupShaderStage(cfg, packet, 0);
                renderStage(buffer, shared, textures, partMaterial, cfg, packet, true);

                if (packet.HasEmissionOrBumpmap)
                {
                    setupShaderStage(cfg, packet, 1);
                    renderStage(buffer, shared, textures, partMaterial, cfg, packet, false);
                }
            }
            else
            {
                setupShaderStage(cfg, packet, 2);
                renderStage(buffer, shared, textures, partMaterial, cfg, packet, false);
            }
        }

        public void setupShaderStage(PropertyConfig cfg, NicxliveNative.NjgPartDrawPacket packet, int stage)
        {
            _ = setDrawBuffersSafe(stage == 0 ? 1 : (stage == 1 ? 2 : 3));
            _props.SetFloat(cfg.ShaderStage, stage);
            switch (stage)
            {
                case 0:
                    applyBlendMode(cfg, (NicxliveNative.BlendMode)packet.BlendingMode, true);
                    break;
                case 1:
                case 2:
                    applyBlendMode(cfg, (NicxliveNative.BlendMode)packet.BlendingMode, false);
                    break;
                default:
                    applyBlendMode(cfg, NicxliveNative.BlendMode.Normal, false);
                    break;
            }
        }

        public void renderStage(
            CommandBuffer buffer,
            SharedBuffers shared,
            TextureRegistry textures,
            Material partMaterial,
            PropertyConfig cfg,
            NicxliveNative.NjgPartDrawPacket packet,
            bool advanced)
        {
            drawPart(buffer, shared, textures, partMaterial, cfg, packet, false, _maskContentStencilRef, false);
            blendModeBarrier((NicxliveNative.BlendMode)packet.BlendingMode, advanced);
        }

        public int setDrawBuffersSafe(int desired)
        {
            var count = Mathf.Clamp(desired, 1, 3);
            _props.SetFloat(_drawBufferCountId, count);
            if (_activeBuffer != null)
            {
                _activeBuffer.SetGlobalFloat(_drawBufferCountId, count);
            }
            return count;
        }

        private unsafe void drawPart(
            CommandBuffer buffer,
            SharedBuffers shared,
            TextureRegistry textures,
            Material partMaterial,
            PropertyConfig cfg,
            NicxliveNative.NjgPartDrawPacket packet,
            bool forceStencilWrite,
            byte stencilRef,
            bool fromMaskApply)
        {
            if (!packet.Renderable || packet.IndexCount == 0 || packet.VertexCount == 0)
            {
                return;
            }

            var mesh = BuildPartMesh(shared, packet);
            if (mesh == null)
            {
                return;
            }

            _props.Clear();
            _props.SetFloat(cfg.Opacity, packet.Opacity);
            _props.SetFloat(cfg.EmissionStrength, packet.EmissionStrength);
            _props.SetFloat(cfg.MaskThreshold, packet.MaskThreshold);
            _props.SetVector(cfg.MultColor, new Vector4(packet.ClampedTint.X, packet.ClampedTint.Y, packet.ClampedTint.Z, 1));
            _props.SetVector(cfg.ScreenColor, new Vector4(packet.ClampedScreen.X, packet.ClampedScreen.Y, packet.ClampedScreen.Z, 1));
            _props.SetFloat(cfg.BlendMode, packet.BlendingMode);

            var t0 = textures.TryGet((ulong)packet.TextureHandle0) ?? _whiteTexture;
            var t1 = textures.TryGet((ulong)packet.TextureHandle1) ?? _whiteTexture;
            var t2 = textures.TryGet((ulong)packet.TextureHandle2) ?? _whiteTexture;
            _props.SetTexture(cfg.MainTex, t0);
            _props.SetTexture(cfg.EmissiveTex, t1);
            _props.SetTexture(cfg.BumpTex, t2);

            ConfigureStencil(cfg, forceStencilWrite, stencilRef);
            _props.SetFloat(cfg.IsMaskPass, packet.IsMask ? 1.0f : 0.0f);
            _props.SetFloat(cfg.LegacyBlendOnly, fromMaskApply ? 1.0f : 0.0f);
            _props.SetFloat(cfg.AdvancedBlend, _advancedBlending ? 1.0f : 0.0f);
            if (_inMaskPass && _inMaskContent && !forceStencilWrite)
            {
                ConfigureStencilTest(cfg, _maskContentStencilRef);
            }

            buffer.DrawMesh(mesh, Matrix4x4.identity, partMaterial, 0, 0, _props);
        }

        public unsafe void executeMaskPacket(
            CommandBuffer buffer,
            SharedBuffers shared,
            Material maskMaterial,
            PropertyConfig cfg,
            NicxliveNative.NjgMaskDrawPacket packet,
            bool forceStencilWrite,
            byte stencilRef)
        {
            if (packet.IndexCount == 0 || packet.VertexCount == 0)
            {
                return;
            }

            var mesh = BuildMaskMesh(shared, packet);
            if (mesh == null)
            {
                return;
            }

            _props.Clear();
            _props.SetTexture(cfg.MainTex, _whiteTexture);
            _props.SetFloat(cfg.IsMaskPass, 1.0f);
            ConfigureStencil(cfg, forceStencilWrite, stencilRef);
            if (_inMaskPass && _inMaskContent && !forceStencilWrite)
            {
                ConfigureStencilTest(cfg, _maskContentStencilRef);
            }

            buffer.DrawMesh(mesh, Matrix4x4.identity, maskMaterial, 0, 0, _props);
        }

        public void applyBlendMode(PropertyConfig cfg, NicxliveNative.BlendMode mode, bool legacyOnly)
        {
            if (!_advancedBlending || legacyOnly)
            {
                setLegacyBlendMode(cfg, mode);
            }
            else
            {
                setAdvancedBlendEquation(cfg, mode);
            }
        }

        public void setLegacyBlendMode(PropertyConfig cfg, NicxliveNative.BlendMode mode)
        {
            _props.SetFloat(cfg.BlendMode, (float)mode);
            _props.SetFloat(cfg.LegacyBlendOnly, 1.0f);
        }

        public void setAdvancedBlendEquation(PropertyConfig cfg, NicxliveNative.BlendMode mode)
        {
            // Unity path maps advanced mode selection to shader properties.
            _props.SetFloat(cfg.BlendMode, (float)mode);
            _props.SetFloat(cfg.LegacyBlendOnly, 0.0f);
            _props.SetFloat(cfg.AdvancedBlend, 1.0f);
        }

        private static bool isAdvancedBlendMode(NicxliveNative.BlendMode mode)
        {
            switch (mode)
            {
                case NicxliveNative.BlendMode.Multiply:
                case NicxliveNative.BlendMode.Screen:
                case NicxliveNative.BlendMode.Overlay:
                case NicxliveNative.BlendMode.Darken:
                case NicxliveNative.BlendMode.Lighten:
                case NicxliveNative.BlendMode.ColorDodge:
                case NicxliveNative.BlendMode.ColorBurn:
                case NicxliveNative.BlendMode.HardLight:
                case NicxliveNative.BlendMode.SoftLight:
                case NicxliveNative.BlendMode.Difference:
                case NicxliveNative.BlendMode.Exclusion:
                    return true;
                default:
                    return false;
            }
        }

        public void blendModeBarrier(NicxliveNative.BlendMode mode, bool advanced)
        {
            if (!advanced)
            {
                return;
            }
            if (_advancedBlending && !_advancedBlendingCoherent && isAdvancedBlendMode(mode))
            {
                issueBlendBarrier();
            }
        }

        public void issueBlendBarrier()
        {
            if (_activeBuffer != null)
            {
                _activeBuffer.SetGlobalFloat("_NicxBlendBarrier", Time.frameCount);
            }
        }

        public void applyBlendingCapabilities(bool advancedEnabled)
        {
            _advancedBlending = advancedEnabled;
        }

        public void setAdvancedBlendCoherent(bool enabled)
        {
            _advancedBlendingCoherent = enabled;
        }

        public void drawDebugPoints(CommandBuffer buffer, Vector3[] points, Material debugMaterial, float size = 1.0f)
        {
            if (buffer == null || points == null || points.Length == 0 || debugMaterial == null)
            {
                return;
            }

            _debugVertices.Clear();
            _debugVertices.AddRange(points);
            var mesh = new Mesh();
            mesh.SetVertices(_debugVertices);
            var idx = new int[_debugVertices.Count];
            for (var i = 0; i < idx.Length; i++) idx[i] = i;
            mesh.SetIndices(idx, MeshTopology.Points, 0);
            _props.Clear();
            _props.SetFloat("_DebugPointSize", size);
            buffer.DrawMesh(mesh, Matrix4x4.identity, debugMaterial, 0, 0, _props);
        }

        public void drawDebugLines(CommandBuffer buffer, Vector3[] lines, Material debugMaterial)
        {
            if (buffer == null || lines == null || lines.Length < 2 || debugMaterial == null)
            {
                return;
            }

            _debugVertices.Clear();
            _debugVertices.AddRange(lines);
            var mesh = new Mesh();
            mesh.SetVertices(_debugVertices);
            var idx = new int[_debugVertices.Count];
            for (var i = 0; i < idx.Length; i++) idx[i] = i;
            mesh.SetIndices(idx, MeshTopology.Lines, 0);
            buffer.DrawMesh(mesh, Matrix4x4.identity, debugMaterial, 0, 0);
        }

        private void ConfigureStencil(PropertyConfig cfg, bool forceStencilWrite, byte stencilRef)
        {
            if (forceStencilWrite)
            {
                _props.SetFloat(cfg.StencilRef, stencilRef);
                _props.SetFloat(cfg.StencilComp, (float)CompareFunction.Always);
                _props.SetFloat(cfg.StencilPass, (float)StencilOp.Replace);
                _props.SetFloat(cfg.ColorMask, 0.0f);
                return;
            }

            _props.SetFloat(cfg.StencilRef, 1.0f);
            _props.SetFloat(cfg.StencilComp, (float)CompareFunction.Always);
            _props.SetFloat(cfg.StencilPass, (float)StencilOp.Keep);
            _props.SetFloat(cfg.ColorMask, 15.0f);

            if (_inMaskPass && !_inMaskContent && !_maskUsesStencil)
            {
                _props.SetFloat(cfg.StencilRef, 1.0f);
                _props.SetFloat(cfg.StencilComp, (float)CompareFunction.Always);
                _props.SetFloat(cfg.StencilPass, (float)StencilOp.Keep);
            }
        }

        private void ConfigureStencilTest(PropertyConfig cfg, byte stencilRef)
        {
            _props.SetFloat(cfg.StencilRef, stencilRef);
            _props.SetFloat(cfg.StencilComp, (float)CompareFunction.Equal);
            _props.SetFloat(cfg.StencilPass, (float)StencilOp.Keep);
            _props.SetFloat(cfg.ColorMask, 15.0f);
        }

        private unsafe Mesh? BuildPartMesh(SharedBuffers shared, NicxliveNative.NjgPartDrawPacket packet)
        {
            var vertexCount = checked((int)packet.VertexCount);
            var indexCount = checked((int)packet.IndexCount);
            if (vertexCount <= 0 || indexCount <= 0 || packet.Indices == null)
            {
                return null;
            }

            if (packet.VertexAtlasStride == 0 || packet.UvAtlasStride == 0 || packet.DeformAtlasStride == 0)
            {
                return null;
            }

            var vxBase = checked((int)packet.VertexOffset);
            var vyBase = checked((int)(packet.VertexOffset + packet.VertexAtlasStride));
            var uxBase = checked((int)packet.UvOffset);
            var uyBase = checked((int)(packet.UvOffset + packet.UvAtlasStride));
            var dxBase = checked((int)packet.DeformOffset);
            var dyBase = checked((int)(packet.DeformOffset + packet.DeformAtlasStride));

            if (!RangeInBounds(vxBase, vertexCount, shared.Vertices.Length) ||
                !RangeInBounds(vyBase, vertexCount, shared.Vertices.Length) ||
                !RangeInBounds(uxBase, vertexCount, shared.Uvs.Length) ||
                !RangeInBounds(uyBase, vertexCount, shared.Uvs.Length) ||
                !RangeInBounds(dxBase, vertexCount, shared.Deform.Length) ||
                !RangeInBounds(dyBase, vertexCount, shared.Deform.Length))
            {
                return null;
            }

            var model = ToMatrix(packet.ModelMatrix);
            var render = ToMatrix(packet.RenderMatrix);
            var mvp = render * model;

            var vertices = new Vector3[vertexCount];
            var uvs = new Vector2[vertexCount];
            for (var i = 0; i < vertexCount; i++)
            {
                var px = shared.Vertices[vxBase + i] + shared.Deform[dxBase + i] - packet.Origin.X;
                var py = shared.Vertices[vyBase + i] + shared.Deform[dyBase + i] - packet.Origin.Y;
                var clip = mvp * new Vector4(px, py, 0, 1);
                var invW = Mathf.Approximately(clip.w, 0f) ? 1f : (1f / clip.w);
                var x = clip.x * invW;
                var y = clip.y * invW;
                applyCompositeTransform(ref x, ref y);
                vertices[i] = new Vector3(x, y, 0f);
                uvs[i] = new Vector2(shared.Uvs[uxBase + i], shared.Uvs[uyBase + i]);
            }

            var triangles = new int[indexCount];
            for (var i = 0; i < indexCount; i++)
            {
                var idx = packet.Indices[i];
                triangles[i] = idx < vertexCount ? idx : 0;
            }

            var mesh = new Mesh
            {
                indexFormat = vertexCount > 65535 ? IndexFormat.UInt32 : IndexFormat.UInt16
            };
            mesh.vertices = vertices;
            mesh.uv = uvs;
            mesh.triangles = triangles;
            mesh.RecalculateBounds();
            return mesh;
        }

        private unsafe Mesh? BuildMaskMesh(SharedBuffers shared, NicxliveNative.NjgMaskDrawPacket packet)
        {
            var vertexCount = checked((int)packet.VertexCount);
            var indexCount = checked((int)packet.IndexCount);
            if (vertexCount <= 0 || indexCount <= 0 || packet.Indices == null)
            {
                return null;
            }

            if (packet.VertexAtlasStride == 0 || packet.DeformAtlasStride == 0)
            {
                return null;
            }

            var vxBase = checked((int)packet.VertexOffset);
            var vyBase = checked((int)(packet.VertexOffset + packet.VertexAtlasStride));
            var dxBase = checked((int)packet.DeformOffset);
            var dyBase = checked((int)(packet.DeformOffset + packet.DeformAtlasStride));

            if (!RangeInBounds(vxBase, vertexCount, shared.Vertices.Length) ||
                !RangeInBounds(vyBase, vertexCount, shared.Vertices.Length) ||
                !RangeInBounds(dxBase, vertexCount, shared.Deform.Length) ||
                !RangeInBounds(dyBase, vertexCount, shared.Deform.Length))
            {
                return null;
            }

            var mvp = ToMatrix(packet.Mvp);
            var vertices = new Vector3[vertexCount];
            var uvs = new Vector2[vertexCount];
            for (var i = 0; i < vertexCount; i++)
            {
                var px = shared.Vertices[vxBase + i] + shared.Deform[dxBase + i] - packet.Origin.X;
                var py = shared.Vertices[vyBase + i] + shared.Deform[dyBase + i] - packet.Origin.Y;
                var clip = mvp * new Vector4(px, py, 0, 1);
                var invW = Mathf.Approximately(clip.w, 0f) ? 1f : (1f / clip.w);
                var x = clip.x * invW;
                var y = clip.y * invW;
                applyCompositeTransform(ref x, ref y);
                vertices[i] = new Vector3(x, y, 0f);
                uvs[i] = Vector2.zero;
            }

            var triangles = new int[indexCount];
            for (var i = 0; i < indexCount; i++)
            {
                var idx = packet.Indices[i];
                triangles[i] = idx < vertexCount ? idx : 0;
            }

            var mesh = new Mesh
            {
                indexFormat = vertexCount > 65535 ? IndexFormat.UInt32 : IndexFormat.UInt16
            };
            mesh.vertices = vertices;
            mesh.uv = uvs;
            mesh.triangles = triangles;
            mesh.RecalculateBounds();
            return mesh;
        }

        private static bool RangeInBounds(int offset, int count, int length)
        {
            if (offset < 0 || count < 0 || length < 0)
            {
                return false;
            }
            if (offset > length)
            {
                return false;
            }
            return count <= length - offset;
        }

        private static Matrix4x4 ToMatrix(NicxliveNative.Mat4 m)
        {
            return new Matrix4x4(
                new Vector4(m.M11, m.M21, m.M31, m.M41),
                new Vector4(m.M12, m.M22, m.M32, m.M42),
                new Vector4(m.M13, m.M23, m.M33, m.M43),
                new Vector4(m.M14, m.M24, m.M34, m.M44));
        }

        private void applyCompositeTransform(ref float x, ref float y)
        {
            if (_dynamicCompositeDepth == 0 || _dynamicPassStack.Count == 0)
            {
                return;
            }
            var pass = _dynamicPassStack.Peek();
            var sx = Mathf.Approximately(pass.Scale.X, 0f) ? 1f : pass.Scale.X;
            var sy = Mathf.Approximately(pass.Scale.Y, 0f) ? 1f : pass.Scale.Y;
            var rz = pass.RotationZ * Mathf.Deg2Rad;
            var cs = Mathf.Cos(rz);
            var sn = Mathf.Sin(rz);
            var tx = x * sx;
            var ty = y * sy;
            x = tx * cs - ty * sn;
            y = tx * sn + ty * cs;
        }
    }
}



namespace Nicxlive.UnityBackend.Managed
{
    public sealed class NicxliveBehaviour : MonoBehaviour
    {
        public string PuppetPath = string.Empty;
        public Camera? TargetCamera;
        public Material? PartMaterial;
        public Material? MaskMaterial;
        public CommandExecutor.PropertyConfig ShaderProperties = new CommandExecutor.PropertyConfig();
        public CameraEvent CameraEvent = CameraEvent.BeforeImageEffects;

        private TextureRegistry? _textureRegistry;
        private NicxliveRenderer? _renderer;
        private CommandExecutor? _executor;
        private CommandBuffer? _commandBuffer;

        private void OnEnable()
        {
            var camera = TargetCamera != null ? TargetCamera : Camera.main;
            if (camera == null)
            {
                throw new InvalidOperationException("Target camera is not set.");
            }

            if (PartMaterial == null || MaskMaterial == null)
            {
                throw new InvalidOperationException("PartMaterial and MaskMaterial are required.");
            }

            _textureRegistry = new TextureRegistry();
            _renderer = new NicxliveRenderer(Screen.width, Screen.height, _textureRegistry);
            _executor = new CommandExecutor();
            _commandBuffer = new CommandBuffer { name = "nicxlive_unity_backend" };
            camera.AddCommandBuffer(CameraEvent, _commandBuffer);

            if (!string.IsNullOrWhiteSpace(PuppetPath))
            {
                _renderer.LoadPuppet(PuppetPath);
            }
        }

        private void Update()
        {
            if (_renderer == null || _executor == null || _commandBuffer == null || PartMaterial == null || MaskMaterial == null)
            {
                return;
            }

            var camera = TargetCamera != null ? TargetCamera : Camera.main;
            if (camera == null)
            {
                return;
            }

            _renderer.BeginFrame(Screen.width, Screen.height);
            _renderer.Tick(Time.deltaTime);
            var view = _renderer.EmitCommands();
            var shared = _renderer.GetSharedBuffers();
            var decoded = CommandStream.Decode(view);
            _executor.Execute(_commandBuffer, camera, decoded, shared, _renderer.TextureRegistry, PartMaterial, MaskMaterial, ShaderProperties);
        }

        private void OnDisable()
        {
            if (TargetCamera == null)
            {
                TargetCamera = Camera.main;
            }

            if (TargetCamera != null && _commandBuffer != null)
            {
                TargetCamera.RemoveCommandBuffer(CameraEvent, _commandBuffer);
            }

            _commandBuffer?.Release();
            _commandBuffer = null;

            _renderer?.Dispose();
            _renderer = null;
            _executor = null;
            _textureRegistry = null;
        }
    }
}






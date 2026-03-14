#nullable enable

using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using Nicxlive.UnityBackend.Interop;
using UnityEngine;
using UnityEngine.Rendering;
using Debug = UnityEngine.Debug;
using Process = System.Diagnostics.Process;

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
        public struct PuppetParameterUpdate
        {
            public uint ParameterUuid;
            public Vec2 Value;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct NjgParameterInfo
        {
            public uint Uuid;
            [MarshalAs(UnmanagedType.I1)] public bool IsVec2;
            public Vec2 Min;
            public Vec2 Max;
            public Vec2 Defaults;
            public IntPtr Name;
            public nuint NameLength;
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

        [DllImport(DllName, EntryPoint = "njgGetParameters", CallingConvention = CallingConvention.Cdecl)]
        public static extern NjgResult GetParameters(IntPtr puppet, IntPtr buffer, nuint bufferLength, out nuint outCount);

        [DllImport(DllName, EntryPoint = "njgUpdateParameters", CallingConvention = CallingConvention.Cdecl)]
        public static extern NjgResult UpdateParameters(IntPtr puppet, IntPtr updates, nuint updateCount);

        [DllImport(DllName, EntryPoint = "njgSetPuppetScale", CallingConvention = CallingConvention.Cdecl)]
        public static extern NjgResult SetPuppetScale(IntPtr puppet, float sx, float sy);

        [DllImport(DllName, EntryPoint = "njgSetPuppetTranslation", CallingConvention = CallingConvention.Cdecl)]
        public static extern NjgResult SetPuppetTranslation(IntPtr puppet, float tx, float ty);

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
    public sealed class PuppetParameterInfo
    {
        public uint Uuid;
        public string Name = string.Empty;
        public bool IsVec2;
        public Vector2 Min;
        public Vector2 Max;
        public Vector2 Defaults;
    }

    [Serializable]
    public sealed class PuppetParameterState
    {
        public uint Uuid;
        public string Name = string.Empty;
        public bool IsVec2;
        public Vector2 Min;
        public Vector2 Max;
        public Vector2 Value;
    }
}



namespace Nicxlive.UnityBackend.Managed
{
    public enum Filtering
    {
        Linear = 0,
        Nearest = 1,
        Bilinear = 2,
        Trilinear = 3,
    }

    public enum Wrapping
    {
        Clamp = 0,
        ClampToEdge = 0,
        Repeat = 1,
        MirroredRepeat = 2,
        ClampToBorder = 3,
    }

    public sealed class ShaderHandle
    {
        public ulong HandleId;
        public Material? Material;
    }

    internal static class UnityObjectUtil
    {
        public static void DestroyObject(UnityEngine.Object? obj)
        {
            if (obj == null)
            {
                return;
            }
#if UNITY_EDITOR
            if (!Application.isPlaying)
            {
                UnityEngine.Object.DestroyImmediate(obj);
                return;
            }
#endif
            UnityEngine.Object.Destroy(obj);
        }
    }

    public sealed class TextureRegistry
    {
        private readonly Dictionary<ulong, Texture> _textures = new Dictionary<ulong, Texture>();
        private readonly Dictionary<ulong, Wrapping> _wrappingByHandle = new Dictionary<ulong, Wrapping>();
        private readonly Dictionary<ulong, ShaderHandle> _shaders = new Dictionary<ulong, ShaderHandle>();
        private readonly Dictionary<ulong, Dictionary<string, int>> _shaderUniformIds = new Dictionary<ulong, Dictionary<string, int>>();
        private ulong _nextHandle = 1;
        private ulong _nextShaderHandle = 1;

        public ulong CreateTexture(int width, int height, int channels, bool renderTarget, bool stencil)
        {
            var handle = _nextHandle++;
            if (renderTarget)
            {
                var desc = new RenderTextureDescriptor(
                    Mathf.Max(1, width),
                    Mathf.Max(1, height),
                    RenderTextureFormat.ARGB32,
                    stencil ? 24 : 0)
                {
                    msaaSamples = 1,
                    useMipMap = false,
                    autoGenerateMips = false,
                    sRGB = false
                };
                var rt = new RenderTexture(desc);
                rt.name = $"nicx_rt_{handle}";
                rt.hideFlags = HideFlags.HideAndDontSave;
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
                var tex = new Texture2D(Mathf.Max(1, width), Mathf.Max(1, height), format, false, true);
                tex.name = $"nicx_tex_{handle}";
                tex.wrapMode = TextureWrapMode.Clamp;
                tex.filterMode = FilterMode.Bilinear;
                tex.anisoLevel = 1;
                _textures[handle] = tex;
            }

            _wrappingByHandle[handle] = Wrapping.Clamp;

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
            tex2D.wrapMode = ToTextureWrapMode(GetWrapping(handle));
            tex2D.filterMode = FilterMode.Bilinear;
            tex2D.anisoLevel = 1;
        }

        public void ReleaseTexture(ulong handle)
        {
            if (!_textures.TryGetValue(handle, out var texture))
            {
                return;
            }

            _textures.Remove(handle);
            _wrappingByHandle.Remove(handle);
            if (texture != null)
            {
                UnityObjectUtil.DestroyObject(texture);
            }
        }

        public void Dispose()
        {
            foreach (var texture in _textures.Values)
            {
                UnityObjectUtil.DestroyObject(texture);
            }
            _textures.Clear();
            _wrappingByHandle.Clear();

            foreach (var shaderHandle in _shaders.Values)
            {
                UnityObjectUtil.DestroyObject(shaderHandle.Material);
            }
            _shaders.Clear();
            _shaderUniformIds.Clear();

            _nextHandle = 1;
            _nextShaderHandle = 1;
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
            _wrappingByHandle[handle] = wrapping;
            texture.wrapMode = ToTextureWrapMode(wrapping);
        }

        public Wrapping GetWrapping(ulong handle)
        {
            return _wrappingByHandle.TryGetValue(handle, out var wrapping) ? wrapping : Wrapping.Clamp;
        }

        private static TextureWrapMode ToTextureWrapMode(Wrapping wrapping)
        {
            return wrapping switch
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
                var copy = new Texture2D(rt.width, rt.height, channels == 3 ? TextureFormat.RGB24 : TextureFormat.RGBA32, false, true);
                copy.ReadPixels(new Rect(0, 0, rt.width, rt.height), 0, 0);
                copy.Apply(false, false);
                var data = copy.GetRawTextureData();
                UnityObjectUtil.DestroyObject(copy);
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
                    UnityObjectUtil.DestroyObject(shaderHandle.Material);
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

        public bool hasShader(ulong handle)
        {
            return _shaders.ContainsKey(handle);
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
            var shader = UrpShaderCatalog.RequirePartShader();
            _material = new Material(shader);
            var vertexSource = source.Stage.Vertex ?? string.Empty;
            var fragmentSource = source.Stage.Fragment ?? string.Empty;
            Handle = BackendRegistry.currentRenderBackend().createShader(vertexSource, fragmentSource).Handle;
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
                UnityObjectUtil.DestroyObject(_material);
            }
        }
    }

    internal static class UrpShaderCatalog
    {
        private const string NicxlivePart = "Nicxlive/URP Part";
        private const string NicxliveMask = "Nicxlive/URP Mask";

        public static UnityEngine.Shader RequirePartShader()
        {
            return RequireShader(NicxlivePart);
        }

        public static UnityEngine.Shader RequireMaskShader()
        {
            return RequireShader(NicxliveMask);
        }

        private static UnityEngine.Shader RequireShader(params string[] shaderNames)
        {
            foreach (var shaderName in shaderNames)
            {
                var shader = UnityEngine.Shader.Find(shaderName);
                if (shader != null)
                {
                    return shader;
                }
            }
            throw new InvalidOperationException($"Required URP shader not found: {string.Join(", ", shaderNames)}");
        }
    }

    internal static class UrpCommandBufferRouter
    {
        private static readonly Dictionary<Camera, List<CommandBuffer>> BuffersByCamera = new Dictionary<Camera, List<CommandBuffer>>();
        private static readonly Dictionary<CommandBuffer, string> LastExecutionByBuffer = new Dictionary<CommandBuffer, string>();
        private static bool _hooked;

        public static void Attach(Camera camera, CommandBuffer buffer)
        {
            if (camera == null || buffer == null)
            {
                return;
            }

            EnsureHooked();
            if (!BuffersByCamera.TryGetValue(camera, out var list))
            {
                list = new List<CommandBuffer>();
                BuffersByCamera[camera] = list;
            }
            if (!list.Contains(buffer))
            {
                list.Add(buffer);
            }
        }

        public static void Detach(Camera camera, CommandBuffer buffer)
        {
            if (camera == null || buffer == null)
            {
                return;
            }
            if (!BuffersByCamera.TryGetValue(camera, out var list))
            {
                return;
            }
            list.Remove(buffer);
            if (list.Count == 0)
            {
                BuffersByCamera.Remove(camera);
            }
        }

        public static string GetLastExecutionDiag(CommandBuffer? buffer)
        {
            if (buffer == null)
            {
                return string.Empty;
            }

            return LastExecutionByBuffer.TryGetValue(buffer, out var diag) ? diag : string.Empty;
        }

        private static void EnsureHooked()
        {
            if (_hooked)
            {
                return;
            }
            RenderPipelineManager.endCameraRendering += OnEndCameraRendering;
            _hooked = true;
        }

        private static void OnEndCameraRendering(ScriptableRenderContext context, Camera camera)
        {
            _ = context;
            if (!BuffersByCamera.TryGetValue(camera, out var list) || list.Count == 0)
            {
                return;
            }

            for (var i = 0; i < list.Count; i++)
            {
                var buffer = list[i];
                if (buffer == null)
                {
                    continue;
                }
                Graphics.ExecuteCommandBuffer(buffer);
                LastExecutionByBuffer[buffer] =
                    $"Phase=end GraphicsExecute Camera={camera.name} Type={camera.cameraType} Frame={Time.frameCount}";
                buffer.Clear();
            }
        }
    }

    public sealed class RenderingBackend
    {
        private sealed class DynamicFramebufferEntry
        {
            public ulong Texture0;
            public ulong Texture1;
            public ulong Texture2;
            public ulong Stencil;

            public bool UsesTextureHandle(ulong handle)
            {
                return Texture0 == handle || Texture1 == handle || Texture2 == handle || Stencil == handle;
            }
        }

        private sealed class ShaderSourceProfile
        {
            public bool WantsMaskPath;
            public bool WantsPresentPath;
            public bool WantsColorKey;
            public readonly List<string> UniformNames = new List<string>();
        }

        public readonly CommandExecutor Executor = new CommandExecutor();
        public readonly TextureRegistry Textures = new TextureRegistry();

        private readonly Dictionary<ulong, ushort[]> _ibo = new Dictionary<ulong, ushort[]>();
        private readonly Dictionary<ulong, DynamicFramebufferEntry> _dynamicFramebuffers = new Dictionary<ulong, DynamicFramebufferEntry>();
        private readonly List<DecodedCommand> _immediateCommands = new List<DecodedCommand>();
        private readonly List<DrawSpan> _queuedSpans = new List<DrawSpan>();
        private readonly Dictionary<ulong, ShaderSourceProfile> _shaderProfiles = new Dictionary<ulong, ShaderSourceProfile>();
        private SharedBuffers _sharedSnapshot = new SharedBuffers();
        private CommandBuffer? _commandBuffer;
        private Camera? _boundCamera;
        private Camera? _attachedCamera;
        private CommandBuffer? _attachedCommandBuffer;
        private Material? _partMaterial;
        private Material? _maskMaterial;
        private readonly CommandExecutor.PropertyConfig _propertyConfig = new CommandExecutor.PropertyConfig();
        private bool _sceneOpen;
        private int _viewportWidth = 1280;
        private int _viewportHeight = 720;
        private bool _blendCapsInitialized;
        private bool _supportsAdvancedBlend = true;
        private bool _supportsAdvancedBlendCoherent = true;

        public void initializeRenderer() => Executor.initializeRenderer();
        public void initializePartBackendResources() => Executor.initializePartBackendResources();
        public void initializeMaskBackend() => Executor.initializeMaskBackend();
        public void bindPartShader() => Executor.bindPartShader();
        public void bindDrawableVao() => Executor.bindDrawableVao();
        public void beginScene(CommandBuffer buffer, int width, int height)
        {
            _commandBuffer = buffer;
            EnsurePipelineObjects();
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
                _maskMaterial.SetFloat(_propertyConfig.MaskThreshold, 0f);
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
            Executor.setAdvancedBlendEquation(_propertyConfig, (NicxliveNative.BlendMode)mode);
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
            if (EnsureRuntimeReady())
            {
                Executor.issueBlendBarrier();
            }
        }

        public bool supportsAdvancedBlend()
        {
            EnsureBlendCapabilitiesInitialized();
            return _supportsAdvancedBlend;
        }
        public bool supportsAdvancedBlendCoherent()
        {
            EnsureBlendCapabilitiesInitialized();
            return _supportsAdvancedBlendCoherent;
        }
        public bool isAdvancedBlendMode(BlendMode mode) => mode is BlendMode.Multiply or BlendMode.Screen or BlendMode.Overlay or BlendMode.Darken or BlendMode.Lighten or BlendMode.ColorDodge or BlendMode.ColorBurn or BlendMode.HardLight or BlendMode.SoftLight or BlendMode.Difference or BlendMode.Exclusion;
        public int maxTextureAnisotropy() => 16;
        public int sharedVertexBufferHandle() => 1;
        public int sharedUvBufferHandle() => 2;
        public int sharedDeformBufferHandle() => 3;
        public ulong textureId(Texture texture) => texture?.backendHandle() ?? 0;
        public bool dynamicFramebufferKeyUsesHandle(ulong handle)
        {
            if (handle == 0)
            {
                return false;
            }
            foreach (var entry in _dynamicFramebuffers.Values)
            {
                if (entry.UsesTextureHandle(handle))
                {
                    return true;
                }
            }
            return false;
        }
        public ulong acquireDynamicFramebuffer(ulong t0, ulong t1, ulong t2, ulong stencil)
        {
            var k = makeDynamicFramebufferKey(t0, t1, t2, stencil);
            _dynamicFramebuffers[k] = new DynamicFramebufferEntry
            {
                Texture0 = t0,
                Texture1 = t1,
                Texture2 = t2,
                Stencil = stencil,
            };
            return k;
        }
        private void glDeleteFramebuffers(ulong fb)
        {
            if (fb == 0)
            {
                return;
            }
            _dynamicFramebuffers.Remove(fb);
        }
        public void releaseDynamicFramebuffersForTextureHandle(ulong handle)
        {
            if (handle == 0)
            {
                return;
            }
            var staleKeys = new List<ulong>();
            foreach (var kv in _dynamicFramebuffers)
            {
                if (kv.Value.UsesTextureHandle(handle))
                {
                    staleKeys.Add(kv.Key);
                }
            }
            foreach (var key in staleKeys)
            {
                glDeleteFramebuffers(key);
            }
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
            var requested = checked((int)Math.Min((nuint)indices.Length, indexCount));
            DrawIndexedRange(indices, 0, requested, 0);
        }
        public void ensureDebugRendererInitialized() => Executor.initializeRenderer();
        public ulong currentRtvHandle() => framebufferHandle();
        public ulong offscreenRtvHandle() => framebufferHandle();
        public ulong offscreenRtvHandleAt(int index)
        {
            if (index <= 0 || _dynamicFramebuffers.Count == 0)
            {
                return offscreenRtvHandle();
            }
            var keys = new List<ulong>(_dynamicFramebuffers.Keys);
            keys.Sort();
            var at = Mathf.Clamp(index - 1, 0, keys.Count - 1);
            return keys[at];
        }
        public ulong dsvHandle() => framebufferHandle();
        public ulong offscreenDsvHandle() => framebufferHandle();
        public ulong descriptorHeapCpuStart() => framebufferHandle();
        public ulong descriptorHeapGpuStart() => framebufferHandle();
        public void enqueueSpan(DrawSpan span)
        {
            if (span.IndexCount <= 0)
            {
                return;
            }
            _queuedSpans.Add(span);
        }
        public void drawMaskPacket(NicxliveNative.NjgMaskDrawPacket packet) => executeMaskPacket(packet);
        public void drawUploadedGeometry(List<DrawSpan> spans, Texture fallbackTexture)
        {
            var localSpans = spans;
            if (localSpans == null || localSpans.Count == 0)
            {
                if (_queuedSpans.Count == 0)
                {
                    return;
                }
                localSpans = new List<DrawSpan>(_queuedSpans);
                _queuedSpans.Clear();
            }
            if (_ibo.Count == 0 || !EnsureRuntimeReady())
            {
                return;
            }
            var fallbackHandle = fallbackTexture?.backendHandle() ?? 0;
            foreach (var kv in _ibo)
            {
                var indices = kv.Value;
                if (indices == null || indices.Length == 0)
                {
                    continue;
                }
                foreach (var span in localSpans)
                {
                    DrawIndexedRange(indices, span.FirstIndex, span.IndexCount, fallbackHandle);
                }
                break;
            }
        }
        public void renderScene(Vector4 area, PostProcessingShader shaderToUse, ulong albedo, ulong emissive, ulong bump)
        {
            _ = (emissive, bump);
            if (shaderToUse.Material == null && !EnsureRuntimeReady())
            {
                return;
            }
            if (_queuedSpans.Count > 0)
            {
                var texture = new Texture
                {
                    BackendHandleValue = albedo,
                    WidthValue = Mathf.Max(1, Mathf.RoundToInt(area.z)),
                    HeightValue = Mathf.Max(1, Mathf.RoundToInt(area.w)),
                    Channels = 4,
                };
                drawUploadedGeometry(new List<DrawSpan>(_queuedSpans), texture);
                _queuedSpans.Clear();
            }
            ExecuteQueuedCommands(_immediateCommands);
            _immediateCommands.Clear();
        }
        public void applyBlendingCapabilities(bool advancedEnabled)
        {
            _supportsAdvancedBlend = supportsAdvancedBlend() && advancedEnabled;
            _supportsAdvancedBlendCoherent = supportsAdvancedBlendCoherent() && advancedEnabled;
            _blendCapsInitialized = true;
            Executor.applyBlendingCapabilities(_supportsAdvancedBlend);
            var coherent = _supportsAdvancedBlend && _supportsAdvancedBlendCoherent;
            setAdvancedBlendCoherent(coherent);
        }
        public void setAdvancedBlendCoherent(bool enabled) => Executor.setAdvancedBlendCoherent(enabled);

        private unsafe void DrawIndexedRange(ushort[] sourceIndices, int firstIndex, int indexCount, ulong textureHandle)
        {
            if (!EnsureRuntimeReady() || sourceIndices == null || sourceIndices.Length == 0 || indexCount <= 0)
            {
                return;
            }
            var start = Mathf.Clamp(firstIndex, 0, sourceIndices.Length);
            var count = Mathf.Clamp(indexCount, 0, sourceIndices.Length - start);
            if (count <= 0)
            {
                return;
            }
            var vertexCount = sharedVertexCountEstimate();
            if (vertexCount <= 0)
            {
                return;
            }

            fixed (ushort* indicesPtr = sourceIndices)
            {
                var packet = new NicxliveNative.NjgPartDrawPacket
                {
                    IsMask = false,
                    Renderable = true,
                    ModelMatrix = identityMat4(),
                    RenderMatrix = identityMat4(),
                    RenderRotation = 0f,
                    ClampedTint = new NicxliveNative.Vec3 { X = 1f, Y = 1f, Z = 1f },
                    ClampedScreen = new NicxliveNative.Vec3 { X = 0f, Y = 0f, Z = 0f },
                    Opacity = 1f,
                    EmissionStrength = 0f,
                    MaskThreshold = 0f,
                    BlendingMode = (int)NicxliveNative.BlendMode.Normal,
                    UseMultistageBlend = false,
                    HasEmissionOrBumpmap = false,
                    TextureHandle0 = (nuint)textureHandle,
                    TextureHandle1 = 0,
                    TextureHandle2 = 0,
                    TextureCount = textureHandle != 0 ? 1u : 0u,
                    Origin = new NicxliveNative.Vec2 { X = 0f, Y = 0f },
                    VertexOffset = 0,
                    VertexAtlasStride = (nuint)vertexCount,
                    UvOffset = 0,
                    UvAtlasStride = (nuint)vertexCount,
                    DeformOffset = 0,
                    DeformAtlasStride = (nuint)vertexCount,
                    IndexHandle = 0,
                    Indices = indicesPtr + start,
                    IndexCount = (nuint)count,
                    VertexCount = (nuint)vertexCount,
                };
                Executor.setupShaderStage(_propertyConfig, packet, 2);
                Executor.renderStage(_commandBuffer!, _sharedSnapshot, Textures, _partMaterial!, _propertyConfig, packet, false);
            }
        }

        private int sharedVertexCountEstimate()
        {
            var fromSnapshot = Mathf.Min(_sharedSnapshot.VertexCount, Mathf.Min(_sharedSnapshot.UvCount, _sharedSnapshot.DeformCount));
            if (fromSnapshot > 0)
            {
                return fromSnapshot;
            }
            var minLength = Mathf.Min(_sharedSnapshot.Vertices.Length, Mathf.Min(_sharedSnapshot.Uvs.Length, _sharedSnapshot.Deform.Length));
            return Mathf.Max(0, minLength / 2);
        }

        private static NicxliveNative.Mat4 identityMat4()
        {
            return new NicxliveNative.Mat4
            {
                M11 = 1f,
                M22 = 1f,
                M33 = 1f,
                M44 = 1f,
            };
        }

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
            var profile = AnalyzeShaderSource(vertexSource, fragmentSource);
            var shader = profile.WantsMaskPath ? UrpShaderCatalog.RequireMaskShader() : UrpShaderCatalog.RequirePartShader();
            var h = Textures.createShader(shader);
            _shaderProfiles[h] = profile;
            InitializeShaderFromProfile(h, profile);
            return new GLShaderHandle { Handle = h };
        }
        public void useShader(GLShaderHandle shader) { _ = Textures.useShader(shader.Handle); }
        public void destroyShader(GLShaderHandle shader)
        {
            _shaderProfiles.Remove(shader.Handle);
            Textures.destroyShader(shader.Handle);
        }
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
                _partMaterial = new Material(UrpShaderCatalog.RequirePartShader());
            }
            if (_maskMaterial == null)
            {
                _maskMaterial = new Material(UrpShaderCatalog.RequireMaskShader());
            }
            if (_attachedCamera != _boundCamera || _attachedCommandBuffer != _commandBuffer)
            {
                if (_attachedCamera != null && _attachedCommandBuffer != null)
                {
                    UrpCommandBufferRouter.Detach(_attachedCamera, _attachedCommandBuffer);
                }
                _attachedCamera = null;
                _attachedCommandBuffer = null;
            }
            if (_boundCamera != null && _commandBuffer != null && _attachedCamera == null)
            {
                UrpCommandBufferRouter.Attach(_boundCamera, _commandBuffer);
                _attachedCamera = _boundCamera;
                _attachedCommandBuffer = _commandBuffer;
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

        private void EnsureBlendCapabilitiesInitialized()
        {
            if (_blendCapsInitialized)
            {
                return;
            }
            _blendCapsInitialized = true;
            var deviceType = SystemInfo.graphicsDeviceType;
            _supportsAdvancedBlend = deviceType != GraphicsDeviceType.Null;
            _supportsAdvancedBlendCoherent = _supportsAdvancedBlend;
            Executor.applyBlendingCapabilities(_supportsAdvancedBlend);
            Executor.setAdvancedBlendCoherent(_supportsAdvancedBlendCoherent);
        }

        private ShaderSourceProfile AnalyzeShaderSource(string vertexSource, string fragmentSource)
        {
            var merged = $"{vertexSource}\n{fragmentSource}";
            var lower = merged.ToLowerInvariant();
            var profile = new ShaderSourceProfile
            {
                WantsMaskPath = lower.Contains("mask") || lower.Contains("stencil"),
                WantsPresentPath = lower.Contains("present") || lower.Contains("colorkey"),
                WantsColorKey = lower.Contains("colorkey"),
            };

            var uniformPattern = new System.Text.RegularExpressions.Regex(@"\buniform\s+\w+\s+([A-Za-z_]\w*)", System.Text.RegularExpressions.RegexOptions.CultureInvariant);
            foreach (System.Text.RegularExpressions.Match match in uniformPattern.Matches(merged))
            {
                if (!match.Success)
                {
                    continue;
                }
                var uniform = match.Groups[1].Value;
                if (string.IsNullOrWhiteSpace(uniform))
                {
                    continue;
                }
                if (!profile.UniformNames.Contains(uniform))
                {
                    profile.UniformNames.Add(uniform);
                }
            }
            return profile;
        }

        private void InitializeShaderFromProfile(ulong shaderHandle, ShaderSourceProfile profile)
        {
            var material = Textures.useShader(shaderHandle);
            if (material == null)
            {
                return;
            }
            material.SetFloat("_MaskThreshold", profile.WantsMaskPath ? 0.5f : 0f);
            material.SetFloat("_UseColorKey", profile.WantsColorKey ? 1f : 0f);
            material.SetFloat("_LegacyBlendOnly", profile.WantsPresentPath ? 0f : 1f);

            foreach (var uniformName in profile.UniformNames)
            {
                _ = Textures.getShaderUniformLocation(shaderHandle, uniformName);
            }
        }
    }

    public static class BackendRegistry
    {
        private static RenderingBackend? _cachedRenderBackend;
        private static readonly Dictionary<int, int> _sdlGlAttributes = new Dictionary<int, int>();
        private static readonly Dictionary<string, IntPtr> _loadedNativeLibraries = new Dictionary<string, IntPtr>(StringComparer.OrdinalIgnoreCase);
        private static readonly Dictionary<int, string> _shaderInfoLogs = new Dictionary<int, string>();
        private static readonly Dictionary<int, string> _programInfoLogs = new Dictionary<int, string>();
        private static IntPtr _rootDescriptorTablePtr = IntPtr.Zero;

        [StructLayout(LayoutKind.Sequential)]
        private struct Win32Rect
        {
            public int Left;
            public int Top;
            public int Right;
            public int Bottom;
        }

        private static class Win32
        {
            [DllImport("user32.dll", SetLastError = true)]
            public static extern bool GetClientRect(IntPtr hWnd, out Win32Rect rect);

            [DllImport("user32.dll")]
            public static extern IntPtr GetActiveWindow();
        }

        private static class NativeLibraryCompat
        {
            private const int RTLD_NOW = 2;

            [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
            private static extern IntPtr LoadLibraryW(string fileName);

            [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Ansi)]
            private static extern IntPtr GetProcAddress(IntPtr module, string symbol);

            [DllImport("libdl")]
            private static extern IntPtr dlopen(string fileName, int flags);

            [DllImport("libdl")]
            private static extern IntPtr dlsym(IntPtr handle, string symbol);

            public static bool TryLoad(string path, out IntPtr handle)
            {
                handle = IntPtr.Zero;
                if (string.IsNullOrWhiteSpace(path))
                {
                    return false;
                }

                if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
                {
                    handle = LoadLibraryW(path);
                }
                else
                {
                    handle = dlopen(path, RTLD_NOW);
                }

                return handle != IntPtr.Zero;
            }

            public static bool TryGetExport(IntPtr handle, string symbol, out IntPtr entry)
            {
                entry = IntPtr.Zero;
                if (handle == IntPtr.Zero || string.IsNullOrWhiteSpace(symbol))
                {
                    return false;
                }

                if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
                {
                    entry = GetProcAddress(handle, symbol);
                }
                else
                {
                    entry = dlsym(handle, symbol);
                }

                return entry != IntPtr.Zero;
            }
        }

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
        public static void glGetShaderiv(int shader)
        {
            if (shader <= 0)
            {
                return;
            }
            var backend = tryRenderBackend();
            if (backend != null && !backend.Textures.hasShader((ulong)shader))
            {
                _shaderInfoLogs[shader] = $"shader handle not found: {shader}";
            }
            else
            {
                _shaderInfoLogs[shader] = string.Empty;
            }
        }
        public static void glGetShaderInfoLog(int shader)
        {
            if (!_shaderInfoLogs.ContainsKey(shader))
            {
                glGetShaderiv(shader);
            }
        }
        public static void checkShader(int shader)
        {
            glGetShaderiv(shader);
            glGetShaderiv(shader);
            glGetShaderInfoLog(shader);
        }
        public static void glDeleteShader(int shader)
        {
            if (shader <= 0)
            {
                return;
            }
            var backend = tryRenderBackend();
            backend?.destroyShader(new GLShaderHandle { Handle = (ulong)shader });
            _shaderInfoLogs.Remove(shader);
        }
        public static void glGetProgramiv(int program)
        {
            if (program <= 0)
            {
                return;
            }
            var backend = tryRenderBackend();
            if (backend != null && !backend.Textures.hasShader((ulong)program))
            {
                _programInfoLogs[program] = $"program handle not found: {program}";
            }
            else
            {
                _programInfoLogs[program] = string.Empty;
            }
        }
        public static void glGetProgramInfoLog(int program)
        {
            if (!_programInfoLogs.ContainsKey(program))
            {
                glGetProgramiv(program);
            }
        }
        public static void checkProgram(int program)
        {
            glGetProgramiv(program);
            glGetProgramiv(program);
            glGetProgramInfoLog(program);
        }
        public static IntPtr dlopen(string path)
        {
            if (string.IsNullOrWhiteSpace(path))
            {
                return IntPtr.Zero;
            }
            if (_loadedNativeLibraries.TryGetValue(path, out var existing) && existing != IntPtr.Zero)
            {
                return existing;
            }

            foreach (var candidate in enumerateLibraryCandidates(path))
            {
                if (NativeLibraryCompat.TryLoad(candidate, out var handle))
                {
                    _loadedNativeLibraries[path] = handle;
                    return handle;
                }
            }
            return IntPtr.Zero;
        }
        public static IntPtr dlsym(IntPtr handle, string symbol)
        {
            if (handle == IntPtr.Zero || string.IsNullOrWhiteSpace(symbol))
            {
                return IntPtr.Zero;
            }
            return NativeLibraryCompat.TryGetExport(handle, symbol, out var entry) ? entry : IntPtr.Zero;
        }
        public static void configureMacOpenGLSurfaceOpacity(IntPtr glContext)
        {
            var h = dlopen("AppKit");
            var opacitySelector = dlsym(h, "NSOpenGLCPSurfaceOpacity");
            var setValuesSelector = dlsym(h, "setValues");
            _ = (opacitySelector, setValuesSelector);
            _ = glContext;
        }
        public static ShaderAsset shaderAsset(string vertexPath, string fragmentPath) => new ShaderAsset { Stage = new ShaderStageSource { Vertex = vertexPath, Fragment = fragmentPath } };
        public static bool loadSDL() => true;
        public static int SDL_Init() => 0;
        public static int SDL_GL_SetAttribute(int key, int value)
        {
            _sdlGlAttributes[key] = value;
            return 0;
        }
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
        public static (int width, int height) queryWindowPixelSize(IntPtr hwnd)
        {
            var defaultWidth = Mathf.Max(0, Screen.width);
            var defaultHeight = Mathf.Max(0, Screen.height);
            if (hwnd != IntPtr.Zero && tryGetWindowClientSize(hwnd, out var width, out var height))
            {
                return (Mathf.Max(0, width), Mathf.Max(0, height));
            }
            return (defaultWidth, defaultHeight);
        }
        public static void queryWindowPixelSize(IntPtr hwnd, out int width, out int height)
        {
            (width, height) = queryWindowPixelSize(hwnd);
        }
        public static string sdlError() => "sdl error";
        public static IntPtr requireWindowHandle()
        {
            var hwnd = Process.GetCurrentProcess().MainWindowHandle;
            if (hwnd != IntPtr.Zero)
            {
                return hwnd;
            }
            if (Application.platform == RuntimePlatform.WindowsEditor || Application.platform == RuntimePlatform.WindowsPlayer)
            {
                hwnd = Win32.GetActiveWindow();
                if (hwnd != IntPtr.Zero)
                {
                    return hwnd;
                }
            }
            return new IntPtr(1);
        }
        public static CompositeState defaultCompositeState() => new CompositeState { Valid = true };
        public static Vector4 float4(float x, float y, float z, float w) => new Vector4(x, y, z, w);
        public static Matrix4x4 mulMat4(Matrix4x4 a, Matrix4x4 b) => a * b;
        public static Vector4 mulMat4Vec4(Matrix4x4 m, float x, float y, float z, float w) => m * new Vector4(x, y, z, w);
        public static bool rangeInBounds(ulong offset, ulong count, ulong length) => offset <= length && count <= (length - offset);
        public static Vector4 screenBlend(Vector4 src, Vector4 dst) => src + dst - Vector4.Scale(src, dst);
        public static Vector4 sampleTex(Texture2D tex, Vector2 uv)
        {
            if (tex == null)
            {
                return Vector4.zero;
            }
            var u = Mathf.Repeat(uv.x, 1f);
            var v = Mathf.Repeat(uv.y, 1f);
            if (tex.isReadable)
            {
                var c = tex.GetPixelBilinear(u, v);
                return new Vector4(c.r, c.g, c.b, c.a);
            }

            var prev = RenderTexture.active;
            var tmp = RenderTexture.GetTemporary(Mathf.Max(1, tex.width), Mathf.Max(1, tex.height), 0, RenderTextureFormat.ARGB32);
            try
            {
                Graphics.Blit(tex, tmp);
                RenderTexture.active = tmp;
                var pixelX = Mathf.Clamp(Mathf.RoundToInt(u * (tex.width - 1)), 0, Mathf.Max(0, tex.width - 1));
                var pixelY = Mathf.Clamp(Mathf.RoundToInt(v * (tex.height - 1)), 0, Mathf.Max(0, tex.height - 1));
                var read = new Texture2D(1, 1, TextureFormat.RGBA32, false, false);
                try
                {
                    read.ReadPixels(new Rect(pixelX, pixelY, 1, 1), 0, 0, false);
                    read.Apply(false, true);
                    var c = read.GetPixel(0, 0);
                    return new Vector4(c.r, c.g, c.b, c.a);
                }
                finally
                {
                    Nicxlive.UnityBackend.Managed.UnityObjectUtil.DestroyObject(read);
                }
            }
            finally
            {
                RenderTexture.active = prev;
                RenderTexture.ReleaseTemporary(tmp);
            }
        }
        public static Vector4 psMain(Vector4 input) => input;
        public static Vector4 vsMain(Vector4 input) => input;
        public static int sanitizeBlendMode(int mode) => Mathf.Clamp(mode, 0, 18);
        public static object buildBlendDesc(int mode) => sanitizeBlendMode(mode);
        public static void setDirectXRuntimeOptions(DirectXRuntimeOptions opts)
        {
            if (opts.SkipDraw)
            {
                _sdlGlAttributes[-1] = 1;
            }
            if (opts.SkipPresent)
            {
                _sdlGlAttributes[-2] = 1;
            }
        }

        public static IntPtr WinD3D12CreateDevice() => resolveD3D12Export("D3D12CreateDevice");
        public static IntPtr WinD3D12GetDebugInterface() => resolveD3D12Export("D3D12GetDebugInterface");
        public static IntPtr WinD3D12SerializeRootSignature()
        {
            var ptr = resolveD3D12Export("D3D12SerializeRootSignature");
            if (ptr != IntPtr.Zero)
            {
                return ptr;
            }
            var compiler = dlopen("d3dcompiler_47");
            return dlsym(compiler, "D3D12SerializeRootSignature");
        }
        public static IntPtr D3D12_ROOT_DESCRIPTOR_TABLE()
        {
            if (_rootDescriptorTablePtr == IntPtr.Zero)
            {
                _rootDescriptorTablePtr = Marshal.AllocHGlobal(IntPtr.Size * 2);
                for (var i = 0; i < IntPtr.Size * 2; i++)
                {
                    Marshal.WriteByte(_rootDescriptorTablePtr, i, 0);
                }
            }
            return _rootDescriptorTablePtr;
        }

        private static bool tryGetWindowClientSize(IntPtr hwnd, out int width, out int height)
        {
            width = 0;
            height = 0;
            if (hwnd == IntPtr.Zero)
            {
                return false;
            }
            if (Application.platform != RuntimePlatform.WindowsEditor && Application.platform != RuntimePlatform.WindowsPlayer)
            {
                return false;
            }
            if (!Win32.GetClientRect(hwnd, out var rect))
            {
                return false;
            }
            width = Math.Max(0, rect.Right - rect.Left);
            height = Math.Max(0, rect.Bottom - rect.Top);
            return true;
        }

        private static IEnumerable<string> enumerateLibraryCandidates(string path)
        {
            yield return path;
            if (path.EndsWith(".dll", StringComparison.OrdinalIgnoreCase) ||
                path.EndsWith(".so", StringComparison.OrdinalIgnoreCase) ||
                path.EndsWith(".dylib", StringComparison.OrdinalIgnoreCase))
            {
                yield break;
            }

            if (Application.platform == RuntimePlatform.WindowsEditor || Application.platform == RuntimePlatform.WindowsPlayer)
            {
                yield return $"{path}.dll";
            }
            else if (Application.platform == RuntimePlatform.OSXEditor || Application.platform == RuntimePlatform.OSXPlayer)
            {
                yield return $"/System/Library/Frameworks/{path}.framework/{path}";
                yield return $"lib{path}.dylib";
            }
            else
            {
                yield return $"lib{path}.so";
            }
        }

        private static IntPtr resolveD3D12Export(string symbol)
        {
            var module = dlopen("d3d12");
            return dlsym(module, symbol);
        }
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
        private readonly Dictionary<int, ulong> _srvBindings = new Dictionary<int, ulong>();
        private readonly List<ulong> _swapChainTargets = new List<ulong>();
        private bool _srvResourcesCreated;
        private byte[] _stagingUpload = Array.Empty<byte>();
        private bool _pipelineStateCreated;
        private bool _initialized;
        private IntPtr _swapChainWindow;
        private ulong _uploadCapacity;
        private int _frameIndex;

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

        public void createPipelineState()
        {
            _backend.initializePartBackendResources();
            _backend.initializeMaskBackend();
            _pipelineStateCreated = true;
        }
        public void createRenderTargets(int width, int height)
        {
            releaseRenderTargets();
            createDepthStencilTarget(width, height);
            const int swapBuffers = 2;
            for (var i = 0; i < swapBuffers; i++)
            {
                var handle = _backend.createTextureHandle();
                _backend.uploadTextureData(handle, _width, _height, 4, new byte[_width * _height * 4]);
                _swapChainTargets.Add(handle);
                _resourceStates[handle] = 0;
            }
        }
        public void createSrvResources()
        {
            _srvResourcesCreated = true;
            _srvBindings.Clear();
            _ = _backend.descriptorHeapCpuStart();
            _ = _backend.descriptorHeapGpuStart();
        }
        public void createSwapChainAndTargets(IntPtr window, int width, int height)
        {
            _swapChainWindow = window;
            _frameIndex = 0;
            createDepthStencilTarget(width, height);
            createRenderTargets(width, height);
        }

        public void ensureOffscreenDepthStencilTarget(int width, int height) => createDepthStencilTarget(width, height);
        public void ensureTextureUploaded(ulong textureHandle)
        {
            if (textureHandle == 0)
            {
                return;
            }
            if (_backend.Textures.TryGet(textureHandle) == null)
            {
                _deviceResetRequested = true;
                return;
            }
            if (!_resourceStates.ContainsKey(textureHandle))
            {
                _resourceStates[textureHandle] = 0;
            }
        }
        public void ensureUploadBuffer(ulong byteSize)
        {
            if (byteSize == 0)
            {
                _stagingUpload = Array.Empty<byte>();
                return;
            }
            var required = byteSize > int.MaxValue ? int.MaxValue : (int)byteSize;
            if (_stagingUpload.Length < required)
            {
                Array.Resize(ref _stagingUpload, required);
            }
            _uploadCapacity = (ulong)_stagingUpload.Length;
        }
        public void initialize()
        {
            if (_initialized)
            {
                return;
            }
            _backend.initializeRenderer();
            if (!_pipelineStateCreated)
            {
                createPipelineState();
            }
            if (_swapChainTargets.Count == 0)
            {
                createRenderTargets(_width, _height);
            }
            _initialized = true;
        }
        public void invalidateGpuObjects() => _deviceResetRequested = true;
        public void recoverFromDeviceLoss() => _deviceResetRequested = true;
        public void releaseDxResource()
        {
            _resourceStates.Clear();
            _srvBindings.Clear();
            _stagingUpload = Array.Empty<byte>();
            _uploadCapacity = 0;
        }
        public void releaseRenderTargets()
        {
            foreach (var handle in _swapChainTargets)
            {
                _backend.destroyTextureHandle(handle);
            }
            _swapChainTargets.Clear();
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
            _srvBindings[index] = textureHandle;
            _resourceStates[textureHandle] = _resourceStates.TryGetValue(textureHandle, out var state) ? state : 0;
        }
        public void uploadData(IntPtr dst, byte[] src)
        {
            var len = (ulong)(src?.Length ?? 0);
            ensureUploadBuffer(len);
            if (src == null || src.Length == 0)
            {
                _stagingUpload = Array.Empty<byte>();
                return;
            }
            _stagingUpload = (byte[])src.Clone();
            if (dst != IntPtr.Zero)
            {
                Marshal.Copy(src, 0, dst, src.Length);
            }
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
            var vertexBytes = packVertices(vertices);
            var indexBytes = new byte[arr.Length * sizeof(ushort)];
            Buffer.BlockCopy(arr, 0, indexBytes, 0, indexBytes.Length);
            var combined = new byte[vertexBytes.Length + indexBytes.Length];
            Buffer.BlockCopy(vertexBytes, 0, combined, 0, vertexBytes.Length);
            Buffer.BlockCopy(indexBytes, 0, combined, vertexBytes.Length, indexBytes.Length);
            uploadData(IntPtr.Zero, combined);
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
            if (_swapChainTargets.Count == 0)
            {
                createRenderTargets(_width, _height);
            }
            if (BackendRegistry.isDxTraceEnabled())
            {
                BackendRegistry.dxTrace("beginFrame");
            }
            if (!_hasPendingView)
            {
                shutdown();
                initialize();
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
            _srvBindings.Clear();
            releaseRenderTargets();
            _initialized = false;
            _frameIndex = 0;
        }

        public void endFrame()
        {
            if (!_hasPendingView)
            {
                _frameIndex = (_frameIndex + 1) % Math.Max(1, _swapChainTargets.Count == 0 ? 1 : _swapChainTargets.Count);
                return;
            }
            _backend.renderCommands(_snapshot, _pendingView, _width, _height);
            _hasPendingView = false;
            _frameIndex = (_frameIndex + 1) % Math.Max(1, _swapChainTargets.Count == 0 ? 1 : _swapChainTargets.Count);
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

        private static byte[] packVertices(List<Vertex> vertices)
        {
            if (vertices == null || vertices.Count == 0)
            {
                return Array.Empty<byte>();
            }
            var packed = new byte[vertices.Count * sizeof(float) * 4];
            var offset = 0;
            for (var i = 0; i < vertices.Count; i++)
            {
                writeFloat(packed, ref offset, vertices[i].X);
                writeFloat(packed, ref offset, vertices[i].Y);
                writeFloat(packed, ref offset, vertices[i].U);
                writeFloat(packed, ref offset, vertices[i].V);
            }
            return packed;
        }

        private static void writeFloat(byte[] dst, ref int offset, float value)
        {
            var bytes = BitConverter.GetBytes(value);
            Buffer.BlockCopy(bytes, 0, dst, offset, bytes.Length);
            offset += bytes.Length;
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
        public bool HasPuppet => _puppet != IntPtr.Zero;
        public string LoadedPuppetPath { get; private set; } = string.Empty;

        public void LoadPuppet(string puppetPath)
        {
            if (string.IsNullOrWhiteSpace(puppetPath))
            {
                throw new InvalidOperationException("Puppet path is empty.");
            }

            var normalizedPath = Path.GetFullPath(puppetPath);
            UnloadPuppet();
            var result = NicxliveNative.LoadPuppet(_renderer, normalizedPath, out _puppet);
            if (result != NicxliveNative.NjgResult.Ok || _puppet == IntPtr.Zero)
            {
                throw new InvalidOperationException($"njgLoadPuppet failed: {result}, path={normalizedPath}");
            }
            LoadedPuppetPath = normalizedPath;
        }

        public void UnloadPuppet()
        {
            if (_puppet == IntPtr.Zero || _renderer == IntPtr.Zero)
            {
                LoadedPuppetPath = string.Empty;
                return;
            }

            NicxliveNative.UnloadPuppet(_renderer, _puppet);
            _puppet = IntPtr.Zero;
            LoadedPuppetPath = string.Empty;
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

        public void SetPuppetScale(float sx, float sy)
        {
            if (_puppet == IntPtr.Zero)
            {
                return;
            }

            var result = NicxliveNative.SetPuppetScale(_puppet, sx, sy);
            if (result != NicxliveNative.NjgResult.Ok)
            {
                throw new InvalidOperationException($"njgSetPuppetScale failed: {result}");
            }
        }

        public void SetPuppetTranslation(float tx, float ty)
        {
            if (_puppet == IntPtr.Zero)
            {
                return;
            }

            var result = NicxliveNative.SetPuppetTranslation(_puppet, tx, ty);
            if (result != NicxliveNative.NjgResult.Ok)
            {
                throw new InvalidOperationException($"njgSetPuppetTranslation failed: {result}");
            }
        }

        public List<PuppetParameterInfo> GetParameters()
        {
            var output = new List<PuppetParameterInfo>();
            if (_puppet == IntPtr.Zero)
            {
                return output;
            }

            var queryResult = NicxliveNative.GetParameters(_puppet, IntPtr.Zero, 0, out var count);
            if (queryResult != NicxliveNative.NjgResult.Ok)
            {
                throw new InvalidOperationException($"njgGetParameters(count) failed: {queryResult}");
            }

            if (count == 0)
            {
                return output;
            }

            var stride = Marshal.SizeOf<NicxliveNative.NjgParameterInfo>();
            var byteLength = checked((int)count * stride);
            var buffer = Marshal.AllocHGlobal(byteLength);
            try
            {
                var result = NicxliveNative.GetParameters(_puppet, buffer, count, out var outCount);
                if (result != NicxliveNative.NjgResult.Ok)
                {
                    throw new InvalidOperationException($"njgGetParameters(data) failed: {result}");
                }

                var actual = Math.Min(count, outCount);
                output.Capacity = checked((int)actual);
                for (nuint i = 0; i < actual; i++)
                {
                    var current = IntPtr.Add(buffer, checked((int)i * stride));
                    var native = Marshal.PtrToStructure<NicxliveNative.NjgParameterInfo>(current);
                    output.Add(new PuppetParameterInfo
                    {
                        Uuid = native.Uuid,
                        IsVec2 = native.IsVec2,
                        Min = new Vector2(native.Min.X, native.Min.Y),
                        Max = new Vector2(native.Max.X, native.Max.Y),
                        Defaults = new Vector2(native.Defaults.X, native.Defaults.Y),
                        Name = DecodeUtf8(native.Name, native.NameLength),
                    });
                }
            }
            finally
            {
                Marshal.FreeHGlobal(buffer);
            }

            return output;
        }

        public void UpdateParameter(uint uuid, Vector2 value)
        {
            Span<NicxliveNative.PuppetParameterUpdate> updates = stackalloc NicxliveNative.PuppetParameterUpdate[1];
            updates[0] = new NicxliveNative.PuppetParameterUpdate
            {
                ParameterUuid = uuid,
                Value = new NicxliveNative.Vec2 { X = value.x, Y = value.y },
            };
            UpdateParameters(updates);
        }

        public void UpdateParameters(ReadOnlySpan<NicxliveNative.PuppetParameterUpdate> updates)
        {
            if (_puppet == IntPtr.Zero || updates.Length == 0)
            {
                return;
            }

            var stride = Marshal.SizeOf<NicxliveNative.PuppetParameterUpdate>();
            var byteLength = checked(updates.Length * stride);
            var buffer = Marshal.AllocHGlobal(byteLength);
            try
            {
                for (var i = 0; i < updates.Length; i++)
                {
                    var current = IntPtr.Add(buffer, i * stride);
                    Marshal.StructureToPtr(updates[i], current, false);
                }

                var result = NicxliveNative.UpdateParameters(_puppet, buffer, (nuint)updates.Length);
                if (result != NicxliveNative.NjgResult.Ok)
                {
                    throw new InvalidOperationException($"njgUpdateParameters failed: {result}");
                }
            }
            finally
            {
                Marshal.FreeHGlobal(buffer);
            }
        }

        public void Dispose()
        {
            if (_disposed)
            {
                return;
            }

            _disposed = true;
            UnloadPuppet();

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

        private static string DecodeUtf8(IntPtr ptr, nuint byteLength)
        {
            if (ptr == IntPtr.Zero || byteLength == 0)
            {
                return string.Empty;
            }

            var length = checked((int)byteLength);
            var bytes = new byte[length];
            Marshal.Copy(ptr, bytes, 0, length);
            return Encoding.UTF8.GetString(bytes);
        }
    }
}



namespace Nicxlive.UnityBackend.Managed
{
    public sealed class CommandExecutor
    {
        private sealed class SceneDebugDraw
        {
            public Mesh? ClipMesh;
            public bool DestroyClipMesh;
            public Texture? MainTexture;
            public Texture? EmissionTexture;
            public float MainWrap;
            public float EmissionWrap;
            public float BumpWrap;
            public float Opacity;
            public float EmissionStrength;
            public float MaskThreshold;
            public Vector4 MultColor;
            public Vector4 ScreenColor;
            public float StencilRef;
            public float StencilComp;
            public float StencilPass;
            public float ColorMask;
            public bool UseMaskMaterial;
            public bool ShowAlbedo;
        }

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
            public string MainWrap = "_WrapMainTex";
            public string EmissiveWrap = "_WrapEmissionTex";
            public string BumpWrap = "_WrapBumpTex";
            public string SrcBlend = "_SrcBlend";
            public string DstBlend = "_DstBlend";
            public string SrcBlendAlpha = "_SrcBlendAlpha";
            public string DstBlendAlpha = "_DstBlendAlpha";
            public string BlendOp = "_BlendOp";
            public string BlendOpAlpha = "_BlendOpAlpha";
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
        private List<SceneDebugDraw> _sceneDebugDraws = new List<SceneDebugDraw>();
        private List<SceneDebugDraw> _sceneDebugPresentedDraws = new List<SceneDebugDraw>();
        private readonly Dictionary<ulong, ushort[]> _sharedIndexRanges = new Dictionary<ulong, ushort[]>();
        private ulong _sharedIndexBufferHandle;
        private bool _rendererInitialized;
        private bool _partBackendInitialized;
        private bool _maskBackendInitialized;
        private bool _drawableBackendInitialized;
        private bool _presentProgramInitialized;
        private Material? _presentMaterial;
        private Material? _sceneDebugMaterial;
        private Material? _sceneDebugMaskMaterial;
        private Mesh? _presentMesh;
        private Mesh? _debugOverlayMesh;
        private CommandBuffer? _sceneDebugReplayBuffer;
        private int _presentProgramHandle;
        private int _viewportW = 1280;
        private int _viewportH = 720;
        private bool _resizingViewportTargets;
        private CommandBuffer? _activeBuffer;
        private readonly int _postSrcId = Shader.PropertyToID("_NicxPostSrc");
        private readonly int _postTmpAId = Shader.PropertyToID("_NicxPostTmpA");
        private readonly int _postTmpBId = Shader.PropertyToID("_NicxPostTmpB");
        private readonly int _sceneColorRtId = Shader.PropertyToID("_NicxSceneColor");
        private readonly int _drawBufferCountId = Shader.PropertyToID("_NicxDrawBufferCount");
        private readonly int _useColorKeyId = Shader.PropertyToID("_UseColorKey");
        private readonly int _colorKeyId = Shader.PropertyToID("_ColorKey");
        private RenderTargetIdentifier _backbufferTarget = new RenderTargetIdentifier(BuiltinRenderTextureType.CameraTarget);
        private RenderTargetIdentifier _rootSceneTarget = new RenderTargetIdentifier(BuiltinRenderTextureType.CameraTarget);
        private RenderTexture? _rootSceneTexture;
        private RenderTexture? _rootSceneEmissiveTexture;
        private RenderTexture? _rootSceneBumpTexture;
        private RenderTexture? _rootSceneDepthTexture;
        private RenderTargetIdentifier[]? _rootSceneColorTargets;
        private Matrix4x4 _clipSpaceDrawMatrix = Matrix4x4.identity;
        private Vector2 _clipPuppetTranslation = Vector2.zero;
        private Vector2 _clipPuppetScale = Vector2.one;
        private bool _disableStencilDebug;
        private bool _queueSceneDebugDraws;
        private bool _mirrorPresentToCurrentActive;
        private ulong _sceneDebugSignature;
        private int _currentPartPacketOrdinal;
        private int _currentShaderStage;
        private NicxliveNative.BlendMode _currentBlendMode = NicxliveNative.BlendMode.Normal;
        private bool _currentLegacyBlendOnly = true;
        private bool _currentAdvancedBlendEnabled;
        private bool _accumulateCurrentPartBounds = true;
        private float _worstPartAbsClip;
        private int _partBoundsWarnCooldown;
        private float _currentStencilRef = 1.0f;
        private float _currentStencilComp = (float)CompareFunction.Always;
        private float _currentStencilPass = (float)StencilOp.Keep;
        private float _currentColorMask = 15.0f;
        private int _currentSrcBlend = (int)UnityEngine.Rendering.BlendMode.One;
        private int _currentDstBlend = (int)UnityEngine.Rendering.BlendMode.OneMinusSrcAlpha;
        private int _currentSrcBlendAlpha = (int)UnityEngine.Rendering.BlendMode.One;
        private int _currentDstBlendAlpha = (int)UnityEngine.Rendering.BlendMode.OneMinusSrcAlpha;
        private int _currentBlendOp = (int)UnityEngine.Rendering.BlendOp.Add;
        private int _currentBlendOpAlpha = (int)UnityEngine.Rendering.BlendOp.Add;
        private bool _forceRootSceneVisiblePass;
        private readonly Dictionary<MaterialRenderStateKey, Material> _renderStateMaterialCache = new();
        private readonly List<ReusableMeshData> _partMeshPool = new();
        private readonly List<ReusableMeshData> _maskMeshPool = new();
        private int _partMeshPoolUsed;
        private int _maskMeshPoolUsed;

        private sealed class ReusableMeshData
        {
            public readonly Mesh Mesh;
            public Vector3[] Vertices = Array.Empty<Vector3>();
            public Vector2[] Uvs = Array.Empty<Vector2>();
            public int[] Indices = Array.Empty<int>();

            public ReusableMeshData(string name)
            {
                Mesh = new Mesh
                {
                    name = name
                };
                Mesh.MarkDynamic();
            }
        }

        private struct MaterialRenderStateKey : IEquatable<MaterialRenderStateKey>
        {
            public int BaseMaterialId;
            public int SrcBlend;
            public int DstBlend;
            public int SrcBlendAlpha;
            public int DstBlendAlpha;
            public int BlendOp;
            public int BlendOpAlpha;
            public int StencilRef;
            public int StencilComp;
            public int StencilPass;
            public int ColorMask;

            public bool Equals(MaterialRenderStateKey other)
            {
                return BaseMaterialId == other.BaseMaterialId &&
                    SrcBlend == other.SrcBlend &&
                    DstBlend == other.DstBlend &&
                    SrcBlendAlpha == other.SrcBlendAlpha &&
                    DstBlendAlpha == other.DstBlendAlpha &&
                    BlendOp == other.BlendOp &&
                    BlendOpAlpha == other.BlendOpAlpha &&
                    StencilRef == other.StencilRef &&
                    StencilComp == other.StencilComp &&
                    StencilPass == other.StencilPass &&
                    ColorMask == other.ColorMask;
            }

            public override bool Equals(object? obj)
            {
                return obj is MaterialRenderStateKey other && Equals(other);
            }

            public override int GetHashCode()
            {
                unchecked
                {
                    var hash = BaseMaterialId;
                    hash = (hash * 397) ^ SrcBlend;
                    hash = (hash * 397) ^ DstBlend;
                    hash = (hash * 397) ^ SrcBlendAlpha;
                    hash = (hash * 397) ^ DstBlendAlpha;
                    hash = (hash * 397) ^ BlendOp;
                    hash = (hash * 397) ^ BlendOpAlpha;
                    hash = (hash * 397) ^ StencilRef;
                    hash = (hash * 397) ^ StencilComp;
                    hash = (hash * 397) ^ StencilPass;
                    hash = (hash * 397) ^ ColorMask;
                    return hash;
                }
            }
        }

        public int LastPartPacketCount { get; private set; }
        public int LastMaskPacketCount { get; private set; }
        public int LastPartDrawIssuedCount { get; private set; }
        public int LastMaskDrawIssuedCount { get; private set; }
        public int LastPartSkippedNoTextureCount { get; private set; }
        public int LastPartSkippedMeshBuildCount { get; private set; }
        public int LastMaskSkippedMeshBuildCount { get; private set; }
        public float LastPartOpacityMin { get; private set; }
        public float LastPartOpacityMax { get; private set; }
        public int LastPartOpacityZeroCount { get; private set; }
        public int LastPartOpacityNonZeroCount { get; private set; }
        public int LastPartTextureMissingCount { get; private set; }
        public Vector4 LastPartClipBounds { get; private set; } = Vector4.zero;
        public Vector4 LastMaskClipBounds { get; private set; } = Vector4.zero;
        public bool HasPartClipBounds { get; private set; }
        public bool HasMaskClipBounds { get; private set; }
        public string LastPartBuildDiag { get; private set; } = string.Empty;
        public string LastRenderPathDiag { get; private set; } = string.Empty;
        public string LastClipFitDiag { get; private set; } = string.Empty;
        public string LastRootSceneDiag { get; private set; } = string.Empty;
        public ulong LastSceneDebugSignature { get; private set; }
        public bool EnableDiagnostics { get; set; }
        public bool EnableRuntimeSceneDebugCapture { get; set; }
        public RenderTexture? RootSceneTexture => _rootSceneTexture;

        public CommandExecutor()
        {
            _whiteTexture = Texture2D.whiteTexture;
        }

        public void RenderSceneDebug(Camera? camera)
        {
            _ = camera;
            if (_sceneDebugPresentedDraws.Count == 0)
            {
                return;
            }

            EnsureSceneDebugMaterial();
            if (_sceneDebugMaterial == null)
            {
                return;
            }

            DrawPresentedSceneDebugContents();
        }

        public void RenderRootSceneOverlay(Camera? camera)
        {
            _ = camera;
            if (_rootSceneTexture == null || !_rootSceneTexture.IsCreated())
            {
                return;
            }

            ensurePresentProgram();
            if (_presentMaterial == null || _presentMesh == null)
            {
                return;
            }

            _props.Clear();
            _props.SetTexture("_MainTex", _rootSceneTexture);
            if (!_presentMaterial.SetPass(0))
            {
                return;
            }

            Graphics.DrawMeshNow(_presentMesh, Matrix4x4.identity);
        }

        public void DrawRootSceneOverlay(Rect destinationRect, Rect sourceRect)
        {
            if (_rootSceneTexture == null || !_rootSceneTexture.IsCreated())
            {
                return;
            }

#if UNITY_EDITOR
            ensurePresentProgram();
            if (_presentMaterial != null &&
                Mathf.Approximately(sourceRect.x, 0f) &&
                Mathf.Approximately(sourceRect.y, 0f) &&
                Mathf.Approximately(sourceRect.width, 1f) &&
                Mathf.Approximately(sourceRect.height, 1f))
            {
                UnityEditor.EditorGUI.DrawPreviewTexture(destinationRect, _rootSceneTexture, _presentMaterial, ScaleMode.StretchToFill);
                return;
            }
#endif
            GUI.DrawTextureWithTexCoords(destinationRect, _rootSceneTexture, sourceRect, true);
        }

        public void RenderRootSceneToTarget(RenderTexture? target)
        {
            if (target == null || _rootSceneTexture == null || !_rootSceneTexture.IsCreated())
            {
                return;
            }

            ensurePresentProgram();
            EnsureSceneDebugReplayBuffer();
            if (_presentMaterial == null || _presentMesh == null || _sceneDebugReplayBuffer == null)
            {
                return;
            }

            _props.Clear();
            _props.SetTexture("_MainTex", _rootSceneTexture);
            _sceneDebugReplayBuffer.Clear();
            _sceneDebugReplayBuffer.SetRenderTarget(target);
            _sceneDebugReplayBuffer.ClearRenderTarget(RTClearFlags.All, Color.clear, 1.0f, 0);
            _sceneDebugReplayBuffer.DrawMesh(_presentMesh, Matrix4x4.identity, _presentMaterial, 0, 0, _props);
            Graphics.ExecuteCommandBuffer(_sceneDebugReplayBuffer);
        }

        public void RenderSceneDebugToTarget(RenderTexture? target)
        {
            if (target == null)
            {
                return;
            }

            EnsureSceneDebugMaterial();
            EnsureSceneDebugMaskMaterial();
            EnsureSceneDebugReplayBuffer();
            if (_sceneDebugReplayBuffer == null)
            {
                return;
            }

            _sceneDebugReplayBuffer.Clear();
            _sceneDebugReplayBuffer.SetViewProjectionMatrices(Matrix4x4.identity, Matrix4x4.identity);
            _sceneDebugReplayBuffer.SetRenderTarget(target);
            _sceneDebugReplayBuffer.ClearRenderTarget(RTClearFlags.All, Color.clear, 1.0f, 0);
            if (_sceneDebugPresentedDraws.Count > 0)
            {
                AppendSceneDebugReplayCommands(_sceneDebugReplayBuffer);
            }
            Graphics.ExecuteCommandBuffer(_sceneDebugReplayBuffer);
        }

        private void DrawPresentedSceneDebugContents()
        {
            EnsureSceneDebugMaskMaterial();
            for (var i = 0; i < _sceneDebugPresentedDraws.Count; i++)
            {
                var draw = _sceneDebugPresentedDraws[i];
                var clipMesh = draw.ClipMesh;
                if (clipMesh == null)
                {
                    continue;
                }

                if (clipMesh.vertexCount == 0)
                {
                    continue;
                }

                var material = draw.UseMaskMaterial ? _sceneDebugMaskMaterial : _sceneDebugMaterial;
                if (material == null)
                {
                    continue;
                }

                ConfigureSceneDebugMaterial(material, draw);
                if (!material.SetPass(0))
                {
                    continue;
                }

                Graphics.DrawMeshNow(clipMesh, Matrix4x4.identity);
            }
        }

        public void CommitSceneDebugDraws(bool presentCurrentFrame)
        {
            if (!presentCurrentFrame)
            {
                return;
            }

            ClearSceneDebugPresentedDraws();
            var swap = _sceneDebugPresentedDraws;
            _sceneDebugPresentedDraws = _sceneDebugDraws;
            _sceneDebugDraws = swap;
            _sceneDebugDraws.Clear();
        }

        private void EnsureSceneDebugMaterial()
        {
            if (_sceneDebugMaterial != null)
            {
                return;
            }

            try
            {
                var shader = Nicxlive.UnityBackend.Compat.UrpShaderCatalog.RequirePartShader();
                if (shader == null)
                {
                    _sceneDebugMaterial = null;
                    return;
                }

                _sceneDebugMaterial = new Material(shader)
                {
                    name = "nicxlive_scene_debug_part",
                    hideFlags = HideFlags.HideAndDontSave
                };
                _sceneDebugMaterial.SetTexture("_MainTex", _whiteTexture);
                _sceneDebugMaterial.SetTexture("_BaseMap", _whiteTexture);
                _sceneDebugMaterial.SetTexture("_EmissionTex", Texture2D.blackTexture);
                _sceneDebugMaterial.SetFloat("_Opacity", 1.0f);
                _sceneDebugMaterial.SetFloat("_EmissionStrength", 0.0f);
                _sceneDebugMaterial.SetVector("_MultColor", Vector4.one);
                _sceneDebugMaterial.SetVector("_ScreenColor", Vector4.zero);
                _sceneDebugMaterial.SetFloat("_StencilRef", 1.0f);
                _sceneDebugMaterial.SetFloat("_StencilComp", (float)CompareFunction.Always);
                _sceneDebugMaterial.SetFloat("_StencilPass", (float)StencilOp.Keep);
                _sceneDebugMaterial.SetFloat("_ColorMask", 15.0f);
                _sceneDebugMaterial.SetFloat("_DebugForceOpaque", 0.0f);
                _sceneDebugMaterial.SetFloat("_DebugFlipY", 1.0f);
                _sceneDebugMaterial.SetFloat("_DebugFlipV", 0.0f);
                _sceneDebugMaterial.SetFloat("_DebugShowAlbedo", 1.0f);
                _sceneDebugMaterial.SetFloat("_WrapMainTex", 0.0f);
                _sceneDebugMaterial.SetFloat("_WrapEmissionTex", 0.0f);
                _sceneDebugMaterial.SetFloat("_WrapBumpTex", 0.0f);
            }
            catch
            {
                _sceneDebugMaterial = null;
            }
        }

        private void EnsureSceneDebugMaskMaterial()
        {
            if (_sceneDebugMaskMaterial != null)
            {
                return;
            }

            try
            {
                var shader = Nicxlive.UnityBackend.Compat.UrpShaderCatalog.RequireMaskShader();
                if (shader == null)
                {
                    _sceneDebugMaskMaterial = null;
                    return;
                }

                _sceneDebugMaskMaterial = new Material(shader)
                {
                    name = "nicxlive_scene_debug_mask",
                    hideFlags = HideFlags.HideAndDontSave
                };
                _sceneDebugMaskMaterial.SetFloat("_StencilRef", 1.0f);
                _sceneDebugMaskMaterial.SetFloat("_StencilComp", (float)CompareFunction.Always);
                _sceneDebugMaskMaterial.SetFloat("_StencilPass", (float)StencilOp.Keep);
                _sceneDebugMaskMaterial.SetFloat("_ColorMask", 0.0f);
            }
            catch
            {
                _sceneDebugMaskMaterial = null;
            }
        }

        private void EnsureSceneDebugReplayBuffer()
        {
            if (_sceneDebugReplayBuffer != null)
            {
                return;
            }

            _sceneDebugReplayBuffer = new CommandBuffer
            {
                name = "nicxlive_scene_debug_replay"
            };
        }

        private void AppendSceneDebugReplayCommands(CommandBuffer buffer)
        {
            AppendSceneDebugReplayCommands(buffer, _sceneDebugPresentedDraws);
        }

        private void AppendSceneDebugReplayCommands(CommandBuffer buffer, List<SceneDebugDraw> draws)
        {
            for (var i = 0; i < draws.Count; i++)
            {
                var draw = draws[i];
                var clipMesh = draw.ClipMesh;
                if (clipMesh == null || clipMesh.vertexCount == 0)
                {
                    continue;
                }

                var material = draw.UseMaskMaterial ? _sceneDebugMaskMaterial : _sceneDebugMaterial;
                if (material == null)
                {
                    continue;
                }
                material = ResolveRenderStateMaterial(
                    material,
                    (int)UnityEngine.Rendering.BlendMode.One,
                    (int)UnityEngine.Rendering.BlendMode.OneMinusSrcAlpha,
                    (int)UnityEngine.Rendering.BlendMode.One,
                    (int)UnityEngine.Rendering.BlendMode.OneMinusSrcAlpha,
                    (int)UnityEngine.Rendering.BlendOp.Add,
                    (int)UnityEngine.Rendering.BlendOp.Add,
                    Mathf.RoundToInt(draw.StencilRef),
                    Mathf.RoundToInt(draw.StencilComp),
                    Mathf.RoundToInt(draw.StencilPass),
                    Mathf.RoundToInt(draw.ColorMask));

                _props.Clear();
                ApplySceneDebugProperties(_props, draw);
                buffer.DrawMesh(clipMesh, Matrix4x4.identity, material, 0, 0, _props);
            }
        }

        private void ReplayCurrentSceneDebugToRootScene(CommandBuffer buffer)
        {
            if (buffer == null || _sceneDebugDraws.Count == 0 || _rootSceneTexture == null)
            {
                return;
            }

            EnsureSceneDebugMaterial();
            EnsureSceneDebugMaskMaterial();
            if (_sceneDebugMaterial == null)
            {
                return;
            }

            var depthTarget = _rootSceneDepthTexture != null
                ? new RenderTargetIdentifier(_rootSceneDepthTexture)
                : _rootSceneTarget;
            buffer.SetViewProjectionMatrices(Matrix4x4.identity, Matrix4x4.identity);
            buffer.SetRenderTarget(_rootSceneTarget, depthTarget);
            buffer.ClearRenderTarget(RTClearFlags.All, Color.clear, 1.0f, 0);
            AppendSceneDebugReplayCommands(buffer, _sceneDebugDraws);
            _renderTargetStack.Clear();
            _renderTargetStack.Push(_rootSceneTarget);
        }

        private void ConfigureSceneDebugMaterial(Material material, SceneDebugDraw draw)
        {
            if (!draw.UseMaskMaterial)
            {
                material.SetTexture("_MainTex", draw.MainTexture != null ? draw.MainTexture : _whiteTexture);
                material.SetTexture("_BaseMap", draw.MainTexture != null ? draw.MainTexture : _whiteTexture);
                material.SetTexture("_EmissionTex", draw.EmissionTexture != null ? draw.EmissionTexture : Texture2D.blackTexture);
                material.SetFloat("_Opacity", Mathf.Clamp01(draw.Opacity));
                material.SetFloat("_EmissionStrength", Mathf.Max(0f, draw.EmissionStrength));
                material.SetFloat("_MaskThreshold", Mathf.Clamp01(draw.MaskThreshold));
                material.SetFloat("_WrapMainTex", draw.MainWrap);
                material.SetFloat("_WrapEmissionTex", draw.EmissionWrap);
                material.SetFloat("_WrapBumpTex", draw.BumpWrap);
                material.SetVector("_MultColor", draw.MultColor);
                material.SetVector("_ScreenColor", draw.ScreenColor);
                material.SetFloat("_DebugForceOpaque", 0.0f);
                material.SetFloat("_DebugFlipY", 1.0f);
                material.SetFloat("_DebugFlipV", 0.0f);
                material.SetFloat("_DebugShowAlbedo", draw.ShowAlbedo ? 1.0f : 0.0f);
            }

            material.SetFloat("_StencilRef", draw.StencilRef);
            material.SetFloat("_StencilComp", draw.StencilComp);
            material.SetFloat("_StencilPass", draw.StencilPass);
            material.SetFloat("_ColorMask", draw.ColorMask);
        }

        private void ApplySceneDebugProperties(MaterialPropertyBlock props, SceneDebugDraw draw)
        {
            if (!draw.UseMaskMaterial)
            {
                props.SetTexture("_MainTex", draw.MainTexture != null ? draw.MainTexture : _whiteTexture);
                props.SetTexture("_BaseMap", draw.MainTexture != null ? draw.MainTexture : _whiteTexture);
                props.SetTexture("_EmissionTex", draw.EmissionTexture != null ? draw.EmissionTexture : Texture2D.blackTexture);
                props.SetFloat("_Opacity", Mathf.Clamp01(draw.Opacity));
                props.SetFloat("_EmissionStrength", Mathf.Max(0f, draw.EmissionStrength));
                props.SetFloat("_MaskThreshold", Mathf.Clamp01(draw.MaskThreshold));
                props.SetFloat("_WrapMainTex", draw.MainWrap);
                props.SetFloat("_WrapEmissionTex", draw.EmissionWrap);
                props.SetFloat("_WrapBumpTex", draw.BumpWrap);
                props.SetVector("_MultColor", draw.MultColor);
                props.SetVector("_ScreenColor", draw.ScreenColor);
                props.SetFloat("_DebugForceOpaque", 0.0f);
                props.SetFloat("_DebugFlipY", 1.0f);
                props.SetFloat("_DebugFlipV", 0.0f);
                props.SetFloat("_DebugShowAlbedo", draw.ShowAlbedo ? 1.0f : 0.0f);
            }

            props.SetFloat("_StencilRef", draw.StencilRef);
            props.SetFloat("_StencilComp", draw.StencilComp);
            props.SetFloat("_StencilPass", draw.StencilPass);
            props.SetFloat("_ColorMask", draw.ColorMask);
        }

        private static void ClearSceneDebugDrawList(List<SceneDebugDraw> draws)
        {
            for (var i = 0; i < draws.Count; i++)
            {
                var draw = draws[i];
                if (draw.DestroyClipMesh && draw.ClipMesh != null)
                {
                    UnityObjectUtil.DestroyObject(draw.ClipMesh);
                }
            }
            draws.Clear();
        }

        private void ClearSceneDebugDraws()
        {
            ClearSceneDebugDrawList(_sceneDebugDraws);
        }

        private void ClearSceneDebugPresentedDraws()
        {
            ClearSceneDebugDrawList(_sceneDebugPresentedDraws);
        }

        private static RenderTexture CreateRootColorTexture(string name, int width, int height)
        {
            var desc = new RenderTextureDescriptor(width, height, RenderTextureFormat.ARGB32, 0)
            {
                msaaSamples = 1,
                useMipMap = false,
                autoGenerateMips = false,
                sRGB = false
            };

            var texture = new RenderTexture(desc)
            {
                name = name,
                filterMode = FilterMode.Bilinear,
                wrapMode = TextureWrapMode.Clamp,
                hideFlags = HideFlags.HideAndDontSave
            };
            texture.Create();
            return texture;
        }

        private static RenderTexture CreateRootDepthTexture(string name, int width, int height)
        {
            var desc = new RenderTextureDescriptor(width, height)
            {
                msaaSamples = 1,
                useMipMap = false,
                autoGenerateMips = false,
                graphicsFormat = UnityEngine.Experimental.Rendering.GraphicsFormat.None,
                depthStencilFormat = UnityEngine.Experimental.Rendering.GraphicsFormat.D24_UNorm_S8_UInt,
                sRGB = false
            };

            var texture = new RenderTexture(desc)
            {
                name = name,
                filterMode = FilterMode.Point,
                wrapMode = TextureWrapMode.Clamp,
                hideFlags = HideFlags.HideAndDontSave
            };
            texture.Create();
            return texture;
        }

        private void EnsureRootSceneTexture(int width, int height)
        {
            width = Mathf.Max(1, width);
            height = Mathf.Max(1, height);
            if (_rootSceneTexture != null &&
                _rootSceneEmissiveTexture != null &&
                _rootSceneBumpTexture != null &&
                _rootSceneDepthTexture != null &&
                _rootSceneTexture.width == width &&
                _rootSceneTexture.height == height &&
                _rootSceneEmissiveTexture.width == width &&
                _rootSceneEmissiveTexture.height == height &&
                _rootSceneBumpTexture.width == width &&
                _rootSceneBumpTexture.height == height &&
                _rootSceneDepthTexture.width == width &&
                _rootSceneDepthTexture.height == height)
            {
                if (!_rootSceneTexture.IsCreated())
                {
                    _rootSceneTexture.Create();
                }
                if (!_rootSceneEmissiveTexture.IsCreated())
                {
                    _rootSceneEmissiveTexture.Create();
                }
                if (!_rootSceneBumpTexture.IsCreated())
                {
                    _rootSceneBumpTexture.Create();
                }
                if (!_rootSceneDepthTexture.IsCreated())
                {
                    _rootSceneDepthTexture.Create();
                }
                _rootSceneTarget = new RenderTargetIdentifier(_rootSceneTexture);
                _rootSceneColorTargets = new[]
                {
                    new RenderTargetIdentifier(_rootSceneTexture),
                    new RenderTargetIdentifier(_rootSceneEmissiveTexture),
                    new RenderTargetIdentifier(_rootSceneBumpTexture),
                };
                return;
            }

            ReleasePersistentResources();
            _rootSceneTexture = CreateRootColorTexture("nicxlive_root_scene_albedo_rt", width, height);
            _rootSceneEmissiveTexture = CreateRootColorTexture("nicxlive_root_scene_emissive_rt", width, height);
            _rootSceneBumpTexture = CreateRootColorTexture("nicxlive_root_scene_bump_rt", width, height);
            _rootSceneDepthTexture = CreateRootDepthTexture("nicxlive_root_scene_depth_rt", width, height);
            _rootSceneTarget = new RenderTargetIdentifier(_rootSceneTexture);
            _rootSceneColorTargets = new[]
            {
                new RenderTargetIdentifier(_rootSceneTexture),
                new RenderTargetIdentifier(_rootSceneEmissiveTexture),
                new RenderTargetIdentifier(_rootSceneBumpTexture),
            };
        }

        public void ReleasePersistentResources()
        {
            ReleaseReusableMeshes(_partMeshPool);
            ReleaseReusableMeshes(_maskMeshPool);
            _partMeshPoolUsed = 0;
            _maskMeshPoolUsed = 0;

            if (_rootSceneTexture == null &&
                _rootSceneEmissiveTexture == null &&
                _rootSceneBumpTexture == null &&
                _rootSceneDepthTexture == null)
            {
                return;
            }

            if (_rootSceneTexture != null && _rootSceneTexture.IsCreated())
            {
                _rootSceneTexture.Release();
            }
            UnityObjectUtil.DestroyObject(_rootSceneTexture);
            _rootSceneTexture = null;
            if (_rootSceneEmissiveTexture != null)
            {
                if (_rootSceneEmissiveTexture.IsCreated())
                {
                    _rootSceneEmissiveTexture.Release();
                }
                UnityObjectUtil.DestroyObject(_rootSceneEmissiveTexture);
                _rootSceneEmissiveTexture = null;
            }
            if (_rootSceneBumpTexture != null)
            {
                if (_rootSceneBumpTexture.IsCreated())
                {
                    _rootSceneBumpTexture.Release();
                }
                UnityObjectUtil.DestroyObject(_rootSceneBumpTexture);
                _rootSceneBumpTexture = null;
            }
            if (_rootSceneDepthTexture != null)
            {
                if (_rootSceneDepthTexture.IsCreated())
                {
                    _rootSceneDepthTexture.Release();
                }
                UnityObjectUtil.DestroyObject(_rootSceneDepthTexture);
                _rootSceneDepthTexture = null;
            }
            _rootSceneColorTargets = null;
            _rootSceneTarget = new RenderTargetIdentifier(BuiltinRenderTextureType.CameraTarget);
        }

        private static void ReleaseReusableMeshes(List<ReusableMeshData> pool)
        {
            for (var i = 0; i < pool.Count; i++)
            {
                UnityObjectUtil.DestroyObject(pool[i].Mesh);
            }
            pool.Clear();
        }

        private bool IsCurrentRenderTargetRootScene()
        {
            return _renderTargetStack.Count > 0 && _renderTargetStack.Peek().Equals(_rootSceneTarget);
        }

        public void initializeRenderer()
        {
            _rendererInitialized = true;
            initializePartBackendResources();
            initializeMaskBackend();
            ensureDrawableBackendInitialized();
            ensurePresentProgram();
        }

        public void ConfigureInjectedPuppetTransform(
            List<DecodedCommand> commands,
            SharedBuffers shared,
            float desiredTx,
            float desiredTy,
            float sx,
            float sy,
            int viewportWidth,
            int viewportHeight)
        {
            var width = Mathf.Max(1, viewportWidth);
            var height = Mathf.Max(1, viewportHeight);
            var targetClip = new Vector2(
                (2f * desiredTx) / width,
                (-2f * desiredTy) / height);

            var requestedScale = new Vector2(
                Mathf.Approximately(sx, 0f) ? 1f : sx,
                Mathf.Approximately(sy, 0f) ? 1f : sy);

            _clipPuppetTranslation = targetClip;
            _clipPuppetScale = requestedScale;
            if (!EnableDiagnostics)
            {
                LastClipFitDiag = string.Empty;
                return;
            }
            LastClipFitDiag = $"Target=({targetClip.x:F3},{targetClip.y:F3}) Scale=({requestedScale.x:F3},{requestedScale.y:F3}) Aggregate=N/A";

            if (commands == null || shared == null || !TryComputeAggregateClipBounds(commands, shared, out var bounds))
            {
                return;
            }

            LastClipFitDiag =
                $"Target=({targetClip.x:F3},{targetClip.y:F3}) ReqScale=({requestedScale.x:F3},{requestedScale.y:F3}) " +
                $"Agg=x:[{bounds.x:F3},{bounds.z:F3}] y:[{bounds.y:F3},{bounds.w:F3}] " +
                $"PivotMode=origin FinalScale=({requestedScale.x:F3},{requestedScale.y:F3}) " +
                $"Adj=({targetClip.x:F3},{targetClip.y:F3})";
        }

        private static Vector4 ExpandBounds(Vector4 current, float x, float y)
        {
            if (float.IsNaN(x) || float.IsNaN(y) || float.IsInfinity(x) || float.IsInfinity(y))
            {
                return current;
            }

            current.x = Mathf.Min(current.x, x);
            current.y = Mathf.Min(current.y, y);
            current.z = Mathf.Max(current.z, x);
            current.w = Mathf.Max(current.w, y);
            return current;
        }

        private static Vector4 MergeBounds(Vector4 lhs, Vector4 rhs)
        {
            return new Vector4(
                Mathf.Min(lhs.x, rhs.x),
                Mathf.Min(lhs.y, rhs.y),
                Mathf.Max(lhs.z, rhs.z),
                Mathf.Max(lhs.w, rhs.w));
        }

        private void ApplyPuppetTransform(ref float x, ref float y)
        {
            if (_dynamicCompositeDepth > 0)
            {
                return;
            }

            x = (x * _clipPuppetScale.x) + _clipPuppetTranslation.x;
            y = (y * _clipPuppetScale.y) + _clipPuppetTranslation.y;
        }

        private bool TryComputeAggregateClipBounds(List<DecodedCommand> commands, SharedBuffers shared, out Vector4 bounds)
        {
            bounds = new Vector4(float.PositiveInfinity, float.PositiveInfinity, float.NegativeInfinity, float.NegativeInfinity);
            var any = false;
            var dynamicCompositeDepth = 0;

            for (var commandIndex = 0; commandIndex < commands.Count; commandIndex++)
            {
                var command = commands[commandIndex];
                switch (command.Kind)
                {
                    case NicxliveNative.NjgRenderCommandKind.BeginDynamicComposite:
                        dynamicCompositeDepth++;
                        continue;
                    case NicxliveNative.NjgRenderCommandKind.EndDynamicComposite:
                        if (dynamicCompositeDepth > 0)
                        {
                            dynamicCompositeDepth--;
                        }
                        continue;
                }

                if (dynamicCompositeDepth > 0)
                {
                    continue;
                }

                switch (command.Kind)
                {
                    case NicxliveNative.NjgRenderCommandKind.DrawPart:
                    {
                        var packet = command.PartPacket;
                        if (!packet.IsMask && packet.TextureCount == 0)
                        {
                            break;
                        }
                        any |= TryExpandPartClipBounds(shared, packet, ref bounds);
                        break;
                    }
                }
            }

            return any && float.IsFinite(bounds.x);
        }

        private bool TryExpandPartClipBounds(SharedBuffers shared, NicxliveNative.NjgPartDrawPacket packet, ref Vector4 bounds)
        {
            if (!packet.Renderable || packet.VertexCount == 0)
            {
                return false;
            }

            var vertexCount = checked((int)packet.VertexCount);
            var vxBase = checked((int)packet.VertexOffset);
            var vyBase = checked((int)(packet.VertexOffset + packet.VertexAtlasStride));
            var dxBase = checked((int)packet.DeformOffset);
            var dyBase = checked((int)(packet.DeformOffset + packet.DeformAtlasStride));
            if (!RangeInBounds(vxBase, vertexCount, shared.Vertices.Length) ||
                !RangeInBounds(vyBase, vertexCount, shared.Vertices.Length) ||
                !RangeInBounds(dxBase, vertexCount, shared.Deform.Length) ||
                !RangeInBounds(dyBase, vertexCount, shared.Deform.Length))
            {
                return false;
            }

            var any = false;
            var mvp = MulMat4(packet.RenderMatrix, packet.ModelMatrix);
            for (var i = 0; i < vertexCount; i++)
            {
                var px = shared.Vertices[vxBase + i] + shared.Deform[dxBase + i] - packet.Origin.X;
                var py = shared.Vertices[vyBase + i] + shared.Deform[dyBase + i] - packet.Origin.Y;
                var clip = MulMat4Vec4(mvp, px, py, 0f, 1f);
                var invW = Mathf.Approximately(clip.w, 0f) ? 1f : (1f / clip.w);
                var x = clip.x * invW;
                var y = clip.y * invW;
                applyCompositeTransform(ref x, ref y);
                bounds = ExpandBounds(bounds, x, y);
                any = true;
            }

            return any;
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
        }

        public void bindDrawableVao()
        {
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
            PropertyConfig propertyConfig,
            bool skipPresentToBackbuffer = false)
        {
            if (buffer == null || camera == null || partMaterial == null || maskMaterial == null)
            {
                return;
            }

            _backbufferTarget = ResolveBackbufferTarget(camera);
            _clipSpaceDrawMatrix = Matrix4x4.identity;
            _disableStencilDebug =
                EnableDiagnostics &&
                !Application.isPlaying &&
                camera.cameraType == CameraType.SceneView;
            _mirrorPresentToCurrentActive =
                Application.isPlaying &&
                camera.cameraType == CameraType.Game &&
                camera.targetTexture == null;
#if UNITY_EDITOR
            _queueSceneDebugDraws =
                (!Application.isPlaying && UnityEditor.SceneView.lastActiveSceneView != null) ||
                (Application.isPlaying && EnableRuntimeSceneDebugCapture && camera.cameraType == CameraType.Game);
#else
            _queueSceneDebugDraws = Application.isPlaying && EnableRuntimeSceneDebugCapture && camera.cameraType == CameraType.Game;
#endif
            _sceneDebugSignature = 1469598103934665603UL;
            ClearSceneDebugDraws();
            LastPartPacketCount = 0;
            LastMaskPacketCount = 0;
            LastPartDrawIssuedCount = 0;
            LastMaskDrawIssuedCount = 0;
            LastPartSkippedNoTextureCount = 0;
            LastPartSkippedMeshBuildCount = 0;
            LastMaskSkippedMeshBuildCount = 0;
            LastPartOpacityMin = float.PositiveInfinity;
            LastPartOpacityMax = float.NegativeInfinity;
            LastPartOpacityZeroCount = 0;
            LastPartOpacityNonZeroCount = 0;
            LastPartTextureMissingCount = 0;
            LastPartClipBounds = Vector4.zero;
            LastMaskClipBounds = Vector4.zero;
            HasPartClipBounds = false;
            HasMaskClipBounds = false;
            LastPartBuildDiag = string.Empty;
            LastRenderPathDiag = string.Empty;
            LastRootSceneDiag = string.Empty;
            _currentPartPacketOrdinal = 0;
            _partMeshPoolUsed = 0;
            _maskMeshPoolUsed = 0;
            _worstPartAbsClip = float.NegativeInfinity;

            buffer.Clear();
            _activeBuffer = buffer;
            buffer.SetViewProjectionMatrices(Matrix4x4.identity, Matrix4x4.identity);
            buffer.SetGlobalVector("_SceneAmbientLight", Vector4.one);
            EnsureRootSceneTexture(camera.pixelWidth, camera.pixelHeight);
            _renderTargetStack.Clear();
            _renderTargetStack.Push(_rootSceneTarget);
            _viewportStack.Clear();
            _dynamicPassStack.Clear();
            _inMaskPass = false;
            _inMaskContent = false;
            _maskUsesStencil = false;
            _maskContentStencilRef = 1;
            _dynamicCompositeDepth = 0;
            _advancedBlending = false;
            _advancedBlendingCoherent = false;
            _currentBlendMode = NicxliveNative.BlendMode.Normal;
            _currentLegacyBlendOnly = true;
            _currentAdvancedBlendEnabled = false;
            _currentSrcBlend = (int)UnityEngine.Rendering.BlendMode.One;
            _currentDstBlend = (int)UnityEngine.Rendering.BlendMode.OneMinusSrcAlpha;
            _currentSrcBlendAlpha = (int)UnityEngine.Rendering.BlendMode.One;
            _currentDstBlendAlpha = (int)UnityEngine.Rendering.BlendMode.OneMinusSrcAlpha;
            _currentBlendOp = (int)UnityEngine.Rendering.BlendOp.Add;
            _currentBlendOpAlpha = (int)UnityEngine.Rendering.BlendOp.Add;
            _forceRootSceneVisiblePass = false;

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
                        LastPartPacketCount++;
                        _currentPartPacketOrdinal = LastPartPacketCount;
                        drawPartPacket(buffer, shared, textures, partMaterial, propertyConfig, command.PartPacket);
                        break;
                    case NicxliveNative.NjgRenderCommandKind.BeginMask:
                        beginMask(buffer, command.UsesStencil);
                        break;
                    case NicxliveNative.NjgRenderCommandKind.ApplyMask:
                        LastMaskPacketCount++;
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
            if (!skipPresentToBackbuffer)
            {
                presentSceneToBackbuffer(buffer, camera.pixelWidth, camera.pixelHeight);
            }
            if (_disableStencilDebug && LastPartDrawIssuedCount > 0)
            {
                drawDebugOverlay(buffer, partMaterial, propertyConfig);
            }
            _renderTargetStack.Clear();
            _renderTargetStack.Push(_backbufferTarget);
            endScene(buffer);
            var gpuProj = GL.GetGPUProjectionMatrix(camera.projectionMatrix, camera.targetTexture != null);
            buffer.SetViewProjectionMatrices(camera.worldToCameraMatrix, gpuProj);
            _activeBuffer = null;
            _mirrorPresentToCurrentActive = false;

            if (EnableDiagnostics && _worstPartAbsClip > 1.25f && LastPartDrawIssuedCount > 0)
            {
                if (_partBoundsWarnCooldown <= 0)
                {
                    Debug.LogWarning($"[nicxlive] Part clip anomaly: {LastPartBuildDiag}");
                    _partBoundsWarnCooldown = 120;
                }
                else
                {
                    _partBoundsWarnCooldown--;
                }
            }
            else
            {
                _partBoundsWarnCooldown = 0;
            }

            if (EnableDiagnostics)
            {
                LastRenderPathDiag =
                    $"Backbuffer={_backbufferTarget}, Root={_rootSceneTarget}, RootMrt={(_rootSceneColorTargets != null ? _rootSceneColorTargets.Length : 0)}, RootDepth={(_rootSceneDepthTexture != null ? 1 : 0)}, PartMat={partMaterial.shader.name}, MaskMat={maskMaterial.shader.name}, " +
                    $"SkipPresent={(skipPresentToBackbuffer ? 1 : 0)}, " +
                    $"MirrorCurrentActive={(_mirrorPresentToCurrentActive ? 1 : 0)}, " +
                    $"StencilDebugOff={(_disableStencilDebug ? 1 : 0)}, " +
                    $"PartDraws={LastPartDrawIssuedCount}, MaskDraws={LastMaskDrawIssuedCount}";
            }
            else
            {
                LastRenderPathDiag = string.Empty;
            }
            LastSceneDebugSignature = _sceneDebugSignature;
        }

        public void UpdateRootSceneDiag(Vector4 clipBounds, bool hasClipBounds)
        {
            if (!EnableDiagnostics)
            {
                LastRootSceneDiag = string.Empty;
                return;
            }

            if (_rootSceneTexture == null || !_rootSceneTexture.IsCreated())
            {
                LastRootSceneDiag = "RootSample=Unavailable";
                return;
            }

            var centerUv = new Vector2(0.5f, 0.5f);
            var boundsUv = centerUv;
            if (hasClipBounds)
            {
                var clipCenterX = 0.5f * (clipBounds.x + clipBounds.z);
                var clipCenterY = 0.5f * (clipBounds.y + clipBounds.w);
                boundsUv = new Vector2(
                    Mathf.Clamp01((clipCenterX * 0.5f) + 0.5f),
                    Mathf.Clamp01((clipCenterY * 0.5f) + 0.5f));
            }

            var center = SampleRenderTexture(_rootSceneTexture, centerUv);
            var boundsCenter = SampleRenderTexture(_rootSceneTexture, boundsUv);
            var hasColor =
                center.maxColorComponent > 0.01f ||
                center.a > 0.01f ||
                boundsCenter.maxColorComponent > 0.01f ||
                boundsCenter.a > 0.01f;

            LastRootSceneDiag =
                $"RootSample=Center({center.r:F3},{center.g:F3},{center.b:F3},{center.a:F3}) " +
                $"BoundsCenter@({boundsUv.x:F3},{boundsUv.y:F3})=({boundsCenter.r:F3},{boundsCenter.g:F3},{boundsCenter.b:F3},{boundsCenter.a:F3}) " +
                $"HasColor={(hasColor ? 1 : 0)}";
        }

        private static Color SampleRenderTexture(RenderTexture texture, Vector2 uv)
        {
            var prev = RenderTexture.active;
            var read = new Texture2D(1, 1, TextureFormat.RGBA32, false, true);
            try
            {
                RenderTexture.active = texture;
                var pixelX = Mathf.Clamp(Mathf.RoundToInt(uv.x * Mathf.Max(0, texture.width - 1)), 0, Mathf.Max(0, texture.width - 1));
                var pixelY = Mathf.Clamp(Mathf.RoundToInt(uv.y * Mathf.Max(0, texture.height - 1)), 0, Mathf.Max(0, texture.height - 1));
                read.ReadPixels(new Rect(pixelX, pixelY, 1, 1), 0, 0, false);
                read.Apply(false, false);
                return read.GetPixel(0, 0);
            }
            finally
            {
                UnityObjectUtil.DestroyObject(read);
                RenderTexture.active = prev;
            }
        }

        private static RenderTargetIdentifier ResolveBackbufferTarget(Camera camera)
        {
            if (camera != null && camera.targetTexture != null)
            {
                return new RenderTargetIdentifier(camera.targetTexture);
            }

            // URP does not reliably preserve CurrentActive at endCameraRendering.
            // Route directly to the camera target unless an explicit RT is bound.
            return new RenderTargetIdentifier(BuiltinRenderTextureType.CameraTarget);
        }

        private static Matrix4x4 ResolveClipSpaceDrawMatrix(Camera camera)
        {
            if (camera == null)
            {
                return Matrix4x4.identity;
            }

            var toRt = camera.targetTexture != null;
            var gpuProj = GL.GetGPUProjectionMatrix(camera.projectionMatrix, toRt);
            var vp = gpuProj * camera.worldToCameraMatrix;
            if (Mathf.Abs(vp.determinant) <= 1e-8f)
            {
                return Matrix4x4.identity;
            }

            return vp.inverse;
        }

        public void rebindActiveTargets(CommandBuffer buffer)
        {
            if (_renderTargetStack.Count == 0)
            {
                _renderTargetStack.Push(_backbufferTarget);
            }
            var current = _renderTargetStack.Peek();
            if (current.Equals(_rootSceneTarget) &&
                _rootSceneTexture != null &&
                _rootSceneColorTargets != null)
            {
                var depthTarget = _rootSceneDepthTexture != null
                    ? new RenderTargetIdentifier(_rootSceneDepthTexture)
                    : _rootSceneTarget;
                buffer.SetRenderTarget(_rootSceneColorTargets, depthTarget);
                return;
            }

            buffer.SetRenderTarget(current);
        }

        public void beginScene(CommandBuffer buffer, int width, int height)
        {
            setViewport(width, height);
            rebindActiveTargets(buffer);
            pushViewport(width, height);
            if (_renderTargetStack.Count > 0 && _renderTargetStack.Peek().Equals(_rootSceneTarget))
            {
                buffer.ClearRenderTarget(RTClearFlags.All, Color.clear, 1.0f, 0);
            }
            else if (_renderTargetStack.Count > 0 && !_renderTargetStack.Peek().Equals(_backbufferTarget))
            {
                buffer.ClearRenderTarget(RTClearFlags.Color, Color.clear, 1.0f, 0);
            }
        }

        public void endScene(CommandBuffer buffer)
        {
            popViewport();
            rebindActiveTargets(buffer);
        }

        public void postProcessScene(CommandBuffer buffer)
        {
            var current = _renderTargetStack.Count > 0 ? _renderTargetStack.Peek() : _backbufferTarget;
            if (current.Equals(_backbufferTarget))
            {
                return;
            }
            buffer.GetTemporaryRT(_postTmpAId, _viewportW, _viewportH, 0, FilterMode.Bilinear, RenderTextureFormat.ARGB32, RenderTextureReadWrite.Linear);
            buffer.GetTemporaryRT(_postTmpBId, _viewportW, _viewportH, 0, FilterMode.Bilinear, RenderTextureFormat.ARGB32, RenderTextureReadWrite.Linear);
            buffer.Blit(current, _postTmpAId);
            buffer.Blit(_postTmpAId, _postTmpBId);
            buffer.Blit(_postTmpBId, current);
            buffer.ReleaseTemporaryRT(_postTmpAId);
            buffer.ReleaseTemporaryRT(_postTmpBId);
        }

        public void presentSceneToBackbuffer(CommandBuffer buffer, int width, int height)
        {
            rebindActiveTargets(buffer);
            var size = new Vector4(Mathf.Max(1, width), Mathf.Max(1, height), 0, 0);
            buffer.SetGlobalVector("_PresentFramebufferSize", size);
            var src = _renderTargetStack.Count > 0 ? _renderTargetStack.Peek() : _backbufferTarget;
            buffer.SetRenderTarget(_backbufferTarget);
            if (!src.Equals(_backbufferTarget))
            {
                ensurePresentProgram();
                var canPresentRootWithAlpha =
                    src.Equals(_rootSceneTarget) &&
                    _rootSceneTexture != null &&
                    _presentMaterial != null &&
                    _presentMesh != null;
                if (canPresentRootWithAlpha)
                {
                    void drawTo(RenderTargetIdentifier target)
                    {
                        buffer.SetRenderTarget(target);
                        _props.Clear();
                        _props.SetTexture("_MainTex", _rootSceneTexture);
                        buffer.DrawMesh(_presentMesh, Matrix4x4.identity, _presentMaterial, 0, 0, _props);
                    }

                    drawTo(_backbufferTarget);
                    if (_mirrorPresentToCurrentActive)
                    {
                        drawTo(new RenderTargetIdentifier(BuiltinRenderTextureType.CurrentActive));
                    }
                }
                else
                {
                    buffer.Blit(src, _backbufferTarget);
                    if (_mirrorPresentToCurrentActive)
                    {
                        buffer.Blit(src, BuiltinRenderTextureType.CurrentActive);
                    }
                }
            }
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
            var shader = Shader.Find("Hidden/Nicxlive/Present");
            if (shader == null)
            {
                throw new InvalidOperationException("Hidden/Nicxlive/Present shader not found.");
            }
            _presentMaterial = new Material(shader)
            {
                name = "nicxlive_present_program",
                hideFlags = HideFlags.HideAndDontSave
            };
            _presentMaterial.SetInt("_Cull", 0);
            _presentMaterial.SetFloat("_ZWrite", 0.0f);
            _presentMaterial.renderQueue = (int)RenderQueue.Transparent;
            ApplyRenderStateToMaterial(
                _presentMaterial,
                (int)UnityEngine.Rendering.BlendMode.One,
                (int)UnityEngine.Rendering.BlendMode.OneMinusSrcAlpha,
                (int)UnityEngine.Rendering.BlendMode.One,
                (int)UnityEngine.Rendering.BlendMode.OneMinusSrcAlpha,
                (int)UnityEngine.Rendering.BlendOp.Add,
                (int)UnityEngine.Rendering.BlendOp.Add,
                1,
                (int)CompareFunction.Always,
                (int)StencilOp.Keep,
                15);
            _presentMesh = buildFullscreenTriangleMesh();
            _presentProgramHandle = _presentMaterial.GetInstanceID();
            _presentProgramInitialized = true;
        }

        private static Mesh buildFullscreenTriangleMesh()
        {
            var mesh = new Mesh
            {
                name = "nicxlive_present_fullscreen_triangle"
            };
            mesh.SetVertices(new[]
            {
                new Vector3(-1f, -1f, 0f),
                new Vector3(-1f, 3f, 0f),
                new Vector3(3f, -1f, 0f),
            });
            mesh.SetUVs(0, new[]
            {
                new Vector2(0f, 0f),
                new Vector2(0f, 2f),
                new Vector2(2f, 0f),
            });
            mesh.SetIndices(new[] { 0, 1, 2 }, MeshTopology.Triangles, 0, false);
            return mesh;
        }

        private Mesh buildDebugOverlayMesh()
        {
            var mesh = new Mesh
            {
                name = "nicxlive_debug_overlay_triangle"
            };
            mesh.SetVertices(new[]
            {
                new Vector3(-1.0f, 1.0f, 0f),
                new Vector3(-0.75f, 1.0f, 0f),
                new Vector3(-1.0f, 0.75f, 0f),
            });
            mesh.SetUVs(0, new[]
            {
                new Vector2(0f, 0f),
                new Vector2(1f, 0f),
                new Vector2(0f, 1f),
            });
            mesh.SetIndices(new[] { 0, 1, 2 }, MeshTopology.Triangles, 0, false);
            return mesh;
        }

        private void drawDebugOverlay(CommandBuffer buffer, Material partMaterial, PropertyConfig cfg)
        {
            if (buffer == null || partMaterial == null)
            {
                return;
            }

            _debugOverlayMesh ??= buildDebugOverlayMesh();
            _props.Clear();
            _props.SetTexture(cfg.MainTex, Texture2D.whiteTexture);
            _props.SetTexture("_BaseMap", Texture2D.whiteTexture);
            _props.SetTexture(cfg.EmissiveTex, Texture2D.blackTexture);
            _props.SetTexture(cfg.BumpTex, Texture2D.blackTexture);
            _props.SetFloat(cfg.Opacity, 1.0f);
            _props.SetFloat(cfg.EmissionStrength, 0.0f);
            _props.SetFloat(cfg.MaskThreshold, 0.0f);
            _props.SetFloat(cfg.BlendMode, 0.0f);
            _props.SetVector(cfg.MultColor, new Vector4(1f, 0f, 0f, 1f));
            _props.SetVector(cfg.ScreenColor, Vector4.zero);
            _props.SetFloat(cfg.StencilRef, 1.0f);
            _props.SetFloat(cfg.StencilComp, (float)CompareFunction.Always);
            _props.SetFloat(cfg.StencilPass, (float)StencilOp.Keep);
            _props.SetFloat(cfg.ColorMask, 15.0f);
            _props.SetFloat(cfg.IsMaskPass, 0.0f);
            _props.SetFloat(cfg.LegacyBlendOnly, 1.0f);
            _props.SetFloat(cfg.AdvancedBlend, 0.0f);
            _props.SetFloat("_DebugForceOpaque", 1.0f);
            buffer.DrawMesh(_debugOverlayMesh, Matrix4x4.identity, partMaterial, 0, 0, _props);
        }

        private void drawRuntimeDebugOverlay(CommandBuffer buffer)
        {
            if (buffer == null || _sceneDebugDraws.Count == 0)
            {
                return;
            }

            EnsureSceneDebugMaterial();
            if (_sceneDebugMaterial == null)
            {
                return;
            }

            void drawTo(RenderTargetIdentifier target)
            {
                buffer.SetRenderTarget(target);
                for (var i = 0; i < _sceneDebugDraws.Count; i++)
                {
                    var draw = _sceneDebugDraws[i];
                    var clipMesh = draw.ClipMesh;
                    if (clipMesh == null || clipMesh.vertexCount == 0)
                    {
                        continue;
                    }

                    _props.Clear();
                    _props.SetTexture("_MainTex", draw.MainTexture != null ? draw.MainTexture : _whiteTexture);
                    _props.SetTexture("_BaseMap", draw.MainTexture != null ? draw.MainTexture : _whiteTexture);
                    _props.SetTexture("_EmissionTex", draw.EmissionTexture != null ? draw.EmissionTexture : Texture2D.blackTexture);
                    _props.SetFloat("_Opacity", Mathf.Clamp01(draw.Opacity));
                    _props.SetFloat("_EmissionStrength", Mathf.Max(0f, draw.EmissionStrength));
                    _props.SetVector("_MultColor", draw.MultColor);
                    _props.SetVector("_ScreenColor", draw.ScreenColor);
                    _props.SetFloat("_StencilRef", 1.0f);
                    _props.SetFloat("_StencilComp", (float)CompareFunction.Always);
                    _props.SetFloat("_StencilPass", (float)StencilOp.Keep);
                    _props.SetFloat("_ColorMask", 15.0f);
                    _props.SetFloat("_DebugForceOpaque", 0.0f);
                    _props.SetFloat("_DebugFlipY", 1.0f);
                    _props.SetFloat("_DebugFlipV", 0.0f);
                    _props.SetFloat("_DebugShowAlbedo", 1.0f);
                    buffer.DrawMesh(clipMesh, Matrix4x4.identity, _sceneDebugMaterial, 0, 0, _props);
                }
            }

            drawTo(_backbufferTarget);
            if (_mirrorPresentToCurrentActive)
            {
                drawTo(new RenderTargetIdentifier(BuiltinRenderTextureType.CurrentActive));
            }
        }

        private void QueueSceneDebugDraw(
            Mesh mesh,
            Material partMaterial,
            PropertyConfig cfg,
            TextureRegistry textures,
            NicxliveNative.NjgPartDrawPacket packet,
            Texture t0,
            Texture t1,
            Texture t2)
        {
            _ = partMaterial;
            _ = cfg;
            _ = t2;
            AccumulateSceneDebugSignature(mesh);
            _sceneDebugDraws.Add(new SceneDebugDraw
            {
                ClipMesh = mesh,
                MainTexture = t0,
                EmissionTexture = t1,
                MainWrap = (float)textures.GetWrapping((ulong)packet.TextureHandle0),
                EmissionWrap = (float)textures.GetWrapping((ulong)packet.TextureHandle1),
                BumpWrap = (float)textures.GetWrapping((ulong)packet.TextureHandle2),
                Opacity = packet.Opacity,
                EmissionStrength = packet.EmissionStrength,
                MaskThreshold = packet.MaskThreshold,
                MultColor = new Vector4(packet.ClampedTint.X, packet.ClampedTint.Y, packet.ClampedTint.Z, 1f),
                ScreenColor = new Vector4(packet.ClampedScreen.X, packet.ClampedScreen.Y, packet.ClampedScreen.Z, 1f),
                StencilRef = _currentStencilRef,
                StencilComp = _currentStencilComp,
                StencilPass = _currentStencilPass,
                ColorMask = _currentColorMask,
                UseMaskMaterial = false,
                ShowAlbedo = true,
            });
        }

        private void QueueSceneDebugMaskDraw(Mesh mesh)
        {
            AccumulateSceneDebugSignature(mesh);
            _sceneDebugDraws.Add(new SceneDebugDraw
            {
                ClipMesh = mesh,
                StencilRef = _currentStencilRef,
                StencilComp = _currentStencilComp,
                StencilPass = _currentStencilPass,
                ColorMask = _currentColorMask,
                UseMaskMaterial = true,
                ShowAlbedo = false,
            });
        }

        private void AccumulateSceneDebugSignature(Mesh mesh)
        {
            if (mesh == null)
            {
                return;
            }

            unchecked
            {
                ulong hash = _sceneDebugSignature;
                static ulong Mix(ulong current, int value)
                {
                    current ^= (uint)value;
                    current *= 1099511628211UL;
                    return current;
                }

                hash = Mix(hash, mesh.vertexCount);
                var triangles = mesh.triangles;
                hash = Mix(hash, triangles.Length);

                var vertices = mesh.vertices;
                var sampleCount = Mathf.Min(vertices.Length, 32);
                if (sampleCount > 0)
                {
                    var step = Mathf.Max(1, vertices.Length / sampleCount);
                    for (var i = 0; i < vertices.Length; i += step)
                    {
                        var v = vertices[i];
                        hash = Mix(hash, Mathf.RoundToInt(v.x * 1000f));
                        hash = Mix(hash, Mathf.RoundToInt(v.y * 1000f));
                    }
                }

                var b = mesh.bounds;
                hash = Mix(hash, Mathf.RoundToInt(b.min.x * 1000f));
                hash = Mix(hash, Mathf.RoundToInt(b.min.y * 1000f));
                hash = Mix(hash, Mathf.RoundToInt(b.max.x * 1000f));
                hash = Mix(hash, Mathf.RoundToInt(b.max.y * 1000f));
                _sceneDebugSignature = hash;
            }
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
            var maskWriteStencilRef = packet.IsDodge ? (byte)0 : (byte)1;
            if (packet.Kind == NicxliveNative.MaskDrawableKind.Part)
            {
                drawPart(buffer, shared, textures, partMaterial, cfg, packet.PartPacket, true, maskWriteStencilRef, true);
            }
            else
            {
                executeMaskPacket(buffer, shared, maskMaterial, cfg, packet.MaskPacket, true, maskWriteStencilRef);
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
            rebindActiveTargets(buffer);
        }

        public void drawPartPacket(
            CommandBuffer buffer,
            SharedBuffers shared,
            TextureRegistry textures,
            Material partMaterial,
            PropertyConfig cfg,
            NicxliveNative.NjgPartDrawPacket packet)
        {
            if (!packet.Renderable)
            {
                return;
            }
            if (packet.TextureCount == 0)
            {
                LastPartSkippedNoTextureCount++;
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
            _currentShaderStage = stage;
            switch (stage)
            {
                case 0:
                    applyBlendMode(cfg, (NicxliveNative.BlendMode)packet.BlendingMode, false);
                    break;
                case 1:
                case 2:
                    applyBlendMode(cfg, (NicxliveNative.BlendMode)packet.BlendingMode, true);
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
            if (((!fromMaskApply && !packet.Renderable) || packet.IndexCount == 0 || packet.VertexCount == 0))
            {
                return;
            }

            var previousAccumulateCurrentPartBounds = _accumulateCurrentPartBounds;
            _accumulateCurrentPartBounds =
                _dynamicCompositeDepth == 0 &&
                IsCurrentRenderTargetRootScene() &&
                !forceStencilWrite &&
                !fromMaskApply &&
                !packet.IsMask;
            var mesh = BuildPartMesh(shared, packet);
            _accumulateCurrentPartBounds = previousAccumulateCurrentPartBounds;
            if (mesh == null)
            {
                LastPartSkippedMeshBuildCount++;
                return;
            }

            _props.Clear();
            _props.SetFloat(cfg.ShaderStage, _currentShaderStage);
            _props.SetFloat(cfg.BlendMode, (float)_currentBlendMode);
            var legacyBlendOnly = fromMaskApply || _currentLegacyBlendOnly;
            _props.SetFloat(cfg.LegacyBlendOnly, legacyBlendOnly ? 1.0f : 0.0f);
            _props.SetFloat(cfg.AdvancedBlend, (!legacyBlendOnly && _currentAdvancedBlendEnabled) ? 1.0f : 0.0f);
            SetBlendState(cfg, _currentBlendMode);
            _props.SetFloat(cfg.Opacity, packet.Opacity);
            _props.SetFloat(cfg.EmissionStrength, packet.EmissionStrength);
            _props.SetFloat(cfg.MaskThreshold, packet.MaskThreshold);
            _props.SetVector(cfg.MultColor, new Vector4(packet.ClampedTint.X, packet.ClampedTint.Y, packet.ClampedTint.Z, 1));
            _props.SetVector(cfg.ScreenColor, new Vector4(packet.ClampedScreen.X, packet.ClampedScreen.Y, packet.ClampedScreen.Z, 1));

            LastPartOpacityMin = Mathf.Min(LastPartOpacityMin, packet.Opacity);
            LastPartOpacityMax = Mathf.Max(LastPartOpacityMax, packet.Opacity);
            if (packet.Opacity <= 1e-4f)
            {
                LastPartOpacityZeroCount++;
            }
            else
            {
                LastPartOpacityNonZeroCount++;
            }

            var t0Src = textures.TryGet((ulong)packet.TextureHandle0);
            var t1Src = textures.TryGet((ulong)packet.TextureHandle1);
            var t2Src = textures.TryGet((ulong)packet.TextureHandle2);
            var t0 = t0Src ?? _whiteTexture;
            var t1 = t1Src ?? _whiteTexture;
            var t2 = t2Src ?? _whiteTexture;
            if (t0Src == null)
            {
                LastPartTextureMissingCount++;
            }
            _props.SetTexture(cfg.MainTex, t0);
            _props.SetTexture("_BaseMap", t0);
            _props.SetColor("_BaseColor", new Color(1f, 1f, 1f, Mathf.Clamp01(packet.Opacity)));
            _props.SetTexture(cfg.EmissiveTex, t1);
            _props.SetTexture(cfg.BumpTex, t2);
            _props.SetFloat(cfg.MainWrap, (float)textures.GetWrapping((ulong)packet.TextureHandle0));
            _props.SetFloat(cfg.EmissiveWrap, (float)textures.GetWrapping((ulong)packet.TextureHandle1));
            _props.SetFloat(cfg.BumpWrap, (float)textures.GetWrapping((ulong)packet.TextureHandle2));
            _props.SetFloat("_DebugForceOpaque", 0.0f);
            _props.SetFloat("_DebugFlipY", 1.0f);
            _props.SetFloat("_DebugFlipV", 0.0f);
            _props.SetFloat("_DebugShowAlbedo", _forceRootSceneVisiblePass ? 1.0f : 0.0f);
            if (_forceRootSceneVisiblePass)
            {
                _props.SetFloat(cfg.BlendMode, (float)NicxliveNative.BlendMode.Normal);
                _props.SetFloat(cfg.LegacyBlendOnly, 1.0f);
                _props.SetFloat(cfg.AdvancedBlend, 0.0f);
                SetStandardAlphaBlendState(cfg);
            }

            ConfigureStencil(cfg, forceStencilWrite, stencilRef);
            var isMaskSourceDraw = packet.IsMask || forceStencilWrite || fromMaskApply;
            _props.SetFloat(cfg.IsMaskPass, isMaskSourceDraw ? 1.0f : 0.0f);
            if (_inMaskPass && _inMaskContent && !forceStencilWrite)
            {
                ConfigureStencilTest(cfg, _maskContentStencilRef);
            }

            var drawMaterial = partMaterial;
            var drawRootVisiblePass =
                _forceRootSceneVisiblePass &&
                _rootSceneTexture != null &&
                IsCurrentRenderTargetRootScene();
            if (_forceRootSceneVisiblePass)
            {
                EnsureSceneDebugMaterial();
                if (_sceneDebugMaterial != null)
                {
                    drawMaterial = _sceneDebugMaterial;
                    ApplySceneDebugProperties(
                        _props,
                        new SceneDebugDraw
                        {
                            ClipMesh = mesh,
                            MainTexture = t0,
                            EmissionTexture = t1,
                            Opacity = packet.Opacity,
                            EmissionStrength = packet.EmissionStrength,
                            MaskThreshold = packet.MaskThreshold,
                            MultColor = new Vector4(packet.ClampedTint.X, packet.ClampedTint.Y, packet.ClampedTint.Z, 1f),
                            ScreenColor = new Vector4(packet.ClampedScreen.X, packet.ClampedScreen.Y, packet.ClampedScreen.Z, 1f),
                            StencilRef = _currentStencilRef,
                            StencilComp = _currentStencilComp,
                            StencilPass = _currentStencilPass,
                            ColorMask = _currentColorMask,
                            UseMaskMaterial = false,
                            ShowAlbedo = true,
                        });
                }
            }

            if (drawRootVisiblePass)
            {
                var depthTarget = _rootSceneDepthTexture != null
                    ? new RenderTargetIdentifier(_rootSceneDepthTexture)
                    : _rootSceneTarget;
                buffer.SetRenderTarget(_rootSceneTarget, depthTarget);
            }
            drawMaterial = ResolveRenderStateMaterial(drawMaterial);
            buffer.DrawMesh(mesh, _clipSpaceDrawMatrix, drawMaterial, 0, 0, _props);
            if (drawRootVisiblePass)
            {
                rebindActiveTargets(buffer);
            }
            var queueSceneDebugDraw =
                (_disableStencilDebug || _queueSceneDebugDraws) &&
                _dynamicCompositeDepth == 0 &&
                IsCurrentRenderTargetRootScene() &&
                !packet.UseMultistageBlend &&
                _currentShaderStage == 2;
            if (queueSceneDebugDraw)
            {
                QueueSceneDebugDraw(mesh, partMaterial, cfg, textures, packet, t0, t1, t2);
            }
            LastPartDrawIssuedCount++;
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
                LastMaskSkippedMeshBuildCount++;
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

            var drawMaterial = ResolveRenderStateMaterial(maskMaterial);
            buffer.DrawMesh(mesh, _clipSpaceDrawMatrix, drawMaterial, 0, 0, _props);
            if ((_disableStencilDebug || _queueSceneDebugDraws) &&
                _dynamicCompositeDepth == 0 &&
                IsCurrentRenderTargetRootScene())
            {
                QueueSceneDebugMaskDraw(mesh);
            }
            LastMaskDrawIssuedCount++;
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
            _currentBlendMode = mode;
            _currentLegacyBlendOnly = true;
            _currentAdvancedBlendEnabled = false;
            _props.SetFloat(cfg.BlendMode, (float)mode);
            _props.SetFloat(cfg.LegacyBlendOnly, 1.0f);
            _props.SetFloat(cfg.AdvancedBlend, 0.0f);
            SetBlendState(cfg, mode);
        }

        public void setAdvancedBlendEquation(PropertyConfig cfg, NicxliveNative.BlendMode mode)
        {
            // Unity path maps advanced mode selection to shader properties.
            _currentBlendMode = mode;
            _currentLegacyBlendOnly = false;
            _currentAdvancedBlendEnabled = true;
            _props.SetFloat(cfg.BlendMode, (float)mode);
            _props.SetFloat(cfg.LegacyBlendOnly, 0.0f);
            _props.SetFloat(cfg.AdvancedBlend, 1.0f);
            SetBlendState(cfg, mode);
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
            if (_disableStencilDebug)
            {
                SetStencilState(cfg, 1.0f, (float)CompareFunction.Always, (float)StencilOp.Keep, 15.0f);
                return;
            }

            if (forceStencilWrite)
            {
                SetStencilState(cfg, stencilRef, (float)CompareFunction.Always, (float)StencilOp.Replace, 0.0f);
                return;
            }

            SetStencilState(cfg, 1.0f, (float)CompareFunction.Always, (float)StencilOp.Keep, 15.0f);

            if (_inMaskPass && !_inMaskContent && !_maskUsesStencil)
            {
                SetStencilState(cfg, 1.0f, (float)CompareFunction.Always, (float)StencilOp.Keep, 15.0f);
            }
        }

        private void ConfigureStencilTest(PropertyConfig cfg, byte stencilRef)
        {
            if (_disableStencilDebug)
            {
                SetStencilState(cfg, 1.0f, (float)CompareFunction.Always, (float)StencilOp.Keep, 15.0f);
                return;
            }

            SetStencilState(cfg, stencilRef, (float)CompareFunction.Equal, (float)StencilOp.Keep, 15.0f);
        }

        private void SetStencilState(PropertyConfig cfg, float stencilRef, float stencilComp, float stencilPass, float colorMask)
        {
            _currentStencilRef = stencilRef;
            _currentStencilComp = stencilComp;
            _currentStencilPass = stencilPass;
            _currentColorMask = colorMask;
            _props.SetFloat(cfg.StencilRef, stencilRef);
            _props.SetFloat(cfg.StencilComp, stencilComp);
            _props.SetFloat(cfg.StencilPass, stencilPass);
            _props.SetFloat(cfg.ColorMask, colorMask);
        }

        private Material ResolveRenderStateMaterial(Material baseMaterial)
        {
            return ResolveRenderStateMaterial(
                baseMaterial,
                _currentSrcBlend,
                _currentDstBlend,
                _currentSrcBlendAlpha,
                _currentDstBlendAlpha,
                _currentBlendOp,
                _currentBlendOpAlpha,
                Mathf.RoundToInt(_currentStencilRef),
                Mathf.RoundToInt(_currentStencilComp),
                Mathf.RoundToInt(_currentStencilPass),
                Mathf.RoundToInt(_currentColorMask));
        }

        private Material ResolveRenderStateMaterial(
            Material baseMaterial,
            int srcBlend,
            int dstBlend,
            int srcBlendAlpha,
            int dstBlendAlpha,
            int blendOp,
            int blendOpAlpha,
            int stencilRef,
            int stencilComp,
            int stencilPass,
            int colorMask)
        {
            var key = new MaterialRenderStateKey
            {
                BaseMaterialId = baseMaterial.GetInstanceID(),
                SrcBlend = srcBlend,
                DstBlend = dstBlend,
                SrcBlendAlpha = srcBlendAlpha,
                DstBlendAlpha = dstBlendAlpha,
                BlendOp = blendOp,
                BlendOpAlpha = blendOpAlpha,
                StencilRef = stencilRef,
                StencilComp = stencilComp,
                StencilPass = stencilPass,
                ColorMask = colorMask,
            };
            if (_renderStateMaterialCache.TryGetValue(key, out var cached) && cached != null)
            {
                return cached;
            }

            var material = new Material(baseMaterial)
            {
                name = $"{baseMaterial.name}_state_{srcBlend}_{dstBlend}_{stencilRef}_{stencilComp}_{stencilPass}_{colorMask}",
                hideFlags = HideFlags.HideAndDontSave
            };
            ApplyRenderStateToMaterial(
                material,
                srcBlend,
                dstBlend,
                srcBlendAlpha,
                dstBlendAlpha,
                blendOp,
                blendOpAlpha,
                stencilRef,
                stencilComp,
                stencilPass,
                colorMask);
            _renderStateMaterialCache[key] = material;
            return material;
        }

        private static void ApplyRenderStateToMaterial(
            Material material,
            int srcBlend,
            int dstBlend,
            int srcBlendAlpha,
            int dstBlendAlpha,
            int blendOp,
            int blendOpAlpha,
            int stencilRef,
            int stencilComp,
            int stencilPass,
            int colorMask)
        {
            if (material.HasProperty("_SrcBlend"))
            {
                material.SetFloat("_SrcBlend", srcBlend);
            }
            if (material.HasProperty("_DstBlend"))
            {
                material.SetFloat("_DstBlend", dstBlend);
            }
            if (material.HasProperty("_SrcBlendAlpha"))
            {
                material.SetFloat("_SrcBlendAlpha", srcBlendAlpha);
            }
            if (material.HasProperty("_DstBlendAlpha"))
            {
                material.SetFloat("_DstBlendAlpha", dstBlendAlpha);
            }
            if (material.HasProperty("_BlendOp"))
            {
                material.SetFloat("_BlendOp", blendOp);
            }
            if (material.HasProperty("_BlendOpAlpha"))
            {
                material.SetFloat("_BlendOpAlpha", blendOpAlpha);
            }
            if (material.HasProperty("_StencilRef"))
            {
                material.SetFloat("_StencilRef", stencilRef);
            }
            if (material.HasProperty("_StencilComp"))
            {
                material.SetFloat("_StencilComp", stencilComp);
            }
            if (material.HasProperty("_StencilPass"))
            {
                material.SetFloat("_StencilPass", stencilPass);
            }
            if (material.HasProperty("_ColorMask"))
            {
                material.SetFloat("_ColorMask", colorMask);
            }
        }

        private void SetBlendState(PropertyConfig cfg, NicxliveNative.BlendMode mode)
        {
            var srcColor = UnityEngine.Rendering.BlendMode.One;
            var dstColor = UnityEngine.Rendering.BlendMode.OneMinusSrcAlpha;
            var srcAlpha = UnityEngine.Rendering.BlendMode.One;
            var dstAlpha = UnityEngine.Rendering.BlendMode.OneMinusSrcAlpha;
            var opColor = UnityEngine.Rendering.BlendOp.Add;
            var opAlpha = UnityEngine.Rendering.BlendOp.Add;

            switch (mode)
            {
                case NicxliveNative.BlendMode.Normal:
                    srcAlpha = UnityEngine.Rendering.BlendMode.One;
                    dstAlpha = UnityEngine.Rendering.BlendMode.OneMinusSrcAlpha;
                    break;
                case NicxliveNative.BlendMode.Multiply:
                    srcColor = UnityEngine.Rendering.BlendMode.DstColor;
                    dstColor = UnityEngine.Rendering.BlendMode.OneMinusSrcAlpha;
                    srcAlpha = UnityEngine.Rendering.BlendMode.DstColor;
                    dstAlpha = UnityEngine.Rendering.BlendMode.OneMinusSrcAlpha;
                    break;
                case NicxliveNative.BlendMode.Screen:
                    srcColor = UnityEngine.Rendering.BlendMode.One;
                    dstColor = UnityEngine.Rendering.BlendMode.OneMinusSrcColor;
                    srcAlpha = UnityEngine.Rendering.BlendMode.One;
                    dstAlpha = UnityEngine.Rendering.BlendMode.OneMinusSrcColor;
                    break;
                case NicxliveNative.BlendMode.Lighten:
                    srcColor = UnityEngine.Rendering.BlendMode.One;
                    dstColor = UnityEngine.Rendering.BlendMode.One;
                    srcAlpha = UnityEngine.Rendering.BlendMode.One;
                    dstAlpha = UnityEngine.Rendering.BlendMode.One;
                    opColor = UnityEngine.Rendering.BlendOp.Max;
                    opAlpha = UnityEngine.Rendering.BlendOp.Max;
                    break;
                case NicxliveNative.BlendMode.ColorDodge:
                    srcColor = UnityEngine.Rendering.BlendMode.DstColor;
                    dstColor = UnityEngine.Rendering.BlendMode.One;
                    srcAlpha = UnityEngine.Rendering.BlendMode.DstColor;
                    dstAlpha = UnityEngine.Rendering.BlendMode.One;
                    break;
                case NicxliveNative.BlendMode.LinearDodge:
                    srcColor = UnityEngine.Rendering.BlendMode.One;
                    dstColor = UnityEngine.Rendering.BlendMode.OneMinusSrcColor;
                    srcAlpha = UnityEngine.Rendering.BlendMode.One;
                    dstAlpha = UnityEngine.Rendering.BlendMode.OneMinusSrcAlpha;
                    break;
                case NicxliveNative.BlendMode.AddGlow:
                    srcColor = UnityEngine.Rendering.BlendMode.One;
                    dstColor = UnityEngine.Rendering.BlendMode.One;
                    srcAlpha = UnityEngine.Rendering.BlendMode.One;
                    dstAlpha = UnityEngine.Rendering.BlendMode.OneMinusSrcAlpha;
                    break;
                case NicxliveNative.BlendMode.Subtract:
                    srcColor = UnityEngine.Rendering.BlendMode.OneMinusDstColor;
                    dstColor = UnityEngine.Rendering.BlendMode.One;
                    srcAlpha = UnityEngine.Rendering.BlendMode.OneMinusDstColor;
                    dstAlpha = UnityEngine.Rendering.BlendMode.One;
                    opColor = UnityEngine.Rendering.BlendOp.ReverseSubtract;
                    opAlpha = UnityEngine.Rendering.BlendOp.Add;
                    break;
                case NicxliveNative.BlendMode.Exclusion:
                    srcColor = UnityEngine.Rendering.BlendMode.OneMinusDstColor;
                    dstColor = UnityEngine.Rendering.BlendMode.OneMinusSrcColor;
                    srcAlpha = UnityEngine.Rendering.BlendMode.One;
                    dstAlpha = UnityEngine.Rendering.BlendMode.One;
                    break;
                case NicxliveNative.BlendMode.Inverse:
                    srcColor = UnityEngine.Rendering.BlendMode.OneMinusDstColor;
                    dstColor = UnityEngine.Rendering.BlendMode.OneMinusSrcAlpha;
                    srcAlpha = UnityEngine.Rendering.BlendMode.OneMinusDstColor;
                    dstAlpha = UnityEngine.Rendering.BlendMode.OneMinusSrcAlpha;
                    break;
                case NicxliveNative.BlendMode.DestinationIn:
                    srcColor = UnityEngine.Rendering.BlendMode.Zero;
                    dstColor = UnityEngine.Rendering.BlendMode.SrcAlpha;
                    srcAlpha = UnityEngine.Rendering.BlendMode.Zero;
                    dstAlpha = UnityEngine.Rendering.BlendMode.SrcAlpha;
                    break;
                case NicxliveNative.BlendMode.ClipToLower:
                    srcColor = UnityEngine.Rendering.BlendMode.DstAlpha;
                    dstColor = UnityEngine.Rendering.BlendMode.OneMinusSrcAlpha;
                    srcAlpha = UnityEngine.Rendering.BlendMode.DstAlpha;
                    dstAlpha = UnityEngine.Rendering.BlendMode.OneMinusSrcAlpha;
                    break;
                case NicxliveNative.BlendMode.SliceFromLower:
                    srcColor = UnityEngine.Rendering.BlendMode.Zero;
                    dstColor = UnityEngine.Rendering.BlendMode.OneMinusSrcAlpha;
                    srcAlpha = UnityEngine.Rendering.BlendMode.Zero;
                    dstAlpha = UnityEngine.Rendering.BlendMode.OneMinusSrcAlpha;
                    break;
                case NicxliveNative.BlendMode.Overlay:
                case NicxliveNative.BlendMode.Darken:
                case NicxliveNative.BlendMode.ColorBurn:
                case NicxliveNative.BlendMode.HardLight:
                case NicxliveNative.BlendMode.SoftLight:
                case NicxliveNative.BlendMode.Difference:
                default:
                    // Match the original fallback path when the exact equation is unavailable.
                    srcColor = UnityEngine.Rendering.BlendMode.One;
                    dstColor = UnityEngine.Rendering.BlendMode.OneMinusSrcAlpha;
                    srcAlpha = UnityEngine.Rendering.BlendMode.One;
                    dstAlpha = UnityEngine.Rendering.BlendMode.OneMinusSrcAlpha;
                    break;
            }

            _currentSrcBlend = (int)srcColor;
            _currentDstBlend = (int)dstColor;
            _currentSrcBlendAlpha = (int)srcAlpha;
            _currentDstBlendAlpha = (int)dstAlpha;
            _currentBlendOp = (int)opColor;
            _currentBlendOpAlpha = (int)opAlpha;
            _props.SetFloat(cfg.SrcBlend, (float)srcColor);
            _props.SetFloat(cfg.DstBlend, (float)dstColor);
            _props.SetFloat(cfg.SrcBlendAlpha, (float)srcAlpha);
            _props.SetFloat(cfg.DstBlendAlpha, (float)dstAlpha);
            _props.SetFloat(cfg.BlendOp, (float)opColor);
            _props.SetFloat(cfg.BlendOpAlpha, (float)opAlpha);
        }

        private void SetStandardAlphaBlendState(PropertyConfig cfg)
        {
            _currentSrcBlend = (int)UnityEngine.Rendering.BlendMode.One;
            _currentDstBlend = (int)UnityEngine.Rendering.BlendMode.OneMinusSrcAlpha;
            _currentSrcBlendAlpha = (int)UnityEngine.Rendering.BlendMode.One;
            _currentDstBlendAlpha = (int)UnityEngine.Rendering.BlendMode.OneMinusSrcAlpha;
            _currentBlendOp = (int)UnityEngine.Rendering.BlendOp.Add;
            _currentBlendOpAlpha = (int)UnityEngine.Rendering.BlendOp.Add;
            _props.SetFloat(cfg.SrcBlend, (float)UnityEngine.Rendering.BlendMode.One);
            _props.SetFloat(cfg.DstBlend, (float)UnityEngine.Rendering.BlendMode.OneMinusSrcAlpha);
            _props.SetFloat(cfg.SrcBlendAlpha, (float)UnityEngine.Rendering.BlendMode.One);
            _props.SetFloat(cfg.DstBlendAlpha, (float)UnityEngine.Rendering.BlendMode.OneMinusSrcAlpha);
            _props.SetFloat(cfg.BlendOp, (float)UnityEngine.Rendering.BlendOp.Add);
            _props.SetFloat(cfg.BlendOpAlpha, (float)UnityEngine.Rendering.BlendOp.Add);
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

            if (!TryValidatePartPacketIndices(packet, shared, vertexCount, indexCount, out var partValidationReason))
            {
                if (EnableDiagnostics)
                {
                    LastPartBuildDiag = $"Pkt={_currentPartPacketOrdinal} InvalidPartPacket={partValidationReason}";
                }
                return null;
            }

            if (!RangeInBounds(vxBase, vertexCount, shared.Vertices.Length) ||
                !RangeInBounds(vyBase, vertexCount, shared.Vertices.Length) ||
                !RangeInBounds(uxBase, vertexCount, shared.Uvs.Length) ||
                !RangeInBounds(uyBase, vertexCount, shared.Uvs.Length) ||
                !RangeInBounds(dxBase, vertexCount, shared.Deform.Length) ||
                !RangeInBounds(dyBase, vertexCount, shared.Deform.Length))
            {
                return null;
            }

            var mvp = MulMat4(packet.RenderMatrix, packet.ModelMatrix);

            var reusable = AcquireReusableMeshData(_partMeshPool, ref _partMeshPoolUsed, "nicxlive_part_mesh");
            EnsureMeshArrayCapacity(reusable, vertexCount, indexCount);
            var vertices = reusable.Vertices;
            var uvs = reusable.Uvs;
            var bounds = new Vector4(float.PositiveInfinity, float.PositiveInfinity, float.NegativeInfinity, float.NegativeInfinity);
            for (var i = 0; i < vertexCount; i++)
            {
                var px = shared.Vertices[vxBase + i] + shared.Deform[dxBase + i] - packet.Origin.X;
                var py = shared.Vertices[vyBase + i] + shared.Deform[dyBase + i] - packet.Origin.Y;
                var clip = MulMat4Vec4(mvp, px, py, 0f, 1f);
                var invW = Mathf.Approximately(clip.w, 0f) ? 1f : (1f / clip.w);
                var x = clip.x * invW;
                var y = clip.y * invW;
                ApplyPuppetTransform(ref x, ref y);
                applyCompositeTransform(ref x, ref y);
                bounds = ExpandBounds(bounds, x, y);
                vertices[i] = new Vector3(x, y, 0f);
                uvs[i] = new Vector2(shared.Uvs[uxBase + i], shared.Uvs[uyBase + i]);
            }
            if (float.IsFinite(bounds.x))
            {
                if (_accumulateCurrentPartBounds)
                {
                    LastPartClipBounds = HasPartClipBounds ? MergeBounds(LastPartClipBounds, bounds) : bounds;
                    HasPartClipBounds = true;

                    var absClip = Mathf.Max(
                        Mathf.Max(Mathf.Abs(bounds.x), Mathf.Abs(bounds.y)),
                        Mathf.Max(Mathf.Abs(bounds.z), Mathf.Abs(bounds.w)));
                    if (absClip > _worstPartAbsClip)
                    {
                        _worstPartAbsClip = absClip;
                        if (EnableDiagnostics)
                        {
                            LastPartBuildDiag =
                                $"Pkt={_currentPartPacketOrdinal} Vtx={vertexCount} Idx={indexCount} " +
                                $"Bounds=x:[{bounds.x:F4},{bounds.z:F4}] y:[{bounds.y:F4},{bounds.w:F4}] " +
                                $"Origin=({packet.Origin.X:F3},{packet.Origin.Y:F3}) " +
                                $"MvpScale=({mvp.M11:F4},{mvp.M22:F4}) " +
                                $"ClipAdj=({_clipPuppetTranslation.x:F3},{_clipPuppetTranslation.y:F3}) " +
                                $"ClipScale=({_clipPuppetScale.x:F3},{_clipPuppetScale.y:F3}) " +
                                $"ModelT=({packet.ModelMatrix.M14:F3},{packet.ModelMatrix.M24:F3}) " +
                                $"RenderT=({packet.RenderMatrix.M14:F3},{packet.RenderMatrix.M24:F3}) " +
                                $"MvpT=({mvp.M14:F3},{mvp.M24:F3})";
                        }
                    }
                }
            }

            var triangles = reusable.Indices;
            for (var i = 0; i < indexCount; i++)
            {
                triangles[i] = packet.Indices[i];
            }

            var mesh = reusable.Mesh;
            mesh.indexFormat = vertexCount > 65535 ? IndexFormat.UInt32 : IndexFormat.UInt16;
            mesh.Clear(false);
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

            if (!TryValidateMaskPacketIndices(packet, shared, vertexCount, indexCount))
            {
                return null;
            }

            if (!RangeInBounds(vxBase, vertexCount, shared.Vertices.Length) ||
                !RangeInBounds(vyBase, vertexCount, shared.Vertices.Length) ||
                !RangeInBounds(dxBase, vertexCount, shared.Deform.Length) ||
                !RangeInBounds(dyBase, vertexCount, shared.Deform.Length))
            {
                return null;
            }

            var mvp = SelectBestMaskMvp(packet.Mvp, packet.ModelMatrix, shared, packet, vxBase, vyBase, dxBase, dyBase, vertexCount);
            var reusable = AcquireReusableMeshData(_maskMeshPool, ref _maskMeshPoolUsed, "nicxlive_mask_mesh");
            EnsureMeshArrayCapacity(reusable, vertexCount, indexCount);
            var vertices = reusable.Vertices;
            var uvs = reusable.Uvs;
            var bounds = new Vector4(float.PositiveInfinity, float.PositiveInfinity, float.NegativeInfinity, float.NegativeInfinity);
            for (var i = 0; i < vertexCount; i++)
            {
                var px = shared.Vertices[vxBase + i] + shared.Deform[dxBase + i] - packet.Origin.X;
                var py = shared.Vertices[vyBase + i] + shared.Deform[dyBase + i] - packet.Origin.Y;
                var clip = MulMat4Vec4(mvp, px, py, 0f, 1f);
                var invW = Mathf.Approximately(clip.w, 0f) ? 1f : (1f / clip.w);
                var x = clip.x * invW;
                var y = clip.y * invW;
                ApplyPuppetTransform(ref x, ref y);
                applyCompositeTransform(ref x, ref y);
                bounds = ExpandBounds(bounds, x, y);
                vertices[i] = new Vector3(x, y, 0f);
                uvs[i] = Vector2.zero;
            }
            if (float.IsFinite(bounds.x))
            {
                LastMaskClipBounds = HasMaskClipBounds ? MergeBounds(LastMaskClipBounds, bounds) : bounds;
                HasMaskClipBounds = true;
            }

            var triangles = reusable.Indices;
            for (var i = 0; i < indexCount; i++)
            {
                triangles[i] = packet.Indices[i];
            }

            var mesh = reusable.Mesh;
            mesh.indexFormat = vertexCount > 65535 ? IndexFormat.UInt32 : IndexFormat.UInt16;
            mesh.Clear(false);
            mesh.vertices = vertices;
            mesh.uv = uvs;
            mesh.triangles = triangles;
            mesh.RecalculateBounds();
            return mesh;
        }

        private static ReusableMeshData AcquireReusableMeshData(List<ReusableMeshData> pool, ref int usedCount, string name)
        {
            if (usedCount >= pool.Count)
            {
                pool.Add(new ReusableMeshData($"{name}_{pool.Count}"));
            }

            return pool[usedCount++];
        }

        private static void EnsureMeshArrayCapacity(ReusableMeshData reusable, int vertexCount, int indexCount)
        {
            if (reusable.Vertices.Length != vertexCount)
            {
                reusable.Vertices = new Vector3[vertexCount];
            }
            if (reusable.Uvs.Length != vertexCount)
            {
                reusable.Uvs = new Vector2[vertexCount];
            }
            if (reusable.Indices.Length != indexCount)
            {
                reusable.Indices = new int[indexCount];
            }
        }

        private static unsafe bool TryValidatePartPacketIndices(
            NicxliveNative.NjgPartDrawPacket packet,
            SharedBuffers shared,
            int vertexCount,
            int indexCount,
            out string reason)
        {
            var maxIndex = 0;
            for (var i = 0; i < indexCount; i++)
            {
                var idx = packet.Indices[i];
                if (idx > maxIndex)
                {
                    maxIndex = idx;
                }
            }

            if (maxIndex >= vertexCount)
            {
                reason = $"index out of vertexCount maxIndex={maxIndex} vertexCount={vertexCount} indexCount={indexCount}";
                return false;
            }

            var vxBase = checked((int)packet.VertexOffset);
            var vyBase = checked((int)(packet.VertexOffset + packet.VertexAtlasStride));
            var uxBase = checked((int)packet.UvOffset);
            var uyBase = checked((int)(packet.UvOffset + packet.UvAtlasStride));
            var dxBase = checked((int)packet.DeformOffset);
            var dyBase = checked((int)(packet.DeformOffset + packet.DeformAtlasStride));

            if (vxBase + maxIndex >= shared.Vertices.Length ||
                vyBase + maxIndex >= shared.Vertices.Length ||
                uxBase + maxIndex >= shared.Uvs.Length ||
                uyBase + maxIndex >= shared.Uvs.Length ||
                dxBase + maxIndex >= shared.Deform.Length ||
                dyBase + maxIndex >= shared.Deform.Length)
            {
                reason =
                    $"soa range overflow maxIndex={maxIndex} vertexCount={vertexCount} " +
                    $"vx={vxBase + maxIndex}/{shared.Vertices.Length} vy={vyBase + maxIndex}/{shared.Vertices.Length} " +
                    $"ux={uxBase + maxIndex}/{shared.Uvs.Length} uy={uyBase + maxIndex}/{shared.Uvs.Length} " +
                    $"dx={dxBase + maxIndex}/{shared.Deform.Length} dy={dyBase + maxIndex}/{shared.Deform.Length}";
                return false;
            }

            reason = string.Empty;
            return true;
        }

        private static unsafe bool TryValidateMaskPacketIndices(
            NicxliveNative.NjgMaskDrawPacket packet,
            SharedBuffers shared,
            int vertexCount,
            int indexCount)
        {
            var maxIndex = 0;
            for (var i = 0; i < indexCount; i++)
            {
                var idx = packet.Indices[i];
                if (idx > maxIndex)
                {
                    maxIndex = idx;
                }
            }

            if (maxIndex >= vertexCount)
            {
                return false;
            }

            var vxBase = checked((int)packet.VertexOffset);
            var vyBase = checked((int)(packet.VertexOffset + packet.VertexAtlasStride));
            var dxBase = checked((int)packet.DeformOffset);
            var dyBase = checked((int)(packet.DeformOffset + packet.DeformAtlasStride));

            return
                vxBase + maxIndex < shared.Vertices.Length &&
                vyBase + maxIndex < shared.Vertices.Length &&
                dxBase + maxIndex < shared.Deform.Length &&
                dyBase + maxIndex < shared.Deform.Length;
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

        private static NicxliveNative.Mat4 MulMat4(in NicxliveNative.Mat4 a, in NicxliveNative.Mat4 b)
        {
            NicxliveNative.Mat4 o;
            o.M11 = (a.M11 * b.M11) + (a.M12 * b.M21) + (a.M13 * b.M31) + (a.M14 * b.M41);
            o.M12 = (a.M11 * b.M12) + (a.M12 * b.M22) + (a.M13 * b.M32) + (a.M14 * b.M42);
            o.M13 = (a.M11 * b.M13) + (a.M12 * b.M23) + (a.M13 * b.M33) + (a.M14 * b.M43);
            o.M14 = (a.M11 * b.M14) + (a.M12 * b.M24) + (a.M13 * b.M34) + (a.M14 * b.M44);
            o.M21 = (a.M21 * b.M11) + (a.M22 * b.M21) + (a.M23 * b.M31) + (a.M24 * b.M41);
            o.M22 = (a.M21 * b.M12) + (a.M22 * b.M22) + (a.M23 * b.M32) + (a.M24 * b.M42);
            o.M23 = (a.M21 * b.M13) + (a.M22 * b.M23) + (a.M23 * b.M33) + (a.M24 * b.M43);
            o.M24 = (a.M21 * b.M14) + (a.M22 * b.M24) + (a.M23 * b.M34) + (a.M24 * b.M44);
            o.M31 = (a.M31 * b.M11) + (a.M32 * b.M21) + (a.M33 * b.M31) + (a.M34 * b.M41);
            o.M32 = (a.M31 * b.M12) + (a.M32 * b.M22) + (a.M33 * b.M32) + (a.M34 * b.M42);
            o.M33 = (a.M31 * b.M13) + (a.M32 * b.M23) + (a.M33 * b.M33) + (a.M34 * b.M43);
            o.M34 = (a.M31 * b.M14) + (a.M32 * b.M24) + (a.M33 * b.M34) + (a.M34 * b.M44);
            o.M41 = (a.M41 * b.M11) + (a.M42 * b.M21) + (a.M43 * b.M31) + (a.M44 * b.M41);
            o.M42 = (a.M41 * b.M12) + (a.M42 * b.M22) + (a.M43 * b.M32) + (a.M44 * b.M42);
            o.M43 = (a.M41 * b.M13) + (a.M42 * b.M23) + (a.M43 * b.M33) + (a.M44 * b.M43);
            o.M44 = (a.M41 * b.M14) + (a.M42 * b.M24) + (a.M43 * b.M34) + (a.M44 * b.M44);
            return o;
        }

        private static Vector4 MulMat4Vec4(in NicxliveNative.Mat4 m, float x, float y, float z, float w)
        {
            return new Vector4(
                (m.M11 * x) + (m.M12 * y) + (m.M13 * z) + (m.M14 * w),
                (m.M21 * x) + (m.M22 * y) + (m.M23 * z) + (m.M24 * w),
                (m.M31 * x) + (m.M32 * y) + (m.M33 * z) + (m.M34 * w),
                (m.M41 * x) + (m.M42 * y) + (m.M43 * z) + (m.M44 * w));
        }

        private static NicxliveNative.Mat4 TransposeMat4(in NicxliveNative.Mat4 m)
        {
            NicxliveNative.Mat4 t;
            t.M11 = m.M11; t.M12 = m.M21; t.M13 = m.M31; t.M14 = m.M41;
            t.M21 = m.M12; t.M22 = m.M22; t.M23 = m.M32; t.M24 = m.M42;
            t.M31 = m.M13; t.M32 = m.M23; t.M33 = m.M33; t.M34 = m.M43;
            t.M41 = m.M14; t.M42 = m.M24; t.M43 = m.M34; t.M44 = m.M44;
            return t;
        }

        private static NicxliveNative.Mat4 SelectBestPartMvp(
            in NicxliveNative.Mat4 model,
            in NicxliveNative.Mat4 render,
            SharedBuffers shared,
            NicxliveNative.NjgPartDrawPacket packet,
            int vxBase,
            int vyBase,
            int dxBase,
            int dyBase,
            int vertexCount)
        {
            var c0 = MulMat4(render, model);
            var c1 = MulMat4(model, render);
            var modelT = TransposeMat4(model);
            var renderT = TransposeMat4(render);
            var c2 = MulMat4(renderT, modelT);
            var c3 = MulMat4(modelT, renderT);

            var candidates = new[]
            {
                c0,
                c1,
                c2,
                c3,
                TransposeMat4(c0),
                TransposeMat4(c1),
                TransposeMat4(c2),
                TransposeMat4(c3),
            };

            var best = candidates[0];
            var bestScore = float.NegativeInfinity;
            for (var i = 0; i < candidates.Length; i++)
            {
                var score = ScoreClipFitness(candidates[i], shared, packet.Origin.X, packet.Origin.Y, vxBase, vyBase, dxBase, dyBase, vertexCount);
                if (score > bestScore)
                {
                    best = candidates[i];
                    bestScore = score;
                }
            }

            return best;
        }

        private static NicxliveNative.Mat4 SelectBestMaskMvp(
            in NicxliveNative.Mat4 mvp,
            in NicxliveNative.Mat4 model,
            SharedBuffers shared,
            NicxliveNative.NjgMaskDrawPacket packet,
            int vxBase,
            int vyBase,
            int dxBase,
            int dyBase,
            int vertexCount)
        {
            var c0 = mvp;
            var c1 = TransposeMat4(mvp);
            var c2 = model;
            var c3 = TransposeMat4(model);
            var candidates = new[]
            {
                c0,
                c1,
                c2,
                c3,
            };

            var best = candidates[0];
            var bestScore = float.NegativeInfinity;
            for (var i = 0; i < candidates.Length; i++)
            {
                var score = ScoreClipFitness(candidates[i], shared, packet.Origin.X, packet.Origin.Y, vxBase, vyBase, dxBase, dyBase, vertexCount);
                if (score > bestScore)
                {
                    best = candidates[i];
                    bestScore = score;
                }
            }
            return best;
        }

        private static float ScoreClipFitness(
            in NicxliveNative.Mat4 mvp,
            SharedBuffers shared,
            float originX,
            float originY,
            int vxBase,
            int vyBase,
            int dxBase,
            int dyBase,
            int vertexCount)
        {
            if (vertexCount <= 0)
            {
                return float.NegativeInfinity;
            }

            var sampleCount = Mathf.Min(64, vertexCount);
            var step = Mathf.Max(1, vertexCount / sampleCount);
            var inClip = 0;
            var nearClip = 0;
            var finite = 0;
            var minX = float.PositiveInfinity;
            var minY = float.PositiveInfinity;
            var maxX = float.NegativeInfinity;
            var maxY = float.NegativeInfinity;

            for (var i = 0; i < vertexCount; i += step)
            {
                var px = shared.Vertices[vxBase + i] + shared.Deform[dxBase + i] - originX;
                var py = shared.Vertices[vyBase + i] + shared.Deform[dyBase + i] - originY;
                var clip = MulMat4Vec4(mvp, px, py, 0f, 1f);
                if (Mathf.Approximately(clip.w, 0f) || float.IsNaN(clip.w) || float.IsInfinity(clip.w))
                {
                    continue;
                }

                var invW = 1f / clip.w;
                var x = clip.x * invW;
                var y = clip.y * invW;
                if (float.IsNaN(x) || float.IsInfinity(x) || float.IsNaN(y) || float.IsInfinity(y))
                {
                    continue;
                }

                finite++;
                minX = Mathf.Min(minX, x);
                minY = Mathf.Min(minY, y);
                maxX = Mathf.Max(maxX, x);
                maxY = Mathf.Max(maxY, y);

                if (x >= -1.2f && x <= 1.2f && y >= -1.2f && y <= 1.2f)
                {
                    inClip++;
                }
                else if (x >= -4f && x <= 4f && y >= -4f && y <= 4f)
                {
                    nearClip++;
                }
            }

            if (finite == 0)
            {
                return float.NegativeInfinity;
            }

            var width = Mathf.Max(0f, maxX - minX);
            var height = Mathf.Max(0f, maxY - minY);
            var area = width * height;

            var score = (inClip * 1000f) + (nearClip * 100f);
            score += Mathf.Min(40f, width + height) * 20f;
            score += Mathf.Min(64f, area) * 10f;

            if (inClip == 0 && nearClip == 0)
            {
                score -= 2000f;
            }
            if (width < 0.05f || height < 0.05f)
            {
                score -= 500f;
            }

            return score;
        }

        private void applyCompositeTransform(ref float x, ref float y)
        {
            // GL/DX backends rely on matrix data emitted in the command stream.
            // Applying dynamic composite transform again here shrinks/offsets output.
            _ = x;
            _ = y;
            return;
        }
    }
}



namespace Nicxlive.UnityBackend.Managed
{
    [ExecuteAlways]
    [AddComponentMenu("Nicxlive/NicxliveBehavior")]
    public sealed partial class NicxliveBehaviour : MonoBehaviour
    {
        public UnityEngine.Object? PuppetAsset;
        public string PuppetPath = string.Empty;
        public Vector2 ModelScale = Vector2.one;
        public Vector2 ModelOffsetPixels = Vector2.zero;
        public bool DrawInEditMode = true;
        public bool EnableManagedDebugDiagnostics;
        public bool ShowRuntimeDebugOverlayComparison;
        public Camera? TargetCamera;
        public Material? PartMaterial;
        public Material? MaskMaterial;
        public CommandExecutor.PropertyConfig ShaderProperties = new CommandExecutor.PropertyConfig();

        private TextureRegistry? _textureRegistry;
        private NicxliveRenderer? _renderer;
        private CommandExecutor? _executor;
        private CommandBuffer? _commandBuffer;
        private CommandBuffer? _editorPreviewCommandBuffer;
        private Camera? _attachedCamera;
        private string _loadedPuppetPath = string.Empty;
        private Material? _autoPartMaterial;
        private Material? _autoMaskMaterial;
        private readonly List<PuppetParameterState> _parameterStates = new List<PuppetParameterState>();
        private readonly Dictionary<uint, PuppetParameterState> _parameterStatesByUuid = new Dictionary<uint, PuppetParameterState>();
        [NonSerialized] public int LastDecodedCommandCount;
        [NonSerialized] public int LastSharedVertexCount;
        [NonSerialized] public int LastPartPacketCount;
        [NonSerialized] public int LastPartDrawIssuedCount;
        [NonSerialized] public int LastPartSkippedNoTextureCount;
        [NonSerialized] public int LastPartSkippedMeshBuildCount;
        [NonSerialized] public float LastPartOpacityMin;
        [NonSerialized] public float LastPartOpacityMax;
        [NonSerialized] public int LastPartOpacityZeroCount;
        [NonSerialized] public int LastPartOpacityNonZeroCount;
        [NonSerialized] public int LastPartTextureMissingCount;
        [NonSerialized] public int LastMaskPacketCount;
        [NonSerialized] public int LastMaskDrawIssuedCount;
        [NonSerialized] public int LastMaskSkippedMeshBuildCount;
        [NonSerialized] public Vector4 LastPartClipBounds;
        [NonSerialized] public Vector4 LastMaskClipBounds;
        [NonSerialized] public bool HasPartClipBounds;
        [NonSerialized] public bool HasMaskClipBounds;
        [NonSerialized] public Vector3 LastScreenPoint;
        [NonSerialized] public Vector2 LastAppliedTranslation;
        [NonSerialized] public Vector2 LastAppliedScale = Vector2.one;
        [NonSerialized] public string LastResolvedCameraName = string.Empty;
        [NonSerialized] public string LastTranslationMode = string.Empty;
        [NonSerialized] public string LastVisibilityDiag = string.Empty;
        [NonSerialized] public string LastPartBuildDiag = string.Empty;
        [NonSerialized] public string LastFrameDiag = string.Empty;
        [NonSerialized] public string LastRenderPathDiag = string.Empty;
        [NonSerialized] public string LastClipFitDiag = string.Empty;
        [NonSerialized] public string LastRootSceneDiag = string.Empty;
        [NonSerialized] public string LastRouterDiag = string.Empty;
        [NonSerialized] public bool LastRootFallbackApplied;
        private int _emptyCommandLogCooldown;
        private int _noIssuedDrawLogCooldown;
        private int _transformGuardLogCooldown;
        private int _visibilityDiagLogCooldown;
        private int _sceneDebugPrevSharedVertexCount = -1;
        private int _sceneDebugPrevDecodedCount = -1;
        private int _sceneDebugPrevPartDrawCount = -1;
        private ulong _sceneDebugPrevSignature;
#if UNITY_EDITOR
        private double _lastEditorFrameTime = -1.0;
#endif
        private Vector2 _desiredPuppetTranslation = Vector2.zero;
        private Vector2 _desiredPuppetScale = Vector2.one;
        private RenderTexture? _runtimeGameOverlayTexture;
        private RenderTexture? _sceneViewOverlayTexture;

        public IReadOnlyList<PuppetParameterState> ParameterStates => _parameterStates;
        public bool HasLoadedPuppet => _renderer != null && _renderer.HasPuppet;

        private void OnEnable()
        {
#if UNITY_EDITOR
            UnityEditor.SceneView.duringSceneGui -= OnSceneViewDuringGui;
            UnityEditor.SceneView.duringSceneGui += OnSceneViewDuringGui;
#endif
            RenderPipelineManager.endCameraRendering -= OnEndCameraRenderingRuntimeOverlay;
            RenderPipelineManager.endCameraRendering += OnEndCameraRenderingRuntimeOverlay;
            TryEnsureRuntime();
            TryReloadPuppet(false);
        }

        private void OnValidate()
        {
            if (!isActiveAndEnabled)
            {
                return;
            }
            TryEnsureRuntime();
            TryReloadPuppet(true);
        }

        private void Update()
        {
            if (!Application.isPlaying && !DrawInEditMode)
            {
                return;
            }

            if (!TryEnsureRuntime() ||
                _renderer == null ||
                _executor == null ||
                _commandBuffer == null ||
                PartMaterial == null ||
                MaskMaterial == null)
            {
                return;
            }

            var camera = ResolveCamera();
            if (camera == null)
            {
                return;
            }
            LastResolvedCameraName = camera.name;
            EnsureAttachedCamera(camera);
            EnsureRuntimeOverlayAfterRouter();

            TryReloadPuppet(false);
            if (_renderer.HasPuppet)
            {
                ApplyTransformToPuppet(camera);
            }

            var viewportWidth = Mathf.Max(1, camera.pixelWidth);
            var viewportHeight = Mathf.Max(1, camera.pixelHeight);
            _renderer.BeginFrame(viewportWidth, viewportHeight);
            _renderer.Tick(ComputeDeltaSeconds());
            var view = _renderer.EmitCommands();
            var shared = _renderer.GetSharedBuffers();
            var decoded = CommandStream.Decode(view);
            LastDecodedCommandCount = decoded.Count;
            LastSharedVertexCount = shared.VertexCount;
            if (!EnableManagedDebugDiagnostics)
            {
                _emptyCommandLogCooldown = 0;
            }
            else if (decoded.Count == 0)
            {
                if (_emptyCommandLogCooldown <= 0)
                {
                    Debug.LogWarning($"[nicxlive] No render commands were emitted. SharedVertices={shared.VertexCount}", this);
                    _emptyCommandLogCooldown = 120;
                }
                else
                {
                    _emptyCommandLogCooldown--;
                }
            }
            else
            {
                _emptyCommandLogCooldown = 0;
            }
            _executor.EnableDiagnostics = EnableManagedDebugDiagnostics;
            _executor.EnableRuntimeSceneDebugCapture = ShowRuntimeDebugOverlayComparison;
            _executor.ConfigureInjectedPuppetTransform(
                decoded,
                shared,
                _desiredPuppetTranslation.x,
                _desiredPuppetTranslation.y,
                _desiredPuppetScale.x,
                _desiredPuppetScale.y,
                camera.pixelWidth,
                camera.pixelHeight);
            _executor.Execute(_commandBuffer, camera, decoded, shared, _renderer.TextureRegistry, PartMaterial, MaskMaterial, ShaderProperties);
#if UNITY_EDITOR
            if (!Application.isPlaying)
            {
                _editorPreviewCommandBuffer ??= new CommandBuffer { name = "nicxlive_editor_preview" };
                _executor.Execute(
                    _editorPreviewCommandBuffer,
                    camera,
                    decoded,
                    shared,
                    _renderer.TextureRegistry,
                    PartMaterial,
                    MaskMaterial,
                    ShaderProperties,
                    true);
                Graphics.ExecuteCommandBuffer(_editorPreviewCommandBuffer);
                _editorPreviewCommandBuffer.Clear();
            }
#endif
            LastPartPacketCount = _executor.LastPartPacketCount;
            LastPartDrawIssuedCount = _executor.LastPartDrawIssuedCount;
            LastPartSkippedNoTextureCount = _executor.LastPartSkippedNoTextureCount;
            LastPartSkippedMeshBuildCount = _executor.LastPartSkippedMeshBuildCount;
            LastPartOpacityMin = _executor.LastPartOpacityMin;
            LastPartOpacityMax = _executor.LastPartOpacityMax;
            LastPartOpacityZeroCount = _executor.LastPartOpacityZeroCount;
            LastPartOpacityNonZeroCount = _executor.LastPartOpacityNonZeroCount;
            LastPartTextureMissingCount = _executor.LastPartTextureMissingCount;
            LastMaskPacketCount = _executor.LastMaskPacketCount;
            LastMaskDrawIssuedCount = _executor.LastMaskDrawIssuedCount;
            LastMaskSkippedMeshBuildCount = _executor.LastMaskSkippedMeshBuildCount;
            LastPartClipBounds = _executor.LastPartClipBounds;
            LastMaskClipBounds = _executor.LastMaskClipBounds;
            HasPartClipBounds = _executor.HasPartClipBounds;
            HasMaskClipBounds = _executor.HasMaskClipBounds;
            LastPartBuildDiag = EnableManagedDebugDiagnostics ? _executor.LastPartBuildDiag : string.Empty;
            LastRenderPathDiag = EnableManagedDebugDiagnostics ? _executor.LastRenderPathDiag : string.Empty;
            LastClipFitDiag = EnableManagedDebugDiagnostics ? _executor.LastClipFitDiag : string.Empty;
            LastRouterDiag = EnableManagedDebugDiagnostics ? Nicxlive.UnityBackend.Compat.UrpCommandBufferRouter.GetLastExecutionDiag(_commandBuffer) : string.Empty;
            CommitSceneDebugIfStable(decoded.Count, shared.VertexCount);
            LastRootSceneDiag = string.Empty;
            if (EnableManagedDebugDiagnostics)
            {
                LastRenderPathDiag =
                    $"{LastRenderPathDiag}, GameGuiOverlay={(_runtimeGameOverlayTexture != null ? 1 : 0)}, GameGuiOverlayEnabled={(ShowRuntimeDebugOverlayComparison ? 1 : 0)}, RootSceneTexture={((_executor?.RootSceneTexture) != null ? 1 : 0)}";
                UpdateVisibilityDiagnostics(camera);
                LastFrameDiag = $"Obj={name}#{GetInstanceID()} Frame={Time.frameCount} Cmds={decoded.Count} PartDraws={LastPartDrawIssuedCount} MaskDraws={LastMaskDrawIssuedCount}";
            }
            else
            {
                LastVisibilityDiag = string.Empty;
                LastFrameDiag = string.Empty;
            }
            if (EnableManagedDebugDiagnostics && decoded.Count > 0 && LastPartDrawIssuedCount == 0 && LastMaskDrawIssuedCount == 0)
            {
                if (_noIssuedDrawLogCooldown <= 0)
                {
                    Debug.LogWarning(
                        $"[nicxlive] Commands exist but no DrawMesh was issued. " +
                        $"PartPackets={LastPartPacketCount}, PartSkipNoTexture={LastPartSkippedNoTextureCount}, " +
                        $"PartSkipMeshBuild={LastPartSkippedMeshBuildCount}, " +
                        $"MaskPackets={LastMaskPacketCount}, MaskSkipMeshBuild={LastMaskSkippedMeshBuildCount}",
                        this);
                    _noIssuedDrawLogCooldown = 120;
                }
                else
                {
                    _noIssuedDrawLogCooldown--;
                }
            }
            else
            {
                _noIssuedDrawLogCooldown = 0;
            }
#if UNITY_EDITOR
            if (!Application.isPlaying)
            {
                UnityEditor.EditorApplication.QueuePlayerLoopUpdate();
                UnityEditor.SceneView.RepaintAll();
            }
#endif
        }

        public bool ReloadPuppetFromCurrentSelection()
        {
            return TryReloadPuppet(true, true);
        }

        public bool RefreshParameters()
        {
            if (_renderer == null || !_renderer.HasPuppet)
            {
                _parameterStates.Clear();
                _parameterStatesByUuid.Clear();
                return false;
            }

            var metadata = _renderer.GetParameters();
            var next = new List<PuppetParameterState>(metadata.Count);
            var nextMap = new Dictionary<uint, PuppetParameterState>(metadata.Count);

            for (var i = 0; i < metadata.Count; i++)
            {
                var info = metadata[i];
                var state = new PuppetParameterState
                {
                    Uuid = info.Uuid,
                    Name = info.Name,
                    IsVec2 = info.IsVec2,
                    Min = info.Min,
                    Max = info.Max,
                    Value = ClampParameterValue(info.Defaults, info.Min, info.Max, info.IsVec2),
                };

                if (_parameterStatesByUuid.TryGetValue(info.Uuid, out var existing))
                {
                    state.Value = ClampParameterValue(existing.Value, state.Min, state.Max, state.IsVec2);
                }

                next.Add(state);
                nextMap[state.Uuid] = state;
            }

            _parameterStates.Clear();
            _parameterStates.AddRange(next);
            _parameterStatesByUuid.Clear();
            foreach (var kv in nextMap)
            {
                _parameterStatesByUuid[kv.Key] = kv.Value;
            }
            return _parameterStates.Count > 0;
        }

        public bool SetParameterValue(uint uuid, Vector2 value)
        {
            if (_renderer == null || !_renderer.HasPuppet)
            {
                return false;
            }
            if (!_parameterStatesByUuid.TryGetValue(uuid, out var state))
            {
                return false;
            }

            var clamped = ClampParameterValue(value, state.Min, state.Max, state.IsVec2);
            _renderer.UpdateParameter(uuid, clamped);
            state.Value = clamped;
            _parameterStatesByUuid[uuid] = state;
            return true;
        }

        public void ApplyTransformNow()
        {
            if (_renderer == null || !_renderer.HasPuppet)
            {
                return;
            }
            var camera = ResolveCamera();
            if (camera == null)
            {
                return;
            }
            ApplyTransformToPuppet(camera);
        }

        private bool TryEnsureRuntime()
        {
            if (!TryEnsureRenderer())
            {
                return false;
            }

            if (!EnsureRuntimeMaterials())
            {
                return false;
            }

            var camera = ResolveCamera();
            if (camera == null)
            {
                return false;
            }

            _executor ??= new CommandExecutor();
            if (_commandBuffer == null)
            {
                _commandBuffer = new CommandBuffer { name = "nicxlive_unity_backend" };
            }
            EnsureAttachedCamera(camera);
            return true;
        }

        private bool TryEnsureRenderer()
        {
            if (_renderer != null)
            {
                return true;
            }

            _textureRegistry ??= new TextureRegistry();
            var camera = ResolveCamera();
            var width = camera != null ? Mathf.Max(1, camera.pixelWidth) : 1280;
            var height = camera != null ? Mathf.Max(1, camera.pixelHeight) : 720;

            try
            {
                _renderer = new NicxliveRenderer(width, height, _textureRegistry);
                return true;
            }
            catch (Exception ex)
            {
                Debug.LogError($"[nicxlive] Failed to initialize renderer: {ex.Message}", this);
                return false;
            }
        }

        private bool EnsureRuntimeMaterials()
        {
            UnityEngine.Shader requiredPartShader;
            UnityEngine.Shader requiredMaskShader;
            try
            {
                requiredPartShader = Nicxlive.UnityBackend.Compat.UrpShaderCatalog.RequirePartShader();
                requiredMaskShader = Nicxlive.UnityBackend.Compat.UrpShaderCatalog.RequireMaskShader();
            }
            catch (Exception ex)
            {
                Debug.LogError($"[nicxlive] Failed to resolve runtime shaders: {ex.Message}", this);
                return false;
            }

            var partLooksAuto = PartMaterial != null && PartMaterial.name == "nicxlive_auto_part";
            var maskLooksAuto = MaskMaterial != null && MaskMaterial.name == "nicxlive_auto_mask";
            if (_autoPartMaterial == null && partLooksAuto)
            {
                _autoPartMaterial = PartMaterial;
            }
            if (_autoMaskMaterial == null && maskLooksAuto)
            {
                _autoMaskMaterial = MaskMaterial;
            }

            // Keep user-assigned materials; auto-create only when missing.
            if (_autoPartMaterial != null && !ReferenceEquals(PartMaterial, _autoPartMaterial))
            {
                UnityObjectUtil.DestroyObject(_autoPartMaterial);
                _autoPartMaterial = null;
            }
            if (_autoMaskMaterial != null && !ReferenceEquals(MaskMaterial, _autoMaskMaterial))
            {
                UnityObjectUtil.DestroyObject(_autoMaskMaterial);
                _autoMaskMaterial = null;
            }

            if (PartMaterial == null)
            {
                try
                {
                    _autoPartMaterial = new Material(requiredPartShader)
                    {
                        name = "nicxlive_auto_part"
                    };
                    _autoPartMaterial.SetInt("_Cull", 0);
                    _autoPartMaterial.SetFloat("_ZWrite", 0f);
                    PartMaterial = _autoPartMaterial;
                }
                catch (Exception ex)
                {
                    Debug.LogError($"[nicxlive] Failed to create default part material: {ex.Message}", this);
                    return false;
                }
            }
            else if ((ReferenceEquals(PartMaterial, _autoPartMaterial) || partLooksAuto) && PartMaterial.shader != requiredPartShader)
            {
                PartMaterial.shader = requiredPartShader;
            }

            if (MaskMaterial == null)
            {
                try
                {
                    _autoMaskMaterial = new Material(requiredMaskShader)
                    {
                        name = "nicxlive_auto_mask"
                    };
                    _autoMaskMaterial.SetInt("_Cull", 0);
                    _autoMaskMaterial.SetFloat("_ZWrite", 0f);
                    MaskMaterial = _autoMaskMaterial;
                }
                catch (Exception ex)
                {
                    Debug.LogError($"[nicxlive] Failed to create default mask material: {ex.Message}", this);
                    return false;
                }
            }
            else if ((ReferenceEquals(MaskMaterial, _autoMaskMaterial) || maskLooksAuto) && MaskMaterial.shader != requiredMaskShader)
            {
                MaskMaterial.shader = requiredMaskShader;
            }

            if (PartMaterial != null)
            {
                ApplyRuntimeMaterialDefaults(PartMaterial);
            }
            if (MaskMaterial != null)
            {
                ApplyRuntimeMaterialDefaults(MaskMaterial);
            }

            return PartMaterial != null && MaskMaterial != null;
        }

        private static void ApplyRuntimeMaterialDefaults(Material material)
        {
            if (material == null)
            {
                return;
            }

            material.SetInt("_Cull", 0);
            material.SetFloat("_ZWrite", 0f);

            if (material.HasProperty("_Surface"))
            {
                material.SetFloat("_Surface", 1f); // Transparent
            }
            if (material.HasProperty("_Blend"))
            {
                material.SetFloat("_Blend", 0f); // Alpha
            }
            if (material.HasProperty("_SrcBlend"))
            {
                material.SetFloat("_SrcBlend", (float)UnityEngine.Rendering.BlendMode.One);
            }
            if (material.HasProperty("_DstBlend"))
            {
                material.SetFloat("_DstBlend", (float)UnityEngine.Rendering.BlendMode.OneMinusSrcAlpha);
            }
            if (material.HasProperty("_SrcBlendAlpha"))
            {
                material.SetFloat("_SrcBlendAlpha", (float)UnityEngine.Rendering.BlendMode.One);
            }
            if (material.HasProperty("_DstBlendAlpha"))
            {
                material.SetFloat("_DstBlendAlpha", (float)UnityEngine.Rendering.BlendMode.OneMinusSrcAlpha);
            }
            if (material.HasProperty("_BlendOp"))
            {
                material.SetFloat("_BlendOp", (float)UnityEngine.Rendering.BlendOp.Add);
            }
            if (material.HasProperty("_BlendOpAlpha"))
            {
                material.SetFloat("_BlendOpAlpha", (float)UnityEngine.Rendering.BlendOp.Add);
            }
            if (material.HasProperty("_BaseColor"))
            {
                material.SetColor("_BaseColor", Color.white);
            }
            if (material.HasProperty("_DebugForceOpaque"))
            {
                material.SetFloat("_DebugForceOpaque", 0f);
            }
            if (material.HasProperty("_DebugFlipY"))
            {
                material.SetFloat("_DebugFlipY", 1f);
            }
            if (material.HasProperty("_DebugFlipV"))
            {
                material.SetFloat("_DebugFlipV", 0f);
            }
            if (material.HasProperty("_DebugShowAlbedo"))
            {
                material.SetFloat("_DebugShowAlbedo", 0f);
            }
            if (material.renderQueue < (int)RenderQueue.Transparent)
            {
                material.renderQueue = (int)RenderQueue.Transparent;
            }
        }

        private Camera? ResolveCamera()
        {
            if (TargetCamera != null)
            {
                return TargetCamera;
            }
            if (Camera.main != null)
            {
                return Camera.main;
            }
#if UNITY_EDITOR
            if (!Application.isPlaying && DrawInEditMode)
            {
                var sceneView = UnityEditor.SceneView.lastActiveSceneView;
                if (sceneView != null && sceneView.camera != null)
                {
                    return sceneView.camera;
                }
            }
#endif
            var cameraCount = Camera.allCamerasCount;
            if (cameraCount > 0)
            {
                var cameras = new Camera[cameraCount];
                Camera.GetAllCameras(cameras);
                if (cameras.Length > 0 && cameras[0] != null)
                {
                    return cameras[0];
                }
            }
            return null;
        }

        private void EnsureRuntimeGameOverlayTexture(int width, int height)
        {
            width = Mathf.Max(1, width);
            height = Mathf.Max(1, height);
            if (_runtimeGameOverlayTexture != null &&
                _runtimeGameOverlayTexture.width == width &&
                _runtimeGameOverlayTexture.height == height)
            {
                if (!_runtimeGameOverlayTexture.IsCreated())
                {
                    _runtimeGameOverlayTexture.Create();
                }
                return;
            }

            ReleaseRuntimeGameOverlayTexture();
            var desc = new RenderTextureDescriptor(width, height, RenderTextureFormat.ARGB32, 0)
            {
                msaaSamples = 1,
                useMipMap = false,
                autoGenerateMips = false,
                sRGB = false
            };
            _runtimeGameOverlayTexture = new RenderTexture(desc)
            {
                name = "nicxlive_game_overlay_rt",
                useMipMap = false,
                autoGenerateMips = false,
                filterMode = FilterMode.Bilinear,
                wrapMode = TextureWrapMode.Clamp,
                hideFlags = HideFlags.HideAndDontSave
            };
            _runtimeGameOverlayTexture.Create();
        }

        private void ReleaseRuntimeGameOverlayTexture()
        {
            if (_runtimeGameOverlayTexture == null)
            {
                return;
            }

            if (_runtimeGameOverlayTexture.IsCreated())
            {
                _runtimeGameOverlayTexture.Release();
            }
            UnityObjectUtil.DestroyObject(_runtimeGameOverlayTexture);
            _runtimeGameOverlayTexture = null;
        }

        private void EnsureSceneViewOverlayTexture(int width, int height)
        {
            width = Mathf.Max(1, width);
            height = Mathf.Max(1, height);
            if (_sceneViewOverlayTexture != null &&
                _sceneViewOverlayTexture.width == width &&
                _sceneViewOverlayTexture.height == height)
            {
                if (!_sceneViewOverlayTexture.IsCreated())
                {
                    _sceneViewOverlayTexture.Create();
                }
                return;
            }

            ReleaseSceneViewOverlayTexture();
            var desc = new RenderTextureDescriptor(width, height, RenderTextureFormat.ARGB32, 0)
            {
                msaaSamples = 1,
                useMipMap = false,
                autoGenerateMips = false,
                sRGB = false
            };
            _sceneViewOverlayTexture = new RenderTexture(desc)
            {
                name = "nicxlive_scene_overlay_rt",
                useMipMap = false,
                autoGenerateMips = false,
                filterMode = FilterMode.Bilinear,
                wrapMode = TextureWrapMode.Clamp,
                hideFlags = HideFlags.HideAndDontSave
            };
            _sceneViewOverlayTexture.Create();
        }

        private void ReleaseSceneViewOverlayTexture()
        {
            if (_sceneViewOverlayTexture == null)
            {
                return;
            }

            if (_sceneViewOverlayTexture.IsCreated())
            {
                _sceneViewOverlayTexture.Release();
            }
            UnityObjectUtil.DestroyObject(_sceneViewOverlayTexture);
            _sceneViewOverlayTexture = null;
        }

        private void OnEndCameraRenderingRuntimeOverlay(ScriptableRenderContext context, Camera camera)
        {
            _ = context;
            if (!Application.isPlaying || _executor == null || camera == null || camera.cameraType != CameraType.Game)
            {
                return;
            }

            var resolved = ResolveCamera();
            if (resolved == null || camera != resolved)
            {
                return;
            }

            if (!EnableManagedDebugDiagnostics)
            {
                LastRouterDiag = string.Empty;
                LastRootSceneDiag = string.Empty;
                LastRootFallbackApplied = false;
                return;
            }

            LastRouterDiag = Nicxlive.UnityBackend.Compat.UrpCommandBufferRouter.GetLastExecutionDiag(_commandBuffer);
            _executor.UpdateRootSceneDiag(LastPartClipBounds, HasPartClipBounds);
            LastRootSceneDiag = _executor.LastRootSceneDiag;
            LastRootFallbackApplied = false;
        }

        private void OnGUI()
        {
            if (!Application.isPlaying ||
                Event.current == null ||
                Event.current.type != EventType.Repaint)
            {
                return;
            }

            var camera = ResolveCamera();
            if (ShowRuntimeDebugOverlayComparison && camera != null)
            {
                EnsureRuntimeGameOverlayTexture(camera.pixelWidth, camera.pixelHeight);
            }
            else
            {
                ReleaseRuntimeGameOverlayTexture();
            }

            if (ShowRuntimeDebugOverlayComparison && _runtimeGameOverlayTexture != null && _executor != null)
            {
                _executor.RenderSceneDebugToTarget(_runtimeGameOverlayTexture);
            }

            Texture? overlayTexture = ShowRuntimeDebugOverlayComparison ? _runtimeGameOverlayTexture : null;

            if (overlayTexture == null)
            {
                return;
            }
            var insetWidth = Mathf.Max(256f, Screen.width * 0.28f);
            var insetHeight = Mathf.Max(144f, Screen.height * 0.28f);
            var insetRect = new Rect(
                Screen.width - insetWidth - 16f,
                16f,
                insetWidth,
                insetHeight);
            GUI.DrawTexture(insetRect, overlayTexture, ScaleMode.ScaleToFit, true);
        }

#if UNITY_EDITOR
        private void OnSceneViewDuringGui(UnityEditor.SceneView sceneView)
        {
            if (sceneView == null || sceneView.camera == null)
            {
                return;
            }
            if (UnityEngine.Event.current != null && UnityEngine.Event.current.type != UnityEngine.EventType.Repaint)
            {
                return;
            }
            if (Application.isPlaying || !DrawInEditMode || _executor == null)
            {
                return;
            }
            if (TargetCamera != null)
            {
                return;
            }

            if (!TryRenderEditorPreviewFrame())
            {
                return;
            }

            var rootSceneTexture = _executor.RootSceneTexture;
            if (rootSceneTexture == null || !rootSceneTexture.IsCreated())
            {
                return;
            }

            var cameraRect = sceneView.camera.pixelRect;
            var guiViewportRect = new Rect(
                cameraRect.x,
                sceneView.position.height - cameraRect.y - cameraRect.height,
                cameraRect.width,
                cameraRect.height);
            var previewWidth = Mathf.Max(1, rootSceneTexture.width);
            var previewHeight = Mathf.Max(1, rootSceneTexture.height);
            EnsureSceneViewOverlayTexture(previewWidth, previewHeight);
            if (_sceneViewOverlayTexture == null)
            {
                return;
            }
            _executor.RenderRootSceneToTarget(_sceneViewOverlayTexture);

            var scale = Mathf.Min(
                1f,
                Mathf.Min(
                    guiViewportRect.width / previewWidth,
                    guiViewportRect.height / previewHeight));
            var drawWidth = previewWidth * scale;
            var drawHeight = previewHeight * scale;
            var drawRect = new Rect(
                guiViewportRect.x + ((guiViewportRect.width - drawWidth) * 0.5f),
                guiViewportRect.y + ((guiViewportRect.height - drawHeight) * 0.5f),
                drawWidth,
                drawHeight);

            UnityEditor.Handles.BeginGUI();
            GUI.DrawTextureWithTexCoords(
                drawRect,
                _sceneViewOverlayTexture,
                new Rect(0f, 1f, 1f, -1f),
                true);
            UnityEditor.Handles.EndGUI();
            UnityEditor.EditorApplication.QueuePlayerLoopUpdate();
            sceneView.Repaint();
        }

        private bool TryRenderEditorPreviewFrame(Camera? previewCameraOverride = null)
        {
            if (Application.isPlaying)
            {
                return false;
            }
            if (!TryEnsureRuntime() ||
                _renderer == null ||
                _executor == null ||
                PartMaterial == null ||
                MaskMaterial == null)
            {
                return false;
            }

            var previewCamera = previewCameraOverride ?? ResolveCamera();
            if (previewCamera == null)
            {
                return false;
            }

            TryReloadPuppet(false);
            if (!_renderer.HasPuppet)
            {
                return false;
            }

            ApplyTransformToPuppet(previewCamera);
            var viewportWidth = Mathf.Max(1, previewCamera.pixelWidth);
            var viewportHeight = Mathf.Max(1, previewCamera.pixelHeight);
            _renderer.BeginFrame(viewportWidth, viewportHeight);
            _renderer.Tick(ComputeDeltaSeconds());
            var view = _renderer.EmitCommands();
            var shared = _renderer.GetSharedBuffers();
            var decoded = CommandStream.Decode(view);

            _executor.EnableDiagnostics = EnableManagedDebugDiagnostics;
            _executor.EnableRuntimeSceneDebugCapture = false;
            _executor.ConfigureInjectedPuppetTransform(
                decoded,
                shared,
                _desiredPuppetTranslation.x,
                _desiredPuppetTranslation.y,
                _desiredPuppetScale.x,
                _desiredPuppetScale.y,
                previewCamera.pixelWidth,
                previewCamera.pixelHeight);

            _editorPreviewCommandBuffer ??= new CommandBuffer { name = "nicxlive_editor_preview" };
            _executor.Execute(
                _editorPreviewCommandBuffer,
                previewCamera,
                decoded,
                shared,
                _renderer.TextureRegistry,
                PartMaterial,
                MaskMaterial,
                ShaderProperties,
                true);
            Graphics.ExecuteCommandBuffer(_editorPreviewCommandBuffer);
            _editorPreviewCommandBuffer.Clear();
            return _executor.RootSceneTexture != null && _executor.RootSceneTexture.IsCreated();
        }
#endif

        private void OnRenderObject()
        {
            if (!Application.isPlaying || _executor == null || !ShowRuntimeDebugOverlayComparison)
            {
                return;
            }

            var current = Camera.current;
            if (current == null || current.cameraType != CameraType.Game)
            {
                return;
            }

            var resolved = ResolveCamera();
            if (resolved == null || current != resolved)
            {
                return;
            }

            _executor.RenderSceneDebug(current);
        }

        private void EnsureAttachedCamera(Camera? camera)
        {
            if (camera == null)
            {
                return;
            }
            if (!Application.isPlaying && camera.cameraType == CameraType.SceneView)
            {
                if (_attachedCamera != null && _commandBuffer != null)
                {
                    Nicxlive.UnityBackend.Compat.UrpCommandBufferRouter.Detach(_attachedCamera, _commandBuffer);
                }
                _attachedCamera = null;
                return;
            }

            if (_commandBuffer == null || _attachedCamera == camera)
            {
                return;
            }
            if (_attachedCamera != null)
            {
                Nicxlive.UnityBackend.Compat.UrpCommandBufferRouter.Detach(_attachedCamera, _commandBuffer);
            }
            Nicxlive.UnityBackend.Compat.UrpCommandBufferRouter.Attach(camera!, _commandBuffer);
            _attachedCamera = camera;
        }

        private void EnsureRuntimeOverlayAfterRouter()
        {
            RenderPipelineManager.endCameraRendering -= OnEndCameraRenderingRuntimeOverlay;
            RenderPipelineManager.endCameraRendering += OnEndCameraRenderingRuntimeOverlay;
        }

        private void ResetRuntimeForPuppetReload()
        {
            if (_attachedCamera != null && _commandBuffer != null)
            {
                Nicxlive.UnityBackend.Compat.UrpCommandBufferRouter.Detach(_attachedCamera, _commandBuffer);
            }
            _attachedCamera = null;

            _commandBuffer?.Release();
            _commandBuffer = null;
            _editorPreviewCommandBuffer?.Release();
            _editorPreviewCommandBuffer = null;

            _executor?.ReleasePersistentResources();
            _executor = null;

            _renderer?.Dispose();
            _renderer = null;

            _textureRegistry?.Dispose();
            _textureRegistry = null;

            ReleaseRuntimeGameOverlayTexture();
            ReleaseSceneViewOverlayTexture();

            _loadedPuppetPath = string.Empty;
            _parameterStates.Clear();
            _parameterStatesByUuid.Clear();
            _sceneDebugPrevSharedVertexCount = -1;
            _sceneDebugPrevDecodedCount = -1;
            _sceneDebugPrevPartDrawCount = -1;
            _sceneDebugPrevSignature = 0;
        }

        private bool TryReloadPuppet(bool forceReload, bool reportFailure = false)
        {
            var resolvedPath = ResolvePuppetPath();
            if (string.IsNullOrWhiteSpace(resolvedPath))
            {
                if (_renderer != null && _renderer.HasPuppet)
                {
                    ResetRuntimeForPuppetReload();
                    _parameterStates.Clear();
                    _parameterStatesByUuid.Clear();
                }
                if (reportFailure)
                {
                    Debug.LogWarning("[nicxlive] Reload Puppet skipped: Puppet Path is empty.", this);
                }
                return false;
            }

            if (!File.Exists(resolvedPath))
            {
                if (reportFailure)
                {
                    Debug.LogError($"[nicxlive] Reload Puppet failed: file not found: {resolvedPath}", this);
                }
                return false;
            }

            var normalizedPath = Path.GetFullPath(resolvedPath);
            var samePath = string.Equals(_loadedPuppetPath, normalizedPath, StringComparison.OrdinalIgnoreCase);
            if (!forceReload &&
                _renderer != null &&
                _renderer.HasPuppet &&
                samePath)
            {
                return true;
            }

            if (forceReload || !samePath)
            {
                ResetRuntimeForPuppetReload();
            }

            if (!TryEnsureRuntime() || _renderer == null)
            {
                if (reportFailure)
                {
                    Debug.LogWarning("[nicxlive] Reload Puppet failed: renderer is not initialized.", this);
                }
                return false;
            }

            var renderer = _renderer;

            try
            {
                renderer.LoadPuppet(normalizedPath);
                _loadedPuppetPath = normalizedPath;
                RefreshParameters();
                ApplyTransformNow();
                return true;
            }
            catch (Exception ex)
            {
                if (reportFailure)
                {
                    Debug.LogError($"[nicxlive] Reload Puppet failed for path: {normalizedPath}\n{ex.Message}", this);
                }
                return false;
            }
        }

        private string ResolvePuppetPath()
        {
#if UNITY_EDITOR
            if (PuppetAsset != null)
            {
                var assetPath = UnityEditor.AssetDatabase.GetAssetPath(PuppetAsset);
                if (!string.IsNullOrWhiteSpace(assetPath))
                {
                    return Path.GetFullPath(assetPath);
                }
            }
#endif
            if (string.IsNullOrWhiteSpace(PuppetPath))
            {
                return string.Empty;
            }
            return Path.GetFullPath(PuppetPath.Trim().Trim('"'));
        }

        private void UpdateVisibilityDiagnostics(Camera camera)
        {
            if (!EnableManagedDebugDiagnostics)
            {
                LastVisibilityDiag = string.Empty;
                _visibilityDiagLogCooldown = 0;
                return;
            }

            if (camera == null)
            {
                LastVisibilityDiag = string.Empty;
                return;
            }

            if (!HasPartClipBounds)
            {
                LastVisibilityDiag =
                    $"PartBounds=N/A, Screen=({LastScreenPoint.x:F1},{LastScreenPoint.y:F1},{LastScreenPoint.z:F1}), " +
                    $"TxTy=({LastAppliedTranslation.x:F1},{LastAppliedTranslation.y:F1}), Cam={camera.name}, Mode={LastTranslationMode}";
                return;
            }

            var b = LastPartClipBounds;
            var fullyOutside = b.z < -1f || b.x > 1f || b.w < -1f || b.y > 1f;
            var spanX = Mathf.Max(0f, b.z - b.x);
            var spanY = Mathf.Max(0f, b.w - b.y);
            var inflatedSpan = spanX > 2.5f || spanY > 2.5f;
            var likelyInvisible = LastPartDrawIssuedCount > 0 && (fullyOutside || inflatedSpan);
            var opacityMin = float.IsFinite(LastPartOpacityMin) ? LastPartOpacityMin : 0f;
            var opacityMax = float.IsFinite(LastPartOpacityMax) ? LastPartOpacityMax : 0f;
            LastVisibilityDiag =
                $"PartBounds x:[{b.x:F3},{b.z:F3}] y:[{b.y:F3},{b.w:F3}], " +
                $"Span=({spanX:F3},{spanY:F3}), " +
                $"Screen=({LastScreenPoint.x:F1},{LastScreenPoint.y:F1},{LastScreenPoint.z:F1}), " +
                $"TxTy=({LastAppliedTranslation.x:F1},{LastAppliedTranslation.y:F1}), " +
                $"Scale=({LastAppliedScale.x:F3},{LastAppliedScale.y:F3}), Cam={camera.name}, Mode={LastTranslationMode}, " +
                $"Opacity=[{opacityMin:F3},{opacityMax:F3}] zero/nonZero={LastPartOpacityZeroCount}/{LastPartOpacityNonZeroCount} " +
                $"Tex0Missing={LastPartTextureMissingCount}";

            if (!likelyInvisible)
            {
                _visibilityDiagLogCooldown = 0;
                return;
            }

            if (_visibilityDiagLogCooldown > 0)
            {
                _visibilityDiagLogCooldown--;
                return;
            }

            Debug.LogWarning($"[nicxlive] Likely offscreen draw. Obj={name}#{GetInstanceID()} Frame={Time.frameCount} {LastVisibilityDiag}", this);
            if (!string.IsNullOrWhiteSpace(LastClipFitDiag))
            {
                Debug.LogWarning($"[nicxlive] Clip fit diag. Obj={name}#{GetInstanceID()} Frame={Time.frameCount} {LastClipFitDiag}", this);
            }
            _visibilityDiagLogCooldown = 120;
        }

        private void ApplyTransformToPuppet(Camera camera)
        {
            if (_renderer == null || !_renderer.HasPuppet)
            {
                return;
            }

            var width = Mathf.Max(1, camera.pixelWidth);
            var height = Mathf.Max(1, camera.pixelHeight);
            var viewport = camera.WorldToViewportPoint(transform.position);
            var screen = new Vector3(viewport.x * width, viewport.y * height, viewport.z);
            LastScreenPoint = screen;

            var tx = (viewport.x - 0.5f) * width + ModelOffsetPixels.x;
            var ty = (0.5f - viewport.y) * height + ModelOffsetPixels.y;
            LastTranslationMode = "world_to_viewport";
            if (!float.IsFinite(tx) || !float.IsFinite(ty))
            {
                tx = ModelOffsetPixels.x;
                ty = ModelOffsetPixels.y;
                LastTranslationMode = "world_to_viewport_guarded";
                if (EnableManagedDebugDiagnostics && _transformGuardLogCooldown <= 0)
                {
                    Debug.LogWarning(
                        $"[nicxlive] Guarded puppet translation. Camera={camera.name}, Mode=world_to_viewport, " +
                        $"Screen=({screen.x:F1},{screen.y:F1},{screen.z:F1}), " +
                        $"Viewport=({width},{height}), Applied=({tx:F1},{ty:F1})",
                        this);
                    _transformGuardLogCooldown = 120;
                }
                else
                {
                    _transformGuardLogCooldown = EnableManagedDebugDiagnostics ? (_transformGuardLogCooldown - 1) : 0;
                }
            }
            else
            {
                _transformGuardLogCooldown = 0;
            }
            LastAppliedTranslation = new Vector2(tx, ty);
            _desiredPuppetTranslation = LastAppliedTranslation;

            var lossy = transform.lossyScale;
            var sx = ModelScale.x * lossy.x;
            var sy = ModelScale.y * lossy.y;
            if (Mathf.Approximately(sx, 0f))
            {
                sx = 1f;
            }
            if (Mathf.Approximately(sy, 0f))
            {
                sy = 1f;
            }
            LastAppliedScale = new Vector2(sx, sy);
            _desiredPuppetScale = LastAppliedScale;
        }

        private double ComputeDeltaSeconds()
        {
            if (Application.isPlaying)
            {
                return Time.deltaTime;
            }
#if UNITY_EDITOR
            var now = UnityEditor.EditorApplication.timeSinceStartup;
            if (_lastEditorFrameTime < 0.0)
            {
                _lastEditorFrameTime = now;
                return 0.0;
            }
            var delta = Math.Max(0.0, now - _lastEditorFrameTime);
            _lastEditorFrameTime = now;
            return delta;
#else
            return 0.0;
#endif
        }

        private void CommitSceneDebugIfStable(int decodedCount, int sharedVertexCount)
        {
            if (_executor == null)
            {
                return;
            }

            if (Application.isPlaying)
            {
                _executor.CommitSceneDebugDraws(
                    ShowRuntimeDebugOverlayComparison &&
                    decodedCount > 0 &&
                    LastPartDrawIssuedCount > 0);
                return;
            }

#if UNITY_EDITOR
            _sceneDebugPrevDecodedCount = decodedCount;
            _sceneDebugPrevSharedVertexCount = sharedVertexCount;
            _sceneDebugPrevPartDrawCount = LastPartDrawIssuedCount;
            _sceneDebugPrevSignature = _executor.LastSceneDebugSignature;
            _executor.CommitSceneDebugDraws(
                decodedCount > 0 &&
                LastPartDrawIssuedCount > 0);
#endif
        }

        private static Vector2 ClampParameterValue(Vector2 value, Vector2 min, Vector2 max, bool isVec2)
        {
            var x = Mathf.Clamp(value.x, min.x, max.x);
            var y = isVec2 ? Mathf.Clamp(value.y, min.y, max.y) : value.y;
            return new Vector2(x, y);
        }

        private void OnDisable()
        {
#if UNITY_EDITOR
            UnityEditor.SceneView.duringSceneGui -= OnSceneViewDuringGui;
#endif
            RenderPipelineManager.endCameraRendering -= OnEndCameraRenderingRuntimeOverlay;
            if (_attachedCamera != null && _commandBuffer != null)
            {
                Nicxlive.UnityBackend.Compat.UrpCommandBufferRouter.Detach(_attachedCamera, _commandBuffer);
            }
            _attachedCamera = null;

            _commandBuffer?.Release();
            _commandBuffer = null;
            _editorPreviewCommandBuffer?.Release();
            _editorPreviewCommandBuffer = null;

            _executor?.ReleasePersistentResources();
            _renderer?.Dispose();
            _renderer = null;
            _executor = null;
            _textureRegistry?.Dispose();
            _textureRegistry = null;
            ReleaseRuntimeGameOverlayTexture();
            ReleaseSceneViewOverlayTexture();
            _loadedPuppetPath = string.Empty;
            if (_autoPartMaterial != null)
            {
                UnityObjectUtil.DestroyObject(_autoPartMaterial);
                if (ReferenceEquals(PartMaterial, _autoPartMaterial))
                {
                    PartMaterial = null;
                }
                _autoPartMaterial = null;
            }
            if (_autoMaskMaterial != null)
            {
                UnityObjectUtil.DestroyObject(_autoMaskMaterial);
                if (ReferenceEquals(MaskMaterial, _autoMaskMaterial))
                {
                    MaskMaterial = null;
                }
                _autoMaskMaterial = null;
            }
            _parameterStates.Clear();
            _parameterStatesByUuid.Clear();
            _emptyCommandLogCooldown = 0;
            _noIssuedDrawLogCooldown = 0;
            _transformGuardLogCooldown = 0;
            _visibilityDiagLogCooldown = 0;
            _sceneDebugPrevSharedVertexCount = -1;
            _sceneDebugPrevDecodedCount = -1;
            _sceneDebugPrevPartDrawCount = -1;
            _sceneDebugPrevSignature = 0;
            LastPartOpacityMin = 0f;
            LastPartOpacityMax = 0f;
            LastPartOpacityZeroCount = 0;
            LastPartOpacityNonZeroCount = 0;
            LastPartTextureMissingCount = 0;
            LastPartClipBounds = Vector4.zero;
            LastMaskClipBounds = Vector4.zero;
            HasPartClipBounds = false;
            HasMaskClipBounds = false;
            LastScreenPoint = Vector3.zero;
            LastAppliedTranslation = Vector2.zero;
            LastAppliedScale = Vector2.one;
            LastResolvedCameraName = string.Empty;
            LastTranslationMode = string.Empty;
            LastVisibilityDiag = string.Empty;
            LastPartBuildDiag = string.Empty;
            LastClipFitDiag = string.Empty;
            LastRootSceneDiag = string.Empty;
            _desiredPuppetTranslation = Vector2.zero;
            _desiredPuppetScale = Vector2.one;
            LastRenderPathDiag = string.Empty;
            LastRouterDiag = string.Empty;
            ReleaseRuntimeGameOverlayTexture();
#if UNITY_EDITOR
            _lastEditorFrameTime = -1.0;
#endif
        }
    }
}







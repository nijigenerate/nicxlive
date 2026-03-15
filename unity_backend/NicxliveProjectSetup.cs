#nullable enable
#if UNITY_EDITOR
using System;
using System.Reflection;
using UnityEditor;
using UnityEngine;
using UnityEngine.Rendering;

namespace Nicxlive.UnityBackend.EditorSetup
{
    public static class NicxliveProjectSetup
    {
        private const string PluginPath = "Assets/Plugins/x86_64/nicxlive.dll";
        private const string SettingsDir = "Assets/Nicxlive/Settings";
        private const string UrpAssetPath = SettingsDir + "/Nicxlive_URP.asset";
        private const string RendererAssetPath = SettingsDir + "/Nicxlive_ForwardRenderer.asset";

        [MenuItem("Tools/Nicxlive/Apply Project Setup")]
        public static void ApplyFromMenu()
        {
            var changed = ApplyIfNeeded();
            if (changed)
            {
                AssetDatabase.SaveAssets();
                AssetDatabase.Refresh();
            }
            Debug.Log("[nicxlive] project setup complete.");
        }

        public static void ApplyFromCommandLine()
        {
            try
            {
                var changed = ApplyIfNeeded();
                if (changed)
                {
                    AssetDatabase.SaveAssets();
                    AssetDatabase.Refresh();
                }
                Debug.Log("[nicxlive] project setup complete.");
                if (Application.isBatchMode)
                {
                    EditorApplication.Exit(0);
                }
            }
            catch (Exception ex)
            {
                Debug.LogError("[nicxlive] project setup failed: " + ex);
                if (Application.isBatchMode)
                {
                    EditorApplication.Exit(1);
                    return;
                }
                throw;
            }
        }

        private static bool ApplyIfNeeded()
        {
            var changed = false;
            changed |= EnsureUnsafeCode();
            changed |= EnsurePluginImporter();
            changed |= EnsureUrpPipeline();
            return changed;
        }

        private static bool EnsureUnsafeCode()
        {
            if (PlayerSettings.allowUnsafeCode)
            {
                Debug.Log("[nicxlive] allowUnsafeCode: already enabled.");
                return false;
            }

            PlayerSettings.allowUnsafeCode = true;
            Debug.Log("[nicxlive] allowUnsafeCode: enabled.");
            return true;
        }

        private static bool EnsurePluginImporter()
        {
            var importer = AssetImporter.GetAtPath(PluginPath) as PluginImporter;
            if (importer == null)
            {
                Debug.LogWarning("[nicxlive] PluginImporter not found at: " + PluginPath);
                return false;
            }

            var changed = false;
            changed |= SetIfDifferent(importer.GetCompatibleWithAnyPlatform(), false, () => importer.SetCompatibleWithAnyPlatform(false));
            changed |= SetIfDifferent(importer.GetCompatibleWithEditor(), true, () => importer.SetCompatibleWithEditor(true));
            changed |= SetIfDifferent(importer.GetCompatibleWithPlatform(BuildTarget.StandaloneWindows64), true, () => importer.SetCompatibleWithPlatform(BuildTarget.StandaloneWindows64, true));
            changed |= SetIfDifferent(importer.GetCompatibleWithPlatform(BuildTarget.StandaloneWindows), false, () => importer.SetCompatibleWithPlatform(BuildTarget.StandaloneWindows, false));
            changed |= SetIfDifferent(importer.GetCompatibleWithPlatform(BuildTarget.StandaloneLinux64), false, () => importer.SetCompatibleWithPlatform(BuildTarget.StandaloneLinux64, false));
            changed |= SetIfDifferent(importer.GetCompatibleWithPlatform(BuildTarget.StandaloneOSX), false, () => importer.SetCompatibleWithPlatform(BuildTarget.StandaloneOSX, false));

            if (changed)
            {
                importer.SaveAndReimport();
                Debug.Log("[nicxlive] PluginImporter: updated.");
            }
            else
            {
                Debug.Log("[nicxlive] PluginImporter: already configured.");
            }

            return changed;
        }

        private static bool EnsureUrpPipeline()
        {
            var current = GetConfiguredGraphicsPipeline();
            if (IsUrpAsset(current))
            {
                var qualityChanged = ApplyUrpToAllQualityLevels(current!);
                if (!qualityChanged)
                {
                    Debug.Log("[nicxlive] URP: already configured.");
                }
                return qualityChanged;
            }

            var urpAsset = FindExistingUrpAsset();
            if (urpAsset == null)
            {
                urpAsset = CreateUrpAssetByReflection();
            }

            if (urpAsset == null)
            {
                Debug.LogWarning("[nicxlive] URP asset was not found/created. Install Universal RP package first.");
                return false;
            }

            if (!SetConfiguredGraphicsPipeline(urpAsset))
            {
                Debug.LogWarning("[nicxlive] Failed to assign GraphicsSettings render pipeline.");
                return false;
            }
            var changed = true;
            changed |= ApplyUrpToAllQualityLevels(urpAsset);
            Debug.Log("[nicxlive] URP: assigned render pipeline asset.");
            return changed;
        }

        private static RenderPipelineAsset? FindExistingUrpAsset()
        {
            var guids = AssetDatabase.FindAssets("t:RenderPipelineAsset");
            for (var i = 0; i < guids.Length; i++)
            {
                var path = AssetDatabase.GUIDToAssetPath(guids[i]);
                var asset = AssetDatabase.LoadAssetAtPath<RenderPipelineAsset>(path);
                if (IsUrpAsset(asset))
                {
                    return asset;
                }
            }
            return null;
        }

        private static RenderPipelineAsset? CreateUrpAssetByReflection()
        {
            var urpAssetType = Type.GetType("UnityEngine.Rendering.Universal.UniversalRenderPipelineAsset, Unity.RenderPipelines.Universal.Runtime");
            var rendererDataType = Type.GetType("UnityEngine.Rendering.Universal.UniversalRendererData, Unity.RenderPipelines.Universal.Runtime");
            if (urpAssetType == null || rendererDataType == null)
            {
                return null;
            }

            EnsureFolder("Assets/Nicxlive");
            EnsureFolder(SettingsDir);

            var rendererData = ScriptableObject.CreateInstance(rendererDataType);
            if (rendererData == null)
            {
                return null;
            }
            AssetDatabase.CreateAsset(rendererData, RendererAssetPath);

            var urpAssetObj = ScriptableObject.CreateInstance(urpAssetType);
            if (!(urpAssetObj is RenderPipelineAsset urpAsset))
            {
                return null;
            }

            var serialized = new SerializedObject(urpAssetObj);
            var list = serialized.FindProperty("m_RendererDataList");
            if (list != null)
            {
                list.arraySize = 1;
                list.GetArrayElementAtIndex(0).objectReferenceValue = rendererData;
            }
            var defaultRenderer = serialized.FindProperty("m_DefaultRendererIndex");
            if (defaultRenderer != null)
            {
                defaultRenderer.intValue = 0;
            }
            serialized.ApplyModifiedPropertiesWithoutUndo();

            AssetDatabase.CreateAsset(urpAssetObj, UrpAssetPath);
            return urpAsset;
        }

        private static bool ApplyUrpToAllQualityLevels(RenderPipelineAsset urpAsset)
        {
            var changed = false;
            var qualityNames = QualitySettings.names;
            var qualityType = typeof(QualitySettings);
            var getAt = qualityType.GetMethod("GetRenderPipelineAssetAt", BindingFlags.Public | BindingFlags.Static);
            var setAt = qualityType.GetMethod("SetRenderPipelineAssetAt", BindingFlags.Public | BindingFlags.Static);
            if (getAt != null && setAt != null)
            {
                for (var i = 0; i < qualityNames.Length; i++)
                {
                    var current = getAt.Invoke(null, new object[] { i }) as RenderPipelineAsset;
                    if (current == urpAsset)
                    {
                        continue;
                    }
                    setAt.Invoke(null, new object[] { i, urpAsset });
                    changed = true;
                }
                return changed;
            }

            var renderPipeline = qualityType.GetProperty("renderPipeline", BindingFlags.Public | BindingFlags.Static);
            if (renderPipeline != null && renderPipeline.CanWrite)
            {
                var current = renderPipeline.GetValue(null) as RenderPipelineAsset;
                if (current != urpAsset)
                {
                    renderPipeline.SetValue(null, urpAsset, null);
                    changed = true;
                }
            }
            return changed;
        }

        private static bool IsUrpAsset(RenderPipelineAsset? asset)
        {
            if (asset == null)
            {
                return false;
            }
            var fullName = asset.GetType().FullName;
            return !string.IsNullOrEmpty(fullName) && fullName.Contains("UniversalRenderPipelineAsset");
        }

        private static RenderPipelineAsset? GetConfiguredGraphicsPipeline()
        {
            var graphicsType = typeof(GraphicsSettings);
            var defaultPipeline = graphicsType.GetProperty("defaultRenderPipeline", BindingFlags.Public | BindingFlags.Static);
            if (defaultPipeline != null)
            {
                return defaultPipeline.GetValue(null) as RenderPipelineAsset;
            }

            var currentPipeline = graphicsType.GetProperty("currentRenderPipeline", BindingFlags.Public | BindingFlags.Static);
            return currentPipeline != null ? currentPipeline.GetValue(null) as RenderPipelineAsset : null;
        }

        private static bool SetConfiguredGraphicsPipeline(RenderPipelineAsset asset)
        {
            var graphicsType = typeof(GraphicsSettings);
            var defaultPipeline = graphicsType.GetProperty("defaultRenderPipeline", BindingFlags.Public | BindingFlags.Static);
            if (defaultPipeline != null && defaultPipeline.CanWrite)
            {
                defaultPipeline.SetValue(null, asset, null);
                return true;
            }

            var currentPipeline = graphicsType.GetProperty("currentRenderPipeline", BindingFlags.Public | BindingFlags.Static);
            if (currentPipeline != null && currentPipeline.CanWrite)
            {
                currentPipeline.SetValue(null, asset, null);
                return true;
            }

            return false;
        }

        private static void EnsureFolder(string folderPath)
        {
            if (AssetDatabase.IsValidFolder(folderPath))
            {
                return;
            }

            var split = folderPath.Split('/');
            var current = split[0];
            for (var i = 1; i < split.Length; i++)
            {
                var next = current + "/" + split[i];
                if (!AssetDatabase.IsValidFolder(next))
                {
                    AssetDatabase.CreateFolder(current, split[i]);
                }
                current = next;
            }
        }

        private static bool SetIfDifferent(bool current, bool expected, Action setter)
        {
            if (current == expected)
            {
                return false;
            }
            setter();
            return true;
        }
    }
}
#endif

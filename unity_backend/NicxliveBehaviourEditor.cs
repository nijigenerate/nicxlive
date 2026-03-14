#nullable enable
#if UNITY_EDITOR
using System;
using System.IO;
using Nicxlive.UnityBackend.Managed;
using UnityEditor;
using UnityEngine;

namespace Nicxlive.UnityBackend.Editor
{
    [CustomEditor(typeof(NicxliveBehaviour))]
    public sealed class NicxliveBehaviourEditor : UnityEditor.Editor
    {
        private SerializedProperty? _puppetAsset;
        private SerializedProperty? _puppetPath;
        private SerializedProperty? _modelScale;
        private SerializedProperty? _modelOffsetPixels;
        private SerializedProperty? _drawInEditMode;
        private SerializedProperty? _showRuntimeDebugOverlayComparison;
        private SerializedProperty? _targetCamera;
        private SerializedProperty? _partMaterial;
        private SerializedProperty? _maskMaterial;
        private SerializedProperty? _shaderProperties;

        private void OnEnable()
        {
            _puppetAsset = serializedObject.FindProperty(nameof(NicxliveBehaviour.PuppetAsset));
            _puppetPath = serializedObject.FindProperty(nameof(NicxliveBehaviour.PuppetPath));
            _modelScale = serializedObject.FindProperty(nameof(NicxliveBehaviour.ModelScale));
            _modelOffsetPixels = serializedObject.FindProperty(nameof(NicxliveBehaviour.ModelOffsetPixels));
            _drawInEditMode = serializedObject.FindProperty(nameof(NicxliveBehaviour.DrawInEditMode));
            _showRuntimeDebugOverlayComparison = serializedObject.FindProperty(nameof(NicxliveBehaviour.ShowRuntimeDebugOverlayComparison));
            _targetCamera = serializedObject.FindProperty(nameof(NicxliveBehaviour.TargetCamera));
            _partMaterial = serializedObject.FindProperty(nameof(NicxliveBehaviour.PartMaterial));
            _maskMaterial = serializedObject.FindProperty(nameof(NicxliveBehaviour.MaskMaterial));
            _shaderProperties = serializedObject.FindProperty(nameof(NicxliveBehaviour.ShaderProperties));
        }

        public override void OnInspectorGUI()
        {
            serializedObject.Update();

            EditorGUILayout.LabelField("Model Source", EditorStyles.boldLabel);
            if (_puppetAsset != null)
            {
                EditorGUILayout.PropertyField(_puppetAsset);
            }
            if (_puppetPath != null)
            {
                EditorGUI.BeginChangeCheck();
                var nextPath = EditorGUILayout.TextField("Puppet Path", _puppetPath.stringValue);
                if (EditorGUI.EndChangeCheck())
                {
                    _puppetPath.stringValue = nextPath;
                }
            }
            if (DrawPuppetPathPicker())
            {
                serializedObject.ApplyModifiedPropertiesWithoutUndo();
                serializedObject.Update();
                EditorUtility.SetDirty(target);
                Repaint();
            }

            if (GUILayout.Button("Use Selected Asset Path") && _puppetAsset != null && _puppetPath != null)
            {
                var selected = _puppetAsset.objectReferenceValue;
                if (selected != null)
                {
                    var relativePath = AssetDatabase.GetAssetPath(selected);
                    if (!string.IsNullOrWhiteSpace(relativePath))
                    {
                        _puppetPath.stringValue = Path.GetFullPath(relativePath);
                    }
                }
            }

            EditorGUILayout.Space();
            EditorGUILayout.LabelField("Transform", EditorStyles.boldLabel);
            if (_modelScale != null)
            {
                EditorGUILayout.PropertyField(_modelScale);
            }
            if (_modelOffsetPixels != null)
            {
                EditorGUILayout.PropertyField(_modelOffsetPixels);
            }
            if (_drawInEditMode != null)
            {
                EditorGUILayout.PropertyField(_drawInEditMode);
            }

            EditorGUILayout.Space();
            EditorGUILayout.LabelField("Rendering", EditorStyles.boldLabel);
            if (_targetCamera != null)
            {
                EditorGUILayout.PropertyField(_targetCamera);
            }
            if (_showRuntimeDebugOverlayComparison != null)
            {
                EditorGUILayout.PropertyField(_showRuntimeDebugOverlayComparison);
            }
            if (_partMaterial != null)
            {
                EditorGUILayout.PropertyField(_partMaterial);
            }
            if (_maskMaterial != null)
            {
                EditorGUILayout.PropertyField(_maskMaterial);
            }
            if (_shaderProperties != null)
            {
                EditorGUILayout.PropertyField(_shaderProperties, true);
            }

            serializedObject.ApplyModifiedProperties();

            var behaviour = (NicxliveBehaviour)target;
            EditorGUILayout.Space();
            EditorGUILayout.LabelField("Runtime Controls", EditorStyles.boldLabel);
            if (!string.IsNullOrWhiteSpace(behaviour.LastRouterDiag))
            {
                EditorGUILayout.HelpBox(behaviour.LastRouterDiag, MessageType.None);
            }
            if (!string.IsNullOrWhiteSpace(behaviour.LastRootSceneDiag))
            {
                EditorGUILayout.HelpBox(behaviour.LastRootSceneDiag, MessageType.None);
            }
            EditorGUILayout.LabelField("Has Loaded Puppet", behaviour.HasLoadedPuppet ? "Yes" : "No");
            EditorGUILayout.LabelField("Decoded Commands", behaviour.LastDecodedCommandCount.ToString());
            EditorGUILayout.LabelField("Shared Vertices", behaviour.LastSharedVertexCount.ToString());
            EditorGUILayout.LabelField("Part Packets", behaviour.LastPartPacketCount.ToString());
            EditorGUILayout.LabelField("Part Draw Issued", behaviour.LastPartDrawIssuedCount.ToString());
            EditorGUILayout.LabelField("Part Skip NoTexture", behaviour.LastPartSkippedNoTextureCount.ToString());
            EditorGUILayout.LabelField("Part Skip MeshBuild", behaviour.LastPartSkippedMeshBuildCount.ToString());
            EditorGUILayout.LabelField("Part Opacity Min/Max", $"{behaviour.LastPartOpacityMin:F3} / {behaviour.LastPartOpacityMax:F3}");
            EditorGUILayout.LabelField("Part Opacity Zero/NonZero", $"{behaviour.LastPartOpacityZeroCount} / {behaviour.LastPartOpacityNonZeroCount}");
            EditorGUILayout.LabelField("Part Tex0 Missing", behaviour.LastPartTextureMissingCount.ToString());
            EditorGUILayout.LabelField("Mask Packets", behaviour.LastMaskPacketCount.ToString());
            EditorGUILayout.LabelField("Mask Draw Issued", behaviour.LastMaskDrawIssuedCount.ToString());
            EditorGUILayout.LabelField("Mask Skip MeshBuild", behaviour.LastMaskSkippedMeshBuildCount.ToString());
            EditorGUILayout.LabelField(
                "Part Clip Bounds",
                behaviour.HasPartClipBounds ? FormatBounds(behaviour.LastPartClipBounds) : "N/A");
            EditorGUILayout.LabelField(
                "Mask Clip Bounds",
                behaviour.HasMaskClipBounds ? FormatBounds(behaviour.LastMaskClipBounds) : "N/A");
            EditorGUILayout.LabelField("Resolved Camera", string.IsNullOrWhiteSpace(behaviour.LastResolvedCameraName) ? "N/A" : behaviour.LastResolvedCameraName);
            EditorGUILayout.LabelField("Translation Mode", string.IsNullOrWhiteSpace(behaviour.LastTranslationMode) ? "N/A" : behaviour.LastTranslationMode);
            EditorGUILayout.LabelField(
                "Screen Point",
                $"({behaviour.LastScreenPoint.x:F1}, {behaviour.LastScreenPoint.y:F1}, {behaviour.LastScreenPoint.z:F1})");
            EditorGUILayout.LabelField(
                "Applied Translation",
                $"({behaviour.LastAppliedTranslation.x:F1}, {behaviour.LastAppliedTranslation.y:F1})");
            EditorGUILayout.LabelField(
                "Applied Scale",
                $"({behaviour.LastAppliedScale.x:F3}, {behaviour.LastAppliedScale.y:F3})");
            if (!string.IsNullOrWhiteSpace(behaviour.LastVisibilityDiag))
            {
                EditorGUILayout.HelpBox(behaviour.LastVisibilityDiag, MessageType.None);
            }
            if (!string.IsNullOrWhiteSpace(behaviour.LastPartBuildDiag))
            {
                EditorGUILayout.HelpBox(behaviour.LastPartBuildDiag, MessageType.Warning);
            }
            if (!string.IsNullOrWhiteSpace(behaviour.LastFrameDiag))
            {
                EditorGUILayout.HelpBox(behaviour.LastFrameDiag, MessageType.None);
            }
            if (!string.IsNullOrWhiteSpace(behaviour.LastClipFitDiag))
            {
                EditorGUILayout.HelpBox(behaviour.LastClipFitDiag, MessageType.None);
            }
            if (!string.IsNullOrWhiteSpace(behaviour.LastRenderPathDiag))
            {
                EditorGUILayout.HelpBox(behaviour.LastRenderPathDiag, MessageType.None);
            }
            if (GUILayout.Button("Reload Puppet"))
            {
                behaviour.ReloadPuppetFromCurrentSelection();
            }
            if (GUILayout.Button("Refresh Parameters"))
            {
                behaviour.RefreshParameters();
            }
            if (GUILayout.Button("Apply Transform To Puppet"))
            {
                behaviour.ApplyTransformNow();
            }

            DrawParameterControls(behaviour);

            if (!Application.isPlaying)
            {
                Repaint();
            }
        }

        private static void DrawParameterControls(NicxliveBehaviour behaviour)
        {
            EditorGUILayout.Space();
            EditorGUILayout.LabelField("Model Parameters", EditorStyles.boldLabel);

            if (!behaviour.HasLoadedPuppet)
            {
                EditorGUILayout.HelpBox("Puppet is not loaded.", MessageType.Info);
                return;
            }

            var parameters = behaviour.ParameterStates;
            if (parameters.Count == 0)
            {
                EditorGUILayout.HelpBox("No editable parameters were reported by native runtime.", MessageType.Info);
                return;
            }

            for (var i = 0; i < parameters.Count; i++)
            {
                var state = parameters[i];
                if (state == null)
                {
                    continue;
                }

                var label = string.IsNullOrWhiteSpace(state.Name) ? $"param_{state.Uuid}" : state.Name;
                EditorGUI.BeginChangeCheck();
                var next = state.Value;
                if (state.IsVec2)
                {
                    next.x = EditorGUILayout.Slider($"{label} X", state.Value.x, state.Min.x, state.Max.x);
                    next.y = EditorGUILayout.Slider($"{label} Y", state.Value.y, state.Min.y, state.Max.y);
                }
                else
                {
                    next.x = EditorGUILayout.Slider(label, state.Value.x, state.Min.x, state.Max.x);
                }

                if (EditorGUI.EndChangeCheck())
                {
                    behaviour.SetParameterValue(state.Uuid, next);
                }
            }
        }

        private bool DrawPuppetPathPicker()
        {
            if (_puppetPath == null)
            {
                return false;
            }

            var changed = false;
            using (new EditorGUILayout.HorizontalScope())
            {
                if (GUILayout.Button("Select Puppet File..."))
                {
                    var initialDirectory = ResolveInitialBrowseDirectory(_puppetPath.stringValue);
                    var selectedPath = EditorUtility.OpenFilePanel("Select Puppet File", initialDirectory, string.Empty);
                    if (!string.IsNullOrWhiteSpace(selectedPath))
                    {
                        var fullPath = Path.GetFullPath(selectedPath);
                        _puppetPath.stringValue = fullPath;
                        SyncPuppetAssetFromPath(fullPath);
                        changed = true;
                    }
                }

                if (GUILayout.Button("Clear", GUILayout.Width(70f)))
                {
                    _puppetPath.stringValue = string.Empty;
                    if (_puppetAsset != null)
                    {
                        _puppetAsset.objectReferenceValue = null;
                    }
                    changed = true;
                }
            }

            return changed;
        }

        private void SyncPuppetAssetFromPath(string fullPath)
        {
            if (_puppetAsset == null)
            {
                return;
            }

            var projectRelative = ToProjectRelativePath(fullPath);
            if (string.IsNullOrWhiteSpace(projectRelative))
            {
                _puppetAsset.objectReferenceValue = null;
                return;
            }

            _puppetAsset.objectReferenceValue = AssetDatabase.LoadAssetAtPath<UnityEngine.Object>(projectRelative);
        }

        private static string ResolveInitialBrowseDirectory(string currentPath)
        {
            if (!string.IsNullOrWhiteSpace(currentPath))
            {
                var full = Path.GetFullPath(currentPath);
                if (File.Exists(full))
                {
                    return Path.GetDirectoryName(full) ?? Application.dataPath;
                }
                if (Directory.Exists(full))
                {
                    return full;
                }
            }
            return Application.dataPath;
        }

        private static string ToProjectRelativePath(string fullPath)
        {
            var normalizedPath = Path.GetFullPath(fullPath).Replace('\\', '/');
            var assetsRoot = Path.GetFullPath(Application.dataPath).Replace('\\', '/');
            if (!assetsRoot.EndsWith("/", StringComparison.Ordinal))
            {
                assetsRoot += "/";
            }
            if (!normalizedPath.StartsWith(assetsRoot, StringComparison.OrdinalIgnoreCase))
            {
                return string.Empty;
            }

            return "Assets/" + normalizedPath.Substring(assetsRoot.Length);
        }

        private static string FormatBounds(Vector4 bounds)
        {
            return $"x:[{bounds.x:F4},{bounds.z:F4}] y:[{bounds.y:F4},{bounds.w:F4}]";
        }
    }
}
#endif

#if UNITY_EDITOR
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
                EditorGUILayout.PropertyField(_puppetPath);
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
    }
}
#endif

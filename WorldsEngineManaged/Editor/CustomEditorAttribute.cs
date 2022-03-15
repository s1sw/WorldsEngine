using System;
namespace WorldsEngine.Editor;

[AttributeUsage(AttributeTargets.Class)]
public class CustomEditorAttribute : Attribute
{
    public Type EditorType { get; private set; }

    public CustomEditorAttribute(Type editorType)
    {
        EditorType = editorType;
    }
}
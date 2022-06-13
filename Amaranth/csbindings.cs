using System;
using System.Runtime.InteropServices;
namespace WorldsEngine.Editor;
class GameProject
{
private IntPtr nativeInstance;
public GameProject(IntPtr instance){
nativeInstance = instance;
}
[DllImport(Engine.NativeModule)]
private static extern IntPtr worlds__GameProject_name(IntPtr inst);
public string Name
{
get
{
IntPtr tmp = worlds__GameProject_name(nativeInstance);
string t = Marshal.PtrToStringUTF8(tmp)!;
Marshal.FreeHGlobal(tmp);
return t;
}
}
[DllImport(Engine.NativeModule)]
private static extern IntPtr worlds__GameProject_root(IntPtr inst);
public string Root
{
get
{
IntPtr tmp = worlds__GameProject_root(nativeInstance);
string t = Marshal.PtrToStringUTF8(tmp)!;
Marshal.FreeHGlobal(tmp);
return t;
}
}
[DllImport(Engine.NativeModule)]
private static extern IntPtr worlds__GameProject_sourceData(IntPtr inst);
public string SourceData
{
get
{
IntPtr tmp = worlds__GameProject_sourceData(nativeInstance);
string t = Marshal.PtrToStringUTF8(tmp)!;
Marshal.FreeHGlobal(tmp);
return t;
}
}
[DllImport(Engine.NativeModule)]
private static extern IntPtr worlds__GameProject_builtData(IntPtr inst);
public string BuildData
{
get
{
IntPtr tmp = worlds__GameProject_builtData(nativeInstance);
string t = Marshal.PtrToStringUTF8(tmp)!;
Marshal.FreeHGlobal(tmp);
return t;
}
}
[DllImport(Engine.NativeModule)]
private static extern IntPtr worlds__GameProject_rawData(IntPtr inst);
public string RawData
{
get
{
IntPtr tmp = worlds__GameProject_rawData(nativeInstance);
string t = Marshal.PtrToStringUTF8(tmp)!;
Marshal.FreeHGlobal(tmp);
return t;
}
}
}class RigidBody
{
private IntPtr nativeInstance;
public RigidBody(IntPtr instance){
nativeInstance = instance;
}
}
using System;
using System.Reflection;
using System.Diagnostics;
using System.Runtime.Serialization;
using System.Runtime.CompilerServices;
using System.Collections.Generic;

namespace WorldsEngine.Hotloading
{
    [System.Serializable]
    public class HotloadSwapFailedException : System.Exception
    {
        public HotloadSwapFailedException() { }
        public HotloadSwapFailedException(string message) : base(message) { }
        public HotloadSwapFailedException(string message, System.Exception inner) : base(message, inner) { }
        protected HotloadSwapFailedException(
            System.Runtime.Serialization.SerializationInfo info,
            System.Runtime.Serialization.StreamingContext context) : base(info, context) { }
    }

    internal class HotloadSwapper
    {
        internal class EverythingEqualityComparer : IEqualityComparer<object>
        {
            public int GetHashCode(object obj) { return RuntimeHelpers.GetHashCode(obj); }
            public new bool Equals(object? a, object? b) { return object.ReferenceEquals(a, b); }
        }

        private Assembly oldAssembly;
        private Assembly newAssembly;

        private Dictionary<object, object> reserialized = new(new EverythingEqualityComparer());

        public HotloadSwapper(Assembly oldAssembly, Assembly newAssembly)
        {
            this.oldAssembly = oldAssembly;
            this.newAssembly = newAssembly;
        }

        public void SwapReferences()
        {
            Assembly worldsAssembly = Assembly.GetExecutingAssembly();

            // Start at the static roots of the Worlds Engine assembly
            // Since we're hosted inside C++, we can guarantee that the only
            // references to hotload assembly objects will be in statics
            foreach (Type type in worldsAssembly.GetTypes())
            {
                if (type.ContainsGenericParameters) continue;

                var fields = type.GetFields(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static);

                if (fields.Length == 0) continue;

                foreach (FieldInfo fieldInfo in fields)
                {
                    if (fieldInfo.IsInitOnly)
                    {
                        // We can ignore the return value of RewriteObject here
                        // as we can't set the field's value anyway
                        RewriteObject(fieldInfo.GetValue(null));
                    }
                    else if (!fieldInfo.IsLiteral)
                    {
                        RewriteField(fieldInfo, null);
                    }
                }
            }

            foreach (Type type in newAssembly.GetTypes())
            {
                if (type.ContainsGenericParameters) continue;

                Type? oldType = oldAssembly.GetType(type.FullName!);

                if (oldType == null)
                {
                    Log.Warn($"Type {type.FullName} appears to be new! Ignoring it.");
                    continue;
                }

                var fields = type.GetFields(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static);

                if (fields.Length == 0) continue;

                foreach (FieldInfo fieldInfo in fields)
                {
                    FieldInfo? oldFieldInfo = oldType.GetField(fieldInfo.Name, BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static);

                    if (oldFieldInfo == null)
                    {
                        Log.Warn($"Couldn't find old field for {type.FullName}.{fieldInfo.Name}!");
                        continue;
                    }


                    if (fieldInfo.IsInitOnly)
                    {
                        // We can ignore the return value of RewriteObject here
                        // as we can't set the field's value anyway
                        RewriteObject(fieldInfo.GetValue(null));
                    }
                    else if (!fieldInfo.IsLiteral)
                    {
                        RewriteField(oldFieldInfo, fieldInfo, null, null);
                    }
                }
            }
        }

        private Type? FindNewType(Type oldType)
        {
            if (!IsDependentOnAssembly(oldType))
                return oldType;
            if (oldType.IsArray)
            {
                Type elType = oldType.GetElementType()!;
                Type? newElType = FindNewType(elType);

                if (newElType == null) return null;

                Type newArrayType = newElType.MakeArrayType(oldType.GetArrayRank());

                return newArrayType;
            }

            if (oldType.IsConstructedGenericType)
            {
                Type[] genericArgs = oldType.GetGenericArguments();
                Type?[] newGenericArgs = new Type[genericArgs.Length];

                for (int i = 0; i < genericArgs.Length; i++)
                {
                    if (IsDependentOnAssembly(genericArgs[i]))
                        newGenericArgs[i] = FindNewType(genericArgs[i]);
                    else
                        newGenericArgs[i] = genericArgs[i];

                    if (newGenericArgs[i] == null) return null;
                }

                return oldType.GetGenericTypeDefinition().MakeGenericType(newGenericArgs!);
            }

            return newAssembly.GetType(oldType.FullName!);
        }

        private bool IsDependentOnAssembly(Type t)
        {
            if (t.IsConstructedGenericType)
            {
                Type[] typeArgs = t.GetGenericArguments();

                foreach (Type genericArg in typeArgs)
                {
                    if (IsDependentOnAssembly(genericArg)) return true;
                }
            }

            if (t.HasElementType)
            {
                return IsDependentOnAssembly(t.GetElementType()!);
            }

            return t.Assembly == oldAssembly;
        }

        private void RewriteField(FieldInfo from, FieldInfo to, object? oldInstance, object? newInstance, int recursionCounter = 0)
        {
            if (recursionCounter >= 250)
            {
                string msg = $"Likely infinite loop detected!!!! Processing field {from.DeclaringType!.FullName}.{from.Name}";
                Log.Error(msg);
                throw new Exception(msg);
            }
            if (from.DeclaringType == typeof(System.Reflection.Pointer)) return;

            object? val = from.GetValue(oldInstance);

            // Don't need to rewrite if it's null!
            if (val == null) return;

            // Some special handling is required for events/delegates

            object? newVal = RewriteObject(val, recursionCounter + 1);
            to.SetValue(newInstance, newVal);
        }

        private void RewriteField(FieldInfo fieldInfo, object? instance, int recusionCounter = 0)
        {
            RewriteField(fieldInfo, fieldInfo, instance, instance, recusionCounter);
        }

        const BindingFlags HotloadFieldBindingFlags = BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic;

        private object? RewriteObject(object? oldObject, int recursionCounter = 0)
        {
            if (oldObject == null) return null;
            if (recursionCounter >= 300)
            {
                throw new Exception($"Likely infinite loop detected!!!! Processing object of type {oldObject.GetType().FullName}");
            }

            if (reserialized.ContainsKey(oldObject))
            {
                return reserialized[oldObject];
            }

            var oldType = oldObject.GetType();

            if (oldType == typeof(Assembly) || oldType == typeof(LoadedAssembly)) return oldObject;

            if (oldType.IsPrimitive)
                return oldObject;

            if (oldType.IsAssignableTo(typeof(Delegate)))
            {
                Delegate del = (Delegate)oldObject;

                if (del.Method != null && del.Method.DeclaringType != null && del.Method.DeclaringType.Assembly != oldAssembly) return oldObject;

                Type? newDelegateType = FindNewType(oldType);
                if (newDelegateType == null)
                    throw new HotloadSwapFailedException($"Couldn't find new type for delegate type {oldType.FullName}");


                Delegate? newDel = null;

                if (del.Target != null && del.Method != null)
                {
                    newDel = Delegate.CreateDelegate(newDelegateType, RewriteObject(del.Target, recursionCounter + 1)!, del.Method.Name, false);
                }
                else if (del.Method != null)
                {
                    if (del.Method.DeclaringType != null)
                    {
                        MethodInfo? newMi = FindNewType(del.Method.DeclaringType)!.GetMethod(del.Method.Name!);

                        if (newMi == null) throw new HotloadSwapFailedException("oops oops");

                        newDel = Delegate.CreateDelegate(newDelegateType, newMi, true)!;
                    }
                    else
                    {
                        newDel = Delegate.CreateDelegate(newDelegateType, del.Method, true)!;
                    }
                }

                return newDel;
            }

            if (oldType.IsArray)
            {
                // Copy elements of the array
                var newArrayType = FindNewType(oldType);

                if (newArrayType == null) return null;

                Array oldArray = (Array)oldObject;
                Array newArray = Array.CreateInstance(newArrayType.GetElementType()!, oldArray.GetLength(0));

                for (int i = 0; i < oldArray.Length; i++)
                {
                    var arrayValue = oldArray.GetValue(i);

                    if (arrayValue == null) continue;

                    newArray.SetValue(RewriteObject(arrayValue, recursionCounter + 1), i);
                }

                // If the types are equal, there's no need to exchange the array
                if (newArrayType == oldType)
                {
                    reserialized.Add(oldObject, oldObject);
                    newArray.CopyTo(oldArray, 0);
                    return oldArray;
                }

                return newArray;
            }

            if (oldType == typeof(Type))
            {
                Type objAsType = (Type)oldObject;

                if (IsDependentOnAssembly(objAsType))
                {
                    Type? newlyFoundType = FindNewType(objAsType);

                    if (newlyFoundType == null) throw new HotloadSwapFailedException($"Couldn't find type {objAsType.FullName}");

                    return newlyFoundType;
                }

                return objAsType;
            }

            if (oldType == typeof(FieldInfo))
            {
                FieldInfo objAsFieldInfo = (FieldInfo)oldObject;
                Type? declaringType = objAsFieldInfo.DeclaringType;

                if (declaringType != null && IsDependentOnAssembly(declaringType))
                {
                    Type? newType = FindNewType(declaringType);

                    if (newType == null) throw new HotloadSwapFailedException($"Couldn't find type {declaringType.FullName}");

                    // Find the equivalent field on the new type
                    FieldInfo? newFieldInfo = newType.GetField(objAsFieldInfo.Name);

                    return newFieldInfo;
                }

                return objAsFieldInfo;
            }

            if (IsDependentOnAssembly(oldType))
            {
                var newType = FindNewType(oldType);
                if (newType == null) return null;

                if (Nullable.GetUnderlyingType(oldType) != null)
                {
                    // OOF OOF OOF
                    return null;
                }

                var newObject = FormatterServices.GetUninitializedObject(newType);
                var oldFields = oldType.GetFields(HotloadFieldBindingFlags);
                reserialized.Add(oldObject, newObject);

                foreach (FieldInfo fieldInfo in oldFields)
                {
                    FieldInfo? newFieldInfo = newType.GetField(fieldInfo.Name, HotloadFieldBindingFlags);

                    if (newFieldInfo == null)
                    {
                        Log.Warn($"Failed to find new field info for {oldType.FullName}.{fieldInfo.Name}");
                        continue;
                    }

                    RewriteField(fieldInfo, newFieldInfo, oldObject, newObject, recursionCounter + 1);
                }

                Type? baseType = newType.BaseType;
                Type? oldBaseType = oldType.BaseType;

                // Walk through the type hierarchy and rewrite the fields of base classes too
                while (baseType != null)
                {
                    if (oldBaseType == null) throw new HotloadSwapFailedException("New and old type hierarchies differ!");
                    
                    var oldBaseFields = oldBaseType.GetFields(HotloadFieldBindingFlags);

                    foreach (FieldInfo baseField in oldBaseFields)
                    {
                        FieldInfo? newBaseField = baseType.GetField(baseField.Name, HotloadFieldBindingFlags);

                        if (newBaseField == null) continue;

                        RewriteField(baseField, newBaseField, oldObject, newObject, recursionCounter + 1);
                    }

                    baseType = baseType.BaseType;
                    oldBaseType = oldBaseType.BaseType;
                }

                return newObject;
            }
            else
            {
                var oldFields = oldType.GetFields(HotloadFieldBindingFlags);
                reserialized.Add(oldObject, oldObject);

                foreach (FieldInfo fieldInfo in oldFields)
                {
                    RewriteField(fieldInfo, oldObject, recursionCounter + 1);
                }

                Type? baseType = oldType.BaseType;
                while (baseType != null)
                {
                    var baseFields = baseType.GetFields();

                    foreach (FieldInfo baseField in baseFields)
                    {
                        FieldInfo? newBaseField = baseType.GetField(baseField.Name, HotloadFieldBindingFlags);

                        if (newBaseField == null) continue;

                        RewriteField(baseField, newBaseField, oldObject, oldObject, recursionCounter + 1);
                    }

                    baseType = baseType.BaseType;
                }

                return oldObject;
            }
        }
    }
}
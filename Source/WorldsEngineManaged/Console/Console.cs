using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Reflection;

namespace WorldsEngine
{
    internal static class Console
    {
        class Command
        {
            public Action<string> Method;
            public bool CurrentlyLoaded = true;
            public bool InGameAssembly = false;

            public Command(Action<string> method)
            {
                Method = method;
            }
        }

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate void CommandCallbackDelegate(int id, string args);

        [DllImport(Engine.NativeModule, CharSet = CharSet.Ansi)]
        private static extern void console_registerCommand(CommandCallbackDelegate cmdDelegate, string name, string? help, int id);
        
        [DllImport(Engine.NativeModule, CharSet = CharSet.Ansi)]
        private static extern void console_executeCommand(string cmd);

        private static readonly CommandCallbackDelegate callbackDelegate;

        private static readonly List<Command> commands = new List<Command>();
        private static readonly Dictionary<string, Command> commandsByCmd = new Dictionary<string, Command>();

        static Console()
        {
            callbackDelegate = CommandCallback;
        }

        internal static void Initialise()
        {
            Engine.AssemblyLoadManager.OnAssemblyLoad += RegisterCommands;
            Engine.AssemblyLoadManager.OnAssemblyUnload += PrepareForUnload;
        }

        private static void CommandCallback(int id, string args)
        {
            var command = commands[id];
            command.Method(args);
        }

        private static void RegisterCommands(Assembly assembly)
        {
            foreach (Type type in assembly.GetTypes())
            {
                foreach (MethodInfo method in type.GetMethods(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static))
                {
                    var attr = method.GetCustomAttribute<ConsoleCommandAttribute>();

                    if (attr != null)
                    {
                        if (commandsByCmd.ContainsKey(attr.Command))
                        {
                            var command = commandsByCmd[attr.Command];
                            command.CurrentlyLoaded = true;
                            command.Method = method.CreateDelegate<Action<string>>();
                        }
                        else
                        {
                            var cmdDelegate = method.CreateDelegate<Action<string>>();
                            Command command = new Command(cmdDelegate)
                            {
                                InGameAssembly = true
                            };

                            int index = commands.Count;
                            commands.Add(command);
                            commandsByCmd.Add(attr.Command, command);
                            console_registerCommand(callbackDelegate, attr.Command, attr.Help, index);
                        }
                    }
                }
            }
        }

        private static void PrepareForUnload()
        {
            foreach (Command cmd in commands)
            {
                if (!cmd.InGameAssembly) continue;

                // prepare for unloading
                cmd.Method = (string arg) =>
                {
                    Logger.LogError("Sorry, but that command doesn't exist anymore.");
                };

                cmd.CurrentlyLoaded = false;
            }
        }

        public static void RunCommand(string command) => console_executeCommand(command);
    }
}

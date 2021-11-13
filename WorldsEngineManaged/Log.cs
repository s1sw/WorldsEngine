using System;
using System.Runtime.InteropServices;

namespace WorldsEngine
{
    public enum MessageSeverity
    {
        Verbose,
        Info,
        Warning,
        Error
    }

    public static class Log
    {
        [DllImport(WorldsEngine.NativeModule)]
        private static extern void logging_log(int severity, string message);

        public static void LogWithSeverity(MessageSeverity severity, string message)
        {
            logging_log((int)severity, message);
        }

        public static void Msg(string str)
        {
            LogWithSeverity(MessageSeverity.Info, str);
        }

        public static void Warn(string str)
        {
            LogWithSeverity(MessageSeverity.Warning, str);
        }

        public static void Error(string str)
        {
            LogWithSeverity(MessageSeverity.Error, str);
        }
    }

    public class Logger
    {
        [DllImport(WorldsEngine.NativeModule)]
        private static extern void logging_log(int severity, string message);

        public static void LogMessage(MessageSeverity severity, string str)
        {
            int severityInt = (int)severity;
            logging_log(severityInt, str);
        }

        public static void Log(string str)
        {
            LogMessage(MessageSeverity.Info, str);
        }

        public static void LogWarning(string str)
        {
            LogMessage(MessageSeverity.Warning, str);
        }

        public static void LogError(string str)
        {
            LogMessage(MessageSeverity.Error, str);
        }
    }
}

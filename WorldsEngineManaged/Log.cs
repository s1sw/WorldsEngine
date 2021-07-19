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

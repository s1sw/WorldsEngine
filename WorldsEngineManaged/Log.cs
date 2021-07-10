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
        private delegate void LogDelegate(int severity, string name);

        private static LogDelegate nativeLog;

        [StructLayout(LayoutKind.Sequential)]
        internal struct LoggingFuncPtrs
        {
            public IntPtr Log;
        }

        internal static void SetFunctionPointers(LoggingFuncPtrs funcPtrs)
        {
            nativeLog = Marshal.GetDelegateForFunctionPointer<LogDelegate>(funcPtrs.Log);
        }

        public static void LogMessage(MessageSeverity severity, string str)
        {
            int severityInt = (int)severity;
            nativeLog(severityInt, str);
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

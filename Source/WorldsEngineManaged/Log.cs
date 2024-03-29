using System;
using System.Runtime.InteropServices;
using System.Text;

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
        [DllImport(Engine.NativeModule)]
        private static extern void logging_log(int severity, string message);

        public static void LogWithSeverity(MessageSeverity severity, string message)
        {
            logging_log((int)severity, message);
        }        
        
        public static void MsgIndented(int indent, string str)
        {
            StringBuilder sb = new();
            for (int i = 0; i < indent; i++)
            {
                sb.Append("  ");
            }
            sb.Append(str);
            LogWithSeverity(MessageSeverity.Info, sb.ToString());
        }
        
        public static void Msg(string str) => LogWithSeverity(MessageSeverity.Info, str);
        public static void Warn(string str) => LogWithSeverity(MessageSeverity.Warning, str);
        public static void Error(string str) => LogWithSeverity(MessageSeverity.Error, str);
        public static void Verbose(string str) => LogWithSeverity(MessageSeverity.Verbose, str);
    }

    public class Logger
    {
        [DllImport(Engine.NativeModule)]
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

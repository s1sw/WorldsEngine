using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;
using System.Threading.Tasks;

namespace WorldsEngine
{
    public class Time
    {
        public static float DeltaTime { get; internal set; }

        public static double CurrentTime { get; internal set; }
        public static float InterpolationAlpha { get; internal set; }
    }

    public class EngineEventAwaiter : INotifyCompletion
    {
        internal Queue<Action> _queuedContinuations = new();

        public bool IsCompleted => false;

        public void OnCompleted(Action continuation)
        {
            _queuedContinuations.Enqueue(continuation);
        }

        public void GetResult() { }

        internal void Run()
        {
            while (_queuedContinuations.Count > 0)
            {
                Action a = _queuedContinuations.Dequeue();
                a.Invoke();
            }

            _queuedContinuations.Clear();
        }
    }

    public class AwaitableWrapper<T>
    {
        internal T Wrapped => _wrapped;
        private readonly T _wrapped;

        public AwaitableWrapper(T toWrap)
        {
            _wrapped = toWrap;
        }

        public T GetAwaiter() => _wrapped;
    }

    public static class Awaitables
    {
        public readonly static AwaitableWrapper<EngineEventAwaiter> NextFrame = new(new EngineEventAwaiter());
        public readonly static AwaitableWrapper<EngineEventAwaiter> NextSimulationTick = new(new EngineEventAwaiter());
    }
}

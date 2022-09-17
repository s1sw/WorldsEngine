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

    public class EngineEventAwaitable : INotifyCompletion
    {
        // Use two queues and swap so we don't end up in an infinite loop
        internal Queue<Action> _queuedContinuationsA = new();
        internal Queue<Action> _queuedContinuationsB = new();
        internal bool _useQueueB = false;

        internal Queue<Action> currentQueue => _useQueueB ? _queuedContinuationsB : _queuedContinuationsA;

        public bool IsCompleted => false;

        public void OnCompleted(Action continuation)
        {
            currentQueue.Enqueue(continuation);
        }

        public void GetResult() { }

        internal void Run()
        {
            Queue<Action> cq = currentQueue;
            _useQueueB = !_useQueueB;
            while (cq.Count > 0)
            {
                Action a = cq.Dequeue();
                a.Invoke();
            }

            cq.Clear();
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
        public readonly static AwaitableWrapper<EngineEventAwaitable> NextFrame = new(new EngineEventAwaitable());
        public readonly static AwaitableWrapper<EngineEventAwaitable> NextSimulationTick = new(new EngineEventAwaitable());
    }
}

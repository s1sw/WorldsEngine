using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Threading;
using System.Collections.Concurrent;

namespace WorldsEngine
{
    internal class EngineSynchronizationContext : SynchronizationContext
    {
        struct CallbackInvocation
        {
            public SendOrPostCallback Callback;
            public object? State;

            public CallbackInvocation(SendOrPostCallback callback, object? state)
            {
                Callback = callback;
                State = state;
            }
        }

        readonly ConcurrentQueue<CallbackInvocation> callbacks = new ConcurrentQueue<CallbackInvocation>();

        public override void Post(SendOrPostCallback d, object? state)
        {
            callbacks.Enqueue(new CallbackInvocation(d, state));
        }

        public void RunCallbacks()
        {
            while (!callbacks.IsEmpty)
            {
                if (!callbacks.TryDequeue(out CallbackInvocation callback)) continue;

                callback.Callback.Invoke(callback.State);
            }
        }
    }
}

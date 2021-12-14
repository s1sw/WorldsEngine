using System;
using System.Collections.Generic;

namespace Game
{
    public struct BackConnection
    {
        public Neuron Source;
        public float Weight;
    }

    /// <summary>
    /// A simple class representing a neuron. Not necessarily
    /// efficient, but it makes adding/removing neurons to a
    /// neural network much easier.
    /// </summary>
    public class Neuron
    {
        public float Bias = 0.0f;
        public List<BackConnection> BackConnections = new();

        public virtual float Value
        {
            get
            {
                float sum = 0.0f;

                // Iterate over all the connections
                foreach (var bc in BackConnections)
                {
                    sum += bc.Source.Value * bc.Weight;
                }

                // Apply the bias
                sum += Bias;

                return sum / (1 + MathF.Abs(sum));
            }
        }

        private float _cachedValue = 0.0f;
        private bool _cacheCanBeUsed = false;

        public void ConnectTo(Neuron other, float weight)
        {
            other.BackConnections.Add(new BackConnection()
            {
                Source = this,
                Weight = weight
            });
        }

        public void InvalidateCached()
        {
            _cacheCanBeUsed = false;
        }
    }

    public class ConstantValueNeuron : Neuron
    {
        public override float Value => OverrideValue;
        public float OverrideValue = 0.0f;
    }
}

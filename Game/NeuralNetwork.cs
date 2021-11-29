using System.Collections.Generic;

namespace Game;

/// <summary>
/// A simple container for a collection of OOP neurons.
/// </summary>
class NeuralNetwork
{
    public List<Neuron> InputNeurons = new();
    public List<Neuron> OutputNeurons = new();

    public void Update()
    {
        foreach (Neuron n in InputNeurons)
            n.InvalidateCached();

        foreach (Neuron n in OutputNeurons)
            n.InvalidateCached();
    }

    public void Train()
    {
    }
}

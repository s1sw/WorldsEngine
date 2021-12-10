using System.Collections.Generic;
using System;

namespace Game;

/// <summary>
/// A simple container for a collection of OOP neurons.
/// </summary>
class NeuralNetwork
{
    public List<Neuron> InputNeurons = new();
    public List<Neuron> OutputNeurons = new();
    public List<Neuron> RelayNeurons = new();

    private static Random _mutationRNG = new();
    private static Random _initialisationRNG = new();

    public NeuralNetwork(int numInput, int numRelay, int numOutput)
    {
        for (int i = 0; i < numInput; i++)
            InputNeurons.Add(new ConstantValueNeuron());

        for (int i = 0; i < numRelay; i++)
            RelayNeurons.Add(new Neuron());

        for (int i = 0; i < numOutput; i++)
            OutputNeurons.Add(new Neuron());

        // Now wire them all up!
        foreach (var outputNeuron in OutputNeurons)
        {
            foreach (var relayNeuron in RelayNeurons)
            {
                BackConnection connection = new()
                {
                    Source = relayNeuron,
                    Weight = _initialisationRNG.NextSingle()
                };

                outputNeuron.BackConnections.Add(connection);
            }
        }

        foreach (var relayNeuron in RelayNeurons)
        {
            foreach (var inputNeuron in InputNeurons)
            {
                BackConnection connection = new()
                {
                    Source = inputNeuron,
                    Weight = _initialisationRNG.NextSingle()
                };

                relayNeuron.BackConnections.Add(connection);
            }
        }
    }

    public void Update()
    {
        foreach (Neuron n in InputNeurons)
            n.InvalidateCached();

        foreach (Neuron n in OutputNeurons)
            n.InvalidateCached();
    }

    public void Mutate()
    {
        int numMutations = _mutationRNG.Next(5, 20);

        for (int i = 0; i < numMutations; i++)
        {
            // Input or output side?
            int side = _mutationRNG.Next(2);

            if (side == 0)
            {
                // Input!
                Neuron relayNeuron = RelayNeurons[_mutationRNG.Next(RelayNeurons.Count)];
                int connection = _mutationRNG.Next(relayNeuron.BackConnections.Count);
                float nudge = _mutationRNG.NextSingle() * 2f - 1f;

                var bc = relayNeuron.BackConnections[connection];
                bc.Weight = nudge;
                relayNeuron.BackConnections[connection] = bc;
            }
            else
            {
                // Output!
                Neuron outputNeuron = OutputNeurons[_mutationRNG.Next(OutputNeurons.Count)];
                int connection = _mutationRNG.Next(outputNeuron.BackConnections.Count);
                float nudge = _mutationRNG.NextSingle() * 2f - 1f;

                var bc = outputNeuron.BackConnections[connection];
                bc.Weight = nudge;
                outputNeuron.BackConnections[connection] = bc;
            }
        }
    }
}

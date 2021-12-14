using System;
using WorldsEngine;
using WorldsEngine.Math;
using WorldsEngine.Input;
using ImGuiNET;
using System.Text.Json.Serialization;
using System.Collections.Generic;

namespace Game;

[Component]
class NeuralDriverComponent
{
    [NonSerialized]
    [JsonIgnore]
    public NeuralNetwork NeuralNetwork = new(5, 5, 3);
}

struct Candidate
{
    public NeuralNetwork Network;
    public float Score;
}

public class NeuralDriverSystem : ISystem
{
    private double _currentGenerationTime = 0.0;
    private List<Candidate> _generation = new();
    private int _candidateIndex = 0;
    private int _generationNumber = 0;

    public void OnSceneStart()
    {
        for (int i = 0; i < 7; i++)
        {
            _generation.Add(new Candidate() { Network = new NeuralNetwork(5, 5, 3), Score = 0f });
        }

        var drivers = Registry.View<NeuralDriverComponent>();
        foreach (var driverEntity in drivers)
        {
            var driver = Registry.GetComponent<NeuralDriverComponent>(driverEntity);
            driver.NeuralNetwork = _generation[_candidateIndex].Network;
        }
    }

    public void OnUpdate()
    {
        var drivers = Registry.View<NeuralDriverComponent>();
        _currentGenerationTime += Time.DeltaTime;

        foreach (var driverEntity in drivers)
        {
            var dpa = Registry.GetComponent<DynamicPhysicsActor>(driverEntity);
            var pose = dpa.Pose;

            var driver = Registry.GetComponent<NeuralDriverComponent>(driverEntity);

            // Raycast from the front in different directions
            Vector3 forward = pose.Forward;
            Vector3 slightLeft = forward + new Vector3(0.25f, 0.0f, 0.0f);
            Vector3 slightRight = forward + new Vector3(-0.25f, 0.0f, 0.0f);

            Physics.Raycast(pose.Position + forward, forward, out RaycastHit hit);
            ((ConstantValueNeuron)driver.NeuralNetwork.InputNeurons[0]).OverrideValue = hit.Distance;
            Physics.Raycast(pose.Position + slightLeft, slightLeft, out hit);
            ((ConstantValueNeuron)driver.NeuralNetwork.InputNeurons[1]).OverrideValue = hit.Distance;
            Physics.Raycast(pose.Position + slightRight, slightRight, out hit);
            ((ConstantValueNeuron)driver.NeuralNetwork.InputNeurons[2]).OverrideValue = hit.Distance;

            ((ConstantValueNeuron)driver.NeuralNetwork.InputNeurons[3]).OverrideValue = pose.Position.DistanceTo(Vector3.Zero);
            ((ConstantValueNeuron)driver.NeuralNetwork.InputNeurons[4]).OverrideValue = pose.Forward.Dot(pose.Position.Normalized);

            driver.NeuralNetwork.Update();
            ImGui.Text($"Acceleration: {driver.NeuralNetwork.OutputNeurons[0].Value}");
            ImGui.Text($"Steer: {driver.NeuralNetwork.OutputNeurons[1].Value}");
            ImGui.Text($"Score: {MathF.Min(10000f, 1.0f / pose.Position.DistanceTo(Vector3.Zero))}");

            for (int i = 0; i < 5; i++)
            {
                ImGui.Text($"Input {i}: {driver.NeuralNetwork.InputNeurons[i].Value}");
            }

            // Then apply the inputs to the car
            var car = Registry.GetComponent<Car>(driverEntity);
            car.Accelerate = driver.NeuralNetwork.OutputNeurons[0].Value > 0.5f;
            car.Steer = Math.Clamp(driver.NeuralNetwork.OutputNeurons[1].Value, -1.0f, 1.0f);

            if (Keyboard.KeyHeld(KeyCode.M))
                driver.NeuralNetwork.Mutate();

            if (ImGui.Begin("Current Network")) {
                ImDrawListPtr drawList = ImGui.GetWindowDrawList();
                Vector2 wpos = ImGui.GetWindowPos();
                const float columnSpacing = 50.0f;

                for (int i = 0; i < driver.NeuralNetwork.InputNeurons.Count; i++) {
                    drawList.AddCircle(new Vector2(30.0f, 50f + (30f * i)) + wpos, 15f, uint.MaxValue);
                }

                for (int i = 0; i < driver.NeuralNetwork.RelayNeurons.Count; i++) {
                    Neuron n = driver.NeuralNetwork.RelayNeurons[i];
                    Vector2 nPos = new Vector2(30.0f + columnSpacing, 50f + (30f * i)) + wpos;
                    drawList.AddCircle(nPos, 15f, uint.MaxValue);

                    int nIdx = 0;
                    foreach (BackConnection bc in n.BackConnections)
                    {
                        Vector2 fromPos = new Vector2(30.0f, 50f + (30f * nIdx)) + wpos;
                        drawList.AddLine(fromPos, nPos, (uint)(uint.MaxValue * MathF.Abs(bc.Weight)));
                        nIdx++;
                    }
                }

                for (int i = 0; i < driver.NeuralNetwork.OutputNeurons.Count; i++) {
                    Neuron n = driver.NeuralNetwork.RelayNeurons[i];
                    Vector2 nPos = new Vector2(30.0f + columnSpacing * 2, 50f + (30f * i)) + wpos;

                    drawList.AddCircle(nPos, 15f, uint.MaxValue);

                    int nIdx = 0;
                    foreach (BackConnection bc in n.BackConnections)
                    {
                        Vector2 fromPos = new Vector2(30.0f + columnSpacing, 50f + (30f * nIdx)) + wpos;
                        drawList.AddLine(fromPos, nPos, (uint)(uint.MaxValue * MathF.Abs(bc.Weight)));
                        nIdx++;
                    }
                }
            }
            ImGui.End();
        }

        if (Keyboard.KeyPressed(KeyCode.R) || _currentGenerationTime > 7.0) {
            NextCandidate();
        }

        ImGui.Text($"Current generation time: {_currentGenerationTime}");
        ImGui.Text($"Current candidate: {_candidateIndex}");
        ImGui.Text($"Current generation: {_generationNumber}");
    }

    public void Reset()
    {
        var drivers = Registry.View<NeuralDriverComponent>();

        foreach (var driverEntity in drivers)
        {
            var dpa = Registry.GetComponent<DynamicPhysicsActor>(driverEntity);
            var pose = dpa.Pose;
            pose.Position = new Vector3(0.0f, 1.0f, 25.0f);
            dpa.Pose = pose;
            Registry.GetComponent<NeuralDriverComponent>(driverEntity).NeuralNetwork.Mutate();
        }
        _currentGenerationTime = 0.0;
    }

    public void NextCandidate()
    {
        if (_candidateIndex == _generation.Count - 1)
        {
            const int numTopCandidates = 7;
            // Descending sort
            _generation.Sort((a, b) => b.Score.CompareTo(a.Score));
            _generation.RemoveRange(numTopCandidates, _generation.Count - numTopCandidates);

            for (int i = 0; i < 3; i++)
            {
                for (int j = 0; j < numTopCandidates; j++)
                {
                    Candidate child = new()
                    {
                        Network = new NeuralNetwork(_generation[j].Network),
                        Score = 0
                    };
                    child.Network.Mutate();
                    _generation.Add(child);
                }
            }
            _candidateIndex = 0;
            _currentGenerationTime = 0.0;
            _generationNumber++;
            return;
        }

        var lastCandidate = _generation[_candidateIndex];
        _candidateIndex++;
        var drivers = Registry.View<NeuralDriverComponent>();

        foreach (var driverEntity in drivers)
        {
            var dpa = Registry.GetComponent<DynamicPhysicsActor>(driverEntity);
            var pose = dpa.Pose;
            lastCandidate.Score = pose.Position.DistanceTo(Vector3.Zero);
            pose.Position = new Vector3(0.0f, 1.0f, 25.0f);
            dpa.Pose = pose;
            dpa.AngularVelocity = Vector3.Zero;
            dpa.Velocity = Vector3.Zero;
            Registry.GetComponent<NeuralDriverComponent>(driverEntity).NeuralNetwork = _generation[_candidateIndex].Network;
        }

        _currentGenerationTime = 0.0;
    }
}

using System;
using WorldsEngine;
using WorldsEngine.Math;
using WorldsEngine.Input;
using ImGuiNET;

namespace Game;

[Component]
class NeuralDriverComponent
{
    [NonSerialized]
    public NeuralNetwork NeuralNetwork = new(5, 5, 3);
}

public class NeuralDriverSystem : ISystem
{
    public void OnUpdate()
    {
        var drivers = Registry.View<NeuralDriverComponent>();

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

            for (int i = 0; i < 5; i++)
            {
                ImGui.Text($"Input {i}: {driver.NeuralNetwork.InputNeurons[i].Value}");
            }

            // Then apply the inputs to the car
            var car = Registry.GetComponent<Car>(driverEntity);
            car.Accelerate = driver.NeuralNetwork.OutputNeurons[0].Value > 0.5f;
            car.Steer = Math.Clamp(driver.NeuralNetwork.OutputNeurons[1].Value, -1.0f, 1.0f);

            if (Keyboard.KeyHeld(KeyCode.M))
            {
                driver.NeuralNetwork.Mutate();
            }
        }
    }
}

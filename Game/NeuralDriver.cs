using WorldsEngine;
using WorldsEngine.Math;

namespace Game;

[Component]
class NeuralDriverComponent
{
    public NeuralNetwork NeuralNetwork;
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
            driver.NeuralNetwork.Update();

            // Raycast from the front in different directions
            Vector3 forward = pose.Forward;
            Vector3 slightLeft = forward + new Vector3(0.25f, 0.0f, 0.0f);
            Vector3 slightRight = forward + new Vector3(-0.25f, 0.0f, 0.0f);

            // Then apply the inputs to the car
            var car = Registry.GetComponent<Car>(driverEntity);
            car.Accelerate = driver.NeuralNetwork.OutputNeurons[0].Value > 0.5f;
            car.Steer = driver.NeuralNetwork.OutputNeurons[1].Value;
        }
    }
}

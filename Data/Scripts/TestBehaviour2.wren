import "worlds_engine/entity" for Entity
import "worlds_engine/math_types" for Vec3

var onSimulate = Fn.new {|entity, deltaTime|
    var transform = entity.getTransform()
    var dpa = entity.getDynamicPhysicsActor()
}

System.print("Loaded testbehaviour2")

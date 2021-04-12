import "worlds_engine/entity" for Entity
import "worlds_engine/math_types" for Vec3

var transform = null
var dpa = null

var onStart = Fn.new{|entity|
    transform = entity.getTransform()
    dpa = entity.getDynamicPhysicsActor()
}

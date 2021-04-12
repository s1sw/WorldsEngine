import "worlds_engine/entity" for Entity
import "worlds_engine/math_types" for Vec3
import "random" for Random

var currentTarget = Vec3.new(0, 0, 0)
var targetInitialized = false
var lastErr = Vec3.new(0, 0, 0)
var timer = 0.0
var rand = Random.new(1337)
var transform = null
var dpa = null

var onStart = Fn.new{|entity|
    rand = Random.new(entity.getId() + 1337)
    transform = entity.getTransform()
    dpa = entity.getDynamicPhysicsActor()
}

var onSimulate = Fn.new{|entity, deltaTime|
    if (!targetInitialized) {
        currentTarget = transform.getPosition() + Vec3.new(0, 0, 20)
        targetInitialized = true
        System.print("set target to %(currentTarget.x), %(currentTarget.y), %(currentTarget.z)")
    }

    timer = timer + deltaTime

    var currPos = transform.getPosition()

    if (timer > 15.0) {
        // find new target
        timer = 0.0
        currentTarget = currPos + Vec3.new(rand.float(-10, 10), 0.0, rand.float(-10, 10))
        System.print("changed target to %(currentTarget.x), %(currentTarget.y), %(currentTarget.z)")
    }

    var desiredVel = currentTarget - currPos
    desiredVel.normalize()
    desiredVel = desiredVel * 5.0

    var currVel = dpa.velocity
    var force = desiredVel - currVel

    dpa.addForce(force * 500.0)
}

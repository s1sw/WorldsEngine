import "worlds_engine/math_types"

foreign class Transform {
    foreign getPosition()
    foreign setPosition(pos)
}

foreign class DynamicPhysicsActor {
    foreign addForce(force)
    velocity { getVelocity() }
    foreign getVelocity()
}

foreign class Entity {
    construct fromId(id) {}
    foreign getTransform()
    foreign getDynamicPhysicsActor()
    foreign getId()
}


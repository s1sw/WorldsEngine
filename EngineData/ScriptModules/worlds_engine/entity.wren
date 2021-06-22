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

foreign class Light {
    foreign getEnabled()
    foreign setEnabled(enabled)

    enabled { getEnabled() }
    enabled=(val) { setEnabled(val) }
}

foreign class Entity {
    construct fromId(id) {}
    foreign getTransform()
    foreign getDynamicPhysicsActor()
    foreign getLight()
    foreign getId()
}


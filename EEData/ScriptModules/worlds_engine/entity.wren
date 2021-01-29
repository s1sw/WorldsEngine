import "worlds_engine/math_types"

foreign class Transform {
    foreign getPosition()
    foreign setPosition(pos)
}

foreign class Entity {
    construct fromId(id) {}
    foreign getTransform()
}


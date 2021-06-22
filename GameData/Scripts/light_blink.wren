import "worlds_engine/entity" for Entity

var light = null
var time = 0.0

var onStart = Fn.new{|entity|
    light = entity.getLight()
}

var onUpdate = Fn.new{|entity, deltaTime|
    time = time + deltaTime
    if (time > 1.0) {
        time = 0.0
        light.enabled = !light.enabled
    }
}

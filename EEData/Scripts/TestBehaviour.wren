import "worlds_engine/entity" for Entity

var onUpdate = Fn.new {|entity, deltaTime|
    var transform = entity.getTransform()
    var position = transform.getPosition()

    position.y = position.y + (deltaTime * 0.75)
    transform.setPosition(position)
}

System.print("Loaded testbehaviour")

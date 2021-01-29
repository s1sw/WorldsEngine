var onSimulate = Fn.new {|entity, deltaTime|
    var transform = entity.getTransform()
    var position = transform.getPosition()
    position.y = position.y + (deltaTime * 0.5)
    transform.setPosition(position)
}

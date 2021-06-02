import "worlds_engine/scene" for SceneManager
import "worlds_engine/entity" for Entity

var onGrab = Fn.new{|entity|
    System.print("grabbed!")
    SceneManager.createPrefab("Prefabs/dummy.json")
}

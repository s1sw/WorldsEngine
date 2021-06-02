import "worlds_engine/scene" for SceneManager
import "worlds_engine/console" for Console
import "worlds_engine/entity" for Entity

var onGrab = Fn.new{|entity|
    System.print("spawning camcorder!")
    Console.executeCommand("lg_spawnCamcorder")
}

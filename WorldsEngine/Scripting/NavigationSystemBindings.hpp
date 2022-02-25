#pragma once
#include <Navigation/Navigation.hpp>
#include "Export.hpp"

using namespace worlds;

extern "C" {
    EXPORT NavigationPath* navigation_findPath(glm::vec3 startPos, glm::vec3 endPos) {
        NavigationPath* path = new NavigationPath;
        NavigationSystem::findPath(startPos, endPos, *path);
        return path;
    }

    EXPORT void navigation_deletePath(NavigationPath* path) {
        delete path;
    }
}
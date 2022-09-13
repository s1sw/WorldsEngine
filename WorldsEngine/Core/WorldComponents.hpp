#pragma once
#include <Core/Transform.hpp>
#include <entt/entity/lw_fwd.hpp>
#include <stdint.h>
#include <vector>

#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/ext/vector_float4.hpp>
#include <slib/Bitset.hpp>
#include <slib/String.hpp>

namespace worlds
{
    typedef uint32_t AssetID;
    const int NUM_SUBMESH_MATS = 32;

    enum class StaticFlags : uint8_t
    {
        None = 0,
        Audio = 1,
        Rendering = 2,
        Navigation = 4
    };

    enum class UVOverride
    {
        None,
        XY,
        XZ,
        ZY,
        PickBest
    };

    struct WorldObject
    {
        WorldObject(AssetID material, AssetID mesh)
            : staticFlags(StaticFlags::None), mesh(mesh), texScaleOffset(1.0f, 1.0f, 0.0f, 0.0f),
              uvOverride(UVOverride::None)
        {
            for (int i = 0; i < NUM_SUBMESH_MATS; i++)
            {
                materials[i] = material;
                presentMaterials[i] = false;
                drawSubmeshes[i] = true;
            }
            presentMaterials[0] = true;
        }

        StaticFlags staticFlags;
        AssetID materials[NUM_SUBMESH_MATS];
        slib::Bitset<NUM_SUBMESH_MATS> presentMaterials;
        AssetID mesh;
        glm::vec4 texScaleOffset;
        UVOverride uvOverride;
        bool castShadows = true;
        slib::Bitset<NUM_SUBMESH_MATS> drawSubmeshes;
    };

    class Pose
    {
      public:
        std::vector<glm::mat4> boneTransforms;
    };

    struct SkinnedWorldObject : public WorldObject
    {
        SkinnedWorldObject(AssetID material, AssetID mesh);
        void resetPose();
        Pose currentPose;
        uint32_t skinnedVertexOffset;
    };

    struct UseWireframe
    {
    };

    enum class LightType
    {
        Point,
        Spot,
        Directional,
        Sphere,
        Tube
    };

    struct WorldLight
    {
        WorldLight()
        {
        }
        WorldLight(LightType type) : type(type)
        {
        }

        // Whether the light should be actually rendered
        bool enabled = true;
        LightType type = LightType::Point;
        glm::vec3 color = glm::vec3{1.0f};
        float intensity = 1.0f;

        // Angle of the spotlight cutoff in radians
        float spotCutoff = glm::pi<float>() * 0.5f;
        float spotOuterCutoff = glm::pi<float>() * 0.6f;

        // Physical dimensions of a tube light
        float tubeLength = 0.25f;
        float tubeRadius = 0.1f;

        // Shadowing settings
        bool enableShadows = false;
        uint32_t shadowmapIdx = ~0u;
        float shadowNear = 0.05f;
        float shadowFar = 100.0f;

        float maxDistance = 1.0f;
        // Index of the light in the light buffer
        uint32_t lightIdx = 0u;
    };

    struct WorldCubemap
    {
        glm::vec3 extent{0.0f};
        bool cubeParallax = false;
        int priority = 0;
        int resolution = 128;
        glm::vec3 captureOffset{0.0f};
        AssetID cubemapId = ~0u;
        uint32_t renderIdx = 0u;
        bool isLoaded = false;
    };

    struct ProxyAOComponent
    {
        glm::vec3 bounds;
    };

    struct SphereAOProxy
    {
        float radius;
    };

    struct EditorLabel
    {
        slib::String label;
    };

    struct DontSerialize
    {
    };
    struct HideFromEditor
    {
    };
    struct EditorGlow
    {
    };
    struct KeepOnSceneLoad
    {
    };

    struct ChildComponent
    {
        ChildComponent();
        Transform offset;

        entt::entity parent;

        entt::entity nextChild;
        entt::entity prevChild;
    };

    struct ParentComponent
    {
        entt::entity firstChild;
    };
}

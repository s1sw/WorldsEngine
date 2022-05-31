#pragma once
#include <physx/PxRigidActor.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include "../Core/Transform.hpp"

namespace worlds {
    typedef uint32_t AssetID;

    enum class PhysicsShapeType : uint32_t {
        Sphere,
        Box,
        Capsule,
        Mesh,
        ConvexMesh,
        Count
    };

    inline PhysicsShapeType& operator++(PhysicsShapeType& type) {
        PhysicsShapeType newType = (PhysicsShapeType)(((int)type) + 1);

        type = newType;

        return type;
    }

    struct PhysicsShape {
        PhysicsShape()
            : pos(0.0f)
            , rot(1.0f, 0.0f, 0.0f, 0.0f)
            , material(nullptr) {
            mesh.mesh = ~0u;
        }
        PhysicsShapeType type;
        union {
            struct {
                glm::vec3 halfExtents;
            } box;
            struct {
                float radius;
            } sphere;
            struct {
                float height;
                float radius;
            } capsule;
            struct {
                AssetID mesh;
            } mesh;
            struct {
                AssetID mesh;
            } convexMesh;
        };
        glm::vec3 pos;
        glm::quat rot;
        physx::PxMaterial* material;

        static PhysicsShape sphereShape(float radius, physx::PxMaterial* mat = nullptr) {
            PhysicsShape shape;
            shape.type = PhysicsShapeType::Sphere;
            shape.sphere.radius = radius;
            shape.material = mat;
            return shape;
        }

        static PhysicsShape boxShape(glm::vec3 halfExtents, physx::PxMaterial* mat = nullptr) {
            PhysicsShape shape;
            shape.type = PhysicsShapeType::Box;
            shape.box.halfExtents = halfExtents;
            shape.material = mat;
            return shape;
        }

        static PhysicsShape capsuleShape(float radius, float height, physx::PxMaterial* mat = nullptr) {
            PhysicsShape shape;
            shape.type = PhysicsShapeType::Capsule;
            shape.capsule.radius = radius;
            shape.capsule.height = height;
            shape.material = mat;
            return shape;
        }
    };

    struct PhysicsActor {
        PhysicsActor(physx::PxRigidActor* actor) : actor(actor) {}
        physx::PxRigidActor* actor;
        std::vector<PhysicsShape> physicsShapes;
        bool scaleShapes = true;
        uint32_t layer = 1;
        bool useContactMod = false;
        float contactOffset = 0.0005f;
    };

    enum class ForceMode : int32_t {
        Force,
        Impulse,
        VelocityChange,
        Acceleration
    };

    enum class DPALockFlags : uint32_t {
        LinearX = (1 << 0),
        LinearY = (1 << 1),
        LinearZ = (1 << 2),
        AngularX = (1 << 3),
        AngularY = (1 << 4),
        AngularZ = (1 << 5)
    };

    inline DPALockFlags operator&(DPALockFlags a, DPALockFlags b) {
        return (DPALockFlags)((uint32_t)a & (uint32_t)b);
    }

    struct RigidBody {
        RigidBody(physx::PxRigidActor* actor) : actor((physx::PxRigidDynamic*)actor), mass(1.0f) {}
        physx::PxRigidDynamic* actor;
        float mass;
        bool enableGravity = true;
        bool enableCCD = false;
        std::vector<PhysicsShape> physicsShapes;
        bool scaleShapes = true;
        uint32_t layer = 1;
        bool useContactMod = false;
        float contactOffset = 0.01f;

        glm::vec3 linearVelocity() const;
        glm::vec3 angularVelocity() const;

        void setLinearVelocity(glm::vec3 vel);
        void setAngularVelocity(glm::vec3 vel);

        void addForce(glm::vec3 force, ForceMode forceMode = ForceMode::Force);
        void addTorque(glm::vec3 torque, ForceMode forceMode = ForceMode::Force);

        void addForceAtPosition(glm::vec3 force, glm::vec3 position, ForceMode forceMode = ForceMode::Force);

        float maxAngularVelocity() const;
        void setMaxAngularVelocity(float vel);

        float maxLinearVelocity() const;
        void setMaxLinearVelocity(float vel);

        DPALockFlags lockFlags() const;
        void setLockFlags(DPALockFlags flags);

        void setEnabled(bool enabled);
        bool enabled() const;

        Transform pose() const;
        void setPose(const Transform& t);
    };
}

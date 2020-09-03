#pragma once
#include <physx/PxRigidActor.h>
#include <glm/glm.hpp>
#include <vector>

namespace worlds {
	typedef uint32_t AssetID;

	enum class PhysicsShapeType {
		Sphere,
		Box,
		Capsule,
		Mesh,
		Count
	};

	inline PhysicsShapeType& operator++(PhysicsShapeType& type) {
		PhysicsShapeType newType = (PhysicsShapeType)(((int)type) + 1);

		type = newType;

		return newType;
	}

	struct PhysicsShape {
		PhysicsShape() : material(nullptr) {}
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
		};
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
	};

	struct PhysicsActor {
		PhysicsActor(physx::PxRigidActor* actor) : actor(actor) {}
		physx::PxRigidActor* actor;
		std::vector<PhysicsShape> physicsShapes;
	};

	struct DynamicPhysicsActor {
		DynamicPhysicsActor(physx::PxRigidActor* actor) : actor(actor), mass(1.0f) {}
		physx::PxRigidActor* actor;
		float mass;
		std::vector<PhysicsShape> physicsShapes;
	};
}
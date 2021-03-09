#pragma once
#include <entt/entity/registry.hpp>
#include <PxRigidDynamic.h>
#include "../WrenVM.hpp"
#include "../../Physics/PhysicsActor.hpp"
#include "../ScriptUtil.hpp"
#include "../../Physics/Physics.hpp"

namespace worlds {
    class DynamicPhyiscsActorBinding : public ScriptBindClass {
    private:
        static void addForce(WrenVM* vm) {
            WrenVMData* dat = (WrenVMData*)wrenGetUserData(vm);

            uint32_t entId = *(uint32_t*)wrenGetSlotForeign(vm, 0);

            auto& dpa = dat->reg.get<DynamicPhysicsActor>((entt::entity)entId);
            glm::vec3 forceVec = *(glm::vec3*)wrenGetSlotForeign(vm, 1);
            ((physx::PxRigidDynamic*)dpa.actor)->addForce(glm2px(forceVec));
        }

        static void getVelocity(WrenVM* vm) {
            WrenVMData* dat = (WrenVMData*)wrenGetUserData(vm);

            uint32_t entId = *(uint32_t*)wrenGetSlotForeign(vm, 0);

            auto& dpa = dat->reg.get<DynamicPhysicsActor>((entt::entity)entId);
            auto vel = px2glm(((physx::PxRigidDynamic*)dpa.actor)->getLinearVelocity());
            makeVec3(vm, vel.x, vel.y, vel.z);
        }
    public:
        std::string getName() override {
            return "DynamicPhysicsActor";
        }

        WrenForeignMethodFn getFn(bool isStatic, const char* sig) override {
            if (!isStatic) {
                if (strcmp(sig, "addForce(_)") == 0) {
                    return addForce;
                } else if (strcmp(sig, "getVelocity()") == 0) {
                    return getVelocity;
                }
            }

            return nullptr;
        }
    };

    DynamicPhyiscsActorBinding dpab;
}

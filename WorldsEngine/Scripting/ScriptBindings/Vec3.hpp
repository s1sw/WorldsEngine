#pragma once
#include "../WrenVM.hpp"
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>
#include <entt/core/hashed_string.hpp>

namespace worlds {
    class Vec3Binding : public ScriptBindClass {
    private:
        static void getComp(WrenVM* vm) {
            wrenEnsureSlots(vm, 1);
            glm::vec3* vPtr = (glm::vec3*)wrenGetSlotForeign(vm, 0);

            wrenSetSlotDouble(vm, 0, (*vPtr)[(int)wrenGetSlotDouble(vm, 1)]);
        }

        static void setComp(WrenVM* vm) {
            glm::vec3* vPtr = (glm::vec3*)wrenGetSlotForeign(vm, 0);

            (*vPtr)[(int)wrenGetSlotDouble(vm, 1)] = wrenGetSlotDouble(vm, 2);
        }

        static void setAll(WrenVM* vm) {
            glm::vec3* vPtr = (glm::vec3*)wrenGetSlotForeign(vm, 0);

            for (int i = 0; i < 3; i++) {
                (*vPtr)[i] = wrenGetSlotDouble(vm, i + 1);
            }
        }

        static void distanceTo(WrenVM* vm) {
            glm::vec3* vPtr = (glm::vec3*)wrenGetSlotForeign(vm, 0);
            glm::vec3* vPtr2 = (glm::vec3*)wrenGetSlotForeign(vm, 1);

            float dist = glm::length((*vPtr2 - *vPtr));
            wrenSetSlotDouble(vm, 0, dist);
        }

        static void normalize(WrenVM* vm) {
            glm::vec3* vPtr = (glm::vec3*)wrenGetSlotForeign(vm, 0);

            (*vPtr) = glm::normalize(*vPtr);
        }

        static void subtract(WrenVM* vm) {
            glm::vec3* vPtr = (glm::vec3*)wrenGetSlotForeign(vm, 0);
            glm::vec3* vPtr2 = (glm::vec3*)wrenGetSlotForeign(vm, 1);

            wrenGetVariable(vm, "worlds_engine/math_types", "Vec3", 0);
            glm::vec3* result = (glm::vec3*)wrenSetSlotNewForeign(vm, 0, 0, sizeof(glm::vec3));
            
            (*result) = (*vPtr) - (*vPtr2);
        }

        static void add(WrenVM* vm) {
            glm::vec3* vPtr = (glm::vec3*)wrenGetSlotForeign(vm, 0);
            glm::vec3* vPtr2 = (glm::vec3*)wrenGetSlotForeign(vm, 1);

            wrenGetVariable(vm, "worlds_engine/math_types", "Vec3", 0);
            glm::vec3* result = (glm::vec3*)wrenSetSlotNewForeign(vm, 0, 0, sizeof(glm::vec3));
            
            (*result) = (*vPtr) + (*vPtr2);
        }

        static void scale(WrenVM* vm) {
            glm::vec3* vPtr = (glm::vec3*)wrenGetSlotForeign(vm, 0);
            float scaleVal = wrenGetSlotDouble(vm, 1);

            wrenGetVariable(vm, "worlds_engine/math_types", "Vec3", 0);
            glm::vec3* result = (glm::vec3*)wrenSetSlotNewForeign(vm, 0, 0, sizeof(glm::vec3));
            (*result) = (*vPtr) * scaleVal;
        }

        static void divide(WrenVM* vm) {
            glm::vec3* vPtr = (glm::vec3*)wrenGetSlotForeign(vm, 0);
            float divVal = wrenGetSlotDouble(vm, 1);

            wrenGetVariable(vm, "worlds_engine/math_types", "Vec3", 0);
            glm::vec3* result = (glm::vec3*)wrenSetSlotNewForeign(vm, 0, 0, sizeof(glm::vec3));
            (*result) = (*vPtr) / divVal;
        }

        static void clampMagnitude(WrenVM* vm) {
            glm::vec3* vPtr = (glm::vec3*)wrenGetSlotForeign(vm, 0);
            float maxMag = wrenGetSlotDouble(vm, 1);

            float mag = glm::length(*vPtr);
            mag = glm::clamp(mag, 0.0f, maxMag);

            wrenGetVariable(vm, "worlds_engine/math_types", "Vec3", 0);
            glm::vec3* result = (glm::vec3*)wrenSetSlotNewForeign(vm, 0, 0, sizeof(glm::vec3));

            *result = glm::normalize(*vPtr) * mag;
        }
    public:
        std::string getName() override {
            return "Vec3";
        }

        WrenForeignMethodFn getFn(bool isStatic, const char* sig) override {
            if (!isStatic) {
                if (strcmp(sig, "getComp(_)") == 0) {
                    return getComp;
                } else if (strcmp(sig, "setComp(_,_)") == 0) {
                    return setComp;
                } else if (strcmp(sig, "setAll(_,_,_)") == 0) {
                    return setAll;
                } else if (strcmp(sig, "distanceTo(_)") == 0) {
                    return distanceTo;
                } else if (strcmp(sig, "normalize()") == 0) {
                    return normalize;
                } else if (strcmp(sig, "-(_)") == 0) {
                    return subtract;
                } else if (strcmp(sig, "*(_)") == 0) {
                    return scale;
                } else if (strcmp(sig, "+(_)") == 0) {
                    return add;
                } else if (strcmp(sig, "/(_)") == 0) {
                    return divide;
                } else if (strcmp(sig, "clampMagnitude(_)") == 0) {
                    return clampMagnitude;
                }
            }

            return nullptr;
        }

        ~Vec3Binding() override {}
    };
    Vec3Binding v3binding;
}

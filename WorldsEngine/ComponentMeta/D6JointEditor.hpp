#include "ComponentEditorUtil.hpp"
#include <Core/Fatal.hpp>
#include <Core/Log.hpp>
#include <Core/NameComponent.hpp>
#include <Editor/GuiUtil.hpp>
#include <ImGui/imgui.h>
#include <Libs/IconsFontAwesome5.h>
#include <Physics/D6Joint.hpp>
#include <Physics/Physics.hpp>
#include <Physics/PhysicsActor.hpp>
#include <entt/entity/registry.hpp>
#include <foundation/PxTransform.h>
#include <nlohmann/json.hpp>
#include <physx/PxPhysicsAPI.h>
#include <physx/extensions/PxD6Joint.h>
#include <physx/extensions/PxJoint.h>

using json = nlohmann::json;

namespace physx
{
    inline void to_json(json& j, const PxTransform& t)
    {
        j = {{"position", {t.p.x, t.p.y, t.p.z}}, {"rotation", {t.q.x, t.q.y, t.q.z, t.q.w}}};
    }

    inline void from_json(const json& j, PxTransform& t)
    {
        const auto& pos = j["position"];
        const auto& rot = j["rotation"];

        t.p = PxVec3(pos[0], pos[1], pos[2]);
        t.q = PxQuat(rot[0], rot[1], rot[2], rot[3]);
    }

    inline void to_json(json& j, const PxJointLinearLimit& l)
    {
        j = {{"value", l.value},         {"restitution", l.restitution}, {"bounceThreshold", l.bounceThreshold},
             {"stiffness", l.stiffness}, {"damping", l.damping}};
    }

    inline void from_json(const json& j, PxJointLinearLimit& l)
    {
        l.value = j["value"];
        l.restitution = j["restitution"];
        l.bounceThreshold = j["bounceThreshold"];
        l.stiffness = j["stiffness"];
        l.damping = j["damping"];
    }
}

namespace worlds
{
    const char* motionNames[3] = {"Locked", "Limited", "Free"};

    const char* motionAxisLabels[physx::PxD6Axis::eCOUNT] = {"X Motion",     "Y Motion",       "Z Motion",
                                                             "Twist Motion", "Swing 1 Motion", "Swing 2 Motion"};

    bool motionDropdown(const char* label, physx::PxD6Motion::Enum& val)
    {
        bool ret = false;
        if (ImGui::BeginCombo(label, motionNames[(int)val]))
        {
            for (int iType = 0; iType < 3; iType++)
            {
                auto type = (physx::PxD6Motion::Enum)iType;
                bool isSelected = val == type;
                if (ImGui::Selectable(motionNames[iType], &isSelected))
                {
                    val = type;
                    ret = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        return ret;
    }

    float readFloat(PHYSFS_File* file)
    {
        float f;
        PHYSFS_readBytes(file, &f, sizeof(f));
        return f;
    }

    class D6JointEditor : public BasicComponentUtil<D6Joint>
    {
      public:
        int getSortID() override
        {
            return 2;
        }
        const char* getName() override
        {
            return "D6 Joint";
        }

        void create(entt::entity ent, entt::registry& reg) override
        {
            reg.emplace<D6Joint>(ent);
        }

#ifdef BUILD_EDITOR
        void edit(entt::entity ent, entt::registry& reg, Editor* ed) override
        {
            auto& j = reg.get<D6Joint>(ent);

            auto* dpa = reg.try_get<RigidBody>(ent);
            auto* pxj = j.pxJoint;

            if (ImGui::CollapsingHeader(ICON_FA_ATOM u8" D6 Joint"))
            {
                if (ImGui::Button("Remove##D6"))
                {
                    reg.remove<D6Joint>(ent);
                    return;
                }

                entt::entity target = j.getTarget();

                if (reg.valid(target))
                {
                    NameComponent* nc = reg.try_get<NameComponent>(target);

                    if (nc)
                    {
                        ImGui::Text("Connected to %s", nc->name.c_str());
                    }
                    else
                    {
                        ImGui::Text("Connected to %u", (uint32_t)target);
                    }

                    Transform t1 = px2glm(j.pxJoint->getLocalPose(physx::PxJointActorIndex::eACTOR0));
                    Transform t2 = px2glm(j.pxJoint->getLocalPose(physx::PxJointActorIndex::eACTOR1));
                    // TODO: this ignores rotations. can we do better??
                    // idk, and my brain is fried so i leave this to future you
                    Transform goal = reg.get<Transform>(target).transformByInverse(t1).transformBy(t2);

                    drawSphere(goal.position, goal.rotation, 0.1f);

                    if (ImGui::Button("Move to target"))
                    {
                        Transform& thisTransform = reg.get<Transform>(ent);
                        thisTransform.position = goal.position;
                        thisTransform.rotation = goal.rotation;
                    }
                }
                else
                {
                    ImGui::Text("Not connected");
                }

                ImGui::SameLine();

                if (ImGui::Button("Choose"))
                {
                    ImGui::OpenPopup("Connect D6 joint to...");
                }

                selectSceneEntity("Connect D6 joint to...", reg, [&](entt::entity e) { j.setTarget(e, reg); });

                ImGui::SameLine();

                static bool changingTarget = false;

                if (!changingTarget)
                {
                    if (ImGui::Button("Pick"))
                    {
                        changingTarget = true;
                    }
                }

                if (changingTarget)
                {
                    if (ed->entityEyedropper(target))
                    {
                        changingTarget = false;
                        j.setTarget(target, reg);
                    }
                }

                if (dpa)
                {
                    dpa->actor->is<physx::PxRigidDynamic>()->wakeUp();
                }
                else
                {
                    if (reg.valid(j.getAttached()))
                    {
                        NameComponent* nc = reg.try_get<NameComponent>(j.getAttached());

                        if (nc)
                        {
                            ImGui::Text("Attached to %s", nc->name.c_str());
                        }
                        else
                        {
                            ImGui::Text("Attached to entity %u", (uint32_t)j.getAttached());
                        }
                    }
                    else
                    {
                        ImGui::Text("Not attached");
                    }

                    ImGui::SameLine();

                    static bool changingAttached = false;

                    if (!changingAttached)
                    {
                        if (ImGui::Button("Change##Attached"))
                        {
                            changingAttached = true;
                        }
                    }

                    if (changingAttached)
                    {
                        entt::entity attached = j.getAttached();
                        if (ed->entityEyedropper(attached))
                        {
                            changingAttached = false;
                            j.setAttached(attached, reg);
                        }
                    }
                }

                for (int axisInt = physx::PxD6Axis::eX; axisInt < physx::PxD6Axis::eCOUNT; axisInt++)
                {
                    auto axis = (physx::PxD6Axis::Enum)axisInt;
                    auto motion = j.pxJoint->getMotion(axis);
                    if (motionDropdown(motionAxisLabels[axis], motion))
                    {
                        j.pxJoint->setMotion(axis, motion);
                    }
                }

                auto t0 = j.pxJoint->getLocalPose(physx::PxJointActorIndex::eACTOR0);
                auto t1 = j.pxJoint->getLocalPose(physx::PxJointActorIndex::eACTOR1);

                if (ImGui::DragFloat3("Local Offset", &t0.p.x))
                {
                    j.pxJoint->setLocalPose(physx::PxJointActorIndex::eACTOR0, t0);
                }

                glm::vec3 localEulerAngles = glm::degrees(glm::eulerAngles(px2glm(t0.q)));

                if (ImGui::DragFloat3("Local Rotation", glm::value_ptr(localEulerAngles)))
                {
                    t0.q = glm2px(glm::quat(glm::radians(localEulerAngles)));
                    j.pxJoint->setLocalPose(physx::PxJointActorIndex::eACTOR0, t0);
                }

                if (ImGui::DragFloat3("Connected Offset", &t1.p.x))
                {
                    j.pxJoint->setLocalPose(physx::PxJointActorIndex::eACTOR1, t1);
                }

                if (!reg.valid(j.getTarget()))
                {
                    if (ImGui::Button("Set Connected Offset"))
                    {
                        auto& t = reg.get<Transform>(ent);
                        auto p = glm2px(t);
                        j.pxJoint->setLocalPose(physx::PxJointActorIndex::eACTOR1, p);
                    }
                }

                if (ImGui::TreeNode("Linear Limits"))
                {
                    for (int axisInt = physx::PxD6Axis::eX; axisInt < physx::PxD6Axis::eTWIST; axisInt++)
                    {
                        auto axis = (physx::PxD6Axis::Enum)axisInt;
                        if (ImGui::TreeNode(motionAxisLabels[axis]))
                        {
                            auto lim = j.pxJoint->getLinearLimit(axis);

                            ImGui::DragFloat("Lower", &lim.lower, 1.0f, -(PX_MAX_F32 / 3.0f), lim.upper);
                            ImGui::DragFloat("Upper", &lim.upper, 1.0f, lim.lower, (PX_MAX_F32 / 3.0f));
                            ImGui::DragFloat("Stiffness", &lim.stiffness);
                            tooltipHover("If greater than zero, the limit is soft, i.e. a spring pulls the joint back "
                                         "to the limit");
                            ImGui::DragFloat("Damping", &lim.damping);
                            ImGui::DragFloat("Bounce Threshold", &lim.bounceThreshold);
                            tooltipHover("The minimum velocity for which the limit will bounce.");
                            ImGui::DragFloat("Restitution", &lim.restitution);
                            tooltipHover("Controls the amount of bounce when the joint hits a limit.");

                            if (!lim.isValid())
                            {
                                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Invalid limit settings!");
                            }

                            j.pxJoint->setLinearLimit(axis, lim);
                            ImGui::TreePop();
                        }
                    }
                    ImGui::TreePop();
                }

                if (ImGui::TreeNode("Angular Limits"))
                {
                    if (ImGui::TreeNode("Swing Limit"))
                    {
                        auto lim = j.pxJoint->getSwingLimit();

                        lim.yAngle = glm::degrees(lim.yAngle);
                        lim.zAngle = glm::degrees(lim.zAngle);

                        ImGui::DragFloat("Y Angle", &lim.yAngle, 15.0f, 0.0f, 180.0f);
                        ImGui::DragFloat("Z Angle", &lim.zAngle, 15.0f, 0.0f, 180.0f);

                        lim.yAngle = glm::radians(lim.yAngle);
                        lim.zAngle = glm::radians(lim.zAngle);

                        ImGui::DragFloat("Stiffness", &lim.stiffness);
                        tooltipHover(
                            "If greater than zero, the limit is soft, i.e. a spring pulls the joint back to the limit");
                        ImGui::DragFloat("Damping", &lim.damping);
                        ImGui::DragFloat("Bounce Threshold", &lim.bounceThreshold);
                        tooltipHover("The minimum velocity for which the limit will bounce.");
                        ImGui::DragFloat("Restitution", &lim.restitution);
                        tooltipHover("Controls the amount of bounce when the joint hits a limit.");

                        j.pxJoint->setSwingLimit(lim);

                        ImGui::TreePop();
                    }

                    if (ImGui::TreeNode("Twist Limit"))
                    {
                        auto lim = j.pxJoint->getTwistLimit();

                        lim.lower = glm::degrees(lim.lower);
                        lim.upper = glm::degrees(lim.upper);

                        ImGui::DragFloat("Lower", &lim.lower, 1.0f, -360.0f, lim.upper);
                        ImGui::DragFloat("Upper", &lim.upper, 1.0f, lim.lower, 360.0f);

                        lim.lower = glm::radians(lim.lower);
                        lim.upper = glm::radians(lim.upper);

                        ImGui::DragFloat("Stiffness", &lim.stiffness);
                        tooltipHover(
                            "If greater than zero, the limit is soft, i.e. a spring pulls the joint back to the limit");
                        ImGui::DragFloat("Damping", &lim.damping);
                        ImGui::DragFloat("Bounce Threshold", &lim.bounceThreshold);
                        tooltipHover("The minimum velocity for which the limit will bounce.");
                        ImGui::DragFloat("Restitution", &lim.restitution);
                        tooltipHover("Controls the amount of bounce when the joint hits a limit.");

                        j.pxJoint->setTwistLimit(lim);
                        ImGui::TreePop();
                    }

                    ImGui::TreePop();
                }

                float localMassScale = 1.0f / j.pxJoint->getInvMassScale0();
                if (ImGui::DragFloat("Local Mass Scale", &localMassScale))
                {
                    j.pxJoint->setInvMassScale0(1.0f / localMassScale);
                }

                float localInertiaScale = 1.0f / j.pxJoint->getInvInertiaScale0();
                if (ImGui::DragFloat("Local Inertia Scale", &localInertiaScale))
                {
                    j.pxJoint->setInvInertiaScale0(1.0f / localInertiaScale);
                }

                float connectedMassScale = 1.0f / j.pxJoint->getInvMassScale1();
                if (ImGui::DragFloat("Connected Mass Scale", &connectedMassScale))
                {
                    j.pxJoint->setInvMassScale1(1.0f / connectedMassScale);
                }

                float connectedInertiaScale = 1.0f / j.pxJoint->getInvInertiaScale1();
                if (ImGui::DragFloat("Connected Inertia Scale", &connectedInertiaScale))
                {
                    j.pxJoint->setInvInertiaScale1(1.0f / connectedInertiaScale);
                }

                float breakForce, breakTorque;
                pxj->getBreakForce(breakForce, breakTorque);

                if (ImGui::DragFloat("Break Torque", &breakTorque))
                {
                    pxj->setBreakForce(breakForce, breakTorque);
                }

                if (ImGui::DragFloat("Break Force", &breakForce))
                {
                    pxj->setBreakForce(breakForce, breakTorque);
                }
            }
        }
#endif

        void clone(entt::entity from, entt::entity to, entt::registry& reg) override
        {
            assert(reg.has<RigidBody>(to));
            auto& newD6 = reg.emplace<D6Joint>(to);
            auto& oldD6 = reg.get<D6Joint>(from);

            auto* newJ = newD6.pxJoint;
            auto* oldJ = oldD6.pxJoint;

            if (reg.valid(oldD6.getTarget()))
                newD6.setTarget(oldD6.getTarget(), reg);

            for (int axis = physx::PxD6Axis::eX; axis < physx::PxD6Axis::eCOUNT; axis++)
            {
                auto actualAxis = (physx::PxD6Axis::Enum)axis;
                newD6.pxJoint->setMotion(actualAxis, oldD6.pxJoint->getMotion(actualAxis));
            }

            newD6.pxJoint->setLocalPose(physx::PxJointActorIndex::eACTOR0,
                                        oldD6.pxJoint->getLocalPose(physx::PxJointActorIndex::eACTOR0));

            newD6.pxJoint->setLocalPose(physx::PxJointActorIndex::eACTOR1,
                                        oldD6.pxJoint->getLocalPose(physx::PxJointActorIndex::eACTOR1));

            for (int axis = physx::PxD6Axis::eX; axis < physx::PxD6Axis::eTWIST; axis++)
            {
                auto actualAxis = (physx::PxD6Axis::Enum)axis;
                newJ->setLinearLimit(actualAxis, oldJ->getLinearLimit(actualAxis));
            }

            newJ->setTwistLimit(oldJ->getTwistLimit());
            newJ->setSwingLimit(oldJ->getSwingLimit());
        }

        void toJson(entt::entity ent, entt::registry& reg, json& j) override
        {
            auto& d6 = reg.get<D6Joint>(ent);
            auto* px = d6.pxJoint;

            json axisMotions;
            for (int axisInt = physx::PxD6Axis::eX; axisInt < physx::PxD6Axis::eCOUNT; axisInt++)
            {
                auto axis = (physx::PxD6Axis::Enum)axisInt;
                auto motion = (unsigned char)px->getMotion(axis);
                axisMotions[axis] = motion;
            }
            j["axisMotions"] = axisMotions;

            auto p0 = px->getLocalPose(physx::PxJointActorIndex::eACTOR0);
            auto p1 = px->getLocalPose(physx::PxJointActorIndex::eACTOR1);

            j["thisPose"] = p0;
            j["connectedPose"] = p1;
            if (reg.valid(d6.getTarget()))
                j["target"] = d6.getTarget();
            else
                logErr("invalid d6 target");

            if (reg.valid(d6.getAttached()))
                j["attached"] = d6.getAttached();

            json linearLimits;
            for (int axisInt = physx::PxD6Axis::eX; axisInt < physx::PxD6Axis::eTWIST; axisInt++)
            {
                auto axis = (physx::PxD6Axis::Enum)axisInt;
                auto l = px->getLinearLimit(axis);

                linearLimits[axis] = {{"lower", l.lower},
                                      {"upper", l.upper},
                                      {"restitution", l.restitution},
                                      {"bounceThreshold", l.bounceThreshold},
                                      {"stiffness", l.stiffness},
                                      {"damping", l.damping}};
            }
            j["linearLimits"] = linearLimits;

            float invMS0 = px->getInvMassScale0();
            float invMS1 = px->getInvMassScale1();
            float invIS0 = px->getInvInertiaScale0();
            float invIS1 = px->getInvInertiaScale1();

            j["inverseMassScale0"] = invMS0;
            j["inverseMassScale1"] = invMS1;
            j["inverseInertiaScale0"] = invIS0;
            j["inverseInertiaScale1"] = invIS1;

            float breakTorque, breakForce;
            px->getBreakForce(breakForce, breakTorque);

            j["breakForce"] = breakForce;
            j["breakTorque"] = breakTorque;

            auto swingLimit = px->getSwingLimit();
            json swingLimitJ = {{"yAngle", swingLimit.yAngle},
                                {"zAngle", swingLimit.zAngle},
                                {"restitution", swingLimit.restitution},
                                {"bounceThreshold", swingLimit.bounceThreshold},
                                {"stiffness", swingLimit.stiffness},
                                {"damping", swingLimit.damping}};

            j["swingLimit"] = swingLimitJ;

            auto twistLimit = px->getTwistLimit();
            json twistLimitJ = {{"lower", twistLimit.lower},
                                {"upper", twistLimit.upper},
                                {"restitution", twistLimit.restitution},
                                {"bounceThreshold", twistLimit.bounceThreshold},
                                {"stiffness", twistLimit.stiffness},
                                {"damping", twistLimit.damping}};

            j["twistLimit"] = twistLimitJ;
        }

        void fromJson(entt::entity ent, entt::registry& reg, EntityIDMap& idMap, const json& j) override
        {
            auto& d6 = reg.emplace<D6Joint>(ent);
            auto* px = d6.pxJoint;

            for (int axisInt = physx::PxD6Axis::eX; axisInt < physx::PxD6Axis::eCOUNT; axisInt++)
            {
                auto axis = (physx::PxD6Axis::Enum)axisInt;
                px->setMotion(axis, j["axisMotions"][axis]);
            }

            px->setLocalPose(physx::PxJointActorIndex::eACTOR0, j["thisPose"]);

            px->setLocalPose(physx::PxJointActorIndex::eACTOR1, j["connectedPose"]);

            for (int axisInt = physx::PxD6Axis::eX; axisInt < physx::PxD6Axis::eTWIST; axisInt++)
            {
                auto axis = (physx::PxD6Axis::Enum)axisInt;

                auto lJson = j["linearLimits"][axis];
                physx::PxJointLinearLimitPair l{physx::PxTolerancesScale{}};
                l.lower = lJson["lower"];
                l.upper = lJson["upper"];
                l.restitution = lJson["restitution"];
                l.bounceThreshold = lJson["bounceThreshold"];
                l.stiffness = lJson["stiffness"];
                l.damping = lJson["damping"];

                px->setLinearLimit(axis, l);
            }

            if (j.contains("swingLimit"))
            {
                auto swingLJ = j["swingLimit"];
                physx::PxJointLimitCone swingLimit{swingLJ.value("yAngle", glm::half_pi<float>()),
                                                   swingLJ.value("zAngle", glm::half_pi<float>()),
                                                   swingLJ.value("contactDistance", 0.1f)};
                swingLimit.stiffness = swingLJ.value("stiffness", 0.0f);
                swingLimit.damping = swingLJ.value("damping", 0.0f);
                swingLimit.restitution = swingLJ.value("restitution", 0.0f);
                swingLimit.bounceThreshold = swingLJ.value("bounceThreshold", 0.5f);
                px->setSwingLimit(swingLimit);
            }

            if (j.contains("twistLimit"))
            {
                auto twistLJ = j["twistLimit"];

                physx::PxJointAngularLimitPair l{twistLJ["lower"], twistLJ["upper"]};
                l.restitution = twistLJ["restitution"];
                l.bounceThreshold = twistLJ["bounceThreshold"];
                l.stiffness = twistLJ["stiffness"];
                l.damping = twistLJ["damping"];

                px->setTwistLimit(l);
            }

            px->setInvMassScale0(j["inverseMassScale0"]);
            px->setInvMassScale1(j["inverseMassScale1"]);

            px->setInvInertiaScale0(j["inverseInertiaScale0"]);
            px->setInvInertiaScale1(j["inverseInertiaScale1"]);

            px->setBreakForce(j["breakForce"], j["breakTorque"]);

            if (j.contains("target"))
            {
                entt::entity target = idMap[j["target"]];
                if (!reg.valid(target))
                    logErr("Invalid target while deserializing D6!");
                else
                    d6.setTarget(target, reg);
            }

            if (j.contains("attached"))
            {
                entt::entity attached = idMap[j["attached"]];
                if (!reg.valid(attached))
                    logErr("Invalid attached entity while deserializing D6 joint");
                else
                    d6.setAttached(attached, reg);
            }
        }
    };

    D6JointEditor d6Ed;
}

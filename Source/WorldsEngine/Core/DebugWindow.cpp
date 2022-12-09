#include <Core/EngineInternal.hpp>
#include <Core/Console.hpp>
#include <Util/CircularBuffer.hpp>
#include <Libs/IconsFontAwesome5.h>
#include <Render/Render.hpp>
#include <Physics/Physics.hpp>

namespace worlds
{
    ConVar showDebugInfo{"showDebugInfo", "0", "Shows the debug info window"};

    void drawDebugInfoWindow(const EngineInterfaces& engineInterfaces, DebugTimeInfo timeInfo)
    {
        Renderer* renderer = engineInterfaces.renderer;
        if (showDebugInfo.getInt())
        {
            bool open = true;
            if (ImGui::Begin("Info", &open))
            {
                static CircularBuffer<float, 128> historicalFrametimes;
                static CircularBuffer<float, 128> historicalUpdateTimes;
                static CircularBuffer<float, 128> historicalPhysicsTimes;

                static CircularBuffer<float, 128> historicalRenderTimes;
                static CircularBuffer<float, 128> historicalGpuTimes;

                historicalFrametimes.add(timeInfo.deltaTime * 1000.0f);
                historicalUpdateTimes.add(timeInfo.updateTime * 1000.0f - timeInfo.simTime);

                if (timeInfo.didSimRun)
                    historicalPhysicsTimes.add((float)timeInfo.simTime);

                const auto& dbgStats = renderer->getDebugStats();
                historicalRenderTimes.add((float)timeInfo.lastTickRendererTime);
                historicalGpuTimes.add(renderer->getLastGPUTime() / 1000.0f / 1000.0f);

                if (ImGui::CollapsingHeader(ICON_FA_CLOCK u8" Performance"))
                {
                    ImGui::PlotLines(
                            "Historical Frametimes",
                            historicalFrametimes.values,
                            historicalFrametimes.size(),
                            historicalFrametimes.idx,
                            nullptr,
                            0.0f,
                            FLT_MAX,
                            ImVec2(0.0f, 125.0f)
                    );

                    ImGui::PlotLines(
                            "Historical UpdateTimes",
                            historicalUpdateTimes.values,
                            historicalUpdateTimes.size(),
                            historicalUpdateTimes.idx,
                            nullptr,
                            0.0f,
                            FLT_MAX,
                            ImVec2(0.0f, 125.0f)
                    );

                    ImGui::PlotLines(
                            "Historical PhysicsTimes",
                            historicalPhysicsTimes.values,
                            historicalPhysicsTimes.size(),
                            historicalPhysicsTimes.idx,
                            nullptr,
                            0.0f,
                            FLT_MAX,
                            ImVec2(0.0f, 125.0f)
                    );

                    ImGui::Text("Frametime: %.3fms", timeInfo.deltaTime * 1000.0);
                    ImGui::Text("Update time: %.3fms", timeInfo.updateTime * 1000.0);

                    double highestUpdateTime = 0.0f;
                    for (int i = 0; i < 128; i++)
                    {
                        highestUpdateTime = glm::max((double)historicalUpdateTimes.values[i], highestUpdateTime);
                    }
                    ImGui::Text("Peak update time: %.3fms", highestUpdateTime);

                    ImGui::Text("Physics time: %.3fms", timeInfo.simTime);
                    ImGui::Text(
                            "Update time without physics: %.3fms", (timeInfo.updateTime * 1000.0) - timeInfo.simTime
                    );
                    ImGui::Text("Framerate: %.1ffps", 1.0 / timeInfo.deltaTime);
                }

                if (ImGui::CollapsingHeader(ICON_FA_BARS u8" Misc"))
                {
                    Camera& cam = *engineInterfaces.mainCamera;
                    ImGui::Text("Frame: %i", timeInfo.frameCounter);
                    ImGui::Text("Cam pos: %.3f, %.3f, %.3f", cam.position.x, cam.position.y, cam.position.z);
                }

                if (ImGui::CollapsingHeader(ICON_FA_PENCIL_ALT u8" Render Stats"))
                {
                    ImGui::PlotLines(
                            "Render times",
                            historicalRenderTimes.values,
                            historicalRenderTimes.size(),
                            historicalRenderTimes.idx,
                            nullptr,
                            0.0f,
                            FLT_MAX,
                            ImVec2(0.0f, 125.0f)
                    );

                    ImGui::PlotLines(
                            "GPU times",
                            historicalGpuTimes.values,
                            historicalGpuTimes.size(),
                            historicalGpuTimes.idx,
                            nullptr,
                            0.0f,
                            FLT_MAX,
                            ImVec2(0.0f, 125.0f)
                    );

                    ImGui::Text("Draw calls: %i", dbgStats.numDrawCalls);
                    ImGui::Text("%i pipeline switches", dbgStats.numPipelineSwitches);
                    ImGui::Text("Frustum culled objects: %i", dbgStats.numCulledObjs);
                    ImGui::Text("Active RTT passes: %i/%i", dbgStats.numActiveRTTPasses, dbgStats.numRTTPasses);
                    ImGui::Text(
                            "Time spent in renderer: %.3fms", timeInfo.lastTickRendererTime
                    );
                    ImGui::Text("- Writing command buffer: %.3fms", dbgStats.cmdBufWriteTime);
                    static float avgGpuTime = 0.0f;
                    float lastGpuTime = renderer->getLastGPUTime() / 1000.0f / 1000.0f;
                    float averageAlpha = 0.05f;
                    avgGpuTime = (averageAlpha * lastGpuTime) + (1.0 - averageAlpha) * avgGpuTime;
                    ImGui::Text("GPU render time: %.3fms", lastGpuTime);
                    ImGui::Text("Average GPU render time: %.3fms", avgGpuTime);
                    ImGui::Text("GPU light cull time: %.3fms", dbgStats.lightCullTime / 1000.0f / 1000.0f);
                    ImGui::Text("V-Sync status: %s", renderer->getVsync() ? "On" : "Off");
                    ImGui::Text("Triangles: %u", dbgStats.numTriangles);
                    ImGui::Text("Lights in view: %i", dbgStats.numLightsInView);
                    ImGui::Text("%i textures loaded", dbgStats.numTexturesLoaded);
                    ImGui::Text("%i materials loaded", dbgStats.numMaterialsLoaded);
                }

                if (ImGui::CollapsingHeader(ICON_FA_MEMORY u8" Memory Stats"))
                {
#ifdef CHECK_NEW_DELETE
                    ImGui::Text("CPU:");
                    ImGui::Text("Live allocations: %lu", liveAllocations);
                    ImGui::Text("Allocated bytes: %lu", allocatedMem);
                    ImGui::Separator();
#endif
                    ImGui::Text("GPU:");
                }

                if (ImGui::CollapsingHeader(ICON_FA_SHAPES u8" Physics Stats"))
                {
                    PhysicsSystem* physicsSystem = engineInterfaces.physics;
                    physx::PxScene* scene = physicsSystem->scene();
                    uint32_t nDynamic = scene->getNbActors(physx::PxActorTypeFlag::eRIGID_DYNAMIC);
                    uint32_t nStatic = scene->getNbActors(physx::PxActorTypeFlag::eRIGID_STATIC);
                    uint32_t nTotal = nDynamic + nStatic;

                    ImGui::Text("%u dynamic actors, %u static actors (%u total)", nDynamic, nStatic, nTotal);
                    uint32_t nConstraints = scene->getNbConstraints();
                    ImGui::Text("%u constraints", nConstraints);
                    uint32_t nShapes = physicsSystem->physics()->getNbShapes();
                    ImGui::Text("%u shapes", nShapes);
                }
            }
            ImGui::End();

            if (!open)
            {
                showDebugInfo.setValue("0");
            }
        }
    }
}
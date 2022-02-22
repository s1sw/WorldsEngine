#include "RenderInternal.hpp"
#include <Util/TimingUtil.hpp>
#include <Core/JobSystem.hpp>
#include <tracy/TracyC.h>
#include <openvr.h>

namespace worlds {
    VKPresentSubmitManager::VKPresentSubmitManager(SDL_Window* window, VkSurfaceKHR surface, VulkanHandles* handles, Queues* queues, RenderDebugStats* dbgStats)
        : sc(nullptr)
        , dbgStats(dbgStats)
        , handles(handles)
        , queues(queues)
        , window(window)
        , surface(surface) {
        VkCommandBufferAllocateInfo cbai{};
        cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cbai.commandPool = handles->commandPool;
        cbai.commandBufferCount = maxFramesInFlight;
        cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

        cmdBufs.resize(maxFramesInFlight);

        VKCHECK(vkAllocateCommandBuffers(handles->device, &cbai, cmdBufs.data()));

        cmdBufFences.resize(maxFramesInFlight);
        cmdBufferSemaphores.resize(maxFramesInFlight);
        imgAvailable.resize(maxFramesInFlight);

        for (int i = 0; i < maxFramesInFlight; i++) {
            std::string cmdBufName = "Command Buffer ";
            cmdBufName += std::to_string(i);

            vku::setObjectName(handles->device, (uint64_t)cmdBufs[i], VK_OBJECT_TYPE_COMMAND_BUFFER, cmdBufName.c_str());
            VkFenceCreateInfo fci{};
            fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            VKCHECK(vkCreateFence(handles->device, &fci, nullptr, &cmdBufFences[i]));

            VkSemaphoreCreateInfo sci{};
            sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

            VKCHECK(vkCreateSemaphore(handles->device, &sci, nullptr, &cmdBufferSemaphores[i]));
            VKCHECK(vkCreateSemaphore(handles->device, &sci, nullptr, &imgAvailable[i]));
        }
    }

    VKPresentSubmitManager::~VKPresentSubmitManager() {
        for (int i = 0; i < maxFramesInFlight; i++) {
            vkDestroyFence(handles->device, cmdBufFences[i], nullptr);
            vkDestroySemaphore(handles->device, cmdBufferSemaphores[i], nullptr);
            vkDestroySemaphore(handles->device, imgAvailable[i], nullptr);
        }

        vkFreeCommandBuffers(handles->device, handles->commandPool, cmdBufs.size(), cmdBufs.data());

        delete sc;
    }

#ifdef TRACY_ENABLE
    void VKPresentSubmitManager::setupTracyContexts(std::vector<TracyVkCtx>& tracyContexts) {
        for (auto& cmdBuf : cmdBufs) {
            tracyContexts.push_back(tracy::CreateVkContext(handles->physicalDevice, handles->device, queues->graphics, cmdBuf, vkGetPhysicalDeviceCalibrateableTimeDomainsEXT, vkGetCalibratedTimestampsEXT));
        }
    }
#endif

    void VKPresentSubmitManager::recreateSwapchain(bool useVsync, uint32_t& width, uint32_t& height) {
        bool fullscreen = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) == SDL_WINDOW_FULLSCREEN;
        VkPresentModeKHR presentMode = useVsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;
        std::lock_guard<std::mutex> lg{swapchainMutex};

        sc = new Swapchain(handles->physicalDevice, handles->device, surface, *queues, fullscreen, sc ? sc->getSwapchain() : VK_NULL_HANDLE, presentMode);
        sc->getSize(&width, &height);

        vku::executeImmediately(handles->device, handles->commandPool, queues->graphics, [this](VkCommandBuffer cb) {
            for (VkImage img : sc->images)
                vku::transitionLayout(cb, img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VkAccessFlags{}, VK_ACCESS_MEMORY_READ_BIT);
        });

        imgFences.clear();
        imgFences.resize(sc->images.size());

        for (auto& s : imgAvailable) {
            vkDestroySemaphore(handles->device, s, nullptr);
        }

        imgAvailable.clear();
        imgAvailable.resize(cmdBufs.size());

        for (size_t i = 0; i < cmdBufs.size(); i++) {
            VkSemaphoreCreateInfo sci{};
            sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

            VKCHECK(vkCreateSemaphore(handles->device, &sci, nullptr, &imgAvailable[i]));
        }
    }

    int VKPresentSubmitManager::acquireFrame(VkCommandBuffer& acquiredCmdBuf, int& imageIndex) {
        currentFrame++;
        if (currentFrame >= maxFramesInFlight) currentFrame = 0;

        {
            ZoneScopedN("Waiting for command buffer fence");
            PerfTimer pt;
            VkResult result = vkWaitForFences(handles->device, 1, &cmdBufFences[currentFrame], VK_TRUE, UINT64_MAX);
            if (result != VK_SUCCESS) {
                fatalErr((std::string("Failed to wait on fences: ") + vku::toString(result)).c_str());
            }
            dbgStats->cmdBufFenceWaitTime = pt.stopGetMs();
        }

        if (vkResetFences(handles->device, 1, &cmdBufFences[currentFrame]) != VK_SUCCESS) {
            fatalErr("Failed to reset fences");
        }

        imageIndex = 0;
        {
            ZoneScopedN("Acquiring Image");
            PerfTimer pt;
            std::lock_guard<std::mutex> lg{swapchainMutex};
            sc->acquireImage(handles->device, imgAvailable[currentFrame], (uint32_t*)&imageIndex);

            dbgStats->imgAcquisitionTime = pt.stopGetMs();
        }

        if (imgFences[imageIndex] && imgFences[imageIndex] != cmdBufFences[currentFrame]) {
            ZoneScopedN("Waiting for image fence");
            PerfTimer pt;
            VkResult result = vkWaitForFences(handles->device, 1, &imgFences[imageIndex], true, UINT64_MAX);
            if (result != VK_SUCCESS) {
                std::string errStr = "Failed to wait on image fence: ";
                errStr += vku::toString(result);
                fatalErr(errStr.c_str());
            }
            dbgStats->imgFenceWaitTime = pt.stopGetMs();
        } else {
            dbgStats->imgFenceWaitTime = 0.0;
        }

        imgFences[imageIndex] = cmdBufFences[currentFrame];

        acquiredCmdBuf = cmdBufs[currentFrame];
        currentImage = imageIndex;
        return currentFrame;
    }

    void VKPresentSubmitManager::submit() {
        ZoneScoped;
        VkCommandBuffer cmdBuf = cmdBufs[currentFrame];
        VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = &imgAvailable[currentFrame];

        VkPipelineStageFlags waitStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        submit.pWaitDstStageMask = &waitStages;

        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmdBuf;
        submit.pSignalSemaphores = &cmdBufferSemaphores[currentFrame];
        submit.signalSemaphoreCount = 1;

        VkResult submitResult = vkQueueSubmit(queues->graphics, 1, &submit, cmdBufFences[currentFrame]);

        if (submitResult != VK_SUCCESS) {
            std::string errStr = vku::toString(submitResult);
            fatalErr(("Failed to submit queue (error: " + errStr + ")").c_str());
        }
    }

    void VKPresentSubmitManager::present() {
        VkQueue presentQueue = queues->present;
        // On Nvidia, vkQueuePresentKHR blocks, seemingly until the next vsync.
        // We therefore run it in a job so we can free up the render thread.
        if (handles->vendor == VKVendor::Nvidia) {
            ZoneScopedN("Submitting Present Job");
            static JobList* lastJobList = nullptr;

            if (lastJobList)
                lastJobList->wait();

            // currentFrame and currentImage are instance variables
            // so we have to copy them into some temp ones to capture
            // them by value in the lambda (god this is dumb)
            uint32_t fIdx = currentFrame;
            uint32_t imgIdx = currentImage;
            JobList& jlist = g_jobSys->getFreeJobList();
            jlist.begin();
            jlist.addJob(Job { [this, imgIdx, fIdx, presentQueue]() {
                VkPresentInfoKHR presentInfo{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
                VkSwapchainKHR cSwapchain = sc->getSwapchain();
                presentInfo.pSwapchains = &cSwapchain;
                presentInfo.swapchainCount = 1;
                presentInfo.pImageIndices = &imgIdx;

                presentInfo.pWaitSemaphores = &cmdBufferSemaphores[fIdx];
                presentInfo.waitSemaphoreCount = 1;

                std::lock_guard<std::mutex> lg{swapchainMutex};
                TracyCZoneN(__presentZone, "vkQueuePresentKHR", true);
                VkResult presentResult = vkQueuePresentKHR(presentQueue, &presentInfo);
                TracyCZoneEnd(__presentZone);

                if (presentResult == VK_ERROR_OUT_OF_DATE_KHR) {
                    logVrb(WELogCategoryRender, "swapchain out of date after present");
                } else if (presentResult == VK_SUBOPTIMAL_KHR) {
                    logVrb(WELogCategoryRender, "swapchain after present suboptimal");
                } else if (presentResult != VK_SUCCESS) {
                    fatalErr("Failed to present");
                }
            }});
            jlist.end();
            g_jobSys->signalJobListAvailable();
            lastJobList = &jlist;
        } else {
            ZoneScopedN("Presenting");
            VkPresentInfoKHR presentInfo{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
            VkSwapchainKHR cSwapchain = sc->getSwapchain();
            presentInfo.pSwapchains = &cSwapchain;
            presentInfo.swapchainCount = 1;

            // if this gets big enough for the distinction between unsigned and signed
            // to matter we have other problems
            presentInfo.pImageIndices = (uint32_t*)&currentImage;

            presentInfo.pWaitSemaphores = &cmdBufferSemaphores[currentFrame];
            presentInfo.waitSemaphoreCount = 1;

            std::lock_guard<std::mutex> lg{swapchainMutex};
            TracyCZoneN(__presentZone, "vkQueuePresentKHR", true);
            VkResult presentResult = vkQueuePresentKHR(presentQueue, &presentInfo);
            TracyCZoneEnd(__presentZone);

            if (presentResult == VK_ERROR_OUT_OF_DATE_KHR) {
                logVrb(WELogCategoryRender, "swapchain out of date after present");
            } else if (presentResult == VK_SUBOPTIMAL_KHR) {
                logVrb(WELogCategoryRender, "swapchain after present suboptimal");
            } else if (presentResult != VK_SUCCESS) {
                fatalErr("Failed to present");
            }
        }
    }

    void VKPresentSubmitManager::presentNothing() {
        VkSemaphore imgSemaphore = imgAvailable[currentFrame];
        VkSemaphore cmdBufSemaphore = cmdBufferSemaphores[currentFrame];

        VkPresentInfoKHR presentInfo{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        VkSwapchainKHR cSwapchain = sc->getSwapchain();
        presentInfo.pSwapchains = &cSwapchain;
        presentInfo.swapchainCount = 1;
        presentInfo.pImageIndices = (uint32_t*)&currentImage;

        presentInfo.pWaitSemaphores = &cmdBufSemaphore;
        presentInfo.waitSemaphoreCount = 1;

        auto& cmdBuf = cmdBufs[currentFrame];
        vku::beginCommandBuffer(cmdBuf);
        VKCHECK(vkEndCommandBuffer(cmdBuf));

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &imgSemaphore;

        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &cmdBufferSemaphores[currentFrame];
        submitInfo.pCommandBuffers = &cmdBuf;
        submitInfo.commandBufferCount = 1;

        VkQueue presentQueue = queues->present;

        vkQueueSubmit(presentQueue, 1, &submitInfo, VK_NULL_HANDLE);

        std::lock_guard<std::mutex> lg{swapchainMutex};
        auto presentResult = vkQueuePresentKHR(presentQueue, &presentInfo);
        if (presentResult != VK_SUCCESS && presentResult != VK_SUBOPTIMAL_KHR)
            fatalErr("Present failed!");
    }

    int VKPresentSubmitManager::numFramesInFlight() {
        return maxFramesInFlight;
    }

    Swapchain& VKPresentSubmitManager::currentSwapchain() {
        return *sc;
    }
}

#include "../R2ImGui.hpp"
#include "ImGuiFS.h"
#include "ImGuiVS.h"
#include <R2/BindlessTextureManager.hpp>
#include <R2/VKBuffer.hpp>
#include <R2/VKCommandBuffer.hpp>
#include <R2/VKCore.hpp>
#include <R2/VKDescriptorSet.hpp>
#include <R2/VKPipeline.hpp>
#include <R2/VKRenderPass.hpp>
#include <R2/VKSampler.hpp>
#include <R2/VKTexture.hpp>
#include <stdint.h>

using namespace R2;

struct ImGuiPCs
{
    float scaleX;
    float scaleY;
    float translateX;
    float translateY;
    uint32_t textureID;
};

struct R2ImplState
{
    VK::Buffer* vertexBuffers[2] { nullptr };
    size_t vertexBufferCapacities[2] { 0 };
    VK::Buffer* indexBuffers[2] { nullptr };
    size_t indexBufferCapacities[2] { 0 };
    VK::Core* core = nullptr;
    VK::PipelineLayout* pipelineLayout = nullptr;
    VK::DescriptorSetLayout* dsl = nullptr;
    VK::Pipeline* pipeline = nullptr;
    VK::Texture* fontTexture = nullptr;
    uint32_t fontTextureID = ~0u;
    VK::Sampler* sampler = nullptr;
    BindlessTextureManager* textureManager = nullptr;
};

R2ImplState* getImplState()
{
    return static_cast<R2ImplState*>(ImGui::GetIO().BackendRendererUserData);
}

void ImGui_ImplR2_CreateFontTexture();

bool ImGui_ImplR2_Init(VK::Core* renderer, BindlessTextureManager* btm)
{
    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererName = "R2";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

    auto* s = new R2ImplState{};
    s->core = renderer;
    s->textureManager = btm;
    io.BackendRendererUserData = s;

    VK::PipelineLayoutBuilder plb{renderer->GetHandles()};
    plb.PushConstants(VK::ShaderStage::Vertex | VK::ShaderStage::Fragment, 0, sizeof(ImGuiPCs))
        .DescriptorSet(&btm->GetTextureDescriptorSetLayout());
    s->pipelineLayout = plb.Build();

    VK::VertexBinding vertexBinding{};
    vertexBinding.Binding = 0;
    vertexBinding.Attributes.emplace_back(
        0, VK::TextureFormat::R32G32_SFLOAT, (uint32_t)offsetof(ImDrawVert, pos));
    vertexBinding.Attributes.emplace_back(
        1, VK::TextureFormat::R32G32_SFLOAT, (uint32_t)offsetof(ImDrawVert, uv));
    vertexBinding.Attributes.emplace_back(
        2, VK::TextureFormat::R8G8B8A8_UNORM, (uint32_t)offsetof(ImDrawVert, col));
    vertexBinding.Size = sizeof(ImDrawVert);

    VK::ShaderModule fsm{renderer->GetHandles(), ImGuiFS, sizeof(ImGuiFS)};
    VK::ShaderModule vsm{renderer->GetHandles(), ImGuiVS, sizeof(ImGuiVS)};

    VK::PipelineBuilder pb{renderer};
    s->pipeline = pb.PrimitiveTopology(VK::Topology::TriangleList)
                    .CullMode(VK::CullMode::None)
                    .Layout(s->pipelineLayout)
                    .ColorAttachmentFormat(VK::TextureFormat::B8G8R8A8_SRGB)
                    .AddVertexBinding(vertexBinding)
                    .AddShader(VK::ShaderStage::Fragment, fsm)
                    .AddShader(VK::ShaderStage::Vertex, vsm)
                    .AlphaBlend(true)
                    .Build();

    ImGui_ImplR2_CreateFontTexture();

    return true;
}

void ImGui_ImplR2_Shutdown()
{
    R2ImplState* s = getImplState();
    s->core->DestroyBuffer(s->indexBuffers[0]);
    s->core->DestroyBuffer(s->vertexBuffers[0]);
    s->core->DestroyBuffer(s->indexBuffers[1]);
    s->core->DestroyBuffer(s->vertexBuffers[1]);
    delete s->dsl;
    delete s->pipeline;
    s->core->DestroyTexture(s->fontTexture);
    delete s->pipelineLayout;
    delete s->sampler;
}

void ImGui_ImplR2_NewFrame()
{
}

void ImGui_ImplR2_CreateFontTexture()
{
    R2ImplState* s = getImplState();

    ImGuiIO& io = ImGui::GetIO();
    uint8_t* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    auto tci = VK::TextureCreateInfo::Texture2D(VK::TextureFormat::R8G8B8A8_SRGB, width, height);
    s->fontTexture = s->core->CreateTexture(tci);

    s->core->QueueTextureUpload(s->fontTexture, pixels, 4 * width * height);

    ImGui::MemFree(pixels);

    s->fontTextureID = s->textureManager->AllocateTextureHandle(s->fontTexture);
    io.Fonts->SetTexID((ImTextureID)(uint64_t)s->fontTextureID);

    VK::SamplerBuilder sb{s->core};
    s->sampler = sb.AddressMode(VK::SamplerAddressMode::Repeat)
                     .MagFilter(VK::Filter::Linear)
                     .MinFilter(VK::Filter::Linear)
                     .MipmapMode(VK::SamplerMipmapMode::Linear)
                     .Build();
}

void resizeBuffersIfNecessary(ImDrawData* drawData, R2ImplState* s)
{
    uint32_t frameIdx = s->core->GetFrameIndex();
    R2::VK::Buffer*& vertexBuffer = s->vertexBuffers[frameIdx];
    R2::VK::Buffer*& indexBuffer = s->indexBuffers[frameIdx];

    if (!vertexBuffer || s->vertexBufferCapacities[frameIdx] < drawData->TotalVtxCount)
    {
        if (vertexBuffer)
        {
            s->core->DestroyBuffer(vertexBuffer);
        }

        s->vertexBufferCapacities[frameIdx] = drawData->TotalVtxCount + 5000;

        VK::BufferCreateInfo bci{};
        bci.Mappable = true;
        bci.Size = s->vertexBufferCapacities[frameIdx] * sizeof(ImDrawVert);
        bci.Usage = VK::BufferUsage::Vertex;

        s->vertexBuffers[frameIdx] = s->core->CreateBuffer(bci);
        vertexBuffer->SetDebugName("ImGui Vertex Buffer");
    }

    if (!indexBuffer || s->indexBufferCapacities[frameIdx] < drawData->TotalIdxCount)
    {
        if (indexBuffer)
        {
            s->core->DestroyBuffer(indexBuffer);
        }

        s->indexBufferCapacities[frameIdx] = drawData->TotalIdxCount + 10000;

        VK::BufferCreateInfo bci{};
        bci.Mappable = true;
        bci.Size = s->indexBufferCapacities[frameIdx] * sizeof(ImDrawIdx);
        bci.Usage = VK::BufferUsage::Index;

        s->indexBuffers[frameIdx] = s->core->CreateBuffer(bci);
        indexBuffer->SetDebugName("ImGui Index Buffer");
    }
}

void ImGui_ImplR2_RenderDrawData(ImDrawData* drawData, VK::CommandBuffer& cb)
{
    R2ImplState* s = getImplState();
    uint32_t frameIdx = s->core->GetFrameIndex();
    R2::VK::Buffer*& vertexBuffer = s->vertexBuffers[frameIdx];
    R2::VK::Buffer*& indexBuffer = s->indexBuffers[frameIdx];

    resizeBuffersIfNecessary(drawData, s);

    auto* verts = (ImDrawVert*)vertexBuffer->Map();
    auto* indices = (ImDrawIdx*)indexBuffer->Map();

    for (int i = 0; i < drawData->CmdListsCount; i++)
    {
        const ImDrawList* cmdList = drawData->CmdLists[i];

        memcpy(verts, cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(indices, cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx));

        verts += cmdList->VtxBuffer.Size;
        indices += cmdList->IdxBuffer.Size;
    }

    vertexBuffer->Unmap();
    indexBuffer->Unmap();

    VK::Viewport vp = VK::Viewport::Simple(drawData->DisplaySize.x, drawData->DisplaySize.y);
    cb.SetViewport(vp);

    ImGuiPCs pcs{};
    pcs.scaleX = 2.0f / drawData->DisplaySize.x;
    pcs.scaleY = 2.0f / drawData->DisplaySize.y;

    pcs.translateX = -1.0f - drawData->DisplayPos.x * pcs.scaleX;
    pcs.translateY = -1.0f - drawData->DisplayPos.y * pcs.scaleY;

    cb.PushConstants(pcs, VK::ShaderStage::Vertex | VK::ShaderStage::Fragment, s->pipelineLayout);

    int globalIndexOffset = 0;
    int globalVertexOffset = 0;
    ImVec2 clipOffset = drawData->DisplayPos;
    R2::VK::DescriptorSet& ds = s->textureManager->GetTextureDescriptorSet();
    cb.BindGraphicsDescriptorSet(s->pipelineLayout, &ds, 0);
    cb.BindIndexBuffer(indexBuffer, 0, VK::IndexType::Uint16);
    cb.BindVertexBuffer(0, vertexBuffer, 0);
    cb.BindPipeline(s->pipeline);

    for (int n = 0; n < drawData->CmdListsCount; n++)
    {
        const ImDrawList* cmdList = drawData->CmdLists[n];
        for (int cmdIndex = 0; cmdIndex < cmdList->CmdBuffer.Size; cmdIndex++)
        {
            const ImDrawCmd* pcmd = &cmdList->CmdBuffer[cmdIndex];
            if (pcmd->UserCallback != nullptr)
            {
                if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                {
                    cb.BindGraphicsDescriptorSet(s->pipelineLayout, &ds, 0);
                    cb.BindIndexBuffer(indexBuffer, 0, VK::IndexType::Uint16);
                    cb.BindVertexBuffer(0, vertexBuffer, 0);
                    cb.BindPipeline(s->pipeline);
                }
                else
                    pcmd->UserCallback(cmdList, pcmd);
            }
            else
            {
                // Project scissor/clipping rectangles into framebuffer space
                ImVec2 clipMin(pcmd->ClipRect.x - clipOffset.x, pcmd->ClipRect.y - clipOffset.y);
                ImVec2 clipMax(pcmd->ClipRect.z - clipOffset.x, pcmd->ClipRect.w - clipOffset.y);
                if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
                    continue;

                // Apply scissor/clipping rectangle

                pcs.textureID = (uint32_t)(uintptr_t)pcmd->GetTexID();
                cb.PushConstants(pcs, VK::ShaderStage::AllRaster, s->pipelineLayout);

                int clipX = (int)clipMin.x;
                int clipY = (int)clipMin.y;
                auto clipW = (uint32_t)(clipMax.x - clipMin.x);
                auto clipH = (uint32_t)(clipMax.y - clipMin.y);

                VK::ScissorRect sr{clipX, clipY, clipW, clipH};

                cb.SetScissor(sr);

                cb.DrawIndexed(pcmd->ElemCount, 1, pcmd->IdxOffset + globalIndexOffset,
                               (int)pcmd->VtxOffset + globalVertexOffset, 0);
            }
        }

        globalIndexOffset += cmdList->IdxBuffer.Size;
        globalVertexOffset += cmdList->VtxBuffer.Size;
    }
}
#include "../R2ImGui.hpp"
#include <stdint.h>
#include "ImGuiFS.h"
#include "ImGuiVS.h"
#include <R2/VKBuffer.hpp>
#include <R2/VKCore.hpp>
#include <R2/VKPipeline.hpp>
#include <R2/VKDescriptorSet.hpp>
#include <R2/VKRenderPass.hpp>
#include <R2/VKCommandBuffer.hpp>
#include <R2/VKTexture.hpp>
#include <R2/VKSampler.hpp>
#include <R2/BindlessTextureManager.hpp>

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
    VK::Buffer* vertexBuffer = nullptr;
    size_t vertexBufferCapacity = 0;
    VK::Buffer* indexBuffer = nullptr;
    size_t indexBufferCapacity = 0;
    VK::Core* core = nullptr;
    VkPipelineLayout pipelineLayout = nullptr;
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

void ImGui_ImplR2_CreateFontTextureAndDS();

bool ImGui_ImplR2_Init(VK::Core* renderer, BindlessTextureManager* btm)
{
    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererName = "R2";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

    R2ImplState* s = new R2ImplState{};
    s->core = renderer;
    s->textureManager = btm;
    io.BackendRendererUserData = s;

    VK::PipelineLayoutBuilder plb{ renderer->GetHandles() };
    plb.PushConstants(VK::ShaderStage::Vertex | VK::ShaderStage::Fragment, 0, sizeof(ImGuiPCs))
       .DescriptorSet(&btm->GetTextureDescriptorSetLayout());
    s->pipelineLayout = plb.Build();

    VK::VertexBinding vertexBinding{};
    vertexBinding.Binding = 0;
    vertexBinding.Attributes.push_back(VK::VertexAttribute{ 0, VK::TextureFormat::R32G32_SFLOAT, offsetof(ImDrawVert, pos) });
    vertexBinding.Attributes.push_back(VK::VertexAttribute{ 1, VK::TextureFormat::R32G32_SFLOAT, offsetof(ImDrawVert, uv) });
    vertexBinding.Attributes.push_back(VK::VertexAttribute{ 2, VK::TextureFormat::R8G8B8A8_UNORM, offsetof(ImDrawVert, col) });
    vertexBinding.Size = sizeof(ImDrawVert);

    VK::ShaderModule fsm{ renderer->GetHandles(), ImGuiFS, sizeof(ImGuiFS) };
    VK::ShaderModule vsm{ renderer->GetHandles(), ImGuiVS, sizeof(ImGuiVS) };

    VK::PipelineBuilder pb{ renderer->GetHandles() };
    s->pipeline = pb
        .PrimitiveTopology(VK::Topology::TriangleList)
        .CullMode(VK::CullMode::None)
        .Layout(s->pipelineLayout)
        .ColorAttachmentFormat(VK::TextureFormat::B8G8R8A8_SRGB)
        .AddVertexBinding(std::move(vertexBinding))
        .AddShader(VK::ShaderStage::Fragment, fsm)
        .AddShader(VK::ShaderStage::Vertex, vsm)
        .AlphaBlend(true)
        .Build();

    ImGui_ImplR2_CreateFontTextureAndDS();

    return true;
}

void ImGui_ImplR2_Shutdown()
{
    R2ImplState* s = getImplState();
    s->core->DestroyBuffer(s->indexBuffer);
    s->core->DestroyBuffer(s->vertexBuffer);
    delete s->dsl;
    delete s->pipeline;
    s->core->DestroyTexture(s->fontTexture);
    //s->pipelineLayout;
    delete s->sampler;
}

void ImGui_ImplR2_NewFrame()
{
}

void ImGui_ImplR2_CreateFontTextureAndDS()
{
    R2ImplState* s = getImplState();

    ImGuiIO& io = ImGui::GetIO();
    uint8_t* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    VK::TextureCreateInfo tci = VK::TextureCreateInfo::Texture2D(VK::TextureFormat::R8G8B8A8_SRGB, width, height);
    s->fontTexture = s->core->CreateTexture(tci);

    s->core->QueueTextureUpload(s->fontTexture, pixels, 4 * width * height);

    ImGui::MemFree(pixels);

    s->fontTextureID = s->textureManager->AllocateTextureHandle(s->fontTexture);
    io.Fonts->SetTexID((ImTextureID)s->fontTextureID);

    VK::SamplerBuilder sb{ s->core->GetHandles() };
    s->sampler = sb
        .AddressMode(VK::SamplerAddressMode::Repeat)
        .MagFilter(VK::Filter::Linear)
        .MinFilter(VK::Filter::Linear)
        .MipmapMode(VK::SamplerMipmapMode::Linear)
        .Build();
}

void ImGui_ImplR2_RenderDrawData(ImDrawData* drawData, VK::CommandBuffer& cb)
{
    R2ImplState* s = getImplState();

    if (!s->vertexBuffer || s->vertexBufferCapacity < drawData->TotalVtxCount)
    {
        if (s->vertexBuffer)
        {
            s->core->DestroyBuffer(s->vertexBuffer);
        }

        s->vertexBufferCapacity = drawData->TotalVtxCount + 5000;

        VK::BufferCreateInfo bci{};
        bci.Mappable = true;
        bci.Size = s->vertexBufferCapacity * sizeof(ImDrawVert);
        bci.Usage = VK::BufferUsage::Vertex;
        
        s->vertexBuffer = s->core->CreateBuffer(bci);
    }

    if (!s->indexBuffer || s->indexBufferCapacity < drawData->TotalIdxCount)
    {
        if (s->indexBuffer)
        {
            s->core->DestroyBuffer(s->indexBuffer);
        }

        s->indexBufferCapacity = drawData->TotalIdxCount + 10000;

        VK::BufferCreateInfo bci{};
        bci.Mappable = true;
        bci.Size = s->indexBufferCapacity * sizeof(ImDrawIdx);
        bci.Usage = VK::BufferUsage::Index;

        s->indexBuffer = s->core->CreateBuffer(bci);
    }

    ImDrawVert* verts = static_cast<ImDrawVert*>(s->vertexBuffer->Map());
    ImDrawIdx* indices = static_cast<ImDrawIdx*>(s->indexBuffer->Map());

    for (int i = 0; i < drawData->CmdListsCount; i++)
    {
        const ImDrawList* cmdList = drawData->CmdLists[i];

        memcpy(verts, cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(indices, cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx));

        verts += cmdList->VtxBuffer.Size;
        indices += cmdList->IdxBuffer.Size;
    }

    s->vertexBuffer->Unmap();
    s->indexBuffer->Unmap();

    VK::Viewport vp = VK::Viewport::Simple(drawData->DisplaySize.x, drawData->DisplaySize.y);
    cb.SetViewport(vp);

    ImGuiPCs pcs{};
    pcs.scaleX = 2.0f / drawData->DisplaySize.x;
    pcs.scaleY = 2.0f / drawData->DisplaySize.y;

    pcs.translateX = -1.0f - drawData->DisplayPos.x * pcs.scaleX;
    pcs.translateY = -1.0f - drawData->DisplayPos.y * pcs.scaleY;

    cb.PushConstants(pcs, VK::ShaderStage::Vertex | VK::ShaderStage::Fragment, s->pipelineLayout);

    int global_idx_offset = 0;
    int global_vtx_offset = 0;
    ImVec2 clip_off = drawData->DisplayPos;
    cb.BindGraphicsDescriptorSet(s->pipelineLayout, s->textureManager->GetTextureDescriptorSet().GetNativeHandle(), 0);
    cb.BindIndexBuffer(s->indexBuffer, 0, VK::IndexType::Uint16);
    cb.BindVertexBuffer(0, s->vertexBuffer, 0);
    cb.BindPipeline(s->pipeline);

    for (int n = 0; n < drawData->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = drawData->CmdLists[n];
        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback != NULL)
            {
                // User callback, registered via ImDrawList::AddCallback()
                // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
                //if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                //    ImGui_ImplDX11_SetupRenderState(draw_data, ctx);
                //else
                //    pcmd->UserCallback(cmd_list, pcmd);
            }
            else
            {
                // Project scissor/clipping rectangles into framebuffer space
                ImVec2 clip_min(pcmd->ClipRect.x - clip_off.x, pcmd->ClipRect.y - clip_off.y);
                ImVec2 clip_max(pcmd->ClipRect.z - clip_off.x, pcmd->ClipRect.w - clip_off.y);
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                    continue;

                // Apply scissor/clipping rectangle

                pcs.textureID = (uint32_t)pcmd->GetTexID();
                cb.PushConstants(pcs, VK::ShaderStage::Vertex | VK::ShaderStage::Fragment, s->pipelineLayout);

                VK::ScissorRect sr{ clip_min.x, clip_min.y, clip_max.x - clip_min.x, clip_max.y - clip_min.y };
                cb.SetScissor(sr);

                cb.DrawIndexed(pcmd->ElemCount, 1, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset, 0);
            }
        }

        global_idx_offset += cmd_list->IdxBuffer.Size;
        global_vtx_offset += cmd_list->VtxBuffer.Size;
    }
}
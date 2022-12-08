#include "HiddenMeshRenderer.hpp"
#include <Core/Engine.hpp>
#include <Core/Log.hpp>
#include <Render/RenderInternal.hpp>
#include <Render/ShaderCache.hpp>
#include <R2/VK.hpp>
#include <VR/OpenXRInterface.hpp>
#include <stdio.h>
#include <Core/ConVar.hpp>

using namespace R2;

namespace worlds
{
    void saveHiddenAreaMesh(HiddenAreaMesh& mesh, const char* path, glm::mat4 proj)
    {
        FILE* f = fopen(path, "wb");
        uint32_t vertexCount = mesh.verts.size();
        fwrite(&vertexCount, sizeof(vertexCount), 1, f);
        uint32_t indexCount = mesh.indices.size();
        fwrite(&indexCount, sizeof(indexCount), 1, f);

        std::vector<glm::vec2> transformedVerts;
        transformedVerts.resize(mesh.verts.size());
        for (uint32_t i = 0; i < vertexCount; i++)
        {
            transformedVerts[i] = proj * glm::vec4(mesh.verts[i], -1.0, 1.0);
        }

        fwrite(transformedVerts.data(), sizeof(glm::vec2), transformedVerts.size(), f);
        fwrite(mesh.indices.data(), sizeof(uint32_t), mesh.indices.size(), f);
        fclose(f);
    }

    void loadHiddenAreaMesh(HiddenAreaMesh& mesh, const char* path, glm::mat4 proj)
    {
        PHYSFS_File* file = PHYSFS_openRead(path);
        uint32_t vertexCount, indexCount;
        PHYSFS_readBytes(file, &vertexCount, sizeof(vertexCount));
        PHYSFS_readBytes(file, &indexCount, sizeof(indexCount));

        mesh.verts.resize(vertexCount);
        mesh.indices.resize(indexCount);

        PHYSFS_readBytes(file, mesh.verts.data(), sizeof(glm::vec2) * vertexCount);
        PHYSFS_readBytes(file, mesh.indices.data(), sizeof(uint32_t) * indexCount);
        PHYSFS_close(file);
    }

    HiddenMeshRenderer::HiddenMeshRenderer(const EngineInterfaces& interfaces, int sampleCount, VK::Buffer* vpBuffer)
        : interfaces(interfaces)
        , leftIndexCount(0)
        , rightIndexCount(0)
    {
        VKRenderer* renderer = (VKRenderer*)interfaces.renderer;
        VK::Core* core = renderer->getCore();

        HiddenAreaMesh leftMesh{};
        HiddenAreaMesh rightMesh{};

        if (!interfaces.vrInterface->getHiddenAreaMesh(Eye::LeftEye, leftMesh) ||
            !interfaces.vrInterface->getHiddenAreaMesh(Eye::RightEye, rightMesh))
        {
            logWarn(WELogCategoryRender, "VR hidden area mesh was empty");
            if (interfaces.vrInterface->getSystemName() == "SteamVR/OpenXR : oculus")
            {
                logMsg(WELogCategoryRender, "Falling back to built-in Q2 mesh...");
                const auto& projL = interfaces.vrInterface->getEyeProjectionMatrix(Eye::LeftEye);
                const auto& projR = interfaces.vrInterface->getEyeProjectionMatrix(Eye::RightEye);
                loadHiddenAreaMesh(leftMesh, "VR/quest2_left_eye_mesh.bin", projL);
                loadHiddenAreaMesh(rightMesh, "VR/quest2_right_eye_mesh.bin", projR);
                meshIsNdc = true;
            }
            else
            {
                return;
            }
        }

        if (!meshIsNdc)
        {
            const auto &projL = interfaces.vrInterface->getEyeProjectionMatrix(Eye::LeftEye);
            const auto &projR = interfaces.vrInterface->getEyeProjectionMatrix(Eye::RightEye);
            saveHiddenAreaMesh(leftMesh, "quest2_left_eye_mesh.bin", projL);
            saveHiddenAreaMesh(rightMesh, "quest2_right_eye_mesh.bin", projR);
        }

        uint64_t totalVerts = leftMesh.verts.size() + rightMesh.verts.size();
        uint64_t totalIndices = leftMesh.indices.size() + rightMesh.indices.size();

        VK::BufferCreateInfo bci{ VK::BufferUsage::Vertex, totalVerts * sizeof(glm::vec2) };
        vertexBuffer = core->CreateBuffer(bci);
        bci.Usage = VK::BufferUsage::Index;
        bci.Size = totalIndices * sizeof(uint32_t);
        indexBuffer = core->CreateBuffer(bci);

        size_t leftVertSize = leftMesh.verts.size() * sizeof(glm::vec2);
        size_t rightVertSize = rightMesh.verts.size() * sizeof(glm::vec2);

        core->QueueBufferUpload(vertexBuffer.Get(), leftMesh.verts.data(), leftVertSize, 0);
        core->QueueBufferUpload(vertexBuffer.Get(), rightMesh.verts.data(), rightVertSize, leftVertSize);

        size_t leftIndicesSize = leftMesh.indices.size() * sizeof(uint32_t);
        size_t rightIndicesSize = rightMesh.indices.size() * sizeof(uint32_t);

        core->QueueBufferUpload(indexBuffer.Get(), leftMesh.indices.data(), leftIndicesSize, 0);
        core->QueueBufferUpload(indexBuffer.Get(), rightMesh.indices.data(), rightIndicesSize, leftIndicesSize);


        VK::DescriptorSetLayoutBuilder dslb{core};
        dslb.Binding(0, VK::DescriptorType::UniformBuffer, 1, VK::ShaderStage::Vertex);
        dsl = dslb.Build();

        ds = core->CreateDescriptorSet(dsl.Get());

        VK::DescriptorSetUpdater dsu{ core, ds.Get() };
        dsu.AddBuffer(0, 0, VK::DescriptorType::UniformBuffer, vpBuffer);
        dsu.Update();

        VK::PipelineLayoutBuilder plb{core};
        plb.PushConstants(VK::ShaderStage::Vertex, 0, sizeof(uint32_t));
        plb.DescriptorSet(dsl.Get());
        pipelineLayout = plb.Build();

        VK::VertexBinding vb{};
        vb.Size = sizeof(glm::vec2);
        vb.Attributes.emplace_back(0, VK::TextureFormat::R32G32_SFLOAT, 0);

        VK::PipelineBuilder pb{core};
        pb
            .Layout(pipelineLayout.Get())
            .AddShader(VK::ShaderStage::Vertex, ShaderCache::getModule("Shaders/vr_hidden.vert.spv"))
            .AddShader(VK::ShaderStage::Fragment, ShaderCache::getModule("Shaders/blank.frag.spv"))
            .DepthAttachmentFormat(VK::TextureFormat::D32_SFLOAT)
            .DepthWrite(true)
            .DepthTest(true)
            .DepthCompareOp(VK::CompareOp::Always)
            .PrimitiveTopology(VK::Topology::TriangleList)
            .AddVertexBinding(vb)
            .MSAASamples(sampleCount)
            .ViewMask(0b11)
            .CullMode(VK::CullMode::None);

        pipeline = pb.Build();

        leftIndexCount = leftMesh.indices.size();
        rightIndexCount = rightMesh.indices.size();
        leftVertCount = leftMesh.verts.size();
    }

    struct HiddenPushConstants
    {
        uint32_t view;
        uint32_t mode;
    };

    ConVar enableCullMesh { "r_enableCullMesh", "1" };

    void HiddenMeshRenderer::Execute(VK::CommandBuffer& cb)
    {
        if (leftIndexCount == 0 || rightIndexCount == 0) return;
        if (!enableCullMesh.getInt()) return;

        cb.BindVertexBuffer(0, vertexBuffer.Get(), 0);
        cb.BindIndexBuffer(indexBuffer.Get(), 0, VK::IndexType::Uint32);

        cb.BindPipeline(pipeline.Get());
        cb.BindGraphicsDescriptorSet(pipelineLayout.Get(), ds.Get(), 0);

        HiddenPushConstants pcs{};
        pcs.mode = meshIsNdc ? 1 : 0;
        pcs.view = 0;
        cb.PushConstants(pcs, VK::ShaderStage::Vertex, pipelineLayout.Get());
        cb.DrawIndexed(leftIndexCount, 1, 0, 0, 0);

        pcs.view = 1;
        cb.PushConstants(pcs, VK::ShaderStage::Vertex, pipelineLayout.Get());
        cb.DrawIndexed(rightIndexCount, 1, leftIndexCount, (int)leftVertCount, 0);
    }
}

#include "RobloxMeshLoader.hpp"

namespace worlds {
    std::vector<std::string> splitString(std::string str, char c) {
        std::vector<std::string> components;
        size_t pos = 0;
        std::string token;
        while ((pos = str.find(c)) != std::string::npos) {
            token = str.substr(0, pos);
            components.push_back(token);
            str.erase(0, pos + 1);
        }

        components.push_back(str);

        return components;
    }

    std::string readUntil(PHYSFS_File* f, char endChar) {
        std::string str;

        char c = '\0';

        while (c != endChar) {
            PHYSFS_readBytes(f, &c, 1);
            str += c;
        }

        return str;
    }

    void loadRobloxMesh(AssetID id, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, LoadedMeshData& lmd) {
        PHYSFS_File* f = g_assetDB.openAssetFileRead(id);

        // look for version header
        auto versionLine = readUntil(f, '\n');

        logMsg("Version is %s", versionLine.c_str());

        float scale = versionLine == "Version 1.00\n" ? 0.5f : 1.0f;

        int faceCount = std::stoi(readUntil(f, '\n'));

        int target = 0;
        int face = 0;

        Vertex v;

        // now comes the hard part
        // reading each vertex... :(
        while (!PHYSFS_eof(f)) {
            std::string vtStr = readUntil(f, ']');

            vtStr = vtStr.substr(1, vtStr.size() - 2);
            std::vector<std::string> split = splitString(vtStr, ',');

            if (target == 0) {
                v.position = glm::vec3{ std::stof(split[0]), std::stof(split[1]), std::stof(split[2]) } * scale;
            } else if (target == 1) {
                v.normal = glm::vec3{ std::stof(split[0]), std::stof(split[1]), std::stof(split[2]) };
            } else if (target == 2) {
                v.uv = glm::vec2{ std::stof(split[0]), 1.0f - std::stof(split[1]) };
            }

            target = (target + 1) % 3;

            if (target == 0) {
                vertices.push_back(v);
                v = Vertex{};

                int v2 = (face++) * 3;
                for (int i = 0; i < 3; i++) indices.push_back(v2 + i);
            }
        }

        PHYSFS_close(f);

        lmd.numSubmeshes = 1;
        lmd.submeshes[0].indexCount = indices.size();
        lmd.submeshes[0].indexOffset = 0;
    }
}
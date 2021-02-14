#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace worlds {
    inline glm::quat getMatrixRotation(glm::mat4 matrix) {
        glm::quat Orientation;

        glm::vec3 Row[3];

        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                Row[i][j] = matrix[i][j];

        int i, j, k = 0;
        float root, trace = Row[0].x + Row[1].y + Row[2].z;
        if (trace > 0.0f) {
            root = sqrt(trace + 1.0f);
            Orientation.w = 0.5f * root;
            root = 0.5f / root;
            Orientation.x = root * (Row[1].z - Row[2].y);
            Orientation.y = root * (Row[2].x - Row[0].z);
            Orientation.z = root * (Row[0].y - Row[1].x);
        } // End if > 0
        else {
            static int Next[3] = { 1, 2, 0 };
            i = 0;
            if (Row[1].y > Row[0].x) i = 1;
            if (Row[2].z > Row[i][i]) i = 2;
            j = Next[i];
            k = Next[j];

            root = sqrt(Row[i][i] - Row[j][j] - Row[k][k] + 1.0f);

            Orientation[i] = 0.5f * root;
            root = 0.5f / root;
            Orientation[j] = root * (Row[i][j] + Row[j][i]);
            Orientation[k] = root * (Row[i][k] + Row[k][i]);
            Orientation.w = root * (Row[j][k] - Row[k][j]);
        }

        return Orientation;
    }

    inline glm::vec3 getMatrixTranslation(glm::mat4 matrix) {
        return matrix[3];
    }
}
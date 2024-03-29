#pragma once
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtx/matrix_decompose.hpp>

struct Transform
{
    Transform() : position(0.0f), rotation(1.0f, 0.0f, 0.0f, 0.0f), scale(1.0f)
    {
    }

    Transform(glm::vec3 position, glm::quat rotation) : position(position), rotation(rotation), scale(1.0f)
    {
    }

    Transform(glm::mat4 mat)
    {
        fromMatrix(mat);
    }

    glm::vec3 position;
    glm::quat rotation;
    glm::vec3 scale;

    Transform transformBy(const Transform& other) const
    {
        return Transform{other.getMatrix() * getMatrix()};
    }

    Transform transformByInverse(const Transform& other) const
    {
        return Transform{glm::inverse(other.getMatrix()) * getMatrix()};
    }

    glm::vec3 transformDirection(glm::vec3 v3) const
    {
        return rotation * v3;
    }

    glm::vec3 transformPoint(glm::vec3 v3) const
    {
        return position + (rotation * v3);
    }

    glm::vec3 inverseTransformDirection(glm::vec3 v3) const
    {
        return glm::inverse(rotation) * v3;
    }

    glm::vec3 inverseTransformPoint(glm::vec3 v3) const
    {
        return glm::inverse(rotation) * (v3 - position);
    }

    glm::mat4 getMatrix() const
    {
        return glm::translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation) *
               glm::scale(glm::mat4(1.0f), scale);
    }

    void fromMatrix(glm::mat4 mat)
    {
        glm::vec3 skew;
        glm::vec4 perspective;
        glm::decompose(mat, scale, rotation, position, skew, perspective);
    }
};

struct UnscaledTransform
{
    glm::vec3 position;
    glm::quat rotation;

    UnscaledTransform()
        : position(0.0f)
        , rotation(1.0f, 0.0f, 0.0f, 0.0f)
    {
    }

    glm::mat4 getMatrix() const
    {
        return glm::translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation);
    }
};

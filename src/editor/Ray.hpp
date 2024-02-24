#pragma once
#include <reactive/Scene/AABB.hpp>

struct Ray {
    glm::vec3 origin;
    glm::vec3 direction;

    bool intersect(rv::AABB aabb, float& t) const {
        glm::vec3 min = aabb.getMin();
        glm::vec3 max = aabb.getMax();

        float t1 = (min.x - origin.x) / direction.x;
        float t2 = (max.x - origin.x) / direction.x;
        float t3 = (min.y - origin.y) / direction.y;
        float t4 = (max.y - origin.y) / direction.y;
        float t5 = (min.z - origin.z) / direction.z;
        float t6 = (max.z - origin.z) / direction.z;

        float tmin = glm::max(glm::max(glm::min(t1, t2), glm::min(t3, t4)), glm::min(t5, t6));
        float tmax = glm::min(glm::min(glm::max(t1, t2), glm::max(t3, t4)), glm::max(t5, t6));

        if (tmax >= tmin && tmax >= 0.0f) {
            t = tmin;
            return true;
        }
        return false;
    }
};

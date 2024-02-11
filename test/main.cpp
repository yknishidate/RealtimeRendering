// Add gtest
#include <gtest/gtest.h>

#include <reactive/Scene/AABB.hpp>
#include <reactive/Scene/Camera.hpp>
#include <reactive/Scene/Frustum.hpp>

// Camera coordinate system
TEST(OrbitalCameraTest, Camera) {
    rv::Camera camera{};
    EXPECT_EQ(camera.getType(), rv::Camera::Type::Orbital);
    EXPECT_EQ(camera.getUp(), glm::vec3(0.0f, 1.0f, 0.0f));      // +Y
    EXPECT_EQ(camera.getRight(), glm::vec3(1.0f, 0.0f, 0.0f));   // +X
    EXPECT_EQ(camera.getFront(), glm::vec3(0.0f, 0.0f, -1.0f));  // -Z

    // Camera position
    camera.setDistance(5.0f);
    EXPECT_EQ(camera.getPosition(), glm::vec3(0.0f, 0.0f, 5.0f));
}

TEST(FirstPersonCameraTest, Camera) {
    rv::Camera camera{rv::Camera::Type::FirstPerson, 1.0f};
    EXPECT_EQ(camera.getType(), rv::Camera::Type::FirstPerson);
    EXPECT_EQ(camera.getUp(), glm::vec3(0.0f, 1.0f, 0.0f));      // +Y
    EXPECT_EQ(camera.getRight(), glm::vec3(1.0f, 0.0f, 0.0f));   // +X
    EXPECT_EQ(camera.getFront(), glm::vec3(0.0f, 0.0f, -1.0f));  // -Z
}

// Plane and distance
TEST(PlaneTest, Plane) {
    // Plane normal is (0, 1, 0) and distance is 0
    rv::Plane plane{glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)};
    EXPECT_EQ(plane.getSignedDistance(glm::vec3(0.0f, 0.0f, 0.0f)), 0.0f);
    EXPECT_EQ(plane.getSignedDistance(glm::vec3(1.0f, 0.0f, 0.0f)), 0.0f);
    EXPECT_EQ(plane.getSignedDistance(glm::vec3(0.0f, 1.0f, 0.0f)), 1.0f);
    EXPECT_EQ(plane.getSignedDistance(glm::vec3(0.0f, 0.0f, 1.0f)), 0.0f);
    EXPECT_EQ(plane.getSignedDistance(glm::vec3(1.0f, 1.0f, 1.0f)), 1.0f);

    EXPECT_EQ(plane.getSignedDistance(glm::vec3(-1.0f, 0.0f, 0.0f)), 0.0f);
    EXPECT_EQ(plane.getSignedDistance(glm::vec3(0.0f, -1.0f, 0.0f)), -1.0f);
    EXPECT_EQ(plane.getSignedDistance(glm::vec3(0.0f, 0.0f, -1.0f)), 0.0f);
    EXPECT_EQ(plane.getSignedDistance(glm::vec3(-1.0f, -1.0f, -1.0f)), -1.0f);
}

// Frustum
TEST(FrustumTest, Frustum) {
    rv::Camera camera{rv::Camera::Type::Orbital, 1.0f};

    rv::Frustum frustum{camera};

    // Near: -Z方向なので、distanceもワールド座標の反対になるのが正しい
    float nearWorldZ = camera.getPosition().z + camera.getNear() * camera.getFront().z;
    EXPECT_EQ(frustum.nearFace.normal, glm::vec3(0.0f, 0.0f, -1.0f));
    EXPECT_EQ(frustum.nearFace.distance, -nearWorldZ);

    // Far: +Z方向
    float farWorldZ = camera.getPosition().z + camera.getFar() * camera.getFront().z;
    EXPECT_EQ(frustum.farFace.normal, glm::vec3(0.0f, 0.0f, 1.0f));
    EXPECT_EQ(frustum.farFace.distance, farWorldZ);

    glm::vec3 X = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 Y = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 Z = glm::vec3(0.0f, 0.0f, 1.0f);
    EXPECT_GT(glm::dot(frustum.rightFace.normal, -X), 0.0);
    EXPECT_GT(glm::dot(frustum.leftFace.normal, +X), 0.0);
    EXPECT_GT(glm::dot(frustum.topFace.normal, -Y), 0.0);
    EXPECT_GT(glm::dot(frustum.bottomFace.normal, +Y), 0.0);
}

// AABB
TEST(AABBTest, AABB) {
    rv::AABB aabb0{glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f)};
    EXPECT_EQ(aabb0.center, glm::vec3(0.0f, 0.0f, 0.0f));
    EXPECT_EQ(aabb0.extents, glm::vec3(0.0f, 0.0f, 0.0f));

    rv::AABB aabb1{glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec3(1.0f, 1.0f, 1.0f)};
    EXPECT_EQ(aabb1.center, glm::vec3(0.0f, 0.0f, 0.0f));
    EXPECT_EQ(aabb1.extents, glm::vec3(1.0f, 1.0f, 1.0f));
}

// Frustum and AABB
TEST(FrustumAABBTest, FrustumAABB) {
    // NOTE: XY平面上で上下左右 5.0 まで視界に入る
    rv::Camera camera{rv::Camera::Type::Orbital, 1.0f};
    camera.setFovY(glm::radians(90.0f));
    camera.setDistance(5.0f);

    rv::Frustum frustum{camera};

    // NOTE: rv::AABB(glm::vec3 min, glm::vec3 max)
    // AABB is inside the frustum
    rv::AABB aabb{glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec3(1.0f, 1.0f, 1.0f)};
    EXPECT_TRUE(aabb.isOnFrustum(frustum));
    aabb = rv::AABB{glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f)};
    EXPECT_TRUE(aabb.isOnFrustum(frustum));
    aabb = rv::AABB{glm::vec3(-5.1f, -5.1f, 0.0f), glm::vec3(-4.9f, -4.9f, 0.0f)};
    EXPECT_TRUE(aabb.isOnFrustum(frustum));
    aabb = rv::AABB{glm::vec3(4.9f, 4.9f, 0.0f), glm::vec3(5.1f, 5.1f, 0.0f)};
    EXPECT_TRUE(aabb.isOnFrustum(frustum));

    // AABB is outside the frustum
    aabb = rv::AABB{glm::vec3(-10.0f, -10.0f, 0.0f), glm::vec3(-9.0f, -9.0f, 0.0f)};
    EXPECT_FALSE(aabb.isOnFrustum(frustum));
    
    aabb = rv::AABB{glm::vec3(-5.2f, 0.0f, 0.0f), glm::vec3(-5.1f, 0.0f, 0.0f)}; // -X
    EXPECT_FALSE(aabb.isOnFrustum(frustum));
    aabb = rv::AABB{glm::vec3(0.0f, -5.2f, 0.0f), glm::vec3(0.0, -5.1f, 0.0f)}; // -Y
    EXPECT_FALSE(aabb.isOnFrustum(frustum));
    aabb = rv::AABB{glm::vec3(-5.2f, -5.2f, 0.0f), glm::vec3(-5.1f, -5.1f, 0.0f)}; // -X&-Y
    EXPECT_FALSE(aabb.isOnFrustum(frustum));

    aabb = rv::AABB{glm::vec3(5.1f, 5.1f, 0.0f), glm::vec3(5.2f, 5.2f, 0.0f)}; // +X
    EXPECT_FALSE(aabb.isOnFrustum(frustum));
    aabb = rv::AABB{glm::vec3(0.0f, 5.1f, 0.0f), glm::vec3(0.0, 5.2f, 0.0f)}; // +Y
    EXPECT_FALSE(aabb.isOnFrustum(frustum));
    aabb = rv::AABB{glm::vec3(5.1f, 5.1f, 0.0f), glm::vec3(5.2f, 5.2f, 0.0f)}; // +X&+Y
    EXPECT_FALSE(aabb.isOnFrustum(frustum));
}

// Run all the tests that were declared with TEST()
int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

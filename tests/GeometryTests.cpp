#include "sf2/Geometry.h"

#include <gtest/gtest.h>

using namespace sf2;

namespace
{
void expectNear (const Vec3& a, const Vec3& b, double tol = 1e-9)
{
    EXPECT_NEAR (a.x, b.x, tol);
    EXPECT_NEAR (a.y, b.y, tol);
    EXPECT_NEAR (a.z, b.z, tol);
}
} // namespace

TEST (Geometry, AzimuthIsPositiveToTheLeft)
{
    expectNear (directionFromAzEl (0, 0), { 1, 0, 0 });
    expectNear (directionFromAzEl (90, 0), { 0, 1, 0 });
    expectNear (directionFromAzEl (0, 90), { 0, 0, 1 });
}

TEST (Geometry, YawMovesFrontSourceLeft)
{
    expectNear (sceneRotation (90, 0, 0) * Vec3 { 1, 0, 0 }, { 0, 1, 0 });
}

TEST (Geometry, PitchMovesFrontSourceUp)
{
    expectNear (sceneRotation (0, 90, 0) * Vec3 { 1, 0, 0 }, { 0, 0, 1 });
}

TEST (Geometry, RollMovesLeftSourceUp)
{
    expectNear (sceneRotation (0, 0, 90) * Vec3 { 0, 1, 0 }, { 0, 0, 1 });
}

TEST (Geometry, RotationsAreOrthonormal)
{
    const Mat3 r = sceneRotation (37, -21, 64);
    const Mat3 i = r * r.transposed();
    for (int a = 0; a < 3; ++a)
        for (int b = 0; b < 3; ++b)
            EXPECT_NEAR (i.m[a][b], a == b ? 1.0 : 0.0, 1e-12);
}

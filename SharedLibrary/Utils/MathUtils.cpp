#include "MathUtils.h"
#include <math.h>
#include <cstring>

namespace SharedLib
{
    // The pView is not equivalent to camera space's z.
    // pView should be the -z direction of a camera space.
    void GenViewMat(
        float* const pView,
        float* const pPos,
        float* const pWorldUp,
        float* pResMat)
    {
        float z[3] = {};
        memcpy(z, pView, 3 * sizeof(float));
        ScalarMul(-1.f, z, 3);

        float pUp[3] = {};
        memcpy(pUp, pWorldUp, 3 * sizeof(float));

        float right[3] = {};
        CrossProductVec3(z, pUp, right);
        NormalizeVec(right, 3);
        ScalarMul(-1.f, right, 3);

        CrossProductVec3(z, right, pUp);
        NormalizeVec(pUp, 3);

        float e03 = -DotProduct(pPos, right, 3);
        float e13 = -DotProduct(pPos, pUp, 3);
        float e23 = -DotProduct(pPos, z, 3);

        memset(pResMat, 0, 16 * sizeof(float));
        memcpy(pResMat, right, sizeof(right));
        memcpy(&pResMat[4], pUp, 3 * sizeof(float));
        memcpy(&pResMat[8], z, 3 * sizeof(float));

        pResMat[3] = e03;
        pResMat[7] = e13;
        pResMat[11] = e23;
        pResMat[15] = 1.f;
    }

    void GenPerspectiveProjMat(
        float near,
        float far,
        float fov,
        float aspect,
        float* pResMat)
    {
        memset(pResMat, 0, 16 * sizeof(float));

        float c = 1.f / tanf(fov / 2.f);

        pResMat[0] = c / aspect;
        pResMat[5] = -c;
        pResMat[10] = near / (far - near);
        pResMat[11] = near * far / (far - near);
        pResMat[14] = -1.f;
    }

    void GenRotationMat(
        float roll,
        float pitch,
        float head,
        float* pResMat)
    {
        pResMat[0] = cosf(roll) * cosf(head) - sinf(roll) * sinf(pitch) * sinf(head);
        pResMat[1] = -sinf(roll) * cosf(pitch);
        pResMat[2] = cosf(roll) * sinf(head) + sinf(roll) * sinf(pitch) * cosf(head);

        pResMat[3] = sinf(roll) * cosf(head) + cosf(roll) * sinf(pitch) * sinf(head);
        pResMat[4] = cosf(roll) * cosf(pitch);
        pResMat[5] = sinf(roll) * sinf(head) - cosf(roll) * sinf(pitch) * cosf(head);

        pResMat[6] = -cosf(pitch) * sinf(head);
        pResMat[7] = sinf(pitch);
        pResMat[8] = cosf(pitch) * cosf(head);
    }

    void GenModelMat(
        float* pPos,
        float roll,
        float pitch,
        float head,
        float* pScale,
        float* pResMat)
    {
        // Concatenation of translation matrix and rotation matrix.
        float rMat[9] = {};
        GenRotationMat(roll, pitch, head, rMat);

        float tRMat[16] = {};
        tRMat[0] = rMat[0];
        tRMat[1] = rMat[1];
        tRMat[2] = rMat[2];
        tRMat[3] = pPos[0];

        tRMat[4] = rMat[3];
        tRMat[5] = rMat[4];
        tRMat[6] = rMat[5];
        tRMat[7] = pPos[1];

        tRMat[8] = rMat[6];
        tRMat[9] = rMat[7];
        tRMat[10] = rMat[8];
        tRMat[11] = pPos[2];

        tRMat[15] = 1.f;

        // Assembly the scale matrix.
        float sMat[16] = {};
        sMat[0] = pScale[0];
        sMat[5] = pScale[1];
        sMat[10] = pScale[2];
        sMat[15] = 1.f;

        // Multiply TR and S matrix together to get the model matrix.
        memset(pResMat, 0, 16 * sizeof(float));
        MatrixMul4x4(tRMat, sMat, pResMat);
    }

    void GenRotationMatArb(
        float* axis,
        float radien,
        float* pResMat)
    {
        pResMat[0] = cosf(radien) + (1.f - cosf(radien)) * axis[0] * axis[0];
        pResMat[1] = (1.f - cosf(radien)) * axis[0] * axis[1] - axis[2] * sinf(radien);
        pResMat[2] = (1.f - cosf(radien)) * axis[0] * axis[2] + axis[1] * sinf(radien);

        pResMat[3] = (1.f - cosf(radien)) * axis[0] * axis[1] + axis[2] * sinf(radien);
        pResMat[4] = cosf(radien) + (1.f - cosf(radien)) * axis[1] * axis[1];
        pResMat[5] = (1.f - cosf(radien)) * axis[1] * axis[2] - axis[0] * sinf(radien);

        pResMat[6] = (1.f - cosf(radien)) * axis[0] * axis[2] - axis[1] * sinf(radien);
        pResMat[7] = (1.f - cosf(radien)) * axis[1] * axis[2] + axis[0] * sinf(radien);
        pResMat[8] = cosf(radien) + (1.f - cosf(radien)) * axis[2] * axis[2];
    }
}
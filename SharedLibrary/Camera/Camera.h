#pragma once
#include "../Utils/MathUtils.h"

namespace SharedLib
{
    class HEvent;

    class Camera
    {
    public:
        Camera();
        ~Camera();

        void OnEvent(HEvent& ievent);

        void GenViewPerspectiveMatrices(float* viewMat, float* perspectiveMat, float* vpMat);
        void GenReverseViewPerspectiveMatrices(float* invVpMat);

        void GetView(float* oVec) { memcpy(oVec, m_view, sizeof(float) * 3); }
        void GetUp(float* oVec) { memcpy(oVec, m_up, sizeof(float) * 3); }
        void GetRight(float* oVec);
        void GetNearPlane(float& width, float& height, float& near);

        void GetPos(float* oVec) { memcpy(oVec, m_pos, sizeof(float) * 3); };

    private:
        void OnMiddleMouseButtonEvent(HEvent& ievent);

        HFVec2 m_holdStartPos;
        float m_holdStartView[3];
        float m_holdStartUp[3];
        float m_holdRight[3];
        bool  m_isHold;

        // NOTE: Vectors are in the world space.
        float m_view[3];
        float m_up[3];
        float m_fov;
        float m_aspect; // Width / Height;
        float m_far;  // Far and near are positive and m_far > m_near > 0.
        float m_near;

        float m_pos[3];
    };
}
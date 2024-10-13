#pragma once
#include "../Utils/MathUtils.h"
#include "../Actor/Actor.h"

namespace SharedLib
{
    class HEvent;

    class Camera : Actor
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

        void SetView(float* iView);
        void SetPos(float* iPos) { memcpy(m_pos, iPos, sizeof(m_pos)); }
        void SetFar(float iFar) { m_far = iFar; }

    private:
        void MoveForward();
        void MoveBackward();
        void MoveLeft();
        void MoveRight();
        void MouseRotate(float iMouseXOffset, float iMouseYOffset);

        // All the following events 
        void OnMiddleMouseButtonEvent(HEvent& ievent);
        void OnKeyWEvent(HEvent& ievent);
        void OnKeySEvent(HEvent& ievent);
        void OnKeyAEvent(HEvent& ievent);
        void OnKeyDEvent(HEvent& ievent);

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
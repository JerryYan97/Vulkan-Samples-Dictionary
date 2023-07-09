#pragma once

namespace SharedLib
{
    class HEvent;

    class Camera
    {
    public:
        Camera();
        ~Camera();

        void OnEvent(const HEvent& ievent);

    private:
        void OnMiddleMouseEvent();

        float m_holdStartPos[2];
        float m_holdStartView[3];
        float m_holdStartUp[3];
        float m_holdRight[3];
        bool  m_isHold;

        float m_view[3];
        float m_up[3];
        float m_fov;
        float m_aspect; // Width / Height;
        float m_far;  // Far and near are positive and m_far > m_near > 0.
        float m_near;
    };
}
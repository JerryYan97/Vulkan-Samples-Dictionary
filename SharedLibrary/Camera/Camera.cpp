#include "Camera.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include "../Event/Event.h"
#include <cstring>
#include <cassert>

namespace SharedLib
{
    // ================================================================================================================
    Camera::Camera()
    {
        memset(&m_holdStartPos, 0, sizeof(m_holdStartPos));
        memset(m_holdStartView, 0, sizeof(m_holdStartView));
        memset(m_holdStartUp, 0, sizeof(m_holdStartUp));
        memset(m_holdRight, 0, sizeof(m_holdRight));
        memset(m_pos, 0, sizeof(m_pos));

        m_isHold = false;

        m_view[0] = 1.0f;
        m_view[1] = 0.0f;
        m_view[2] = 0.0f;
        NormalizeVec(m_view, 3);

        m_up[0] = 0.f;
        m_up[1] = 1.f;
        m_up[2] = 0.f;

        CrossProductVec3(m_view, m_up, m_holdRight);
        NormalizeVec(m_holdRight, 3);

        m_fov = 47.f * M_PI / 180.f; // vertical field of view.
        // m_aspect = 960.f / 680.f;
        m_aspect = 1280.f / 640.f;

        m_far = 100.f;
        m_near = 0.1f;
    }

    // ================================================================================================================
    Camera::~Camera()
    {

    }

    // ================================================================================================================
    void Camera::SetView(
        float* iView)
    {
        memcpy(m_view, iView, sizeof(m_view));
        NormalizeVec(m_view, 3);

        CrossProductVec3(m_view, m_up, m_holdRight);
        NormalizeVec(m_holdRight, 3);
    }

    // ================================================================================================================
    void Camera::OnEvent(
        HEvent& ievent)
    {
        switch (ievent.GetEventType()) {
        case crc32("MOUSE_MIDDLE_BUTTON"):
            OnMiddleMouseButtonEvent(ievent);
            break;
        case crc32("KEY_W"):
            OnKeyWEvent(ievent);
            break;
        case crc32("KEY_S"):
            OnKeySEvent(ievent);
            break;
        case crc32("KEY_A"):
            OnKeyAEvent(ievent);
            break;
        case crc32("KEY_D"):
            OnKeyDEvent(ievent);
            break;
        default:
            break;
        }
    }

    // ================================================================================================================
    void Camera::OnKeyWEvent(
        HEvent& ievent)
    {
        HEventArguments& args = ievent.GetArgs();
        bool isDown = std::any_cast<bool>(args[crc32("IS_DOWN")]);
        if (isDown)
        {
            float moveOffset[3] = {};
            memcpy(moveOffset, m_view, 3 * sizeof(float));

            ScalarMul(0.1f, moveOffset, 3);

            VecAdd(moveOffset, m_pos, 3, m_pos);
        }
    }

    // ================================================================================================================
    void Camera::OnKeySEvent(
        HEvent& ievent)
    {
        HEventArguments& args = ievent.GetArgs();
        bool isDown = std::any_cast<bool>(args[crc32("IS_DOWN")]);
        if (isDown)
        {
            float moveOffset[3] = {};
            memcpy(moveOffset, m_view, 3 * sizeof(float));

            ScalarMul(-0.1f, moveOffset, 3);

            VecAdd(moveOffset, m_pos, 3, m_pos);
        }
    }

    // ================================================================================================================
    void Camera::OnKeyAEvent(
        HEvent& ievent)
    {
        HEventArguments& args = ievent.GetArgs();
        bool isDown = std::any_cast<bool>(args[crc32("IS_DOWN")]);
        if (isDown)
        {
            float right[3] = {};
            CrossProductVec3(m_view, m_up, right);

            ScalarMul(-0.1f, right, 3);

            VecAdd(right, m_pos, 3, m_pos);
        }
    }

    // ================================================================================================================
    void Camera::OnKeyDEvent(
        HEvent& ievent)
    {
        HEventArguments& args = ievent.GetArgs();
        bool isDown = std::any_cast<bool>(args[crc32("IS_DOWN")]);
        if (isDown)
        {
            float right[3] = {};
            CrossProductVec3(m_view, m_up, right);

            ScalarMul(0.1f, right, 3);

            VecAdd(right, m_pos, 3, m_pos);
        }
    }

    // ================================================================================================================
    void Camera::OnMiddleMouseButtonEvent(
        HEvent& ievent)
    {
        HEventArguments& args = ievent.GetArgs();
        bool isDown = std::any_cast<bool>(args[crc32("IS_DOWN")]);
        if (isDown)
        {
            if (m_isHold)
            {
                // Continues holding:
                // UP-Down -- Pitch; Left-Right -- Head;
                HFVec2 curPos = std::any_cast<HFVec2>(args[crc32("POS")]);

                float xOffset = -(curPos.ele[0] - m_holdStartPos.ele[0]);
                float yOffset = -(curPos.ele[1] - m_holdStartPos.ele[1]);

                float pitchRadien = 0.5f * yOffset * M_PI / 180.f;
                float headRadien = 0.5f * xOffset * M_PI / 180.f;

                float pitchRotMat[9] = {};
                GenRotationMatArb(m_holdRight, pitchRadien, pitchRotMat);

                float headRotMat[9] = {};
                float worldUp[3] = { 0.f, 1.f, 0.f };
                GenRotationMatArb(worldUp, headRadien, headRotMat);

                float rotMat[9] = {};
                MatMulMat(headRotMat, pitchRotMat, rotMat, 3);

                float newView[3];
                MatMulVec(rotMat, m_holdStartView, 3, newView);

                float newUp[3];
                MatMulVec(rotMat, m_holdStartUp, 3, newUp);

                NormalizeVec(newView, 3);
                NormalizeVec(newUp, 3);

                memcpy(m_view, newView, 3 * sizeof(float));
                memcpy(m_up, newUp, 3 * sizeof(float));
            }
            else
            {
                // First hold:
                m_holdStartPos = std::any_cast<HFVec2>(args[crc32("POS")]);
                memcpy(m_holdStartView, m_view, 3 * sizeof(float));
                memcpy(m_holdStartUp, m_up, 3 * sizeof(float));

                CrossProductVec3(m_view, m_up, m_holdRight);
                NormalizeVec(m_holdRight, 3);
            }
        }

        m_isHold = isDown;
    }

    // ================================================================================================================
    // NOTE: viewMat and perspectiveMat cannot be same!
    void Camera::GenViewPerspectiveMatrices(
        float* viewMat, 
        float* perspectiveMat, 
        float* vpMat)
    {
        assert(viewMat != perspectiveMat, "vMat cannot be same as persMat!");
        GenViewMat(m_view, m_pos, m_up, viewMat);
        GenPerspectiveProjMat(m_near, m_far, m_fov, m_aspect, perspectiveMat);
        MatrixMul4x4(perspectiveMat, viewMat, vpMat);
    }

    // ================================================================================================================
    void Camera::GenReverseViewPerspectiveMatrices(
        float* invVpMat)
    {

    }

    // ================================================================================================================
    void Camera::GetNearPlane(
        float& width,
        float& height,
        float& near)
    {
        near   = m_near;
        height = 2.f * near * tanf(m_fov / 2.f);
        width  = m_aspect * height;
    }

    // ================================================================================================================
    void Camera::GetRight(
        float* oVec)
    {
        CrossProductVec3(m_view, m_up, oVec);
        NormalizeVec(oVec, 3);
    }
}
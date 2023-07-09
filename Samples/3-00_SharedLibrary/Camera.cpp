#include "Camera.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include "MathUtils.h"
#include "Event.h"
#include <cstring>

namespace SharedLib
{
    Camera::Camera()
    {
        memset(m_holdStartPos, 0, sizeof(m_holdStartPos));
        memset(m_holdStartView, 0, sizeof(m_holdStartView));
        memset(m_holdStartUp, 0, sizeof(m_holdStartUp));
        memset(m_holdRight, 0, sizeof(m_holdRight));

        m_isHold = false;

        m_view[0] = 0.f;
        m_view[1] = 0.f;
        m_view[2] = 1.f;

        m_up[0] = 0.f;
        m_up[1] = 1.f;
        m_up[2] = 0.f;

        m_fov = 47.f * M_PI / 180.f; // vertical field of view.
        m_aspect = 960.f / 680.f;

        m_far = 100.f;
        m_near = 0.1f;
    }

    Camera::~Camera()
    {

    }

    void Camera::OnEvent(
        const HEvent& ievent)
    {
        
    }
}
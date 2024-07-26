#include "Level.h"

namespace SharedLib
{
    // ================================================================================================================
    bool Level::AddMshEntity(const std::string& name,
                             MeshEntity*         entity)
    {
        if(m_meshEntities.count(name) > 0)
        {
            return false;
        }
        else
        {
            m_meshEntities[name] = entity;
            return true;
        }
    }

    // ================================================================================================================
    bool Level::AddLightEntity(const std::string& name,
                               LightEntity*       entity)
    {
        if (m_lightEntities.count(name) > 0)
        {
            return false;
        }
        else
        {
            m_lightEntities[name] = entity;
            return true;
        }   
    }
}
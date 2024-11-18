#pragma once

#include "Entity.hpp"

using EntityVec = std::vector<std::shared_ptr<Entity>>;

class EntityManager
{
    EntityVec                           m_entities;
    EntityVec                           m_entitiesToAdd;
    std::map<std::string, EntityVec>    m_entityMap;
    size_t                              m_totalEntities = 0;

    void removeDeadEntities(EntityVec& vec)
    {
        std::erase_if(vec, [](auto const& e) { return !(e->isActive()); });
    }

public:

    EntityManager() = default;

    void update()
    {
        // Add entities from m_entitiesToAdd to the proper locations(s)
        for (auto& e : m_entitiesToAdd)
        {
            m_entities.push_back(e);
            // not sure if this is needed here because it seems that entities
            // are being added to the map at the time of creation within addEntity()
            // m_entityMap[e->tag()].push_back(e);
        }
        m_entitiesToAdd.clear();

        // remove dead entities from the vector of all entities
        removeDeadEntities(m_entities);

        // remove dead entities from each vector in the entity map
        // C++20 way of iterating through [key,value] pairs in a map
        for (auto& [tag, entityVec] : m_entityMap)
        {
            removeDeadEntities(entityVec);
        }
    }

    std::shared_ptr<Entity> addEntity(const std::string& tag)
    {
        // create the entity shared pointer
        auto entity = std::shared_ptr<Entity>(new Entity(m_totalEntities++, tag));

        // add it to the vec of all entities
        m_entitiesToAdd.push_back(entity);

        // add it to the entity map
        if (m_entityMap.find(tag) == m_entityMap.end()) { m_entityMap[tag] = EntityVec(); }
        m_entityMap[tag].push_back(entity);

        return entity;
    }

    const EntityVec& getEntities()
    {
        return m_entities;
    }

    const EntityVec& getEntities(const std::string& tag)
    {
        if (m_entityMap.find(tag) == m_entityMap.end()) { m_entityMap[tag] = EntityVec(); }
        return m_entityMap[tag];
    }

    const std::map<std::string, EntityVec>& getEntityMap()
    {
        return m_entityMap;
    }
};
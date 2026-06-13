#include "world.hpp"

entity_id world::create_entity()
{
    return m_next_id++;
}

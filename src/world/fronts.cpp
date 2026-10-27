#include "fronts.hpp"

#include "world.hpp"

// BL-1427 (fronts) T1: the contract. Every body below is a stub that returns the
// empty answer; the world lane (T2) fills them. See fronts.hpp.

std::vector<front> derive_fronts(const world& /*w*/, entity_id /*nation*/)
{
    return {};
}

const front* front_at(const std::vector<front>& fronts, entity_id tile)
{
    for (const front& f : fronts)
        for (entity_id t : f.span)
            if (t == tile)
                return &f;
    return nullptr;
}

front_order front_order_of(const world& w, entity_id nation, entity_id key)
{
    for (const front_order_entry& e : w.front_orders)
        if (e.nation == nation && e.key == key)
            return e.order;
    return front_order::hold;
}

void reanchor_fronts(world& /*w*/)
{
}

void advance_fronts(world& /*w*/)
{
}

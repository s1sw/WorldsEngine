#ifndef ENTT_ENTITY_LW_FWD_HPP
#define ENTT_ENTITY_LW_FWD_HPP
#include "../core/fwd.hpp"

namespace entt {
    /*! @brief Default entity identifier. */
    enum class entity: id_type {};
    
    template<typename>
    class basic_registry;
    
    /*! @brief Alias declaration for the most common use case. */
    using registry = basic_registry<entity>;
}
#endif
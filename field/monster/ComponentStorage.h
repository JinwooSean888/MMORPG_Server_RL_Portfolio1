#pragma once
#include <unordered_map>
#include "EntityTypes.h"

namespace monster_ecs {

    template <typename T>
    class ComponentStorage {
    public:
        bool has(Entity e) const {
            return data_.find(e) != data_.end();
        }

        T& get(Entity e) {
            return data_.at(e);
        }

        const T& get(Entity e) const {
            return data_.at(e);
        }

        void add(Entity e, const T& v) {
            data_[e] = v;
        }

        void remove(Entity e) {
            data_.erase(e);
        }

        auto begin() { return data_.begin(); }
        auto end() { return data_.end(); }

        auto begin() const { return data_.begin(); }
        auto end()   const { return data_.end(); }

    private:
        std::unordered_map<Entity, T> data_;
    };

} // namespace monster_ecs

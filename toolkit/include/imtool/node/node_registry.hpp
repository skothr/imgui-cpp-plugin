#pragma once

// NodeRegistry — name -> factory map for creating nodes by type name. Lifted
// from 19-logos nodeRegistry.hpp; the variadic-template addType is kept but the
// beta default-constructs (constructor-arg capture is deferred post-beta).

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <imtool/node/node.hpp>

namespace imtool {

class NodeRegistry {
public:
    // Register a Node subclass under `name`. create(name) default-constructs it.
    template<typename T>
    void addType(std::string name) {
        static_assert(std::is_base_of_v<Node, T>, "NodeRegistry::addType<T>: T must derive from imtool::Node");
        if(m_factories.find(name) == m_factories.end()) { m_order.push_back(name); }
        m_factories[name] = []() -> std::unique_ptr<Node> { return std::make_unique<T>(); };
    }

    [[nodiscard]] std::unique_ptr<Node> create(std::string_view name) const {
        const auto it = m_factories.find(std::string(name));
        if(it == m_factories.end()) { return nullptr; }
        std::unique_ptr<Node> node = it->second();
        if(node) { node->m_typeName = it->first; }   // stamp the factory key (friend access)
        return node;
    }

    [[nodiscard]] bool contains(std::string_view name) const {
        return m_factories.find(std::string(name)) != m_factories.end();
    }
    [[nodiscard]] const std::vector<std::string>& typeNames() const { return m_order; }
    [[nodiscard]] std::size_t size() const { return m_factories.size(); }

private:
    std::map<std::string, std::function<std::unique_ptr<Node>()>> m_factories;
    std::vector<std::string>                                      m_order;   // registration order
};

}  // namespace imtool

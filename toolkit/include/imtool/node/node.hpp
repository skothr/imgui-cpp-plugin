#pragma once

// NodeGraph data-model primitives — Node + Connector (port). Data only; no ImGui.
// Lifted + modernized from 19-logos (node/node.hpp, nodeConnector.hpp) and
// 16-nodegraph: open type system via std::type_index, std::unique_ptr ownership,
// JSON param hooks. Deferred (post-beta, see Linear): worker-thread execution,
// std::stop_token cancellation, push/pull data payloads, dirty-flag bookkeeping,
// DataTensor, mid-sequence port insertion, settings auto-inspector.

#include <memory>
#include <string>
#include <string_view>
#include <typeindex>
#include <vector>

#include <nlohmann/json.hpp>

#include <imtool/common/type_registry.hpp>
#include <imtool/common/vector.hpp>

namespace imtool {

class Node;
class NodeGraph;
class NodeRegistry;

enum class PortDir { Input, Output };

// A typed connection point on a node. Type identity is a std::type_index (open
// type system): two ports are connection-compatible iff their type_index match.
// Links point at the opposite-direction connectors this port is wired to.
class Connector {
public:
    Connector(Node *owner, PortDir dir, int index, std::type_index type, std::string name)
        : m_node(owner), m_dir(dir), m_index(index), m_type(type), m_name(std::move(name)) {}

    [[nodiscard]] Node*            node()         { return m_node; }
    [[nodiscard]] const Node*      node()   const { return m_node; }
    [[nodiscard]] PortDir          dir()    const { return m_dir; }
    [[nodiscard]] bool             isInput()  const { return m_dir == PortDir::Input; }
    [[nodiscard]] bool             isOutput() const { return m_dir == PortDir::Output; }
    [[nodiscard]] int              index()  const { return m_index; }
    [[nodiscard]] std::type_index  type()   const { return m_type; }
    [[nodiscard]] std::string_view name()   const { return m_name; }
    [[nodiscard]] std::string      typeName() const { return getTypeName(m_type); }

    [[nodiscard]] const std::vector<Connector*>& links() const { return m_links; }
    [[nodiscard]] bool connected() const { return !m_links.empty(); }

private:
    friend class NodeGraph;   // NodeGraph owns wiring (mutates m_links under validation)

    Node                    *m_node;
    PortDir                  m_dir;
    int                      m_index;
    std::type_index          m_type;
    std::string              m_name;
    std::vector<Connector*>  m_links;
};

// Base node. Subclass and declare ports in the constructor via addInput<T> /
// addOutput<T>; override evaluate()/saveParams()/loadParams() for behavior.
class Node {
public:
    Node() = default;
    explicit Node(std::string name) : m_name(std::move(name)) {}
    virtual ~Node() = default;

    Node(const Node&)            = delete;   // owns connectors (unique_ptr) + has identity
    Node& operator=(const Node&) = delete;

    [[nodiscard]] int              id()       const { return m_id; }
    [[nodiscard]] std::string_view name()     const { return m_name; }
    [[nodiscard]] std::string_view typeName() const { return m_typeName; }
    void setName(std::string name) { m_name = std::move(name); }

    [[nodiscard]] const Vec2f& pos() const { return m_pos; }   // graph-space position (view reads/writes)
    void setPos(const Vec2f &p) { m_pos = p; }

    [[nodiscard]] std::size_t inputCount()  const { return m_inputs.size(); }
    [[nodiscard]] std::size_t outputCount() const { return m_outputs.size(); }
    // Bounds-checked: out-of-range (incl. negative) returns nullptr rather than UB.
    [[nodiscard]] Connector*  input(int i)  const {
        return (i >= 0 && static_cast<std::size_t>(i) < m_inputs.size())  ? m_inputs[static_cast<std::size_t>(i)].get()  : nullptr;
    }
    [[nodiscard]] Connector*  output(int i) const {
        return (i >= 0 && static_cast<std::size_t>(i) < m_outputs.size()) ? m_outputs[static_cast<std::size_t>(i)].get() : nullptr;
    }

    // Synchronous evaluation hook (the beta stub). Subclasses do real work and
    // return false on failure. The graph topo-walks and calls this in order.
    // DEFERRED: async/worker-thread execution, cancellation, data propagation.
    virtual bool evaluate() { return true; }

    // JSON param hooks for subclass-specific state. The graph passes a dedicated
    // per-node "params" sub-object (NOT the node's top-level object), so a param
    // named "id"/"type"/"name"/"pos" cannot collide with the reserved node keys.
    virtual void saveParams(nlohmann::json & /*params*/) const {}
    virtual void loadParams(const nlohmann::json & /*params*/) {}

protected:
    template<typename T>
    Connector* addInput(std::string name) {
        m_inputs.push_back(std::make_unique<Connector>(
            this, PortDir::Input, static_cast<int>(m_inputs.size()), getTypeIndex<T>(), std::move(name)));
        return m_inputs.back().get();
    }
    template<typename T>
    Connector* addOutput(std::string name) {
        m_outputs.push_back(std::make_unique<Connector>(
            this, PortDir::Output, static_cast<int>(m_outputs.size()), getTypeIndex<T>(), std::move(name)));
        return m_outputs.back().get();
    }

private:
    friend class NodeGraph;
    friend class NodeRegistry;

    int                                       m_id       = -1;   // assigned by NodeGraph::add
    std::string                               m_name;
    std::string                               m_typeName;        // registry key; set by NodeRegistry::create
    Vec2f                                     m_pos      {};
    std::vector<std::unique_ptr<Connector>>   m_inputs;
    std::vector<std::unique_ptr<Connector>>   m_outputs;
};

// Recover a typed handle from a base Node* (e.g. the Node* that NodeGraph::create
// / find return). Returns nullptr if `n` is not a T. The open type system hands
// back Node*, so any consumer that wants subclass state downcasts — this is the
// one verb for it, so callers don't each hand-roll a dynamic_cast.
template<typename T>
[[nodiscard]] T* node_cast(Node *n) { return dynamic_cast<T*>(n); }
template<typename T>
[[nodiscard]] const T* node_cast(const Node *n) { return dynamic_cast<const T*>(n); }

}  // namespace imtool

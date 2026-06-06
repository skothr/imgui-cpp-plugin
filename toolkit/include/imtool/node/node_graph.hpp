#pragma once

// NodeGraph — owns nodes (std::unique_ptr) and the connections between their
// connectors. Validates connections (direction, exact type_index match, no
// duplicate, no self-loop, no cycle) and offers a synchronous topo-ordered
// evaluate() plus JSON save/load. Lifted + modernized from 19-logos nodeGraph.
// Deferred (post-beta): worker-thread stepping, cancellation, dataflow payloads,
// dirty-flag incremental eval, undo/redo integration.

#include <memory>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include <imtool/node/node.hpp>
#include <imtool/node/node_registry.hpp>

namespace imtool {

class NodeGraph {
public:
    NodeGraph() = default;
    explicit NodeGraph(NodeRegistry *registry) : m_registry(registry) {}

    NodeGraph(const NodeGraph&)            = delete;   // unique_ptr ownership + subclass slicing
    NodeGraph& operator=(const NodeGraph&) = delete;

    // ---- nodes ----
    Node* add(std::unique_ptr<Node> node);           // assigns id, takes ownership, returns raw ptr
    Node* create(std::string_view typeName);         // via registry; nullptr if unknown / no registry
    void  remove(Node *node);                         // disconnects every port, then erases
    void  clear();
    [[nodiscard]] Node* find(int id) const;
    [[nodiscard]] const std::vector<Node*>& nodes() const { return m_nodeView; }
    [[nodiscard]] std::size_t nodeCount() const { return m_nodes.size(); }

    // ---- connections ----
    [[nodiscard]] bool canConnect(const Connector *out, const Connector *in) const;
    bool connect(Connector *out, Connector *in);      // false (+ logs) if rejected
    void disconnect(Connector *out, Connector *in);
    [[nodiscard]] std::size_t edgeCount() const;      // total wired output->input links

    // ---- evaluation (synchronous beta stub) ----
    bool evaluate();                                  // false (+ logs) if a cycle remains

    // ---- serialization ----
    [[nodiscard]] nlohmann::json toJson() const;
    bool fromJson(const nlohmann::json &js);          // needs a registry; logs + skips unknown types

    // ---- registry ----
    [[nodiscard]] NodeRegistry* registry() const { return m_registry; }
    void setRegistry(NodeRegistry *r) { m_registry = r; }

private:
    void rebuildNodeView();
    // Would adding edge src->dst (data flows src -> dst) introduce a cycle? True
    // iff dst can already reach src along existing output->input edges.
    [[nodiscard]] bool reaches(const Node *from, const Node *to) const;

    NodeRegistry                        *m_registry = nullptr;
    int                                  m_nextId   = 0;
    std::vector<std::unique_ptr<Node>>   m_nodes;     // ownership
    std::vector<Node*>                   m_nodeView;  // raw views, rebuilt on add/remove
};

}  // namespace imtool

#include <imtool/node/node_graph.hpp>

#include <algorithm>
#include <queue>
#include <string>
#include <unordered_map>

#include <imtool/common/logging.hpp>

namespace imtool {

void NodeGraph::rebuildNodeView() {
    m_nodeView.clear();
    m_nodeView.reserve(m_nodes.size());
    for(const auto &up : m_nodes) { m_nodeView.push_back(up.get()); }
}

Node* NodeGraph::add(std::unique_ptr<Node> node) {
    if(!node) { return nullptr; }
    if(node->m_id < 0) { node->m_id = m_nextId++; }
    else               { m_nextId = std::max(m_nextId, node->m_id + 1); }
    Node *raw = node.get();
    m_nodes.push_back(std::move(node));
    rebuildNodeView();
    return raw;
}

Node* NodeGraph::create(std::string_view typeName) {
    if(!m_registry) {
        log() << LogLevel::Error << "NodeGraph::create: no registry set"; log().flush();
        return nullptr;
    }
    std::unique_ptr<Node> node = m_registry->create(typeName);
    if(!node) {
        log() << LogLevel::Warning << "NodeGraph::create: unknown node type '" << std::string(typeName) << "'";
        log().flush();
        return nullptr;
    }
    return add(std::move(node));
}

void NodeGraph::remove(Node *node) {
    if(!node) { return; }
    for(std::size_t i = 0; i < node->outputCount(); ++i) {
        Connector *o = node->output(static_cast<int>(i));
        while(o->connected()) { disconnect(o, o->m_links.front()); }
    }
    for(std::size_t i = 0; i < node->inputCount(); ++i) {
        Connector *in = node->input(static_cast<int>(i));
        while(in->connected()) { disconnect(in->m_links.front(), in); }
    }
    m_nodes.erase(std::remove_if(m_nodes.begin(), m_nodes.end(),
                                 [&](const std::unique_ptr<Node> &up) { return up.get() == node; }),
                  m_nodes.end());
    rebuildNodeView();
}

void NodeGraph::clear() {
    m_nodes.clear();
    m_nodeView.clear();
    m_nextId = 0;
}

Node* NodeGraph::find(int id) const {
    for(Node *n : m_nodeView) { if(n->id() == id) { return n; } }
    return nullptr;
}

bool NodeGraph::reaches(const Node *from, const Node *to) const {
    // DFS downstream (output -> input edges) from `from`; true if `to` is found.
    std::unordered_map<const Node*, bool> seen;
    std::vector<const Node*> stack{from};
    while(!stack.empty()) {
        const Node *n = stack.back();
        stack.pop_back();
        if(n == to) { return true; }
        if(seen[n]) { continue; }
        seen[n] = true;
        for(std::size_t i = 0; i < n->outputCount(); ++i) {
            for(const Connector *link : n->output(static_cast<int>(i))->links()) {
                stack.push_back(link->node());
            }
        }
    }
    return false;
}

bool NodeGraph::canConnect(const Connector *out, const Connector *in) const {
    if(!out || !in)                          { return false; }
    if(!out->isOutput() || !in->isInput())   { return false; }   // direction
    if(out->node() == in->node())            { return false; }   // no self-loop
    if(out->type() != in->type())            { return false; }   // exact type match
    if(in->connected())                      { return false; }   // exclusive input (caller disconnects first)
    if(std::find(out->m_links.begin(), out->m_links.end(), in) != out->m_links.end()) { return false; }  // dup
    if(reaches(in->node(), out->node()))     { return false; }   // would close a cycle
    return true;
}

bool NodeGraph::connect(Connector *out, Connector *in) {
    if(!canConnect(out, in)) {
        log() << LogLevel::Warning << "NodeGraph::connect rejected ("
              << (out ? out->typeName() : "null") << " -> " << (in ? in->typeName() : "null") << ")";
        log().flush();
        return false;
    }
    out->m_links.push_back(in);
    in->m_links.push_back(out);
    return true;
}

void NodeGraph::disconnect(Connector *out, Connector *in) {
    if(!out || !in) { return; }
    auto rm = [](std::vector<Connector*> &v, Connector *c) {
        v.erase(std::remove(v.begin(), v.end(), c), v.end());
    };
    rm(out->m_links, in);
    rm(in->m_links, out);
}

std::size_t NodeGraph::edgeCount() const {
    std::size_t n = 0;
    for(Node *node : m_nodeView) {
        for(std::size_t i = 0; i < node->outputCount(); ++i) { n += node->output(static_cast<int>(i))->links().size(); }
    }
    return n;
}

bool NodeGraph::evaluate() {
    std::unordered_map<Node*, int> indeg;
    indeg.reserve(m_nodes.size());
    for(Node *n : m_nodeView) {
        int d = 0;
        for(std::size_t i = 0; i < n->inputCount(); ++i) { if(n->input(static_cast<int>(i))->connected()) { ++d; } }
        indeg[n] = d;
    }
    std::queue<Node*> q;
    for(const auto &[n, d] : indeg) { if(d == 0) { q.push(n); } }

    std::size_t processed = 0;
    while(!q.empty()) {
        Node *n = q.front();
        q.pop();
        ++processed;
        if(!n->evaluate()) {
            log() << LogLevel::Error << "Node '" << std::string(n->name()) << "' evaluate() failed"; log().flush();
        }
        for(std::size_t i = 0; i < n->outputCount(); ++i) {
            for(Connector *link : n->output(static_cast<int>(i))->links()) {
                if(--indeg[link->node()] == 0) { q.push(link->node()); }
            }
        }
    }
    if(processed < m_nodes.size()) {
        log() << LogLevel::Error << "NodeGraph::evaluate: cycle detected ("
              << static_cast<int>(m_nodes.size() - processed) << " nodes unprocessed)";
        log().flush();
        return false;
    }
    return true;
}

nlohmann::json NodeGraph::toJson() const {
    nlohmann::json js;
    js["nextId"] = m_nextId;
    js["nodes"]  = nlohmann::json::array();
    for(Node *n : m_nodeView) {
        nlohmann::json jn;
        jn["id"]   = n->id();
        jn["type"] = std::string(n->typeName());
        jn["name"] = std::string(n->name());
        jn["pos"]  = n->pos();           // to_json(Vec2f) from vector.hpp
        n->saveParams(jn);               // subclass merges its own params
        js["nodes"].push_back(jn);
    }
    js["edges"] = nlohmann::json::array();
    for(Node *n : m_nodeView) {
        for(std::size_t i = 0; i < n->outputCount(); ++i) {
            Connector *o = n->output(static_cast<int>(i));
            for(Connector *link : o->links()) {
                js["edges"].push_back({
                    {"from", {{"node", n->id()},            {"port", static_cast<int>(i)}}},
                    {"to",   {{"node", link->node()->id()}, {"port", link->index()}}},
                });
            }
        }
    }
    return js;
}

bool NodeGraph::fromJson(const nlohmann::json &js) {
    if(!js.is_object() || !js.contains("nodes")) {
        log() << LogLevel::Error << "NodeGraph::fromJson: malformed root"; log().flush();
        return false;
    }
    if(!m_registry) {
        log() << LogLevel::Error << "NodeGraph::fromJson: no registry set"; log().flush();
        return false;
    }
    clear();

    // pass 1: create nodes, restoring exact ids (so edges resolve).
    for(const auto &jn : js["nodes"]) {
        const std::string type = jn.value("type", std::string{});
        std::unique_ptr<Node> node = m_registry->create(type);
        if(!node) {
            log() << LogLevel::Warning << "NodeGraph::fromJson: skipping unknown type '" << type << "'"; log().flush();
            continue;
        }
        node->m_id = jn.value("id", -1);
        node->setName(jn.value("name", std::string{}));
        if(jn.contains("pos")) { node->setPos(jn["pos"].get<Vec2f>()); }
        node->loadParams(jn);
        add(std::move(node));
    }

    // pass 2: rewire edges via connect() (re-validates type/cycle/dup).
    if(js.contains("edges")) {
        for(const auto &je : js["edges"]) {
            Node *fn = find(je["from"].value("node", -1));
            Node *tn = find(je["to"].value("node", -1));
            if(!fn || !tn) {
                log() << LogLevel::Warning << "NodeGraph::fromJson: dropping edge to missing node"; log().flush();
                continue;
            }
            const int fp = je["from"].value("port", -1);
            const int tp = je["to"].value("port", -1);
            if(fp < 0 || tp < 0 || fp >= static_cast<int>(fn->outputCount()) || tp >= static_cast<int>(tn->inputCount())) {
                log() << LogLevel::Warning << "NodeGraph::fromJson: dropping edge with bad port index"; log().flush();
                continue;
            }
            connect(fn->output(fp), tn->input(tp));
        }
    }

    if(js.contains("nextId")) { m_nextId = std::max(m_nextId, js["nextId"].get<int>()); }
    return true;
}

}  // namespace imtool

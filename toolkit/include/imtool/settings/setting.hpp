#pragma once

// Settings framework (Epic B) — runtime-adjustable, typed, auto-inspected,
// JSON-serializable parameters. A SettingGroup is owned by every layer with
// parameters (Node, Application, NodeGraphDisplay); a Setting<T> binds a typed
// value (observer or owned) + metadata + an optional change callback, renders
// itself via the type's drawWidget, and round-trips through nlohmann JSON.
//
// Design notes:
//  - This header is ImGui-free. The only ImGui-touching code (drawWidget + the
//    label-column chrome in SettingBase::draw + SettingGroup::draw) lives in
//    src/settings/setting.cpp; Setting<T>::onDrawWidget just calls the declared
//    drawWidget (whose signature has no ImGui types).
//  - to_json/from_json here are MEMBER virtuals (polymorphic over the type-erased
//    SettingBase). A leaf Setting<T>::to_json() returns the value, dispatching to
//    the genuine nlohmann ADL (native scalars; vector.hpp's to_json for Vec types).

#include <cstddef>
#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>

#include <imtool/common/logging.hpp>
#include <imtool/settings/setting_widgets.hpp>
#include <imtool/settings/toolkit_error.hpp>

namespace imtool {

// Type-erased base: the element SettingGroup owns and the inspector/JSON iterate.
class SettingBase {
public:
    SettingBase(std::string id, std::string label) : id_(std::move(id)), label_(std::move(label)) {}
    virtual ~SettingBase() = default;
    SettingBase(const SettingBase &)            = delete;   // owned via unique_ptr; has identity
    SettingBase &operator=(const SettingBase &) = delete;

    // Render this setting's row; returns true if it (or a descendant) changed this
    // frame. Defined in setting.cpp (the leaf label-column chrome — the one
    // ImGui-touching part of the base). SettingGroup overrides with its own header.
    virtual bool draw(float scale = 1.0f);

    [[nodiscard]] virtual nlohmann::json to_json() const = 0;
    virtual void                         from_json(const nlohmann::json &js) = 0;   // log + skip, never throws
    [[nodiscard]] virtual bool           isGroup() const { return false; }
    [[nodiscard]] virtual bool           isDefault() const = 0;
    virtual void                         resetToDefault() = 0;

    [[nodiscard]] std::string_view id()      const { return id_; }
    [[nodiscard]] std::string_view label()   const { return label_; }
    [[nodiscard]] std::string_view tooltip() const { return tooltip_; }
    SettingBase &setTooltip(std::string t) { tooltip_ = std::move(t); return *this; }

protected:
    // The typed widget hook SettingBase::draw() invokes after the label chrome.
    // Leaf Setting<T> overrides it; groups override draw() directly.
    [[nodiscard]] virtual bool onDrawWidget(float /*scale*/) { return false; }

    std::string id_;
    std::string label_;
    std::string tooltip_;
};

// One typed setting. storage_ is the lifetime category, readable from the type:
//   T*  -> OBSERVER (the value lives in the owning object; the setting binds &field)
//   T   -> OWNED    (the value lives in the setting by value)
template<Settable T>
class Setting : public SettingBase {
public:
    Setting(std::string id, std::string label, T *bound, SettingMeta<T> meta = {})
        : SettingBase(std::move(id), std::move(label)), storage_(bound), meta_(std::move(meta)) {}
    Setting(std::string id, std::string label, T value, SettingMeta<T> meta = {})
        : SettingBase(std::move(id), std::move(label)), storage_(std::move(value)), meta_(std::move(meta)) {}

    [[nodiscard]] T &value() {
        return std::holds_alternative<T *>(storage_) ? *std::get<T *>(storage_) : std::get<T>(storage_);
    }
    [[nodiscard]] const T &value() const {
        return std::holds_alternative<T *>(storage_) ? *std::get<T *>(storage_) : std::get<T>(storage_);
    }

    Setting &onChange(std::function<void(const T &)> cb) { on_change_ = std::move(cb); return *this; }
    [[nodiscard]] SettingMeta<T>       &meta()       { return meta_; }
    [[nodiscard]] const SettingMeta<T> &meta() const { return meta_; }

    [[nodiscard]] nlohmann::json to_json() const override { return value(); }   // nlohmann ADL (native / vector.hpp)
    void from_json(const nlohmann::json &js) override {
        try {
            value() = js.get<T>();
            if(on_change_) { on_change_(value()); }
        } catch(const nlohmann::json::exception &e) {
            log() << LogLevel::Warning << "Setting '" << id_ << "' load skipped: " << e.what(); log().flush();
        }
    }
    [[nodiscard]] bool isDefault() const override { return value() == meta_.default_v; }
    void resetToDefault() override {
        value() = meta_.default_v;
        if(on_change_) { on_change_(value()); }
    }

protected:
    [[nodiscard]] bool onDrawWidget(float /*scale*/) override {
        const bool changed = drawWidget(value(), meta_);
        if(changed && on_change_) { on_change_(value()); }
        return changed;
    }

private:
    std::variant<T *, T>           storage_;
    SettingMeta<T>                 meta_;
    std::function<void(const T &)> on_change_;
};

// A named, ordered, nestable group of settings — owned by every layer with params.
class SettingGroup : public SettingBase {
public:
    using SettingBase::SettingBase;

    // Bind an OBSERVER setting (value lives in the caller's field).
    template<Settable T>
    Setting<T> &add(std::string id, std::string label, T *bound, SettingMeta<T> meta = {}) {
        auto s = std::make_unique<Setting<T>>(std::move(id), std::move(label), bound, std::move(meta));
        Setting<T> &ref = *s;
        addChild(std::move(s));
        return ref;
    }
    // Add an OWNED setting (value lives in the group).
    template<Settable T>
    Setting<T> &add(std::string id, std::string label, T value, SettingMeta<T> meta = {}) {
        auto s = std::make_unique<Setting<T>>(std::move(id), std::move(label), std::move(value), std::move(meta));
        Setting<T> &ref = *s;
        addChild(std::move(s));
        return ref;
    }
    SettingGroup &addGroup(std::string id, std::string label) {
        auto g = std::make_unique<SettingGroup>(std::move(id), std::move(label));
        SettingGroup &ref = *g;
        addChild(std::move(g));
        return ref;
    }

    // Recursive typed lookup; nullptr if absent or the wrong type.
    template<typename T>
    [[nodiscard]] Setting<T> *get(std::string_view id) {
        return dynamic_cast<Setting<T> *>(find(id));
    }
    [[nodiscard]] SettingBase *find(std::string_view id) {
        if(auto it = index_.find(std::string(id)); it != index_.end()) { return children_[it->second].get(); }
        for(auto &c : children_) {
            if(c->isGroup()) {
                if(SettingBase *r = static_cast<SettingGroup *>(c.get())->find(id)) { return r; }
            }
        }
        return nullptr;
    }

    [[nodiscard]] bool        empty() const { return children_.empty(); }
    [[nodiscard]] std::size_t size()  const { return children_.size(); }
    [[nodiscard]] const std::vector<std::unique_ptr<SettingBase>> &children() const { return children_; }

    [[nodiscard]] bool isGroup() const override { return true; }
    bool draw(float scale = 1.0f) override;   // setting.cpp (CollapsingHeader + children)

    [[nodiscard]] nlohmann::json to_json() const override {
        nlohmann::json js = nlohmann::json::object();
        for(const auto &c : children_) { js[std::string(c->id())] = c->to_json(); }
        return js;   // {} when empty -> NodeGraph's omit-empty-params branch stays correct
    }
    void from_json(const nlohmann::json &js) override {
        if(!js.is_object()) {
            log() << LogLevel::Warning << "SettingGroup '" << id_ << "' load: not an object; skipped"; log().flush();
            return;
        }
        for(auto &c : children_) {
            const std::string key(c->id());
            if(js.contains(key)) { c->from_json(js.at(key)); }   // missing -> keep default; extra keys ignored
        }
    }
    [[nodiscard]] bool isDefault() const override {
        for(const auto &c : children_) { if(!c->isDefault()) { return false; } }
        return true;
    }
    void resetToDefault() override { for(auto &c : children_) { c->resetToDefault(); } }

private:
    void addChild(std::unique_ptr<SettingBase> child) {
        index_[std::string(child->id())] = children_.size();
        children_.push_back(std::move(child));
    }
    std::vector<std::unique_ptr<SettingBase>>    children_;   // ownership (mirrors Node m_inputs/m_outputs)
    std::unordered_map<std::string, std::size_t> index_;      // id -> child index (this group's direct children)
};

// File-IO boundary — std::expected per conventions (never throws across the API).
// Per-entry load still log+skips inside SettingGroup::from_json; only open/parse
// failures surface as a ToolkitError. Defined in setting.cpp.
[[nodiscard]] std::expected<void, ToolkitError> saveSettingsFile(const SettingGroup &group, const std::filesystem::path &path);
[[nodiscard]] std::expected<void, ToolkitError> loadSettingsFile(SettingGroup &group, const std::filesystem::path &path);

}  // namespace imtool

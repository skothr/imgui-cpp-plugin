#include "imtest.hpp"

#include <string>

#include <imtool/settings/setting.hpp>

// Headless coverage of the Settings framework (Epic B): value access (observer +
// owned), metadata, isDefault/resetToDefault + onChange, per-type JSON round-trip
// (native encodings), SettingGroup nesting + JSON, version-tolerance, find/get,
// and the Settable concept. draw() needs a live ImGui frame and is covered by the
// example; everything here runs frame-free (links the core lib only).

using namespace imtool;
using nlohmann::json;

namespace {
struct Unsupported { int a = 0; };   // no drawWidget overload -> must NOT be Settable
}

int main() {
    IMTEST_SUITE("settings");

    // --- Settable concept: supported types pass, an unregistered type fails ---
    static_assert(Settable<bool> && Settable<int> && Settable<float>, "scalars settable");
    static_assert(Settable<Vec2f> && Settable<Vec4f> && Settable<std::string>, "vec/string settable");
    static_assert(!Settable<Unsupported>, "no drawWidget overload -> not Settable (compile-time gate)");

    // --- observer vs owned value access ---
    {
        float field = 1.0f;
        Setting<float> obs("speed", "Speed", &field);   // observer: binds &field
        obs.value() = 5.0f;
        CHECK_NEAR(field, 5.0f, 1e-6);                   // writes through to the bound field
        field = 7.0f;
        CHECK_NEAR(obs.value(), 7.0f, 1e-6);             // reads the bound field

        Setting<int> owned("count", "Count", 42);        // owned: value lives in the setting
        CHECK(owned.value() == 42);
        owned.value() = 9;
        CHECK(owned.value() == 9);
    }

    // --- metadata is T-typed (a Vec2f bound is a Vec2f, not a widened scalar) ---
    {
        SettingMeta<Vec2f> m;
        m.default_v = Vec2f(1.0f, 2.0f);
        m.min = Vec2f(0.0f, 0.0f);
        m.max = Vec2f(10.0f, 20.0f);
        m.step = Vec2f(0.5f, 0.5f);
        CHECK_NEAR(m.max->x, 10.0f, 1e-6);
        CHECK_NEAR(m.max->y, 20.0f, 1e-6);
        Setting<Vec2f> s("pos", "Pos", Vec2f(3, 4), m);
        CHECK(s.meta().min.has_value());
        CHECK_NEAR(s.meta().min->x, 0.0f, 1e-6);
    }

    // --- isDefault / resetToDefault + onChange fires ---
    {
        int changes = 0;
        Setting<float> s("g", "Gain", 0.0f, SettingMeta<float>{ .default_v = 2.0f });
        s.onChange([&](const float &) { changes++; });
        CHECK(!s.isDefault());            // value 0 != default 2
        s.resetToDefault();
        CHECK(s.isDefault());             // now value == default
        CHECK_NEAR(s.value(), 2.0f, 1e-6);
        CHECK(changes == 1);              // resetToDefault fired onChange once
    }

    // --- per-type JSON round-trip: NATIVE encodings (not stringstream like heritage) ---
    {
        CHECK(Setting<bool>("b", "B", true).to_json().is_boolean());
        CHECK(Setting<int>("i", "I", 7).to_json().is_number_integer());
        const json jf = Setting<float>("f", "F", 1.5f).to_json();
        CHECK(jf.is_number() && !jf.is_string());          // float is a JSON number, not "1.5"
        CHECK(Setting<std::string>("s", "S", std::string("hi")).to_json().is_string());
        const json jv = Setting<Vec2f>("v", "V", Vec2f(3, 4)).to_json();
        CHECK(jv.is_array() && jv.size() == 2 && jv[0].get<float>() == 3.0f);

        // round-trip into a fresh setting restores the value
        Setting<Vec4f> src("c", "C", Vec4f(0.1f, 0.2f, 0.3f, 1.0f));
        Setting<Vec4f> dst("c", "C", Vec4f(0, 0, 0, 0));
        dst.from_json(src.to_json());
        CHECK_NEAR(dst.value().x, 0.1f, 1e-6);
        CHECK_NEAR(dst.value().w, 1.0f, 1e-6);
    }

    // --- SettingGroup: typed children + nested group; JSON round-trip ---
    {
        SettingGroup g("params", "Parameters");
        g.add<float>("freq", "Frequency", 440.0f);
        g.add<bool>("loop", "Loop", true);
        SettingGroup &sub = g.addGroup("filter", "Filter");
        sub.add<float>("cutoff", "Cutoff", 1000.0f);
        CHECK(g.size() == 3);            // freq, loop, filter
        CHECK(!g.empty());

        const json js = g.to_json();
        CHECK(js.is_object());
        CHECK(js.contains("freq") && js.contains("loop") && js.contains("filter"));
        CHECK(js["filter"].contains("cutoff"));

        // mutate, then restore from the saved json
        g.get<float>("freq")->value() = 880.0f;
        g.get<float>("cutoff")->value() = 50.0f;   // nested lookup
        g.from_json(js);
        CHECK_NEAR(g.get<float>("freq")->value(), 440.0f, 1e-4);
        CHECK_NEAR(g.get<float>("cutoff")->value(), 1000.0f, 1e-4);   // nested restored
    }

    // --- find / get typing ---
    {
        SettingGroup g("p", "P");
        g.add<int>("n", "N", 3);
        CHECK(g.find("n") != nullptr);
        CHECK(g.get<int>("n") != nullptr);
        CHECK(g.get<float>("n") == nullptr);   // wrong type -> nullptr
        CHECK(g.find("missing") == nullptr);
    }

    // --- version tolerance: missing key keeps default, extra key skipped, wrong
    //     type log+skips (no throw), non-object root skipped ---
    {
        SettingGroup g("p", "P");
        g.add<float>("a", "A", 1.0f);
        g.add<int>("b", "B", 2);
        // load a json missing "b", carrying an extra "z", and a wrong-typed "a"
        g.from_json(json::parse(R"({"a":"notafloat","z":99})"));
        CHECK_NEAR(g.get<float>("a")->value(), 1.0f, 1e-6);   // wrong-typed -> unchanged (log+skip)
        CHECK(g.get<int>("b")->value() == 2);                 // missing -> default kept
        // valid partial load applies only the present key
        g.from_json(json::parse(R"({"a":3.5})"));
        CHECK_NEAR(g.get<float>("a")->value(), 3.5f, 1e-6);
        CHECK(g.get<int>("b")->value() == 2);
        g.from_json(json::array());                           // non-object root -> skipped, no throw
        CHECK_NEAR(g.get<float>("a")->value(), 3.5f, 1e-6);
    }

    // --- empty group serializes to an empty object (NodeGraph omit-empty relies on it) ---
    {
        SettingGroup g("empty", "Empty");
        CHECK(g.empty());
        const json js = g.to_json();
        CHECK(js.is_object() && js.empty());
    }

    return imtest::report();
}

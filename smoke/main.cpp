#include <imgui.h>

#include <imtool/common/colors.hpp>
#include <imtool/common/geometry.hpp>
#include <imtool/common/imgui_ops.hpp>
#include <imtool/common/logging.hpp>
#include <imtool/common/matrix.hpp>
#include <imtool/common/paths.hpp>
#include <imtool/common/range.hpp>
#include <imtool/common/rect.hpp>
#include <imtool/common/timing.hpp>
#include <imtool/common/type_registry.hpp>
#include <imtool/common/vector.hpp>
#include <imtool/common/version.hpp>

#include <cstdio>

int main() {
    using namespace imtool;   // smoke test: no name conflicts, keeps the math readable

    Vec2f a{1.0f, 2.0f};
    Vec3f b{1.0f, 2.0f, 3.0f};
    Vec4f c{0.5f, 0.5f, 0.5f, 1.0f};
    Vec2i pi{3, 4};

    a += {0.5f, 0.5f};
    const float len = a.length();
    const float dt  = a.dot(Vec2f{1.0f, 0.0f});
    const auto nb   = b.normalized();
    const auto cz   = cross(b, {0.0f, 1.0f, 0.0f});

    ImVec2 iv(10.0f, 20.0f);
    Vec2f mixed = iv + a;
    ImVec2 back = toImVec(mixed);

    Rect2f r{a, a + Vec2f{10.0f, 10.0f}};
    const bool inside = r.fixed().contains(a + Vec2f{1.0f, 1.0f});
    const auto combined = r.combined(Vec2f{50.0f, 50.0f});
    const auto inter = r.intersection(combined);

    Mat4f m;
    auto t = Mat4f::makeTranslate({1.0f, 2.0f, 3.0f});
    auto s = Mat4f::makeScale({2.0f, 2.0f, 2.0f});
    auto p = Mat4f::makeProjection(1.0f, 1.5f, 0.1f, 100.0f);
    auto product = t ^ s;

    Rangef rng{0.0f, 1.0f};
    rng.fit(2.0f);
    const float clipped = rng.clip(5.0f);

    const auto col_red = color("red");
    const auto col_grey50 = color("grey50");

    const float alpha = 0.3f;
    const auto lerped = lerp(Vec2f{0.0f, 0.0f}, Vec2f{10.0f, 10.0f}, alpha);
    const auto polar = toPolar(Vec2f{1.0f, 1.0f});

    const auto ti = getTypeIndex<Vec3f>();
    const auto tn = getTypeName<Vec3f>();

    const auto ts = getTimestamp();
    const auto home = getHomeDir();
    const auto local = getLocalStorageDir();

    log().setPrintLevel(LogLevel::Debug);
    log() << LogLevel::Info << "smoke ok: len=" << len << " dt=" << dt;
    log().log(LogLevel::Debug, "version: %s", version_string().data());

    std::printf("imtool smoke: v%s len=%.3f rect_inside=%d tn=%s ts=%llu home=%s\n",
                version_string().data(), static_cast<double>(len),
                static_cast<int>(inside), tn.c_str(),
                static_cast<unsigned long long>(ts), home.string().c_str());

    (void)c; (void)pi; (void)nb; (void)cz; (void)back; (void)inter;
    (void)product; (void)p; (void)clipped; (void)col_red; (void)col_grey50;
    (void)lerped; (void)polar; (void)ti; (void)local; (void)m;
    return 0;
}

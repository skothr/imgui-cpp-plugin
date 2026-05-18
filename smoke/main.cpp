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
    imtool::Vec2f a(1.0f, 2.0f);
    imtool::Vec3f b(1.0f, 2.0f, 3.0f);
    imtool::Vec4f c(0.5f, 0.5f, 0.5f, 1.0f);
    imtool::Vec2i pi(3, 4);

    a += imtool::Vec2f(0.5f, 0.5f);
    const float len = a.length();
    const float dt  = a.dot(imtool::Vec2f(1.0f, 0.0f));
    const auto nb   = b.normalized();
    const auto cz   = imtool::cross(b, imtool::Vec3f(0.0f, 1.0f, 0.0f));

    ImVec2 iv(10.0f, 20.0f);
    imtool::Vec2f mixed = iv + a;
    ImVec2 back = imtool::toImVec(mixed);

    imtool::Rect2f r(a, a + imtool::Vec2f(10.0f, 10.0f));
    const bool inside = r.fixed().contains(a + imtool::Vec2f(1.0f, 1.0f));
    const auto combined = r.combined(imtool::Vec2f(50.0f, 50.0f));
    const auto inter = r.intersection(combined);

    imtool::Mat4f m;
    auto t = imtool::Mat4f::makeTranslate(imtool::Vec3f(1.0f, 2.0f, 3.0f));
    auto s = imtool::Mat4f::makeScale(imtool::Vec3f(2.0f, 2.0f, 2.0f));
    auto p = imtool::Mat4f::makeProjection(1.0f, 1.5f, 0.1f, 100.0f);
    auto product = t ^ s;

    imtool::Rangef rng(0.0f, 1.0f);
    rng.fit(2.0f);
    const float clipped = rng.clip(5.0f);

    const auto col_red = imtool::color("red");
    const auto col_grey50 = imtool::color("grey50");

    const float alpha = 0.3f;
    const auto lerped = imtool::lerp(imtool::Vec2f(0.0f, 0.0f), imtool::Vec2f(10.0f, 10.0f), alpha);
    const auto polar = imtool::toPolar(imtool::Vec2f(1.0f, 1.0f));

    const auto ti = imtool::getTypeIndex<imtool::Vec3f>();
    const auto tn = imtool::getTypeName<imtool::Vec3f>();

    const auto ts = imtool::getTimestamp();
    const auto home = imtool::getHomeDir();
    const auto local = imtool::getLocalStorageDir();

    imtool::log().setPrintLevel(imtool::LogLevel::Debug);
    imtool::log() << imtool::LogLevel::Info << "smoke ok: len=" << len << " dt=" << dt;
    imtool::log().log(imtool::LogLevel::Debug, "version: %s", imtool::version_string().data());

    std::printf("imtool smoke: v%s len=%.3f rect_inside=%d tn=%s ts=%llu home=%s\n",
                imtool::version_string().data(), static_cast<double>(len),
                static_cast<int>(inside), tn.c_str(),
                static_cast<unsigned long long>(ts), home.string().c_str());

    (void)c; (void)pi; (void)nb; (void)cz; (void)back; (void)inter;
    (void)product; (void)p; (void)clipped; (void)col_red; (void)col_grey50;
    (void)lerped; (void)polar; (void)ti; (void)local; (void)m;
    return 0;
}

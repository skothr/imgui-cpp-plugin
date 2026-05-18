#pragma once

#include <sstream>
#include <string>
#include <unordered_map>

#include <imtool/common/vector.hpp>

namespace imtool {

// X11 / CSS4 named colors. Source: https://en.wikipedia.org/wiki/X11_color_names
inline const std::unordered_map<std::string, Vec4f>& colorTable() {
    static const std::unordered_map<std::string, Vec4f> COLORS = {
        { "alice blue",          Vec4f(0.94f, 0.97f, 1.00f, 1.00f) },
        { "antique white",       Vec4f(0.98f, 0.92f, 0.84f, 1.00f) },
        { "aqua",                Vec4f(0.00f, 1.00f, 1.00f, 1.00f) },
        { "aquamarine",          Vec4f(0.50f, 1.00f, 0.83f, 1.00f) },
        { "azure",               Vec4f(0.94f, 1.00f, 1.00f, 1.00f) },
        { "beige",               Vec4f(0.96f, 0.96f, 0.86f, 1.00f) },
        { "bisque",              Vec4f(1.00f, 0.89f, 0.77f, 1.00f) },
        { "black",               Vec4f(0.00f, 0.00f, 0.00f, 1.00f) },
        { "blanched almond",     Vec4f(1.00f, 0.92f, 0.80f, 1.00f) },
        { "blue",                Vec4f(0.00f, 0.00f, 1.00f, 1.00f) },
        { "blue violet",         Vec4f(0.54f, 0.17f, 0.89f, 1.00f) },
        { "brown",               Vec4f(0.65f, 0.16f, 0.16f, 1.00f) },
        { "burlywood",           Vec4f(0.87f, 0.72f, 0.53f, 1.00f) },
        { "cadet blue",          Vec4f(0.37f, 0.62f, 0.63f, 1.00f) },
        { "chartreuse",          Vec4f(0.50f, 1.00f, 0.00f, 1.00f) },
        { "chocolate",           Vec4f(0.82f, 0.41f, 0.12f, 1.00f) },
        { "coral",               Vec4f(1.00f, 0.50f, 0.31f, 1.00f) },
        { "cornflower blue",     Vec4f(0.39f, 0.58f, 0.93f, 1.00f) },
        { "cornsilk",            Vec4f(1.00f, 0.97f, 0.86f, 1.00f) },
        { "crimson",             Vec4f(0.86f, 0.08f, 0.24f, 1.00f) },
        { "cyan",                Vec4f(0.00f, 1.00f, 1.00f, 1.00f) },
        { "dark blue",           Vec4f(0.00f, 0.00f, 0.55f, 1.00f) },
        { "dark cyan",           Vec4f(0.00f, 0.55f, 0.55f, 1.00f) },
        { "dark goldenrod",      Vec4f(0.72f, 0.53f, 0.04f, 1.00f) },
        { "dark gray",           Vec4f(0.66f, 0.66f, 0.66f, 1.00f) },
        { "dark green",          Vec4f(0.00f, 0.39f, 0.00f, 1.00f) },
        { "dark khaki",          Vec4f(0.74f, 0.72f, 0.42f, 1.00f) },
        { "dark magenta",        Vec4f(0.55f, 0.00f, 0.55f, 1.00f) },
        { "dark olive green",    Vec4f(0.33f, 0.42f, 0.18f, 1.00f) },
        { "dark orange",         Vec4f(1.00f, 0.55f, 0.00f, 1.00f) },
        { "dark orchid",         Vec4f(0.60f, 0.20f, 0.80f, 1.00f) },
        { "dark red",            Vec4f(0.55f, 0.00f, 0.00f, 1.00f) },
        { "dark salmon",         Vec4f(0.91f, 0.59f, 0.48f, 1.00f) },
        { "dark sea green",      Vec4f(0.56f, 0.74f, 0.56f, 1.00f) },
        { "dark slate blue",     Vec4f(0.28f, 0.24f, 0.55f, 1.00f) },
        { "dark slate gray",     Vec4f(0.18f, 0.31f, 0.31f, 1.00f) },
        { "dark turquoise",      Vec4f(0.00f, 0.81f, 0.82f, 1.00f) },
        { "dark violet",         Vec4f(0.58f, 0.00f, 0.83f, 1.00f) },
        { "deep pink",           Vec4f(1.00f, 0.08f, 0.58f, 1.00f) },
        { "deep sky blue",       Vec4f(0.00f, 0.75f, 1.00f, 1.00f) },
        { "dim gray",            Vec4f(0.41f, 0.41f, 0.41f, 1.00f) },
        { "dodger blue",         Vec4f(0.12f, 0.56f, 1.00f, 1.00f) },
        { "firebrick",           Vec4f(0.70f, 0.13f, 0.13f, 1.00f) },
        { "floral white",        Vec4f(1.00f, 0.98f, 0.94f, 1.00f) },
        { "forest green",        Vec4f(0.13f, 0.55f, 0.13f, 1.00f) },
        { "fuchsia",             Vec4f(1.00f, 0.00f, 1.00f, 1.00f) },
        { "gainsboro",           Vec4f(0.86f, 0.86f, 0.86f, 1.00f) },
        { "ghost white",         Vec4f(0.97f, 0.97f, 1.00f, 1.00f) },
        { "gold",                Vec4f(1.00f, 0.84f, 0.00f, 1.00f) },
        { "goldenrod",           Vec4f(0.85f, 0.65f, 0.13f, 1.00f) },
        { "gray",                Vec4f(0.75f, 0.75f, 0.75f, 1.00f) },
        { "web gray",            Vec4f(0.50f, 0.50f, 0.50f, 1.00f) },
        { "green",               Vec4f(0.00f, 1.00f, 0.00f, 1.00f) },
        { "web green",           Vec4f(0.00f, 0.50f, 0.00f, 1.00f) },
        { "green yellow",        Vec4f(0.68f, 1.00f, 0.18f, 1.00f) },
        { "honeydew",            Vec4f(0.94f, 1.00f, 0.94f, 1.00f) },
        { "hot pink",            Vec4f(1.00f, 0.41f, 0.71f, 1.00f) },
        { "indian red",          Vec4f(0.80f, 0.36f, 0.36f, 1.00f) },
        { "indigo",              Vec4f(0.29f, 0.00f, 0.51f, 1.00f) },
        { "ivory",               Vec4f(1.00f, 1.00f, 0.94f, 1.00f) },
        { "khaki",               Vec4f(0.94f, 0.90f, 0.55f, 1.00f) },
        { "lavender",            Vec4f(0.90f, 0.90f, 0.98f, 1.00f) },
        { "lavender blush",      Vec4f(1.00f, 0.94f, 0.96f, 1.00f) },
        { "lawn green",          Vec4f(0.49f, 0.99f, 0.00f, 1.00f) },
        { "lemon chiffon",       Vec4f(1.00f, 0.98f, 0.80f, 1.00f) },
        { "light blue",          Vec4f(0.68f, 0.85f, 0.90f, 1.00f) },
        { "light coral",         Vec4f(0.94f, 0.50f, 0.50f, 1.00f) },
        { "light cyan",          Vec4f(0.88f, 1.00f, 1.00f, 1.00f) },
        { "light goldenrod",     Vec4f(0.98f, 0.98f, 0.82f, 1.00f) },
        { "light gray",          Vec4f(0.83f, 0.83f, 0.83f, 1.00f) },
        { "light green",         Vec4f(0.56f, 0.93f, 0.56f, 1.00f) },
        { "light pink",          Vec4f(1.00f, 0.71f, 0.76f, 1.00f) },
        { "light salmon",        Vec4f(1.00f, 0.63f, 0.48f, 1.00f) },
        { "light sea green",     Vec4f(0.13f, 0.70f, 0.67f, 1.00f) },
        { "light sky blue",      Vec4f(0.53f, 0.81f, 0.98f, 1.00f) },
        { "light slate gray",    Vec4f(0.47f, 0.53f, 0.60f, 1.00f) },
        { "light steel blue",    Vec4f(0.69f, 0.77f, 0.87f, 1.00f) },
        { "light yellow",        Vec4f(1.00f, 1.00f, 0.88f, 1.00f) },
        { "lime",                Vec4f(0.00f, 1.00f, 0.00f, 1.00f) },
        { "lime green",          Vec4f(0.20f, 0.80f, 0.20f, 1.00f) },
        { "linen",               Vec4f(0.98f, 0.94f, 0.90f, 1.00f) },
        { "magenta",             Vec4f(1.00f, 0.00f, 1.00f, 1.00f) },
        { "maroon",              Vec4f(0.69f, 0.19f, 0.38f, 1.00f) },
        { "web maroon",          Vec4f(0.50f, 0.00f, 0.00f, 1.00f) },
        { "medium aquamarine",   Vec4f(0.40f, 0.80f, 0.67f, 1.00f) },
        { "medium blue",         Vec4f(0.00f, 0.00f, 0.80f, 1.00f) },
        { "medium orchid",       Vec4f(0.73f, 0.33f, 0.83f, 1.00f) },
        { "medium purple",       Vec4f(0.58f, 0.44f, 0.86f, 1.00f) },
        { "medium sea green",    Vec4f(0.24f, 0.70f, 0.44f, 1.00f) },
        { "medium slate blue",   Vec4f(0.48f, 0.41f, 0.93f, 1.00f) },
        { "medium spring green", Vec4f(0.00f, 0.98f, 0.60f, 1.00f) },
        { "medium turquoise",    Vec4f(0.28f, 0.82f, 0.80f, 1.00f) },
        { "medium violet red",   Vec4f(0.78f, 0.08f, 0.52f, 1.00f) },
        { "midnight blue",       Vec4f(0.10f, 0.10f, 0.44f, 1.00f) },
        { "mint cream",          Vec4f(0.96f, 1.00f, 0.98f, 1.00f) },
        { "misty rose",          Vec4f(1.00f, 0.89f, 0.88f, 1.00f) },
        { "moccasin",            Vec4f(1.00f, 0.89f, 0.71f, 1.00f) },
        { "navajo white",        Vec4f(1.00f, 0.87f, 0.68f, 1.00f) },
        { "navy blue",           Vec4f(0.00f, 0.00f, 0.50f, 1.00f) },
        { "old lace",            Vec4f(0.99f, 0.96f, 0.90f, 1.00f) },
        { "olive",               Vec4f(0.50f, 0.50f, 0.00f, 1.00f) },
        { "olive drab",          Vec4f(0.42f, 0.56f, 0.14f, 1.00f) },
        { "orange",              Vec4f(1.00f, 0.65f, 0.00f, 1.00f) },
        { "orange red",          Vec4f(1.00f, 0.27f, 0.00f, 1.00f) },
        { "orchid",              Vec4f(0.85f, 0.44f, 0.84f, 1.00f) },
        { "pale goldenrod",      Vec4f(0.93f, 0.91f, 0.67f, 1.00f) },
        { "pale green",          Vec4f(0.60f, 0.98f, 0.60f, 1.00f) },
        { "pale turquoise",      Vec4f(0.69f, 0.93f, 0.93f, 1.00f) },
        { "pale violet red",     Vec4f(0.86f, 0.44f, 0.58f, 1.00f) },
        { "papaya whip",         Vec4f(1.00f, 0.94f, 0.84f, 1.00f) },
        { "peach puff",          Vec4f(1.00f, 0.85f, 0.73f, 1.00f) },
        { "peru",                Vec4f(0.80f, 0.52f, 0.25f, 1.00f) },
        { "pink",                Vec4f(1.00f, 0.75f, 0.80f, 1.00f) },
        { "plum",                Vec4f(0.87f, 0.63f, 0.87f, 1.00f) },
        { "powder blue",         Vec4f(0.69f, 0.88f, 0.90f, 1.00f) },
        { "purple",              Vec4f(0.63f, 0.13f, 0.94f, 1.00f) },
        { "web purple",          Vec4f(0.50f, 0.00f, 0.50f, 1.00f) },
        { "rebecca purple",      Vec4f(0.40f, 0.20f, 0.60f, 1.00f) },
        { "red",                 Vec4f(1.00f, 0.00f, 0.00f, 1.00f) },
        { "rosy brown",          Vec4f(0.74f, 0.56f, 0.56f, 1.00f) },
        { "royal blue",          Vec4f(0.25f, 0.41f, 0.88f, 1.00f) },
        { "saddle brown",        Vec4f(0.55f, 0.27f, 0.07f, 1.00f) },
        { "salmon",              Vec4f(0.98f, 0.50f, 0.45f, 1.00f) },
        { "sandy brown",         Vec4f(0.96f, 0.64f, 0.38f, 1.00f) },
        { "sea green",           Vec4f(0.18f, 0.55f, 0.34f, 1.00f) },
        { "seashell",            Vec4f(1.00f, 0.96f, 0.93f, 1.00f) },
        { "sienna",              Vec4f(0.63f, 0.32f, 0.18f, 1.00f) },
        { "silver",              Vec4f(0.75f, 0.75f, 0.75f, 1.00f) },
        { "sky blue",            Vec4f(0.53f, 0.81f, 0.92f, 1.00f) },
        { "slate blue",          Vec4f(0.42f, 0.35f, 0.80f, 1.00f) },
        { "slate gray",          Vec4f(0.44f, 0.50f, 0.56f, 1.00f) },
        { "snow",                Vec4f(1.00f, 0.98f, 0.98f, 1.00f) },
        { "spring green",        Vec4f(0.00f, 1.00f, 0.50f, 1.00f) },
        { "steel blue",          Vec4f(0.27f, 0.51f, 0.71f, 1.00f) },
        { "tan",                 Vec4f(0.82f, 0.71f, 0.55f, 1.00f) },
        { "teal",                Vec4f(0.00f, 0.50f, 0.50f, 1.00f) },
        { "thistle",             Vec4f(0.85f, 0.75f, 0.85f, 1.00f) },
        { "tomato",              Vec4f(1.00f, 0.39f, 0.28f, 1.00f) },
        { "turquoise",           Vec4f(0.25f, 0.88f, 0.82f, 1.00f) },
        { "violet",              Vec4f(0.93f, 0.51f, 0.93f, 1.00f) },
        { "wheat",               Vec4f(0.96f, 0.87f, 0.70f, 1.00f) },
        { "white",               Vec4f(1.00f, 1.00f, 1.00f, 1.00f) },
        { "white smoke",         Vec4f(0.96f, 0.96f, 0.96f, 1.00f) },
        { "yellow",              Vec4f(1.00f, 1.00f, 0.00f, 1.00f) },
        { "yellow green",        Vec4f(0.60f, 0.80f, 0.20f, 1.00f) },
    };
    return COLORS;
}

[[nodiscard]] inline Vec4f greyValue(int val) {
    const float v = val / 255.0f;
    return Vec4f(v, v, v, 1.0f);
}

[[nodiscard]] inline Vec4f color(const std::string &name) {
    const auto &table = colorTable();
    const auto iter = table.find(name);
    if(iter != table.end()) { return iter->second; }

    auto greyp = name.find("grey");
    if(greyp == std::string::npos) { greyp = name.find("gray"); }
    if(greyp != std::string::npos) {
        std::istringstream ss(name.substr(greyp + 4));
        int val;
        if(ss >> val) { return greyValue(val); }
    }
    return Vec4f(0.0f, 0.0f, 0.0f, 1.0f);
}

}

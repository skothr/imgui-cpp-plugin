#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>

#include <imtool/common/matrix.hpp>
#include <imtool/common/vector.hpp>

namespace imtool {

// constexpr string_view: one definition across TUs (a namespace-scope std::string
// gives every translation unit its own copy).
inline constexpr std::string_view BAD_TYPE_NAME = "<badtype>";

inline std::unordered_map<std::type_index, std::string>& type_names() {
    static std::unordered_map<std::type_index, std::string> map = {
        { std::type_index(typeid(char)),            "char"     },
        { std::type_index(typeid(short)),           "short"    },
        { std::type_index(typeid(int)),             "int"      },
        { std::type_index(typeid(long int)),        "long"     },
        { std::type_index(typeid(unsigned char)),   "uchar"    },
        { std::type_index(typeid(unsigned short)),  "ushort"   },
        { std::type_index(typeid(unsigned int)),    "uint"     },
        { std::type_index(typeid(unsigned long)),   "ulong"    },
        { std::type_index(typeid(float)),           "float"    },
        { std::type_index(typeid(double)),          "double"   },
        { std::type_index(typeid(int8_t)),          "int8"     },
        { std::type_index(typeid(int16_t)),         "int16"    },
        { std::type_index(typeid(int32_t)),         "int32"    },
        { std::type_index(typeid(int64_t)),         "int64"    },
        { std::type_index(typeid(uint8_t)),         "uint8"    },
        { std::type_index(typeid(uint16_t)),        "uint16"   },
        { std::type_index(typeid(uint32_t)),        "uint32"   },
        { std::type_index(typeid(uint64_t)),        "uint64"   },
        { std::type_index(typeid(Vec2i)),           "Vec2i"    },
        { std::type_index(typeid(Vec3i)),           "Vec3i"    },
        { std::type_index(typeid(Vec4i)),           "Vec4i"    },
        { std::type_index(typeid(Vec2ui)),          "Vec2ui"   },
        { std::type_index(typeid(Vec3ui)),          "Vec3ui"   },
        { std::type_index(typeid(Vec4ui)),          "Vec4ui"   },
        { std::type_index(typeid(Vec2f)),           "Vec2f"    },
        { std::type_index(typeid(Vec3f)),           "Vec3f"    },
        { std::type_index(typeid(Vec4f)),           "Vec4f"    },
        { std::type_index(typeid(Vec2d)),           "Vec2d"    },
        { std::type_index(typeid(Vec3d)),           "Vec3d"    },
        { std::type_index(typeid(Vec4d)),           "Vec4d"    },
        { std::type_index(typeid(Mat2i)),           "Mat2i"    },
        { std::type_index(typeid(Mat3i)),           "Mat3i"    },
        { std::type_index(typeid(Mat4i)),           "Mat4i"    },
        { std::type_index(typeid(Mat2ui)),          "Mat2ui"   },
        { std::type_index(typeid(Mat3ui)),          "Mat3ui"   },
        { std::type_index(typeid(Mat4ui)),          "Mat4ui"   },
        { std::type_index(typeid(Mat2f)),           "Mat2f"    },
        { std::type_index(typeid(Mat3f)),           "Mat3f"    },
        { std::type_index(typeid(Mat4f)),           "Mat4f"    },
        { std::type_index(typeid(Mat2d)),           "Mat2d"    },
        { std::type_index(typeid(Mat3d)),           "Mat3d"    },
        { std::type_index(typeid(Mat4d)),           "Mat4d"    },
        { std::type_index(typeid(bool)),            "bool"     },
        { std::type_index(typeid(std::string)),     "string"   },
    };
    return map;
}

inline std::unordered_map<std::type_index, int>& type_nargs() {
    static std::unordered_map<std::type_index, int> map = {
        { std::type_index(typeid(char)),           1 },
        { std::type_index(typeid(short)),          1 },
        { std::type_index(typeid(int)),            1 },
        { std::type_index(typeid(long int)),       1 },
        { std::type_index(typeid(unsigned char)),  1 },
        { std::type_index(typeid(unsigned short)), 1 },
        { std::type_index(typeid(unsigned int)),   1 },
        { std::type_index(typeid(unsigned long)),  1 },
        { std::type_index(typeid(float)),          1 },
        { std::type_index(typeid(double)),         1 },
        { std::type_index(typeid(int8_t)),         1 },
        { std::type_index(typeid(int16_t)),        1 },
        { std::type_index(typeid(int32_t)),        1 },
        { std::type_index(typeid(int64_t)),        1 },
        { std::type_index(typeid(uint8_t)),        1 },
        { std::type_index(typeid(uint16_t)),       1 },
        { std::type_index(typeid(uint32_t)),       1 },
        { std::type_index(typeid(uint64_t)),       1 },
        { std::type_index(typeid(Vec2i)),          2 },
        { std::type_index(typeid(Vec3i)),          3 },
        { std::type_index(typeid(Vec4i)),          4 },
        { std::type_index(typeid(Vec2ui)),         2 },
        { std::type_index(typeid(Vec3ui)),         3 },
        { std::type_index(typeid(Vec4ui)),         4 },
        { std::type_index(typeid(Vec2f)),          2 },
        { std::type_index(typeid(Vec3f)),          3 },
        { std::type_index(typeid(Vec4f)),          4 },
        { std::type_index(typeid(Vec2d)),          2 },
        { std::type_index(typeid(Vec3d)),          3 },
        { std::type_index(typeid(Vec4d)),          4 },
        { std::type_index(typeid(Mat2i)),          4  },
        { std::type_index(typeid(Mat3i)),          9  },
        { std::type_index(typeid(Mat4i)),          16 },
        { std::type_index(typeid(Mat2ui)),         4  },
        { std::type_index(typeid(Mat3ui)),         9  },
        { std::type_index(typeid(Mat4ui)),         16 },
        { std::type_index(typeid(Mat2f)),          4  },
        { std::type_index(typeid(Mat3f)),          9  },
        { std::type_index(typeid(Mat4f)),          16 },
        { std::type_index(typeid(Mat2d)),          4  },
        { std::type_index(typeid(Mat3d)),          9  },
        { std::type_index(typeid(Mat4d)),          16 },
        { std::type_index(typeid(bool)),           0 },
        { std::type_index(typeid(std::string)),    1 },
    };
    return map;
}

template<typename T>
[[nodiscard]] inline std::type_index getTypeIndex() { return std::type_index(typeid(T)); }

// Runtime lookup by type_index (non-template — the previous template<T> form
// took T it never used and so was uncallable without an explicit, ignored T).
[[nodiscard]] inline std::string getTypeName(std::type_index index) {
    auto iter = type_names().find(index);
    return (iter != type_names().end() ? iter->second : index.name());
}

template<typename T>
[[nodiscard]] inline std::string getTypeName() {
    const auto &tid = typeid(T);
    auto iter = type_names().find(std::type_index(tid));
    return (iter != type_names().end() ? iter->second : tid.name());
}

template<typename T>
[[nodiscard]] inline int getTypeNumArgs() {
    // typeid(T) needs no object operand — avoids requiring T be default-constructible.
    auto iter = type_nargs().find(std::type_index(typeid(T)));
    return (iter != type_nargs().end() ? iter->second : 0);
}

}

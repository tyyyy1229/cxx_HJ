
#pragma once

// ===================== 新增开始 =====================
#include <ostream>
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

// 为 glm 所有常用向量类型实现流式输出重载
namespace glm {
inline std::ostream& operator<<(std::ostream& os, const dvec2& vec) {
    os << vec.x << " " << vec.y;
    return os;
}
inline std::ostream& operator<<(std::ostream& os, const vec2& vec) {
    os << vec.x << " " << vec.y;
    return os;
}
inline std::ostream& operator<<(std::ostream& os, const dvec3& vec) {
    os << vec.x << " " << vec.y << " " << vec.z;
    return os;
}
inline std::ostream& operator<<(std::ostream& os, const vec3& vec) {
    os << vec.x << " " << vec.y << " " << vec.z;
    return os;
}
inline std::ostream& operator<<(std::ostream& os, const dvec4& vec) {
    os << vec.x << " " << vec.y << " " << vec.z << " " << vec.w;
    return os;
}
inline std::ostream& operator<<(std::ostream& os, const vec4& vec) {
    os << vec.x << " " << vec.y << " " << vec.z << " " << vec.w;
    return os;
}
} // namespace glm
// ===================== 新增结束 =====================

#ifndef _BHC_INCLUDING_COMPONENTS_
#error "Must be included from common.hpp!"
#endif


namespace bhc {

struct bhcInternal;

class PrintFileEmu {
public:
    PrintFileEmu(
        bhcInternal *internal, const std::string &FileRoot,
        void (*prtCallback)(const char *message))
        : callback(nullptr)
    {
        if(prtCallback == nullptr) {
            ofs.open(FileRoot + ".prt");
            if(!ofs.good()) {
                ExternalError(
                    internal, "Could not open print file: %s.prt", FileRoot.c_str());
            }
            ofs << std::unitbuf;
        } else {
            callback = prtCallback;
        }
    }
    ~PrintFileEmu()
    {
        if(ofs.is_open()) ofs.close();
    }

    template<typename T> PrintFileEmu &operator<<(const T &x)
    {
        if(callback != nullptr) {
            linebuf << x;
            if(linebuf.str().length() > 0) {
                callback(linebuf.str().c_str());
                linebuf.str("");
            }
        } else if(ofs.good()) {
            ofs << x;
        }
        return *this;
    }

private:
    std::ofstream ofs;
    std::stringstream linebuf;
    void (*callback)(const char *message);
};

} // namespace bhc

#include "scaling.hpp"

namespace {

using namespace mbgl;

template <size_t i>
inline const uint8_t& b(const uint32_t& w) {
    return reinterpret_cast<const uint8_t*>(&w)[i];
}

template <size_t i>
inline uint8_t& b(uint32_t& w) {
    return reinterpret_cast<uint8_t*>(&w)[i];
}

vec2<double> getFactor(const Rect<uint32_t>& srcPos, const Rect<uint32_t>& dstPos) {
    return {
        double(srcPos.w) / dstPos.w,
        double(srcPos.h) / dstPos.h
    };
}

vec2<uint32_t> getBounds(const vec2<uint32_t>& srcSize, const Rect<uint32_t>& srcPos,
                         const vec2<uint32_t>& dstSize, const Rect<uint32_t>& dstPos,
                         const vec2<double>& factor) {
    if (srcPos.x > srcSize.x || srcPos.y > srcSize.y ||
        dstPos.x > dstSize.x || dstPos.y > dstSize.y) {
        // Source or destination position is out of range.
        return { 0, 0 };
    }

    // Make sure we don't read/write values out of range.
    return { std::min(uint32_t(double(srcSize.x - srcPos.x) / factor.x),
                      std::min(dstSize.x - dstPos.x, dstPos.w)),
             std::min(uint32_t(double(srcSize.y - srcPos.y) / factor.y),
                      std::min(dstSize.y - dstPos.y, dstPos.h)) };
}
} // namespace

namespace mbgl {
namespace util {

void nearestNeighborScale(const uint32_t* srcData, const vec2<uint32_t>& srcSize,
                          const Rect<uint32_t>& srcPos, uint32_t* dstData,
                          const vec2<uint32_t>& dstSize, const Rect<uint32_t>& dstPos) {
    const auto factor = getFactor(srcPos, dstPos);
    const auto bounds = getBounds(srcSize, srcPos, dstSize, dstPos, factor);

    double fractSrcY = srcPos.y;
    double fractSrcX;
    size_t i = dstSize.x * dstPos.y + dstPos.x;
    uint32_t srcY;
    uint32_t x, y;
    for (y = 0; y < bounds.y; y++) {
        fractSrcX = srcPos.x;
        srcY = srcSize.x * uint32_t(fractSrcY);
        for (x = 0; x < bounds.x; x++) {
            dstData[i + x] = srcData[srcY + uint32_t(fractSrcX)];
            fractSrcX += factor.x;
        }
        i += dstSize.x;
        fractSrcY += factor.y;
    }
}

} // namespace util
} // namespace mbgl

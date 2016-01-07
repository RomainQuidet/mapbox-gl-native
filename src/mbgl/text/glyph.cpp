#include <mbgl/text/glyph.hpp>

namespace mbgl {

// Note: this only works for the BMP
GlyphRange getGlyphRange(char32_t glyph) {
    unsigned start = (glyph/256) * 256;
    unsigned end = (start + 255);
    if (start > 65280) start = 65280;
    if (end > 65535) end = 65535;
    return { start, end };
}

SDFGlyph& SDFGlyph::operator=(const SDFGlyph& rhs) {
    id = rhs.id;
    bitmap = rhs.bitmap;
    metrics = rhs.metrics;
    return *this;
}

} // namespace mbgl

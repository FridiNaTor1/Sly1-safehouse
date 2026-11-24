// The rhythm minigame's notes drawn as the buttons of the device in use. See rhythmicons.h.
// (CANE_SAFEHOUSE builds only: the code uses SafeHouse's shared platform layer)
#if defined(CANE_SAFEHOUSE)
#include "rhythmicons.h"
#include "actionbind.h"
#include "prompt_glyphs.h"
#include "../alo.h"
#include "../gl.h"
#include "../glob.h"
#include "../render.h"
#include "../ruby.h"
#include "../rwm.h"
#include "../rythm.h"
#include "../shd.h"
#include "../sw.h"
#include "../emitter.h"
#include "../blip.h"
#include <safehouse/button_prompts.h>
#include <safehouse/controller_family.h>
#include <safehouse/system_settings.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <map>
#include <tuple>
#include <vector>

namespace
{
using cane::input::Button;
using cane::input::Family;

// The notes in Mz. Ruby's weapon order (ruby.cpp aprwmAttack, rythm.cpp's button order).
constexpr int kcnote = 4;
constexpr JOY_BUTTON s_abtnNote[kcnote] = { BTN_SQUARE, BTN_TRIANGLE, BTN_CIRCLE, BTN_CROSS };
// The original notes' neon colours (their strip textures: the saturated centre, the lighter edges), by weapon.
struct NEON { glm::vec3 centre, edge; };
const NEON s_aneonNote[kcnote] = {
    { { 255, 24, 255 }, { 255, 140, 255 } },    // Square: pink
    { { 24, 206, 90 }, { 130, 245, 160 } },     // Triangle: green
    { { 231, 50, 74 }, { 245, 140, 155 } },     // Circle: red
    { { 150, 75, 239 }, { 195, 150, 248 } },    // Cross: blue
};
// The trails' blips show their texture turned half a turn, but the circle's (its note lies turned a quarter turn).
constexpr bool s_afTrailTurned[kcnote] = { true, true, false, true };
// A button's position as a weapon (South Cross, East Circle, West Square, North Triangle), -1 off the face.
int WeaponOfButton(Button b)
{
    switch (b)
    {
    case Button::West: return 0;
    case Button::North: return 1;
    case Button::East: return 2;
    case Button::South: return 3;
    default: return -1;
    }
}

// The icon's size on the note (the original shapes span about 72 to 83 units) and in its texture.
constexpr float kExtentIcon = 80.0f;
constexpr float kFillTexture = 0.86f;
constexpr int kcpxTexture = 256;

struct RHNOTE
{
    GLuint vao = 0;          // the original glob's (every clone of the ammo shares it)
    GLOB globOrig;           // a copy of the original (a remapped PlayStation note draws another note's)
    GLOB globIcon;           // the quad
    // the shape's cel border: a glob of its own (edges only) beside the surface's, shared by the clones; the icon has
    // none (its border is in its texture), a remapped PlayStation note takes the other note's
    GLuint edgeCel = 0;
    GLOB globCelOrig, globCelNone;
    // the trail: the note's emitter's blips (sprites of the shape's outline, BLIPG::pshd, a shader of their own); its
    // texture is swapped for the glyph's trail texture each frame, and back
    RWM* prwm = nullptr;
    SHD* pshdBlip = nullptr;
    std::vector<GLuint> aglBlipOrig;
    SHD shdIcon, shdSide;    // the faces' texture; the inner layers' (the extrusion's sides)
    GLuint vaoQuad = 0, vbo = 0, ebo = 0;
    GLOB* pglobDraw = nullptr;   // this frame (nullptr: the original)
};

struct STATE
{
    SW* psw = nullptr;
    bool fBuilt = false;
    int cframe = -1;
    RYTHM* prythm = nullptr;
    std::array<RHNOTE, kcnote> anote;
    std::map<std::tuple<int, int, int>, GLuint> mptex;   // (family or -1 keyboard, button or key, colour) -> texture
    int iatlasDecoded = -1;
    cane::input::embedded::Image imageAtlas;
};
STATE s_state;

void ReleaseAtlas()
{
    if (s_state.imageAtlas.rgba != nullptr)
        cane::input::embedded::freeImage(s_state.imageAtlas);
    s_state.imageAtlas = {};
    s_state.iatlasDecoded = -1;
}

void Reset()
{
    for (RHNOTE& note : s_state.anote)
    {
        if (note.vaoQuad != 0)
            glDeleteVertexArrays(1, &note.vaoQuad);
        if (note.vbo != 0)
            glDeleteBuffers(1, &note.vbo);
        if (note.ebo != 0)
            glDeleteBuffers(1, &note.ebo);
        note = RHNOTE{};
    }
    for (auto& entry : s_state.mptex)
        if (entry.second != 0)
            glDeleteTextures(1, &entry.second);
    s_state.mptex.clear();
    ReleaseAtlas();
    s_state.fBuilt = false;
    s_state.psw = nullptr;
    s_state.prythm = nullptr;
    s_state.cframe = -1;
}

// ---- the icon textures

const cane::input::embedded::Image* PimageAtlas(int iatlas)
{
    if (s_state.iatlasDecoded != iatlas)
    {
        ReleaseAtlas();
        s_state.imageAtlas = cane::input::embedded::decodePromptAtlas(iatlas);
        if (s_state.imageAtlas.rgba == nullptr)
            return nullptr;
        s_state.iatlasDecoded = iatlas;
    }
    return &s_state.imageAtlas;
}

// One line of the squared Euclidean distance transform (Felzenszwalb and Huttenlocher): f 0 on the sources, "infinite"
// elsewhere; d the squared distance to the nearest source.
void Edt1(const float* f, float* d, int n, std::vector<int>& v, std::vector<float>& z)
{
    constexpr float kInf = 1e20f;
    auto s = [&](int q, int p) {
        return ((f[q] + float(q) * float(q)) - (f[p] + float(p) * float(p))) / (2.0f * float(q - p));
    };
    int k = 0;
    v[0] = 0;
    z[0] = -kInf;
    z[1] = kInf;
    for (int q = 1; q < n; ++q)
    {
        float sq = s(q, v[size_t(k)]);
        while (sq <= z[size_t(k)])
            sq = s(q, v[size_t(--k)]);
        ++k;
        v[size_t(k)] = q;
        z[size_t(k)] = sq;
        z[size_t(k) + 1] = kInf;
    }
    k = 0;
    for (int q = 0; q < n; ++q)
    {
        while (z[size_t(k) + 1] < float(q))
            ++k;
        const float dq = float(q - v[size_t(k)]);
        d[q] = dq * dq + f[v[size_t(k)]];
    }
}

// The distance (pixels) from each pixel to the nearest pixel where mask == fTo.
std::vector<float> Distance(const std::vector<bool>& mask, bool fTo, int n)
{
    std::vector<float> grid(size_t(n) * size_t(n)), f(static_cast<size_t>(n)), d(static_cast<size_t>(n)), z(static_cast<size_t>(n) + 1);
    std::vector<int> v(static_cast<size_t>(n));
    for (size_t i = 0; i < grid.size(); ++i)
        grid[i] = mask[i] == fTo ? 0.0f : 1e20f;
    for (int x = 0; x < n; ++x)
    {
        for (int y = 0; y < n; ++y)
            f[size_t(y)] = grid[size_t(y) * n + x];
        Edt1(f.data(), d.data(), n, v, z);
        for (int y = 0; y < n; ++y)
            grid[size_t(y) * n + x] = d[size_t(y)];
    }
    for (int y = 0; y < n; ++y)
    {
        Edt1(&grid[size_t(y) * n], d.data(), n, v, z);
        for (int x = 0; x < n; ++x)
            grid[size_t(y) * n + x] = std::sqrt(d[size_t(x)]);
    }
    return grid;
}

float Luma(const glm::vec3& rgb) { return (0.299f * rgb.r + 0.587f * rgb.g + 0.114f * rgb.b) / 255.0f; }

// A prompt icon (an atlas cell: a disc or keycap with a dark glyph) redrawn as the notes are drawn: only its glyph (the
// letter, the word, the shape), big, as a neon tube of the original notes' weight with a dark border; the disc or
// keycap itself is left out. neon: the colours when the icon has none of its own. texk: the faces; the extrusion's
// inner layers (the tube's sides: the same shape in a darker, flat shade); the trail's sprites (the original trail
// texture's look: the shape's neon with a soft dark rim in its own colour instead of the black border).
enum TEXK { TEXK_Face, TEXK_Side, TEXK_Trail, TEXK_TrailTurned, TEXK_Max };
std::vector<uint8_t> NeonIcon(const cane::input::embedded::Image& atlas, const cane::input::PromptRect& rect, const NEON& neon,
                              TEXK texk)
{
    const int n = kcpxTexture;
    auto src = [&](int x, int y) -> const uint8_t* {
        x = std::clamp(x, rect.x, rect.x + rect.w - 1);
        y = std::clamp(y, rect.y, rect.y + rect.h - 1);
        return atlas.rgba + (size_t(y) * size_t(atlas.width) + size_t(x)) * 4;
    };
    // the glyph: the icon's holes (the letter or shape cut out of the disc or keycap: clear pixels the cell's edge
    // cannot reach through clear pixels), as coverage 0..1; the body's colour: its opaque pixels' mean
    const int w = rect.w, hgt = rect.h;
    std::vector<uint8_t> outside(size_t(w) * size_t(hgt), 0);
    std::vector<int> stack;
    auto push = [&](int x, int y) {
        if (x < 0 || y < 0 || x >= w || y >= hgt || outside[size_t(y) * w + x] || src(rect.x + x, rect.y + y)[3] > 127)
            return;
        outside[size_t(y) * w + x] = 1;
        stack.push_back(y * w + x);
    };
    for (int x = 0; x < w; ++x)
        push(x, 0), push(x, hgt - 1);
    for (int y = 0; y < hgt; ++y)
        push(0, y), push(w - 1, y);
    while (!stack.empty())
    {
        const int i = stack.back();
        stack.pop_back();
        const int x = i % w, y = i / w;
        push(x + 1, y), push(x - 1, y), push(x, y + 1), push(x, y - 1);
    }
    std::vector<float> cover(size_t(w) * size_t(hgt), 0.0f);
    glm::vec3 colourBody(0.0f);
    int cBody = 0;
    int bx0 = w, by0 = hgt, bx1 = -1, by1 = -1;
    for (int y = 0; y < hgt; ++y)
        for (int x = 0; x < w; ++x)
        {
            const uint8_t* p = src(rect.x + x, rect.y + y);
            if (p[3] > 127)
                colourBody += glm::vec3(p[0], p[1], p[2]), ++cBody;
            if (outside[size_t(y) * w + x])
                continue;
            const float c = 1.0f - float(p[3]) / 255.0f;
            cover[size_t(y) * w + x] = c;
            if (c > 0.5f)
                bx0 = std::min(bx0, x), by0 = std::min(by0, y), bx1 = std::max(bx1, x), by1 = std::max(by1, y);
        }
    if (bx1 < bx0 || cBody == 0)
        return {};
    colourBody /= float(cBody);
    // the icon's own colour when it has one (Xbox, GameCube), else the original note's
    const float mx = std::max({ colourBody.r, colourBody.g, colourBody.b }), mn = std::min({ colourBody.r, colourBody.g, colourBody.b });
    NEON colours = neon;
    if (mx > 1.0f && (mx - mn) / mx > 0.3f)
        colours = { colourBody, glm::mix(colourBody, glm::vec3(255.0f), 0.5f) };
    // the glyph's box, centred and scaled to kFillTexture of the texture; bilinear coverage
    const float cx = 0.5f * float(bx0 + bx1 + 1), cy = 0.5f * float(by0 + by1 + 1);
    const float scale = float(std::max(bx1 - bx0 + 1, by1 - by0 + 1)) / (kFillTexture * float(n));
    auto coverAt = [&](int x, int y) {
        return x < 0 || y < 0 || x >= w || y >= hgt ? 0.0f : cover[size_t(y) * w + x];
    };
    std::vector<bool> glyph(size_t(n) * size_t(n));
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
        {
            const float sx = cx + (float(x) + 0.5f - 0.5f * float(n)) * scale - 0.5f;
            const float sy = cy + (float(y) + 0.5f - 0.5f * float(n)) * scale - 0.5f;
            const int x0 = int(std::floor(sx)), y0 = int(std::floor(sy));
            const float fx = sx - float(x0), fy = sy - float(y0);
            const float c = (1.0f - fy) * ((1.0f - fx) * coverAt(x0, y0) + fx * coverAt(x0 + 1, y0)) +
                            fy * ((1.0f - fx) * coverAt(x0, y0 + 1) + fx * coverAt(x0 + 1, y0 + 1));
            glyph[size_t(y) * n + x] = c > 0.5f;
        }
    // the strokes thinned to the original tubes' weight (thin ones, e.g. a word's letters, stay as they are)
    const std::vector<float> dGlyph = Distance(glyph, false, n);
    std::vector<float> ad;
    for (size_t i = 0; i < glyph.size(); ++i)
        if (glyph[i])
            ad.push_back(dGlyph[i]);
    if (ad.empty())
        return {};
    std::sort(ad.begin(), ad.end());
    const float hwGlyph = ad[ad.size() * 95 / 100];
    const float hwTube = float(n) * 0.045f;
    const float erode = std::max(0.0f, hwGlyph - hwTube);
    const float hw = std::max(2.0f, hwGlyph - erode);
    std::vector<bool> stroke(glyph.size());
    for (size_t i = 0; i < glyph.size(); ++i)
        stroke[i] = glyph[i] && dGlyph[i] > erode;
    const std::vector<float> dIn = Distance(stroke, false, n), dOut = Distance(stroke, true, n);
    const float wBorder = float(n) * 0.02f;
    const bool fTrail = texk == TEXK_Trail || texk == TEXK_TrailTurned;
    const glm::vec3 black = fTrail ? colours.centre * 0.35f : glm::vec3(10.0f);
    std::vector<uint8_t> out(size_t(n) * size_t(n) * 4, 0);
    for (size_t i = 0; i < stroke.size(); ++i)
    {
        const float sd = stroke[i] ? dIn[i] - 0.5f : 0.5f - dOut[i];   // (signed: + inside a stroke)
        float a = std::clamp(sd + wBorder + 0.5f, 0.0f, 1.0f);
        if (fTrail && sd < 0.0f)
            a *= 0.6f;
        if (a <= 0.0f)
            continue;
        const float p = std::clamp(sd / hw, 0.0f, 1.0f);
        const float u = p * p * (3.0f - 2.0f * p);
        const glm::vec3 neonCol = texk == TEXK_Side ? colours.centre * 0.55f
                                        : glm::mix(colours.edge, colours.centre, std::clamp((u - 0.1f) / 0.75f, 0.0f, 1.0f));
        const glm::vec3 c = glm::mix(black, neonCol, std::clamp(sd + 0.5f, 0.0f, 1.0f));
        out[i * 4 + 0] = uint8_t(std::clamp(c.r, 0.0f, 255.0f));
        out[i * 4 + 1] = uint8_t(std::clamp(c.g, 0.0f, 255.0f));
        out[i * 4 + 2] = uint8_t(std::clamp(c.b, 0.0f, 255.0f));
        out[i * 4 + 3] = uint8_t(a * 255.0f + 0.5f);
    }
    // (TEXK_TrailTurned: turned half a turn, for the trails whose blips show their texture so; the level's own trail
    // textures are symmetric shapes)
    if (texk == TEXK_TrailTurned)
        for (size_t i = 0, j = size_t(n) * size_t(n) - 1; i < j; ++i, --j)
            for (int c = 0; c < 4; ++c)
                std::swap(out[i * 4 + size_t(c)], out[j * 4 + size_t(c)]);
    return out;
}

GLuint UploadTexture(const std::vector<uint8_t>& rgba)
{
    const int n = kcpxTexture;
    int levels = 1;
    for (int d = n; d > 1; d >>= 1)
        ++levels;
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexStorage2D(GL_TEXTURE_2D, levels, GL_RGBA8, n, n);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, n, n, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    return texture;
}

// The texture of a button (family >= 0) or key (family -1) in the colours of weapon icolour, of kind texk (NeonIcon);
// 0 when there is no icon.
GLuint Texture(int family, int buttonOrKey, int icolour, TEXK texk)
{
    const auto id = std::make_tuple(family, buttonOrKey, icolour + int(texk) * kcnote);
    if (const auto it = s_state.mptex.find(id); it != s_state.mptex.end())
        return it->second;
    namespace pa = cane::input::prompt_atlas;
    cane::input::PromptRect rect{ 0, 0, 0, 0 };
    int iatlas = family;
    if (family < 0)
    {
        iatlas = int(Family::Keyboard);
        for (const cane::input::PromptKeyRect& key : pa::kPromptKeyRects)
            if (key.key == cane::input::promptKeyForBinding(buttonOrKey))
                rect = key.rect;
    }
    else if (family < cane::input::kFamilyCount && buttonOrKey >= 0 && buttonOrKey < cane::input::kButtonCount)
        rect = pa::kPromptRects[family][buttonOrKey];
    GLuint texture = 0;
    if (rect.w > 0 && rect.h > 0)
        if (const cane::input::embedded::Image* pimage = PimageAtlas(iatlas))
        {
            const std::vector<uint8_t> rgba = NeonIcon(*pimage, rect, s_aneonNote[icolour], texk);
            if (!rgba.empty())
                texture = UploadTexture(rgba);
        }
    s_state.mptex[id] = texture;
    return texture;
}

// ---- the notes and their quads

// The note of one of Mz. Ruby's RWMs: the first ALO under its ammo with one textured subglob.
ALO* PaloNote(ALO* palo, int depth)
{
    if (palo == nullptr || depth > 4)
        return nullptr;
    if (!palo->globset.aglob.empty() && palo->globset.aglob[0].asubglob.size() == 1)
    {
        const SUBGLOB& sub = palo->globset.aglob[0].asubglob[0];
        if (sub.pshd != nullptr && !sub.pshd->atex.empty() && !sub.vertices.empty())
            return palo;
    }
    for (ALO* pchild = palo->dlChild.paloFirst; pchild != nullptr; pchild = pchild->dleChild.paloNext)
        if (ALO* pnote = PaloNote(pchild, depth + 1))
            return pnote;
    return nullptr;
}

struct PLANE
{
    glm::vec3 centre{ 0.0f }, normal{ 0.0f };   // (the normal: the side the shape's vertex normals face)
    float depth = 0.0f;                          // the shape's thickness along it
};

PLANE PlaneOf(const SUBGLOB& sub)
{
    glm::vec3 lo(std::numeric_limits<float>::max()), hi(-std::numeric_limits<float>::max()), nsum(0.0f);
    for (const VERTICE& v : sub.vertices)
    {
        lo = glm::min(lo, v.pos);
        hi = glm::max(hi, v.pos);
        nsum += v.normal;
    }
    const glm::vec3 e = hi - lo;
    const int axis = e.x <= e.y && e.x <= e.z ? 0 : e.y <= e.z ? 1 : 2;
    PLANE plane;
    plane.centre = 0.5f * (lo + hi);
    plane.normal[axis] = nsum[axis] < 0.0f ? -1.0f : 1.0f;
    plane.depth = e[axis];
    return plane;
}

void BuildQuad(RHNOTE& note, const PLANE& plane, glm::vec3 up)
{
    const glm::vec3 n = plane.normal;
    up -= n * glm::dot(up, n);
    if (glm::length(up) < 0.3f)
    {
        // (no direction from the triangle: an axis of the plane)
        up = n.x != 0.0f ? glm::vec3(0, 0, 1) : glm::vec3(1, 0, 0);
        up -= n * glm::dot(up, n);
    }
    up = glm::normalize(up);
    const float h = 0.5f * kExtentIcon / kFillTexture;
    const VERTICE& v0 = note.globOrig.asubglob[0].vertices[0];
    std::vector<VERTICE> av;
    std::vector<uint32_t> ai;
    // The glyph extruded to the original shape's thickness: its two faces (each upright and unmirrored seen from its
    // side; the world is drawn with back faces culled) and, between them, layers of the darker side texture, both
    // ways, close enough that seen at an angle they read as the tube's sides.
    auto quad = [&](float offset, float side) {
        const glm::vec3 normal = n * side, right = glm::cross(up, normal);
        const uint32_t base = uint32_t(av.size());
        static const glm::vec2 s_acorner[4] = { { -1, 1 }, { -1, -1 }, { 1, -1 }, { 1, 1 } };   // TL BL BR TR
        for (const glm::vec2& c : s_acorner)
        {
            VERTICE v = v0;
            v.pos = plane.centre + n * offset + right * (c.x * h) + up * (c.y * h);
            v.normal = normal;
            v.uv = glm::vec2(0.5f * (c.x + 1.0f), 0.5f * (1.0f - c.y));
            av.push_back(v);
        }
        for (const uint32_t i : { 0u, 1u, 2u, 0u, 2u, 3u })
            ai.push_back(base + i);
    };
    const float half = 0.5f * std::max(plane.depth, 2.0f);
    quad(half, 1.0f);
    quad(-half, -1.0f);
    const uint32_t cindexFaces = uint32_t(ai.size());
    constexpr int kclayer = 6;
    for (int k = 1; k < kclayer; ++k)
    {
        const float offset = -half + 2.0f * half * float(k) / float(kclayer);
        quad(offset, 1.0f);
        quad(offset, -1.0f);
    }

    glGenVertexArrays(1, &note.vaoQuad);
    glBindVertexArray(note.vaoQuad);
    glGenBuffers(1, &note.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, note.vbo);
    glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(av.size() * sizeof(VERTICE)), av.data(), GL_STATIC_DRAW);
    glGenBuffers(1, &note.ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, note.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, GLsizeiptr(ai.size() * sizeof(uint32_t)), ai.data(), GL_STATIC_DRAW);
    // (glob.cpp's layout)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VERTICE), (void*)offsetof(VERTICE, pos));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VERTICE), (void*)offsetof(VERTICE, normal));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(VERTICE), (void*)offsetof(VERTICE, color));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(VERTICE), (void*)offsetof(VERTICE, uv));
    glEnableVertexAttribArray(3);
    glVertexAttribIPointer(4, 4, GL_UNSIGNED_INT, sizeof(VERTICE), (void*)offsetof(VERTICE, boneIndices));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(VERTICE), (void*)offsetof(VERTICE, boneWeights));
    glEnableVertexAttribArray(5);
    BindDrawIndexAttribute();
    glBindVertexArray(0);

    // the quad's glob: the original's, drawn as a cutout (its clear corners discarded, the border blended)
    note.shdIcon = *note.globOrig.asubglob[0].pshd;
    note.shdSide = note.shdIcon;
    GLOB& g = note.globIcon;
    g = note.globOrig;
    g.VAO = note.vaoQuad;
    g.VBO = note.vbo;
    g.EBO = note.ebo;
    g.poseCount = 0;
    g.poseDposSSBO = g.poseDnormalSSBO = 0;
    g.edgeSSBO = 0;
    g.edgeCount = 0;
    g.edgePoseDposSSBO = 0;
    g.fTransluscentSort = 1;
    g.rp = RP_Cutout;
    SetGlobDraw(&g);
    SUBGLOB& sub = g.asubglob[0];
    sub.vertices = av;
    sub.baseVertex = 0;
    sub.firstIndex = 0;
    sub.indexCount = cindexFaces;
    sub.pshd = &note.shdIcon;
    // (the sides: a second subglob over the same buffers)
    SUBGLOB subSide = sub;
    subSide.firstIndex = cindexFaces;
    subSide.indexCount = uint32_t(ai.size()) - cindexFaces;
    subSide.pshd = &note.shdSide;
    g.asubglob.push_back(subSide);
    g.csubglob = int(g.asubglob.size());
}

void Build(RUBY* pruby)
{
    std::array<ALO*, kcnote> apalo{};
    for (int i = 0; i < kcnote; ++i)
    {
        RWM* prwm = pruby->aprwmAttack[i];
        if (prwm == nullptr)
            return;
        for (const RWC& rwc : prwm->arwc)
            for (SO* pso : rwc.apso)
                if (apalo[size_t(i)] == nullptr)
                    apalo[size_t(i)] = PaloNote(pso, 0);
        if (apalo[size_t(i)] == nullptr)
            return;   // (the ammo is not made yet: again next update)
    }
    // up: from the triangle's apex (weapon 1) into its parent's frame, and from there into each note's own frame
    const SUBGLOB& subTriangle = apalo[1]->globset.aglob[0].asubglob[0];
    const PLANE planeTriangle = PlaneOf(subTriangle);
    glm::vec3 dApex(0.0f);
    for (const VERTICE& v : subTriangle.vertices)
    {
        glm::vec3 d = v.pos - planeTriangle.centre;
        d -= planeTriangle.normal * glm::dot(d, planeTriangle.normal);
        if (glm::length(d) > glm::length(dApex))
            dApex = d;
    }
    const glm::vec3 upParent = apalo[1]->xf.mat * (glm::length(dApex) > 0.0f ? glm::normalize(dApex) : glm::vec3(1, 0, 0));
    for (int i = 0; i < kcnote; ++i)
    {
        RHNOTE& note = s_state.anote[size_t(i)];
        note.globOrig = apalo[size_t(i)]->globset.aglob[0];
        note.vao = note.globOrig.VAO;
        note.prwm = pruby->aprwmAttack[i];
        for (size_t iglob = 1; iglob < apalo[size_t(i)]->globset.aglob.size(); ++iglob)
        {
            const GLOB& glob = apalo[size_t(i)]->globset.aglob[iglob];
            if (glob.asubglob.empty() && glob.edgeSSBO != 0 && glob.edgeCount > 0 && note.edgeCel == 0)
            {
                note.edgeCel = glob.edgeSSBO;
                note.globCelOrig = note.globCelNone = glob;
                note.globCelNone.edgeCount = 0;
            }
        }
        BuildQuad(note, PlaneOf(note.globOrig.asubglob[0]), glm::transpose(apalo[size_t(i)]->xf.mat) * upParent);
    }
    s_state.fBuilt = true;
}

// The trail's blip shader of a note, once its emitter has made its blips (the first throw).
void FindTrail(RHNOTE& note)
{
    if (note.pshdBlip != nullptr || note.prwm == nullptr)
        return;
    for (const RWC& rwc : note.prwm->arwc)
        for (SO* pso : rwc.apso)
        {
            ALO* palo = PaloNote(pso, 0);
            ALO* paloParent = palo != nullptr ? palo->paloParent : nullptr;
            for (ALO* pchild = paloParent != nullptr ? paloParent->dlChild.paloFirst : nullptr; pchild != nullptr;
                 pchild = pchild->dleChild.paloNext)
            {
                if (!FIsBasicDerivedFrom(pchild, CID_EMITTER))
                    continue;
                EMITTER* pemitter = static_cast<EMITTER*>(pchild);
                if (pemitter->pblipg != nullptr && pemitter->pblipg->pshd != nullptr && !pemitter->pblipg->pshd->atex.empty())
                {
                    note.pshdBlip = pemitter->pblipg->pshd;
                    note.aglBlipOrig = note.pshdBlip->atex[0].glDiffuseMap;
                    return;
                }
            }
        }
}

void SetTrail(RHNOTE& note, GLuint texture)
{
    if (note.pshdBlip != nullptr && !note.pshdBlip->atex.empty() && !note.aglBlipOrig.empty())
        note.pshdBlip->atex[0].glDiffuseMap.assign(note.aglBlipOrig.size(), texture);
}

// This frame's choice for each note.
void Frame()
{
    if (s_state.cframe == g_cframe)
        return;
    s_state.cframe = g_cframe;
    for (RHNOTE& note : s_state.anote)
    {
        note.pglobDraw = nullptr;
        FindTrail(note);
        if (note.pshdBlip != nullptr && !note.pshdBlip->atex.empty())
            note.pshdBlip->atex[0].glDiffuseMap = note.aglBlipOrig;
    }
    if (!PromptSubstituteActive())
        return;
    const Family family = PromptFamily();
    for (int i = 0; i < kcnote; ++i)
    {
        RHNOTE& note = s_state.anote[size_t(i)];
        GLuint texture = 0, textureSide = 0, textureTrail = 0;
        if (family == Family::Keyboard)
        {
            const int key = sly1actions::RhythmKey(cane::settings::sly1RhythmSlot(s_abtnNote[i]));
            if (key != GLFW_KEY_UNKNOWN)
                texture = Texture(-1, key, i, TEXK_Face), textureSide = Texture(-1, key, i, TEXK_Side),
                textureTrail = Texture(-1, key, i, s_afTrailTurned[i] ? TEXK_TrailTurned : TEXK_Trail);
        }
        else
        {
            const Button b = cane::input::fromGlfwBinding(sly1actions::BindingForButton(s_abtnNote[i], true));
            if (b == Button::Count)
                continue;
            const int j = WeaponOfButton(b);
            if (cane::input::isPlayStationFamily(family) && j >= 0)
            {
                // (the original notes: the bound button's own, and its trail)
                if (j != i)
                {
                    note.pglobDraw = &s_state.anote[size_t(j)].globOrig;
                    if (!s_state.anote[size_t(j)].aglBlipOrig.empty())
                        SetTrail(note, s_state.anote[size_t(j)].aglBlipOrig[0]);
                }
                continue;
            }
            texture = Texture(int(family), int(b), j >= 0 ? j : i, TEXK_Face);
            textureSide = Texture(int(family), int(b), j >= 0 ? j : i, TEXK_Side);
            textureTrail = Texture(int(family), int(b), j >= 0 ? j : i, s_afTrailTurned[i] ? TEXK_TrailTurned : TEXK_Trail);
        }
        if (texture == 0 || textureSide == 0 || textureTrail == 0 || note.shdIcon.atex.empty())
            continue;
        SetTrail(note, textureTrail);
        note.shdIcon.atex[0].glDiffuseMap.assign(std::max<size_t>(1, note.shdIcon.atex[0].glDiffuseMap.size()), texture);
        note.shdSide.atex[0].glDiffuseMap.assign(std::max<size_t>(1, note.shdSide.atex[0].glDiffuseMap.size()), textureSide);
        note.pglobDraw = &note.globIcon;
    }
}

// ---- the tests' automatic player and log
struct AUTO
{
    bool fOn = false;
    int key = GLFW_KEY_UNKNOWN;   // held now
    float tMatchHeld = -1.0f;     // the note it was pressed for
    int cframeHeld = 0, cframeReleased = -100, cframe = -1;
    int chit = 0, cmiss = 0;
};
AUTO s_auto;

void DecideAutoKey()
{
    if (s_auto.cframe == g_cframe)
        return;
    s_auto.cframe = g_cframe;
    RYTHM* prythm = s_state.prythm;
    int keyWanted = GLFW_KEY_UNKNOWN;
    float tMatch = -1.0f;
    if (prythm != nullptr && prythm->pruby != nullptr && prythm->fInputEnabled && prythm->iMatch < prythm->cMatch &&
        sly1actions::CurrentContext() == sly1actions::Context::Rhythm)
    {
        const RYTHMMATCH& match = prythm->aMatch[prythm->iMatch];
        const float t = float(prythm->pruby->iMeasure) * 4.0f + prythm->pruby->uBeat;
        // (the default rhythm keys by the note's button, whatever is bound: the check that W, A, D and Space play it)
        static constexpr int s_akey[kcnote] = { GLFW_KEY_A, GLFW_KEY_W, GLFW_KEY_D, GLFW_KEY_SPACE };
        if (match.button >= 0 && match.button < kcnote && std::abs(match.time - t) < prythm->dtInputWindow * 0.3f)
            keyWanted = s_akey[match.button], tMatch = match.time;
    }
    if (s_auto.key != GLFW_KEY_UNKNOWN)
    {
        // held until its note is answered (or a few frames), then released for a couple of frames
        if (tMatch == s_auto.tMatchHeld && g_cframe - s_auto.cframeHeld < 8)
            return;
        s_auto.key = GLFW_KEY_UNKNOWN;
        s_auto.cframeReleased = g_cframe;
        return;
    }
    if (keyWanted != GLFW_KEY_UNKNOWN && tMatch != s_auto.tMatchHeld && g_cframe - s_auto.cframeReleased >= 2)
    {
        s_auto.key = keyWanted;
        s_auto.tMatchHeld = tMatch;
        s_auto.cframeHeld = g_cframe;
    }
}
} // namespace

void ShRhythmNotesFound(RUBY* pruby)
{
    if (pruby == nullptr || g_psw == nullptr)
        return;
    if (s_state.psw != g_psw)
    {
        Reset();
        s_state.psw = g_psw;
    }
    s_state.prythm = pruby->prythm;
    if (!s_state.fBuilt)
        Build(pruby);
    if (s_state.fBuilt)
        Frame();   // (also while only trails fly: a device switch reaches them at once)
}

void ShRhythmNoteRpl(RPL* prpl)
{
    if (!s_state.fBuilt || prpl->pglob == nullptr)
        return;
    if (s_state.psw != g_psw)
    {
        Reset();
        return;
    }
    const GLuint vao = prpl->pglob->VAO;
    for (RHNOTE& note : s_state.anote)
    {
        if (note.edgeCel != 0 && prpl->pglob->asubglob.empty() && prpl->pglob->edgeSSBO == note.edgeCel)
        {
            // (the cel border: none for the icon, the bound button's for a remapped PlayStation note)
            Frame();
            for (RHNOTE& other : s_state.anote)
                if (note.pglobDraw == &other.globOrig && other.edgeCel != 0)
                    prpl->pglob = &other.globCelOrig;
            if (note.pglobDraw == &note.globIcon)
                prpl->pglob = &note.globCelNone;
            return;
        }
        if (note.vao != vao || note.vao == 0)
            continue;
        Frame();
        if (note.pglobDraw == nullptr)
            return;
        prpl->pglob = note.pglobDraw;
        if (note.pglobDraw == &note.globIcon && prpl->rp == RP_Opaque)
        {
            prpl->rp = RP_Cutout;
            prpl->fTransluscentSort = 1;
        }
        return;
    }
}

void ShRhythmNoteResult(int button, bool fHit)
{
    if (!s_auto.fOn)
        return;
    ++(fHit ? s_auto.chit : s_auto.cmiss);
    std::printf("rhythm: note button=%d %s (hits %d, misses %d)\n", button, fHit ? "hit" : "missed", s_auto.chit, s_auto.cmiss);
    std::fflush(stdout);
}

void ShRhythmTestAuto(bool fOn)
{
    s_auto = AUTO{};
    s_auto.fOn = fOn;
}

bool FShRhythmAutoKey(int key)
{
    if (!s_auto.fOn)
        return false;
    DecideAutoKey();
    return key == s_auto.key;
}
#endif

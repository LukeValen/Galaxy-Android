#include "gx_builder.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "port/port.h"

namespace gpu {

namespace {

inline float bitsToFloat(u32 v) {
    float f;
    memcpy(&f, &v, 4);
    return f;
}

void rgba8ToVec(u32 c, float* out) {
    out[0] = ((c >> 24) & 255) / 255.0f;
    out[1] = ((c >> 16) & 255) / 255.0f;
    out[2] = ((c >> 8) & 255) / 255.0f;
    out[3] = (c & 255) / 255.0f;
}

inline float s11(u32 v) {
    int x = (int)(v & 0x7FF);
    if (x & 0x400) x -= 0x800;
    return x / 255.0f;
}

u64 hashWords(const u32* w, size_t n) {
    u64 h = 1469598103934665603ull;
    for (size_t i = 0; i < n; i++) h = (h ^ w[i]) * 1099511628211ull;
    return h;
}

u32 usedTexUnits(const u32* bp) {
    u32 gen = bp[0x00];
    u32 stages = ((gen >> 10) & 15) + 1, ind = (gen >> 16) & 7;
    u32 mask = 0;
    for (u32 st = 0; st < stages; st++) {
        u32 order = (bp[0x28 + st / 2] >> ((st & 1) * 12)) & 0xFFF;
        if ((order >> 6) & 1) mask |= 1u << (order & 7);
    }
    for (u32 st = 0; st < ind; st++) mask |= 1u << ((bp[0x27] >> (st * 6)) & 7);
    return mask;
}

bool isTevColorReg(u32 reg) {
    return reg >= 0xE0 && reg <= 0xE7;
}

}  // namespace

bool sameBatchState(const Item& a, const Item& b) {
    if (a.uid != b.uid || a.prim != b.prim || a.useVr != b.useVr || a.hud != b.hud || a.hudSafe != b.hudSafe || a.player != b.player || a.flags != b.flags) return false;
    if (a.blend != b.blend || a.colorMask != b.colorMask || a.depthTest != b.depthTest || a.depthWrite != b.depthWrite || a.cull != b.cull ||
        a.dstAlpha != b.dstAlpha)
        return false;
    if ((a.blend || a.dstAlpha) && (a.srcRGB != b.srcRGB || a.dstRGB != b.dstRGB || a.eq != b.eq)) return false;
    if (a.dstAlpha && a.dstAlphaValue != b.dstAlphaValue) return false;
    if (a.depthTest && a.depthFunc != b.depthFunc) return false;
    if (a.cull && a.cullFace != b.cullFace) return false;
    if (memcmp(a.vp, b.vp, sizeof(a.vp)) != 0 || memcmp(a.sc, b.sc, sizeof(a.sc)) != 0) return false;
    if (a.texMask != b.texMask) return false;
    for (int t = 0; t < 8; t++) {
        if ((a.texMask & (1u << t)) && (a.tex[t] != b.tex[t] || a.texMode0[t] != b.texMode0[t] || a.texMode1[t] != b.texMode1[t])) return false;
    }
    return true;
}

void Builder::fillLights(LightState& b) {
    for (int l = 0; l < 8; l++) {
        const float* m = xfMem + 0x600 + l * 16;
        u32 col;
        memcpy(&col, &m[3], 4);
        rgba8ToVec(col, b.lights[l][0]);
        for (int k = 0; k < 3; k++) {
            b.lights[l][1][k] = m[4 + k];
            b.lights[l][2][k] = m[7 + k];
            b.lights[l][3][k] = m[10 + k];
            b.lights[l][4][k] = m[13 + k];
        }
        float lposLenSq = b.lights[l][3][0] * b.lights[l][3][0] + b.lights[l][3][1] * b.lights[l][3][1] + b.lights[l][3][2] * b.lights[l][3][2];
        if (lposLenSq > 1e10f) {
            float inv = 10000.0f / sqrtf(lposLenSq);
            b.lights[l][3][0] *= inv;
            b.lights[l][3][1] *= inv;
            b.lights[l][3][2] *= inv;
        }
        b.lights[l][1][3] = b.lights[l][2][3] = b.lights[l][3][3] = b.lights[l][4][3] = 0.0f;
    }
    rgba8ToVec(xfRegs[0x0C], b.matColor[0]);
    rgba8ToVec(xfRegs[0x0D], b.matColor[1]);
    rgba8ToVec(xfRegs[0x0A], b.ambColor[0]);
    rgba8ToVec(xfRegs[0x0B], b.ambColor[1]);
}

void Builder::fillProj(ProjState& b) {
    float p[6];
    for (int i = 0; i < 6; i++) p[i] = bitsToFloat(xfRegs[0x20 + i]);
    memset(b.proj, 0, sizeof(b.proj));
    if (xfRegs[0x26] & 1) {  // orthographic
        b.proj[0][0] = p[0];
        b.proj[0][3] = p[1];
        b.proj[1][1] = p[2];
        b.proj[1][3] = p[3];
        b.proj[2][2] = p[4];
        b.proj[2][3] = p[5];
        b.proj[3][3] = 1.0f;
    } else {
        b.proj[0][0] = p[0];
        b.proj[0][2] = p[1];
        b.proj[1][1] = p[2];
        b.proj[1][2] = p[3];
        b.proj[2][2] = p[4];
        b.proj[2][3] = p[5];
        b.proj[3][2] = -1.0f;
    }
    b.depthParams[0] = bitsToFloat(xfRegs[0x1C]);
    b.depthParams[1] = bitsToFloat(xfRegs[0x1F]);
    b.depthParams[2] = b.depthParams[3] = 0.0f;
}

void Builder::fillPix(PixelState& b) {
    for (int i = 0; i < 4; i++) {
        u32 lo = tevReg[i * 2], hi = tevReg[i * 2 + 1];
        b.tevReg[i][0] = s11(lo);
        b.tevReg[i][3] = s11(lo >> 12);
        b.tevReg[i][2] = s11(hi);
        b.tevReg[i][1] = s11(hi >> 12);
        u32 klo = tevKonst[i * 2], khi = tevKonst[i * 2 + 1];
        b.konst[i][0] = (klo & 0xFF) / 255.0f;
        b.konst[i][3] = ((klo >> 12) & 0xFF) / 255.0f;
        b.konst[i][2] = (khi & 0xFF) / 255.0f;
        b.konst[i][1] = ((khi >> 12) & 0xFF) / 255.0f;
    }
    b.alphaRef[0] = (float)(bp[0xF3] & 0xFF);
    b.alphaRef[1] = (float)((bp[0xF3] >> 8) & 0xFF);
    b.alphaRef[2] = (float)(bp[0xF4] & 0xFFFFFF);
    b.alphaRef[3] = 0.0f;
    u32 fc = bp[0xF2];
    b.fogColor[0] = ((fc >> 16) & 255) / 255.0f;
    b.fogColor[1] = ((fc >> 8) & 255) / 255.0f;
    b.fogColor[2] = (fc & 255) / 255.0f;
    b.fogColor[3] = 1.0f;
    auto fogFloat = [](u32 reg) {
        u32 bits = (reg & 0xFFFFF) << 12;
        float f;
        memcpy(&f, &bits, 4);
        return f;
    };
    b.fogParams[0] = fogFloat(bp[0xEE]);
    b.fogParams[1] = fogFloat(bp[0xF1]);
    b.fogParams[2] = (float)(bp[0xEF] & 0xFFFFFF);
    b.fogParams[3] = (float)(bp[0xF0] & 0x1F);
    memset(b.fogRange, 0, sizeof(b.fogRange));
    static const u8 base[8] = {0x88, 0x89, 0x8A, 0x8B, 0xA8, 0xA9, 0xAA, 0xAB};
    static const u8 mode0[8] = {0x80, 0x81, 0x82, 0x83, 0xA0, 0xA1, 0xA2, 0xA3};
    for (int i = 0; i < 8; i++) {
        u32 img = bp[base[i]];
        float w = (float)((img & 0x3FF) + 1), h = (float)(((img >> 10) & 0x3FF) + 1);
        int bias = (int)((bp[mode0[i]] >> 9) & 0xFF);
        b.texSize[i][0] = (float)(bias >= 0x80 ? bias - 0x100 : bias) / 32.0f;
        b.texSize[i][1] = h;
        b.texSize[i][2] = 1.0f / w;
        b.texSize[i][3] = 1.0f / h;
    }
    for (int m = 0; m < 3; m++) {
        u32 ra = bp[0x06 + m * 3], rb = bp[0x07 + m * 3], rc = bp[0x08 + m * 3];
        int scale = (int)(((ra >> 22) & 3) | (((rb >> 22) & 3) << 2) | (((rc >> 22) & 1) << 4)) - 17;
        float sc = ldexpf(1.0f, scale) / 1024.0f;
        auto sv = [](u32 v) {
            int x = (int)(v & 0x7FF);
            if (x & 0x400) x -= 0x800;
            return (float)x;
        };
        b.indMtx[m][0][0] = sv(ra) * sc;
        b.indMtx[m][0][1] = sv(rb) * sc;
        b.indMtx[m][0][2] = sv(rc) * sc;
        b.indMtx[m][0][3] = 0.0f;
        b.indMtx[m][1][0] = sv(ra >> 11) * sc;
        b.indMtx[m][1][1] = sv(rb >> 11) * sc;
        b.indMtx[m][1][2] = sv(rc >> 11) * sc;
        b.indMtx[m][1][3] = 0.0f;
    }
    b.efbSize[0] = (float)nativeW;
    b.efbSize[1] = (float)nativeH;
    b.efbSize[2] = b.efbSize[3] = 0.0f;
}

u32 Builder::appendRows(const float* src, int n, int stride) {
    u32 base = (u32)(rows.size() / 4);
    for (int r = 0; r < n; r++) {
        for (int k = 0; k < 4; k++) rows.push_back(k < stride ? src[r * stride + k] : 0.0f);
    }
    return base;
}

u32 Builder::addState(const float* data, size_t vec4s) {
    const u32* w = (const u32*)data;
    u64 h = hashWords(w, vec4s * 4) ^ vec4s;
    std::vector<u32>& bases = stateIndex[h];
    for (u32 b : bases) {
        if (memcmp(&states[(size_t)b * 4], data, vec4s * 16) == 0) return b;
    }
    u32 base = (u32)(states.size() / 4);
    states.insert(states.end(), data, data + vec4s * 4);
    bases.push_back(base);
    return base;
}

void Builder::invalidateXf(u32 a0, u32 a1) {
    if (a0 < 0x100) {
        u32 r0 = a0 / 4, r1 = ((a1 < 0x100 ? a1 : 0x100) - 1) / 4;
        for (u32 slot = r0 / 3; slot <= r1 / 3 && slot < 22; slot++) posSlot[slot] = ~0u;
    }
    if (a1 > 0x400 && a0 < 0x460) {
        u32 r0 = ((a0 > 0x400 ? a0 : 0x400) - 0x400) / 3, r1 = ((a1 < 0x460 ? a1 : 0x460) - 0x400 - 1) / 3;
        for (u32 slot = r0 / 3; slot <= r1 / 3 && slot < 11; slot++) nrmSlot[slot] = ~0u;
    }
    if (a1 > 0x500 && a0 < 0x600) {
        int r0 = (int)((a0 > 0x500 ? a0 : 0x500) - 0x500) / 4, r1 = (int)((a1 < 0x600 ? a1 : 0x600) - 0x500 - 1) / 4;
        for (int start = r0 - 2; start <= r1; start++) {
            if (start >= 0 && start < 64) postSlot[start] = ~0u;
        }
    }
    if (a1 > 0x600 && a0 < 0x680) lightBase = ~0u;
    if (a1 > 0x100A && a0 < 0x100E) lightBase = ~0u;
    if (a1 > 0x101C && a0 < 0x1027) projBase = ~0u;
}

u32 Builder::drawRecord(bool useVr, bool sky, bool pointer, bool dualTex, u32 numTexGens) {
    u32 rec[kRecWords];
    memset(rec, 0, sizeof(rec));
    for (int i = 0; i < 22; i++) {
        if (posSlot[i] == ~0u) posSlot[i] = appendRows(xfMem + i * 12, 3, 4);
        rec[kRecPosSlots + i] = posSlot[i];
    }
    for (int i = 0; i < 11; i++) {
        if (nrmSlot[i] == ~0u) nrmSlot[i] = appendRows(xfMem + 0x400 + i * 9, 3, 3);
        rec[kRecNrmSlots + i] = nrmSlot[i];
    }
    if (dualTex) {
        for (u32 i = 0; i < numTexGens && i < 8; i++) {
            u32 p = xfRegs[0x50 + i] & 63;
            if (postSlot[p] == ~0u) postSlot[p] = appendRows(xfMem + 0x500 + p * 4, 3, 4);
            rec[kRecPost + i] = postSlot[p];
        }
    }
    if (lightBase == ~0u) {
        LightState ls;
        fillLights(ls);
        lightBase = addState(&ls.lights[0][0][0], sizeof(ls) / 16);
    }
    if (projBase == ~0u) {
        ProjState ps;
        fillProj(ps);
        projBase = addState(&ps.proj[0][0], sizeof(ps) / 16);
    }
    if (pixBase == ~0u) {
        PixelState px;
        fillPix(px);
        pixBase = addState(&px.tevReg[0][0], sizeof(px) / 16);
    }
    rec[kRecLights] = lightBase;
    rec[kRecProj] = projBase;
    rec[kRecPixel] = pixBase;
    rec[kRecMtxIdxA] = xfRegs[0x18] ? xfRegs[0x18] : 0x27921780;
    rec[kRecMtxIdxB] = xfRegs[0x19] ? xfRegs[0x19] : 0x00cf0b6a;
    rec[kRecFlags] = (useVr ? 1u : 0u) | (sky ? 2u : 0u) | (pointer ? 4u : 0u);
    size_t n = recs.size();
    if (n >= kRecWords && memcmp(&recs[n - kRecWords], rec, sizeof(rec)) == 0) {
        return (u32)(n / kRecWords) - 1;
    }
    recs.insert(recs.end(), rec, rec + kRecWords);
    return (u32)(n / kRecWords);
}

u32 Builder::screenTexGens(const Frame& f, const Item& it, u32 first, const ShaderUid& uid) {
    // Screen-space texgen override is a VR-specific heuristic from Petari VR designed for
    // stereoscopic HMDs where left and right eyes have custom head poses and frustums.
    // On flat-screen rendering (mobile/desktop), overriding the GX hardware texture matrix
    // with normalized clipPos coordinates breaks planar reflections (such as the lake in
    // front of Peach's Castle) and transparent refractive objects, causing the reflected image
    // to stick to the camera viewport instead of correctly transforming with the scene geometry.
    // For flat-screen rendering, screenTexGens must return 0 so the exact GX texture matrix is evaluated.
    return 0;
}

u32 Builder::uidFor(const ShaderUid& uid) {
    auto it = uidIndex.find(uid);
    if (it != uidIndex.end()) return it->second;
    u32 i = (u32)uids.size();
    uids.push_back(uid);
    uidIndex.emplace(uid, i);
    return i;
}

void Builder::appendIndices(u32 kind, u32 base, u32 count) {
    u32 n;
    switch (kind) {
    case 0x80:
    case 0x88:  // quads
        n = count / 4 * 6;
        break;
    case 0x90:  // triangles
        n = count / 3 * 3;
        break;
    case 0x98:  // strip
    case 0xA0:  // fan
        n = count >= 3 ? (count - 2) * 3 : 0;
        break;
    case 0xA8:  // lines
        n = count / 2 * 2;
        break;
    case 0xB0:  // line strip
        n = count >= 2 ? (count - 1) * 2 : 0;
        break;
    default:  // points
        n = count;
        break;
    }
    size_t at = indices.size();
    indices.resize(at + n);
    u32* o = indices.data() + at;
    switch (kind) {
    case 0x80:
    case 0x88:
        for (u32 v = base; v + 4 <= base + count; v += 4, o += 6) {
            o[0] = v, o[1] = v + 1, o[2] = v + 2, o[3] = v, o[4] = v + 2, o[5] = v + 3;
        }
        break;
    case 0x98:
        for (u32 i = 0; i + 2 < count; i++, o += 3) {
            o[0] = base + i + (i & 1), o[1] = base + i + 1 - (i & 1), o[2] = base + i + 2;
        }
        break;
    case 0xA0:
        for (u32 i = 1; i + 1 < count; i++, o += 3) {
            o[0] = base, o[1] = base + i, o[2] = base + i + 1;
        }
        break;
    case 0xB0:
        for (u32 i = 0; i + 1 < count; i++, o += 2) {
            o[0] = base + i, o[1] = base + i + 1;
        }
        break;
    default:
        for (u32 i = 0; i < n; i++) {
            o[i] = base + i;
        }
        break;
    }
}

void Builder::build(std::shared_ptr<const Frame> frame, Prepared& out) {
    const Frame& f = *frame;
    items.clear();
    uids.clear();
    uidIndex.clear();
    rows.clear();
    recs.clear();
    states.clear();
    vtxRec.clear();
    indices.clear();
    stateIndex.clear();
    memcpy(bp, f.startBp, sizeof(bp));
    if (bp[0xF6] == 0 && bp[0xF7] == 0) {
        bp[0xF6] = 0x000004;
        bp[0xF7] = 0x00000E;
        bp[0xF8] = 0x000000;
        bp[0xF9] = 0x00000C;
        bp[0xFA] = 0x000005;
        bp[0xFB] = 0x00000D;
        bp[0xFC] = 0x00000A;
        bp[0xFD] = 0x00000E;
    }
    memcpy(tevReg, f.startTevReg, sizeof(tevReg));
    memcpy(tevKonst, f.startTevKonst, sizeof(tevKonst));
    memcpy(xfRegs, f.startXfRegs, sizeof(xfRegs));
    memcpy(xfMem, f.startXfMem, sizeof(xfMem));
    // Hardwired XF identity matrix at rows 60..63 (GX_IDENTITY = 60 = 0x3C):
    xfMem[240] = 1.0f; xfMem[241] = 0.0f; xfMem[242] = 0.0f; xfMem[243] = 0.0f;
    xfMem[244] = 0.0f; xfMem[245] = 1.0f; xfMem[246] = 0.0f; xfMem[247] = 0.0f;
    xfMem[248] = 0.0f; xfMem[249] = 0.0f; xfMem[250] = 1.0f; xfMem[251] = 0.0f;
    xfMem[252] = 0.0f; xfMem[253] = 0.0f; xfMem[254] = 0.0f; xfMem[255] = 1.0f;

    // Post-transform matrix memory rows 60..63 (0x05F0..0x05FF):
    xfMem[1520] = 1.0f; xfMem[1521] = 0.0f; xfMem[1522] = 0.0f; xfMem[1523] = 0.0f;
    // GX_PTIDENTITY (125 - 64 = 61): 3x4 identity matrix starting at row 61 (indices 1524..1535):
    xfMem[1524] = 1.0f; xfMem[1525] = 0.0f; xfMem[1526] = 0.0f; xfMem[1527] = 0.0f;
    xfMem[1528] = 0.0f; xfMem[1529] = 1.0f; xfMem[1530] = 0.0f; xfMem[1531] = 0.0f;
    xfMem[1532] = 0.0f; xfMem[1533] = 0.0f; xfMem[1534] = 1.0f; xfMem[1535] = 0.0f;

    for (u32& v : posSlot) v = ~0u;
    for (u32& v : nrmSlot) v = ~0u;
    for (u32& v : postSlot) v = ~0u;
    lightBase = projBase = pixBase = ~0u;
    inHud = false;
    inSky = false;
    inPlayer = false;
    inPointer = false;
    uidDirty = true;
    lastUidFlags = ~0u;
    drawStateDirty = true;
    cam = CameraInfo();
    for (u32& t : boundTex) t = ~0u;
    clearAR = f.startBp[0x4F];
    clearGB = f.startBp[0x50];
    clearZ = f.startBp[0x51] & 0xFFFFFF;
    u32 ordinal = 0;

    const u32* c = f.cmds.data();
    const u32* end = c + f.cmds.size();
    while (c < end) {
        u32 op = *c++;
        if (op != CMD_DRAW) {
            drawStateDirty = true;
        }
        switch (op) {
        case CMD_XF: {
            u32 addr = c[0], count = c[1];
            c += 2;
            for (u32 i = 0; i < count; i++) {
                u32 a = addr + i;
                if (a < 0x1000) {
                    if (a >= 240 && a <= 255) continue;  // Don't overwrite identity ROM
                    xfMem[a] = bitsToFloat(c[i]);
                } else if (a < 0x1100) {
                    xfRegs[a - 0x1000] = c[i];
                }
            }
            invalidateXf(addr, addr + count);
            auto touches = [&](u32 lo, u32 hi) { return addr < hi && addr + count > lo; };
            if (touches(0x1009, 0x100A) || touches(0x100E, 0x1013) || touches(0x103F, 0x1048) || touches(0x1050, 0x1058)) {
                uidDirty = true;
            }
            c += count;
            break;
        }
        case CMD_BP: {
            u32 v = *c++;
            u32 reg = v >> 24;
            bp[reg] = v & 0xFFFFFF;
            if (isTevColorReg(reg)) {
                trackTevColorWrite(tevReg, tevKonst, reg, v & 0xFFFFFF);
            }
            if ((reg >= 0x06 && reg <= 0x0E) || (reg >= 0x80 && reg <= 0x8B) || (reg >= 0xA0 && reg <= 0xAB) || (reg >= 0xE0 && reg <= 0xE7) ||
                (reg >= 0xEE && reg <= 0xF4)) {
                pixBase = ~0u;
            }
            if (reg == 0x00 || (reg >= 0x10 && reg <= 0x1F) || (reg >= 0x25 && reg <= 0x2F) || reg == 0x43 || (reg >= 0xC0 && reg <= 0xDF) ||
                reg == 0xF1 || reg == 0xF3 || (reg >= 0xF5 && reg <= 0xFD)) {
                uidDirty = true;
            }
            break;
        }
        case CMD_TEX:
            boundTex[c[0] & 7] = c[1];
            c += 2;
            break;
        case CMD_TEX_EFB:
            boundTex[c[0] & 7] = 0x80000000u | (c[4] >> 5);
            c += 5;
            break;
        case CMD_MARKER: {
            Item it{};
            it.type = IT_MARKER;
            it.marker = *c++;
            if (it.marker == MARK_HUD_BEGIN) inHud = true;
            if (it.marker == MARK_HUD_END) inHud = false;
            if (it.marker == MARK_SKY_BEGIN) inSky = true;
            if (it.marker == MARK_SKY_END) inSky = false;
            if (it.marker == MARK_PLAYER_BEGIN || it.marker == MARK_PLAYER_END) {
                inPlayer = it.marker == MARK_PLAYER_BEGIN;
                drawStateDirty = true;
            }
            if (it.marker == MARK_POINTER_BEGIN || it.marker == MARK_POINTER_END) {
                inPointer = it.marker == MARK_POINTER_BEGIN;
                drawStateDirty = true;
            }
            items.push_back(it);
            break;
        }
        case CMD_CAMERA: {
            if (!cam.valid) {
                cam.valid = true;
                memcpy(cam.view, c, 12 * 4);
                memcpy(cam.pos, c + 12, 3 * 4);
                memcpy(cam.watch, c + 15, 3 * 4);
                memcpy(cam.up, c + 18, 3 * 4);
                memcpy(cam.watchUp, c + 21, 3 * 4);
                memcpy(&cam.fovy, c + 24, 4);
                cam.flags = c[25];
                memcpy(&cam.aspect, c + 26, 4);
                memcpy(cam.player, c + 27, 3 * 4);
                memcpy(cam.playerUp, c + 30, 3 * 4);
            }
            c += kCameraWords;
            break;
        }
        case CMD_EFB_COPY: {
            Item it{};
            it.type = IT_COPY;
            memcpy(it.copy, c, sizeof(it.copy));
            it.copyPixFmt = bp[0x43];
            it.copyCmode = bp[0x41];
            it.copyZmode = bp[0x40];
            c += 10;
            items.push_back(it);
            if ((it.copy[9] >> 14) & 1) {
                u32 w = (it.copy[2] & 0x3FF) + 1;
                u32 h = ((it.copy[2] >> 10) & 0x3FF) + 1;
                if (w >= 600 && h >= 400) {
                    nativeW = w;
                    nativeH = h;
                }
            }
            break;
        }
        case CMD_DRAW: {
            u32 prim = c[0], flags = c[1], first = c[2], count = c[3];
            c += 4;
            if (!count) break;
            u32 kind = prim & 0xF8;
            uint32_t glPrim = kind <= 0xA0 ? 4 : kind <= 0xB0 ? 1 : 0; // GL_TRIANGLES=4, GL_LINES=1, GL_POINTS=0
            if (!drawStateDirty && lastDrawReusable && flags == lastDrawFlags && glPrim == lastDrawPrim && !items.empty() &&
                items.back().type == IT_DRAW) {
                Item* b = &items.back();
                if (b->endWord != first) {
                    Item it = *b;
                    it.firstWord = first;
                    it.firstOrdinal = ordinal;
                    it.idxOffset = (u32)indices.size();
                    it.idxCount = 0;
                    it.draws = 0;
                    items.push_back(it);
                    b = &items.back();
                }
                vtxRec.insert(vtxRec.end(), count, lastDrawRec);
                appendIndices(kind, ordinal - b->firstOrdinal, count);
                ordinal += count;
                b->idxCount = (u32)indices.size() - b->idxOffset;
                b->endWord = first + count * vtxWords(flags);
                b->draws++;
                break;
            }
            Item it{};
            it.type = IT_DRAW;
            it.flags = flags;
            it.hud = inHud;
            it.player = inPlayer;
            it.prim = glPrim;
            float sx = bitsToFloat(xfRegs[0x1A]), sy = bitsToFloat(xfRegs[0x1B]);
            float ox = bitsToFloat(xfRegs[0x1D]), oy = bitsToFloat(xfRegs[0x1E]);
            float xoff = (float)((bp[0x59] & 0x3FF) * 2), yoff = (float)(((bp[0x59] >> 10) & 0x3FF) * 2);
            it.vp[0] = ox - fabsf(sx) - xoff;
            it.vp[1] = oy - fabsf(sy) - yoff;
            it.vp[2] = 2.0f * fabsf(sx);
            it.vp[3] = 2.0f * fabsf(sy);
            int soffX = (int)xoff, soffY = (int)yoff;
            it.sc[0] = (int)((bp[0x20] >> 12) & 0x7FF) - soffX;
            it.sc[1] = (int)(bp[0x20] & 0x7FF) - soffY;
            it.sc[2] = (int)((bp[0x21] >> 12) & 0x7FF) - soffX + 1;
            it.sc[3] = (int)(bp[0x21] & 0x7FF) - soffY + 1;

            bool perspective = !(xfRegs[0x26] & 1);
            bool fullscreen = it.vp[0] <= 1.0f && it.vp[1] <= 1.0f && it.vp[2] >= (float)nativeW - 2.0f && it.vp[3] >= (float)nativeH - 2.0f;
            it.useVr = perspective && fullscreen && !inHud && cam.valid;
            float p1 = bitsToFloat(xfRegs[0x21]);
            it.hudSafe = inHud && !perspective && (fabsf(p1) < 0.5f);

            if (uidDirty || flags != lastUidFlags || inHud != lastUidHud) {
                ShaderUid uid = makeShaderUid(bp, xfRegs, flags, inHud);
                uid.screenTexGens = 0;
                it.uid = uidFor(uid);
                uidDirty = false;
                lastUid = it.uid;
                lastUidFlags = flags;
                lastUidHud = inHud;
            } else {
                it.uid = lastUid;
            }
            const ShaderUid& uid = uids[it.uid];

            u32 cm = bp[0x41];
            it.colorMask = (((cm >> 3) & 1) ? 7 : 0) | (((cm >> 4) & 1) ? 8 : 0);
            if ((bp[0x43] & 7) != 1) {
                it.colorMask &= 7;
            }
            it.blend = cm & 1;
            // OpenGL blend factor constants: 0, 1, 0x306 (SRC_ALPHA), 0x307 (ONE_MINUS_SRC_ALPHA), etc.
            static const uint32_t srcF[8] = {0, 1, 0x0306, 0x0307, 0x0302, 0x0303, 0x0304, 0x0305};
            static const uint32_t dstF[8] = {0, 1, 0x0300, 0x0301, 0x0302, 0x0303, 0x0304, 0x0305};
            if ((cm >> 11) & 1) {
                it.blend = 1;
                it.srcRGB = 1;
                it.dstRGB = 1;
                it.eq = 0x8008; // GL_FUNC_REVERSE_SUBTRACT
            } else {
                it.srcRGB = srcF[(cm >> 8) & 7];
                it.dstRGB = dstF[(cm >> 5) & 7];
                it.eq = 0x8006; // GL_FUNC_ADD
            }
            it.dstAlpha = (bp[0x42] >> 8) & 1;
            it.dstAlphaValue = (bp[0x42] & 0xFF) / 255.0f;
            u32 zm = bp[0x40];
            it.depthTest = zm & 1;
            it.depthWrite = (zm >> 4) & 1;
            // GL_NEVER=0x0200, GL_LESS=0x0201, GL_EQUAL=0x0202, GL_LEQUAL=0x0203, GL_GREATER=0x0204, GL_NOTEQUAL=0x0205, GL_GEQUAL=0x0206, GL_ALWAYS=0x0207
            static const uint32_t zf[8] = {0x0200, 0x0201, 0x0202, 0x0203, 0x0204, 0x0205, 0x0206, 0x0207};
            it.depthFunc = zf[(zm >> 1) & 7];
            u32 cull = (bp[0x00] >> 14) & 3;
            it.cull = (cull != 0);
            it.cullFace = (cull == 1) ? 0x0404 : (cull == 2) ? 0x0405 : 0x0408; // GL_FRONT=0x0404, GL_BACK=0x0405

            it.texMask = usedTexUnits(bp);
            static const u8 m0[8] = {0x80, 0x81, 0x82, 0x83, 0xA0, 0xA1, 0xA2, 0xA3};
            for (int t = 0; t < 8; t++) {
                bool used = (it.texMask >> t) & 1;
                it.tex[t] = used ? boundTex[t] : ~0u;
                it.texMode0[t] = used ? bp[m0[t]] : 0;
                it.texMode1[t] = used ? bp[m0[t] + 4] : 0;
            }

            u32 rec = drawRecord(it.useVr, inSky, inPointer, uid.dualTex != 0, uid.numTexGens);
            u32 firstOrdinal = ordinal;
            ordinal += count;
            vtxRec.insert(vtxRec.end(), count, rec);
            drawStateDirty = false;
            lastDrawReusable = !uidDirty;
            lastDrawFlags = flags;
            lastDrawPrim = it.prim;
            lastDrawRec = rec;

            Item* b = (!items.empty() && items.back().type == IT_DRAW) ? &items.back() : nullptr;
            if (!b || b->endWord != first || !sameBatchState(*b, it)) {
                it.firstWord = first;
                it.firstOrdinal = firstOrdinal;
                it.idxOffset = (u32)indices.size();
                items.push_back(it);
                b = &items.back();
            }
            appendIndices(kind, firstOrdinal - b->firstOrdinal, count);
            b->idxCount = (u32)indices.size() - b->idxOffset;
            b->endWord = first + count * vtxWords(flags);
            b->draws++;
            break;
        }
        default:
            port_log("builder: bad recording opcode %u", op);
            c = end;
            break;
        }
    }

    if (rows.empty()) rows.resize(4, 0.0f);
    if (recs.empty()) recs.resize(kRecWords, 0);
    if (states.empty()) states.resize(4, 0.0f);
    if (vtxRec.empty()) vtxRec.push_back(0);

    out.frame = std::move(frame);
    out.number = f.number;
    std::swap(out.items, items);
    std::swap(out.uids, uids);
    std::swap(out.rows, rows);
    std::swap(out.recs, recs);
    std::swap(out.states, states);
    std::swap(out.vtxRec, vtxRec);
    std::swap(out.indices, indices);
    out.cam = cam;
    out.clearAR = clearAR;
    out.clearGB = clearGB;
    out.clearZ = clearZ;
    out.nativeW = nativeW;
    out.nativeH = nativeH;
}

void logItems(const Prepared& p) {
    const std::vector<Item>& items = p.items;
    const std::vector<float>& rows = p.rows;
    const std::vector<u32>& recs = p.recs;
    const std::vector<float>& states = p.states;
    const std::vector<u32>& indices = p.indices;
    u64 preparedNumber = p.number;
    u32 nativeW = p.nativeW, nativeH = p.nativeH;
    port_log("items: frame %llu, %zu items, native %ux%u, %zu rows, %zu records, %zu state vec4s, %zu indices", (unsigned long long)preparedNumber,
             items.size(), nativeW, nativeH, rows.size() / 4, recs.size() / kRecWords, states.size() / 4, indices.size());
    for (const Item& it : items) {
        if (it.type == IT_MARKER) {
            port_log("items: marker %u", it.marker);
        } else if (it.type == IT_COPY) {
            const u32* cp = it.copy;
            u32 ctrl = cp[9];
            port_log("items: COPY src %u,%u %ux%u dst %06x %s fmt %u clear %u (color %06x %06x z %06x) cmode %06x zmode %06x", cp[1] & 0x3FF,
                     (cp[1] >> 10) & 0x3FF, (cp[2] & 0x3FF) + 1, ((cp[2] >> 10) & 0x3FF) + 1, cp[3], (ctrl >> 14) & 1 ? "XFB" : "TEX",
                     ((ctrl >> 4) & 7) | ((ctrl >> 0) & 8), (ctrl >> 11) & 1, cp[6], cp[7], cp[8], it.copyCmode, it.copyZmode);
        } else {
            port_log("items: batch %u draws prog %u prim %x flags %05x idx %u vr %u hud %u player %u blend %u(%x,%x,%x) cmask %x z %u/%u/%x cull %u vp %.0f,%.0f "
                     "%.0fx%.0f tex0 %08x tex1 %08x screen texgens %x",
                     it.draws, it.program, it.prim, it.flags, it.idxCount, it.useVr, it.hud, it.player, it.blend, it.srcRGB, it.dstRGB, it.eq, it.colorMask,
                     it.depthTest, it.depthWrite, it.depthFunc, it.cull, it.vp[0], it.vp[1], it.vp[2], it.vp[3], it.tex[0], it.tex[1],
                     p.uids[it.uid].screenTexGens);
            if (it.player) {
                const ShaderUid& u = p.uids[it.uid];
                port_log("items:   player: chans %u cc %06x %06x ac %06x %06x; texgens %u: %05x %05x %05x %05x post %03x %03x; tev %u ind %u; "
                         "tex %08x %08x %08x %08x",
                         u.numChans, u.colorCtrl[0], u.colorCtrl[1], u.alphaCtrl[0], u.alphaCtrl[1], u.numTexGens, u.texMtxInfo[0],
                         u.texMtxInfo[1], u.texMtxInfo[2], u.texMtxInfo[3], u.postMtxInfo[0], u.postMtxInfo[1], u.numTevStages, u.numIndStages,
                         it.tex[0], it.tex[1], it.tex[2], it.tex[3]);
                port_log("items:   player tev: order %06x %06x; color %06x %06x %06x %06x; alpha %06x %06x %06x %06x; ksel %06x %06x %06x %06x",
                         u.tevOrder[0], u.tevOrder[1], u.tevColor[0], u.tevColor[1], u.tevColor[2], u.tevColor[3], u.tevAlpha[0], u.tevAlpha[1],
                         u.tevAlpha[2], u.tevAlpha[3], u.ksel[0], u.ksel[1], u.ksel[2], u.ksel[3]);
                if (it.firstOrdinal < p.vtxRec.size()) {
                    size_t rec = (size_t)p.vtxRec[it.firstOrdinal] * kRecWords;
                    if (rec + kRecLights < recs.size()) {
                        size_t light = recs[rec + kRecLights];
                        if ((light + 44) * 4 <= states.size()) {
                            const float* s = states.data() + light * 4;
                            port_log("items:   player light: mat0 %.2f %.2f %.2f; mat1 %.2f %.2f %.2f; amb0 %.2f %.2f %.2f; amb1 %.2f %.2f %.2f",
                                     s[160], s[161], s[162], s[164], s[165], s[166], s[168], s[169], s[170], s[172], s[173], s[174]);
                            for (int l = 0; l < 8; l++) {
                                const float* v = s + l * 20;
                                port_log("items:   light%d: rgb %.2f %.2f %.2f; cos %.3f %.3f %.3f; dist %.3f %.3f %.3f; pos %.3f %.3f %.3f; dir %.3f %.3f %.3f",
                                         l, v[0], v[1], v[2], v[4], v[5], v[6], v[8], v[9], v[10], v[12], v[13], v[14], v[16], v[17], v[18]);
                            }
                        }
                    }
                }
            }
        }
    }
}

}  // namespace gpu

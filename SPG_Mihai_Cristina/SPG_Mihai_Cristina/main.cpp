#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <freeglut.h>
#include <iostream>
#include <cmath>
#include <vector>
#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

// ===================== TEXTURI =====================
GLuint texPX = 0, texNX = 0, texPY = 0, texNY = 0, texPZ = 0, texNZ = 0;
GLuint texGround = 0, texRoad = 0, texBuilding = 0, texTree = 0, texStalp = 0;

// ===================== CAMERA =====================
float camX = 0, camY = 18, camZ = 80;
float yaw = -90, pitch = -12, moveSpeed = 1.2f, mouseSensitivity = 0.22f;
bool rightMouseDown = false;
int lastMouseX = -1, lastMouseY = -1;
bool keys[256] = { false };

// ===================== LUMINA SOARE =====================
GLfloat sunPos[] = { 80, 120, 60, 1 };
GLfloat sunAmbient[] = { 0.18f, 0.18f, 0.20f, 1 };
GLfloat sunDiffuse[] = { 0.55f, 0.52f, 0.45f, 1 };
GLfloat sunSpecular[] = { 0.30f, 0.30f, 0.25f, 1 };

// ===================== STALPI =====================
struct LampPost { float x, y, z, lightY, armAngle; };
std::vector<LampPost> lampPosts;

GLfloat lampAmbient[] = { 0.15f, 0.12f, 0.02f, 1 };
GLfloat lampDiffuse[] = { 1.0f,  0.85f, 0.30f, 1 };
GLfloat lampSpecular[] = { 0.8f,  0.65f, 0.20f, 1 };
GLfloat lampConstAtt = 0.3f, lampLinAtt = 0.01f, lampQuadAtt = 0.002f;

// ===================== OBJ MESH =====================
struct ObjMesh {
    struct Face { int vi[4], ti[4], ni[4]; int vcount; };
    struct V3 { float x, y, z; };
    struct V2 { float u, v; };
    std::vector<V3> verts;
    std::vector<V2> uvs;
    std::vector<V3> normals;
    std::vector<Face> faces;
};

ObjMesh benchMesh;
bool benchLoaded = false;

// ===================== BANCI =====================
struct Bench { float x, z, rot; int lampIdx; };
std::vector<Bench> benches;

static float deg2rad(float d) { return d * 3.1415926535f / 180.0f; }
static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

void shadowMatrix(GLfloat m[16], GLfloat l[4], GLfloat p[4]) {
    float dot = p[0] * l[0] + p[1] * l[1] + p[2] * l[2] + p[3] * l[3];
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            m[i * 4 + j] = -p[j] * l[i];
            if (i == j) m[i * 4 + j] += dot;
        }
}

// ===================== TEXTURA =====================
GLuint LoadTexture2D(const char* f, bool rep, bool flip) {
    GLuint id; glGenTextures(1, &id); glBindTexture(GL_TEXTURE_2D, id);
    int w, h, c; stbi_set_flip_vertically_on_load(flip ? 1 : 0);
    unsigned char* d = stbi_load(f, &w, &h, &c, 0);
    if (!d) { std::cout << "Nu pot incarca: " << f << "\n"; return 0; }
    GLenum fmt = (c == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, d);
    stbi_image_free(d);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    GLint wr = rep ? GL_REPEAT : GL_CLAMP_TO_EDGE;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wr);
    return id;
}

// ===================== OBJ LOADER =====================
bool LoadOBJ(const char* path, ObjMesh& mesh) {
    FILE* f = fopen(path, "r");
    if (!f) { std::cout << "Nu pot deschide: " << path << "\n"; return false; }
    mesh.verts.clear(); mesh.uvs.clear(); mesh.normals.clear(); mesh.faces.clear();
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == 'v' && line[1] == ' ') {
            ObjMesh::V3 v; sscanf(line + 2, "%f %f %f", &v.x, &v.y, &v.z);
            mesh.verts.push_back(v);
        }
        else if (line[0] == 'v' && line[1] == 't') {
            ObjMesh::V2 uv; sscanf(line + 3, "%f %f", &uv.u, &uv.v);
            mesh.uvs.push_back(uv);
        }
        else if (line[0] == 'v' && line[1] == 'n') {
            ObjMesh::V3 n; sscanf(line + 3, "%f %f %f", &n.x, &n.y, &n.z);
            mesh.normals.push_back(n);
        }
        else if (line[0] == 'f' && line[1] == ' ') {
            ObjMesh::Face face; face.vcount = 0;
            char* p = line + 2;
            while (*p&& face.vcount < 4) {
                int vi = 0, ti = 0, ni = 0;
                if (sscanf(p, "%d/%d/%d", &vi, &ti, &ni) == 3) {}
                else if (sscanf(p, "%d//%d", &vi, &ni) == 2) { ti = 0; }
                else if (sscanf(p, "%d/%d", &vi, &ti) == 2) { ni = 0; }
                else if (sscanf(p, "%d", &vi) == 1) { ti = ni = 0; }
                else break;
                face.vi[face.vcount] = vi - 1;
                face.ti[face.vcount] = ti > 0 ? ti - 1 : -1;
                face.ni[face.vcount] = ni > 0 ? ni - 1 : -1;
                face.vcount++;
                while (*p && !isspace((unsigned char)*p)) p++;
                while (*p && isspace((unsigned char)*p)) p++;
            }
            if (face.vcount >= 3) mesh.faces.push_back(face);
        }
    }
    fclose(f);
    std::cout << "OBJ: " << mesh.verts.size() << " verts, " << mesh.faces.size() << " faces\n";
    return !mesh.faces.empty();
}

// ===================== RELIEF =====================
static float hash2(int x, int y) {
    unsigned int h = (unsigned int)(x * 374761393u + y * 668265263u);
    h = (h ^ (h >> 13)) * 1274126177u; h ^= (h >> 16);
    return (h & 0x00FFFFFF) / 16777215.0f;
}
static float lerp(float a, float b, float t) { return a + (b - a) * t; }
static float smooth(float t) { return t * t * (3 - 2 * t); }
static float valueNoise(float x, float z) {
    int x0 = (int)floorf(x), z0 = (int)floorf(z);
    float sx = smooth(x - x0), sz = smooth(z - z0);
    return lerp(lerp(hash2(x0, z0), hash2(x0 + 1, z0), sx),
        lerp(hash2(x0, z0 + 1), hash2(x0 + 1, z0 + 1), sx), sz) * 2 - 1;
}
static float fbm(float x, float z) {
    float s = 0, a = 1, f = 1;
    for (int i = 0; i < 5; i++) { s += a * valueNoise(x * f, z * f); a *= 0.5f; f *= 2; }
    return s;
}
static float bump(float x, float z, float cx, float cz, float amp, float r) {
    float dx = x - cx, dz = z - cz; return amp * expf(-(dx * dx + dz * dz) / (r * r));
}
float HeightFunc(float x, float z) {
    float h = 3.0f * fbm(x * 0.06f, z * 0.06f)
        + 1.6f * fbm(x * 0.15f, z * 0.15f)
        + bump(x, z, -35, -15, 7, 28)
        + bump(x, z, 30, 20, 6, 24)
        + bump(x, z, 5, -40, 5.5f, 22);
    float rr = 55.0f, flatHalf = 18.0f, fade = 10.0f;
    float dc = sqrtf(x * x + z * z), keep = 1.0f;
    auto blend = [&](float dist) {
        float t = clampf((dist - flatHalf) / fade, 0.0f, 1.0f); return smooth(t);
        };
    if (fabs(dc - rr) < flatHalf + fade)
        keep = (keep < blend(fabs(dc - rr))) ? keep : blend(fabs(dc - rr));
    if (fabs(x) < flatHalf + fade && fabs(z) > rr - flatHalf)
        keep = (keep < blend(fabs(x))) ? keep : blend(fabs(x));
    if (fabs(z) < flatHalf + fade && fabs(x) > rr - flatHalf)
        keep = (keep < blend(fabs(z))) ? keep : blend(fabs(z));
    return clampf(h * keep, -2.0f, 14.0f);
}

// ===================== SKYBOX =====================
void DrawSkybox2D(float s = 520) {
    glDepthMask(0); glDisable(GL_LIGHTING); glEnable(GL_TEXTURE_2D); glColor3f(1, 1, 1);
    float e = 0.0018f, u0 = e, u1 = 1 - e, v0 = e, v1 = 1 - e;
    auto B = [&](GLuint t) {
        glBindTexture(GL_TEXTURE_2D, t);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        };
    B(texPZ); glBegin(GL_QUADS); glTexCoord2f(u0, v0); glVertex3f(-s, -s, s); glTexCoord2f(u1, v0); glVertex3f(s, -s, s); glTexCoord2f(u1, v1); glVertex3f(s, s, s); glTexCoord2f(u0, v1); glVertex3f(-s, s, s); glEnd();
    B(texNZ); glBegin(GL_QUADS); glTexCoord2f(u0, v0); glVertex3f(s, -s, -s); glTexCoord2f(u1, v0); glVertex3f(-s, -s, -s); glTexCoord2f(u1, v1); glVertex3f(-s, s, -s); glTexCoord2f(u0, v1); glVertex3f(s, s, -s); glEnd();
    B(texPX); glBegin(GL_QUADS); glTexCoord2f(u0, v0); glVertex3f(s, -s, s); glTexCoord2f(u1, v0); glVertex3f(s, -s, -s); glTexCoord2f(u1, v1); glVertex3f(s, s, -s); glTexCoord2f(u0, v1); glVertex3f(s, s, s); glEnd();
    B(texNX); glBegin(GL_QUADS); glTexCoord2f(u0, v0); glVertex3f(-s, -s, -s); glTexCoord2f(u1, v0); glVertex3f(-s, -s, s); glTexCoord2f(u1, v1); glVertex3f(-s, s, s); glTexCoord2f(u0, v1); glVertex3f(-s, s, -s); glEnd();
    B(texPY); glBegin(GL_QUADS); glTexCoord2f(u0, v0); glVertex3f(-s, s, s); glTexCoord2f(u1, v0); glVertex3f(s, s, s); glTexCoord2f(u1, v1); glVertex3f(s, s, -s); glTexCoord2f(u0, v1); glVertex3f(-s, s, -s); glEnd();
    B(texNY); glBegin(GL_QUADS); glTexCoord2f(u0, v0); glVertex3f(-s, -s, -s); glTexCoord2f(u1, v0); glVertex3f(s, -s, -s); glTexCoord2f(u1, v1); glVertex3f(s, -s, s); glTexCoord2f(u0, v1); glVertex3f(-s, -s, s); glEnd();
    glDisable(GL_TEXTURE_2D); glDepthMask(1);
}

// ===================== SOL =====================
void DrawGround(float sz = 260, float y = 0, float tile = 45) {
    glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, texGround); glColor3f(1, 1, 1);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0);       glVertex3f(-sz, y, -sz);
    glTexCoord2f(tile, 0);    glVertex3f(sz, y, -sz);
    glTexCoord2f(tile, tile); glVertex3f(sz, y, sz);
    glTexCoord2f(0, tile);    glVertex3f(-sz, y, sz);
    glEnd(); glDisable(GL_TEXTURE_2D);
}

// ===================== RELIEF =====================
void DrawRelief(float half = 90, int steps = 150, float uvT = 24) {
    glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, texGround); glColor3f(1, 1, 1);
    glEnable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(-1, -1);
    float step = (2 * half) / steps;
    for (int z = 0; z < steps; z++) {
        float z0 = -half + z * step, z1 = z0 + step;
        float v0 = (float)z / steps * uvT, v1 = (float)(z + 1) / steps * uvT;
        glBegin(GL_TRIANGLE_STRIP);
        for (int x = 0; x <= steps; x++) {
            float x0 = -half + x * step, u = (float)x / steps * uvT;
            glTexCoord2f(u, v0); glVertex3f(x0, 0.45f + HeightFunc(x0, z0), z0);
            glTexCoord2f(u, v1); glVertex3f(x0, 0.45f + HeightFunc(x0, z1), z1);
        }
        glEnd();
    }
    glDisable(GL_POLYGON_OFFSET_FILL); glDisable(GL_TEXTURE_2D);
}

// ===================== STRADA =====================
void DrawRoads() {
    glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, texRoad); glColor3f(1, 1, 1);
    glEnable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(-3, -3);
    float rw = 7, rr = 55, rh = 0.52f;
    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= 150; i++) {
        float a = 2 * 3.14159f * (float)i / 150.0f, u = (float)i / 150.0f * 12;
        glTexCoord2f(0, u); glVertex3f(cosf(a) * (rr - rw), rh, sinf(a) * (rr - rw));
        glTexCoord2f(1, u); glVertex3f(cosf(a) * (rr + rw), rh, sinf(a) * (rr + rw));
    }
    glEnd();
    auto DS = [&](float sx, float sz, float ex, float ez, bool hz) {
        glBegin(GL_TRIANGLE_STRIP);
        for (int i = 0; i <= 50; i++) {
            float t = (float)i / 50.0f, cx = sx + (ex - sx) * t, cz = sz + (ez - sz) * t, u = t * 15;
            if (!hz) { glTexCoord2f(0, u); glVertex3f(cx - rw, rh, cz); glTexCoord2f(1, u); glVertex3f(cx + rw, rh, cz); }
            else { glTexCoord2f(0, u); glVertex3f(cx, rh, cz - rw); glTexCoord2f(1, u); glVertex3f(cx, rh, cz + rw); }
        }
        glEnd();
        };
    DS(0, rr, 0, 240, false); DS(0, -rr, 0, -240, false);
    DS(rr, 0, 240, 0, true);  DS(-rr, 0, -240, 0, true);
    glDisable(GL_POLYGON_OFFSET_FILL); glDisable(GL_TEXTURE_2D);
}

// ===================== CLADIRI =====================
void DrawRealisticBlock(float x, float z, float rot, float W, float H, float D) {
    float y = HeightFunc(x, z);
    glPushMatrix(); glTranslatef(x, y, z); glRotatef(rot, 0, 1, 0);
    glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, texBuilding); glColor3f(1, 1, 1);
    float tW = W / 8, tH = H / 12, tD = D / 8;
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex3f(-W, 0, D); glTexCoord2f(tW, 0); glVertex3f(W, 0, D); glTexCoord2f(tW, tH); glVertex3f(W, H, D); glTexCoord2f(0, tH); glVertex3f(-W, H, D);
    glTexCoord2f(0, 0); glVertex3f(-W, 0, -D); glTexCoord2f(tW, 0); glVertex3f(W, 0, -D); glTexCoord2f(tW, tH); glVertex3f(W, H, -D); glTexCoord2f(0, tH); glVertex3f(-W, H, -D);
    glTexCoord2f(0, 0); glVertex3f(W, 0, -D); glTexCoord2f(tD, 0); glVertex3f(W, 0, D); glTexCoord2f(tD, tH); glVertex3f(W, H, D); glTexCoord2f(0, tH); glVertex3f(W, H, -D);
    glTexCoord2f(0, 0); glVertex3f(-W, 0, -D); glTexCoord2f(tD, 0); glVertex3f(-W, 0, D); glTexCoord2f(tD, tH); glVertex3f(-W, H, D); glTexCoord2f(0, tH); glVertex3f(-W, H, -D);
    glEnd(); glDisable(GL_TEXTURE_2D);
    glColor3f(0.2f, 0.2f, 0.2f);
    glBegin(GL_QUADS); glVertex3f(-W, H, D); glVertex3f(W, H, D); glVertex3f(W, H, -D); glVertex3f(-W, H, -D); glEnd();
    glPopMatrix();
}

// ===================== POMI =====================
void DrawTree(float x, float z, float sc) {
    float y = HeightFunc(x, z);
    glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, texTree);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_ALPHA_TEST); glAlphaFunc(GL_GREATER, 0.1f);
    glPushMatrix(); glTranslatef(x, y, z);
    for (int i = 0; i < 2; i++) {
        glRotatef(i * 90.0f, 0, 1, 0);
        glBegin(GL_QUADS); glColor3f(1, 1, 1);
        glTexCoord2f(0, 0); glVertex3f(-sc, 0, 0);    glTexCoord2f(1, 0); glVertex3f(sc, 0, 0);
        glTexCoord2f(1, 1); glVertex3f(sc, sc * 2, 0);  glTexCoord2f(0, 1); glVertex3f(-sc, sc * 2, 0);
        glEnd();
    }
    glPopMatrix();
    glDisable(GL_ALPHA_TEST); glDisable(GL_BLEND); glDisable(GL_TEXTURE_2D);
}

// ===================== GENERARE STALPI =====================
void BuildLampPosts() {
    lampPosts.clear();
    const float side = 14.0f, lh = 14.0f, pos = 68.0f;
    {
        float z = pos, gL = HeightFunc(-side, z), gR = HeightFunc(side, z);
        lampPosts.push_back({ -side,gL,z,gL + lh, 90 }); lampPosts.push_back({ side,gR,z,gR + lh,-90 });
    }
    {
        float z = -pos, gL = HeightFunc(-side, z), gR = HeightFunc(side, z);
        lampPosts.push_back({ -side,gL,z,gL + lh, 90 }); lampPosts.push_back({ side,gR,z,gR + lh,-90 });
    }
    {
        float x = pos, gL = HeightFunc(x, -side), gR = HeightFunc(x, side);
        lampPosts.push_back({ x,gL,-side,gL + lh,  0 }); lampPosts.push_back({ x,gR,side,gR + lh,180 });
    }
    {
        float x = -pos, gL = HeightFunc(x, -side), gR = HeightFunc(x, side);
        lampPosts.push_back({ x,gL,-side,gL + lh,  0 }); lampPosts.push_back({ x,gR,side,gR + lh,180 });
    }
}

// ===================== BANCA =====================
void BuildBenches() {
    benches.clear();
    if (lampPosts.empty()) return;
    const LampPost& lp = lampPosts[0];
    benches.push_back({ lp.x + 4.5f, lp.z, 90.0f, 0 });
}

// ===================== ACTIVARE LUMINI =====================
void ActivateLampLights() {
    for (int i = 0; i < 8; i++) glDisable(GL_LIGHT0 + i);
    if (lampPosts.empty()) return;
    std::vector<int> idx(lampPosts.size());
    for (int i = 0; i < (int)idx.size(); i++) idx[i] = i;
    for (int i = 0; i < (int)idx.size() - 1; i++)
        for (int j = i + 1; j < (int)idx.size(); j++) {
            float da = (lampPosts[idx[i]].x - camX) * (lampPosts[idx[i]].x - camX) + (lampPosts[idx[i]].z - camZ) * (lampPosts[idx[i]].z - camZ);
            float db = (lampPosts[idx[j]].x - camX) * (lampPosts[idx[j]].x - camX) + (lampPosts[idx[j]].z - camZ) * (lampPosts[idx[j]].z - camZ);
            if (db < da) { int t = idx[i]; idx[i] = idx[j]; idx[j] = t; }
        }
    int cnt = (int)idx.size() < 8 ? (int)idx.size() : 8;
    for (int i = 0; i < cnt; i++) {
        const LampPost& lp = lampPosts[idx[i]];
        GLenum lid = GL_LIGHT0 + i;
        float rad = lp.armAngle * 3.14159f / 180.0f;
        GLfloat pos[4] = { lp.x + sinf(rad) * 1.8f,lp.lightY,lp.z + cosf(rad) * 1.8f,1 };
        GLfloat sd[3] = { 0,-1,0 };
        glLightfv(lid, GL_POSITION, pos);
        glLightfv(lid, GL_AMBIENT, lampAmbient); glLightfv(lid, GL_DIFFUSE, lampDiffuse); glLightfv(lid, GL_SPECULAR, lampSpecular);
        glLightf(lid, GL_CONSTANT_ATTENUATION, lampConstAtt);
        glLightf(lid, GL_LINEAR_ATTENUATION, lampLinAtt);
        glLightf(lid, GL_QUADRATIC_ATTENUATION, lampQuadAtt);
        glLightfv(lid, GL_SPOT_DIRECTION, sd);
        glLightf(lid, GL_SPOT_CUTOFF, 55.0f); glLightf(lid, GL_SPOT_EXPONENT, 6.0f);
        glEnable(lid);
    }
}

// ===================== DESENARE STALP =====================
void DrawOneLampPost(const LampPost& lp) {
    const float pr = 0.25f, ph = lp.lightY - lp.y, cr = 0.55f, al = 1.8f, ah = 0.18f, aw = 0.18f;
    glPushMatrix(); glTranslatef(lp.x, lp.y, lp.z);
    glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, texStalp); glColor3f(1, 1, 1);
    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= 10; i++) {
        float a = 2 * 3.14159f * (float)i / 10.0f, u = (float)i / 10.0f;
        glTexCoord2f(u, 0);        glVertex3f(cosf(a) * pr, 0, sinf(a) * pr);
        glTexCoord2f(u, ph / 6.0f); glVertex3f(cosf(a) * pr, ph, sinf(a) * pr);
    }
    glEnd(); glDisable(GL_TEXTURE_2D);
    glPushMatrix(); glRotatef(lp.armAngle, 0, 1, 0);
    glColor3f(0.55f, 0.55f, 0.55f);
    glBegin(GL_QUADS);
    glVertex3f(-aw, ph, al);   glVertex3f(aw, ph, al);   glVertex3f(aw, ph, 0);     glVertex3f(-aw, ph, 0);
    glVertex3f(-aw, ph - ah, 0); glVertex3f(aw, ph - ah, 0); glVertex3f(aw, ph - ah, al); glVertex3f(-aw, ph - ah, al);
    glVertex3f(-aw, ph, 0);    glVertex3f(-aw, ph, al);   glVertex3f(-aw, ph - ah, al); glVertex3f(-aw, ph - ah, 0);
    glVertex3f(aw, ph, 0);    glVertex3f(aw, ph, al);   glVertex3f(aw, ph - ah, al); glVertex3f(aw, ph - ah, 0);
    glEnd();
    GLfloat eon[] = { 1.0f,0.9f,0.3f,1 }, eoff[] = { 0,0,0,1 };
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, eon);
    glColor3f(1, 0.95f, 0.4f);
    glPushMatrix(); glTranslatef(0, ph - ah * 0.5f, al);
    GLUquadric* q = gluNewQuadric(); gluSphere(q, cr, 12, 10); gluDeleteQuadric(q);
    glPopMatrix();
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, eoff);
    glPopMatrix(); glPopMatrix();
}
void DrawAllLampPosts() { for (const auto& lp : lampPosts) DrawOneLampPost(lp); }

// ===================== GLOW PE SOL =====================
void DrawLampGlow(const LampPost& lp) {
    float rad = lp.armAngle * 3.14159f / 180.0f;
    float cx = lp.x + sinf(rad) * 1.8f, cz = lp.z + cosf(rad) * 1.8f, gy = lp.y + 0.58f;
    glDisable(GL_LIGHTING); glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE); glDepthMask(GL_FALSE);
    const int sg = 56; const float ri = 7.0f, ro = 24.0f;
    glBegin(GL_TRIANGLE_FAN);
    glColor4f(1.0f, 0.85f, 0.25f, 0.68f); glVertex3f(cx, gy, cz);
    glColor4f(0.90f, 0.70f, 0.10f, 0.25f);
    for (int i = 0; i <= sg; i++) { float a = 2 * 3.14159f * (float)i / sg; glVertex3f(cx + cosf(a) * ri, gy, cz + sinf(a) * ri); }
    glEnd();
    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= sg; i++) {
        float a = 2 * 3.14159f * (float)i / sg, ca = cosf(a), sa = sinf(a);
        glColor4f(0.90f, 0.70f, 0.10f, 0.22f); glVertex3f(cx + ca * ri, gy, cz + sa * ri);
        glColor4f(0.80f, 0.55f, 0.05f, 0.0f); glVertex3f(cx + ca * ro, gy, cz + sa * ro);
    }
    glEnd();
    glDepthMask(GL_TRUE); glDisable(GL_BLEND); glEnable(GL_LIGHTING);
}
void DrawAllLampGlows() { for (const auto& lp : lampPosts) DrawLampGlow(lp); }

// ===================== UMBRE STALPI =====================
void DrawLampPostShadow(const LampPost& lp, GLfloat ls[4]) {
    GLfloat pl[4] = { 0,1,0,-(lp.y + 0.55f) };
    GLfloat mat[16]; shadowMatrix(mat, ls, pl);
    glPushMatrix(); glMultMatrixf(mat); glTranslatef(lp.x, lp.y, lp.z);
    float ph = lp.lightY - lp.y, al = 1.8f, ah = 0.18f, aw = 0.18f;
    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= 10; i++) {
        float a = 2 * 3.14159f * (float)i / 10.0f;
        glVertex3f(cosf(a) * 0.25f, 0, sinf(a) * 0.25f);
        glVertex3f(cosf(a) * 0.25f, ph, sinf(a) * 0.25f);
    }
    glEnd();
    glBegin(GL_QUADS);
    glVertex3f(-aw, ph, al); glVertex3f(aw, ph, al); glVertex3f(aw, ph, 0); glVertex3f(-aw, ph, 0);
    glVertex3f(-aw, ph - ah, 0); glVertex3f(aw, ph - ah, 0); glVertex3f(aw, ph - ah, al); glVertex3f(-aw, ph - ah, al);
    glEnd();
    glPopMatrix();
}

// ===================== UMBRE CLADIRI =====================
void DrawBuildingShadowDirect(float bx, float bz, float rot, float W, float H, float D, GLfloat ls[4]) {
    float gy = HeightFunc(bx, bz) + 0.62f;
    float lx = ls[0], ly = ls[1], lz = ls[2];
    float sx = -(lx / ly) * H, sz = -(lz / ly) * H;
    float rad = rot * 3.14159f / 180.0f, cr = cosf(rad), sr = sinf(rad);
    float corners[4][2] = {
        {bx + cr * (-W) - sr * (-D),bz + sr * (-W) + cr * (-D)},
        {bx + cr * (W)-sr * (-D),bz + sr * (W)+cr * (-D)},
        {bx + cr * (W)-sr * (D),bz + sr * (W)+cr * (D)},
        {bx + cr * (-W) - sr * (D),bz + sr * (-W) + cr * (D)},
    };
    float proj[4][2];
    for (int i = 0; i < 4; i++) { proj[i][0] = corners[i][0] + sx; proj[i][1] = corners[i][1] + sz; }
    glBegin(GL_QUADS);
    for (int i = 0; i < 4; i++) {
        int j = (i + 1) % 4;
        glVertex3f(corners[i][0], gy, corners[i][1]); glVertex3f(corners[j][0], gy, corners[j][1]);
        glVertex3f(proj[j][0], gy, proj[j][1]);      glVertex3f(proj[i][0], gy, proj[i][1]);
    }
    glEnd();
    glBegin(GL_QUADS);
    glVertex3f(proj[0][0], gy, proj[0][1]); glVertex3f(proj[1][0], gy, proj[1][1]);
    glVertex3f(proj[2][0], gy, proj[2][1]); glVertex3f(proj[3][0], gy, proj[3][1]);
    glEnd();
    glBegin(GL_QUADS);
    glVertex3f(corners[0][0], gy, corners[0][1]); glVertex3f(corners[1][0], gy, corners[1][1]);
    glVertex3f(corners[2][0], gy, corners[2][1]); glVertex3f(corners[3][0], gy, corners[3][1]);
    glEnd();
}



static void ConvexHull2D(std::vector<std::pair<float, float>>& pts) {
    int n = (int)pts.size(); if (n < 3)return;
    int bot = 0;
    for (int i = 1; i < n; i++)
        if (pts[i].second < pts[bot].second || (pts[i].second == pts[bot].second && pts[i].first < pts[bot].first))bot = i;
    std::swap(pts[0], pts[bot]);
    float ox = pts[0].first, oz = pts[0].second;
    std::sort(pts.begin() + 1, pts.end(), [&](const std::pair<float, float>& a, const std::pair<float, float>& b2) {
        float ax = a.first - ox, az = a.second - oz, bx = b2.first - ox, bz = b2.second - oz;
        float cross = ax * bz - az * bx;
        if (fabsf(cross) > 0.0001f)return cross > 0;
        return (ax * ax + az * az) < (bx * bx + bz * bz);
        });
    std::vector<std::pair<float, float>> hull;
    for (auto& p : pts) {
        while (hull.size() >= 2) {
            auto& a = hull[hull.size() - 2]; auto& b2 = hull[hull.size() - 1];
            float cross = (b2.first - a.first) * (p.second - a.second) - (b2.second - a.second) * (p.first - a.first);
            if (cross <= 0)hull.pop_back(); else break;
        }
        hull.push_back(p);
    }
    pts = hull;
}

void DrawBenchShadow(const Bench& b, GLfloat ls[4], float alpha, float yOff) {
    if (!benchLoaded) return;
    float groundY = HeightFunc(b.x, b.z);
    float gy = groundY + yOff;
    const float scale = 5.0f;
    float rad = b.rot * 3.14159f / 180.0f, cr = cosf(rad), sr = sinf(rad);
    std::vector<std::pair<float, float>> shadow2D;
    shadow2D.reserve(benchMesh.verts.size());
    for (const auto& vraw : benchMesh.verts) {
        if (fabsf(vraw.x) > 2.0f || fabsf(vraw.z) > 2.0f || vraw.y > 1.5f) continue;
        float lx = vraw.x * scale, ly = vraw.y * scale, lz = vraw.z * scale;
        float wx = b.x + cr * lx - sr * lz;
        float wz = b.z + sr * lx + cr * lz;
        float h = ly - yOff; 

        float ox, oz;
        if (ls[3] < 0.5f) {
            ox = wx - (ls[0] / ls[1]) * h;
            oz = wz - (ls[2] / ls[1]) * h;
        }
        else {
           
            float wy = groundY + ly;
            float dy = wy - ls[1];
            if (fabsf(dy) < 0.0001f) { ox = wx; oz = wz; }
            else {
                float t = (gy - ls[1]) / dy;
                ox = ls[0] + t * (wx - ls[0]);
                oz = ls[2] + t * (wz - ls[2]);
            }
        }
        shadow2D.push_back({ ox, oz });
    }
    ConvexHull2D(shadow2D);
    if (shadow2D.size() < 3) return;
    glColor4f(0.0f, 0.0f, 0.0f, alpha);
    glBegin(GL_TRIANGLE_FAN);
    for (auto& p : shadow2D) glVertex3f(p.first, gy, p.second);
    glEnd();
}

// ===================== UMBRE BANCI =====================
void DrawAllBenchShadows() {
    if (benches.empty() || !benchLoaded) return;
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-5, -5);

    for (const auto& b : benches) {
        float gy = HeightFunc(b.x, b.z) + 0.55f;
        float H = 4.0f; 
        float sx = -(sunPos[0] / sunPos[1]) * H;
        float sz = -(sunPos[2] / sunPos[1]) * H;
        float rad = b.rot * 3.14159f / 180.0f;
        float cr = cosf(rad), sr = sinf(rad);
        float W = 0.75f * 5.0f; 
        float D = 0.25f * 5.0f;
        float corners[4][2] = {
            { b.x + cr * (-W) - sr * (-D),  b.z + sr * (-W) + cr * (-D) },
            { b.x + cr * (W)-sr * (-D),  b.z + sr * (W)+cr * (-D) },
            { b.x + cr * (W)-sr * (D),  b.z + sr * (W)+cr * (D) },
            { b.x + cr * (-W) - sr * (D),  b.z + sr * (-W) + cr * (D) },
        };
        float proj[4][2];
        for (int i = 0; i < 4; i++) {
            proj[i][0] = corners[i][0] + sx;
            proj[i][1] = corners[i][1] + sz;
        }
        glColor4f(0.0f, 0.0f, 0.0f, 0.12f);  

        glBegin(GL_QUADS);
        for (int i = 0; i < 4; i++) {
            int j = (i + 1) % 4;
            glVertex3f(corners[i][0], gy, corners[i][1]);
            glVertex3f(corners[j][0], gy, corners[j][1]);
            glVertex3f(proj[j][0], gy, proj[j][1]);
            glVertex3f(proj[i][0], gy, proj[i][1]);
        }
        glEnd();

        glBegin(GL_QUADS);
        glVertex3f(proj[0][0], gy, proj[0][1]);
        glVertex3f(proj[1][0], gy, proj[1][1]);
        glVertex3f(proj[2][0], gy, proj[2][1]);
        glVertex3f(proj[3][0], gy, proj[3][1]);
        glEnd();

        glBegin(GL_QUADS);
        glVertex3f(corners[0][0], gy, corners[0][1]);
        glVertex3f(corners[1][0], gy, corners[1][1]);
        glVertex3f(corners[2][0], gy, corners[2][1]);
        glVertex3f(corners[3][0], gy, corners[3][1]);
        glEnd();

        if (b.lampIdx >= 0 && b.lampIdx < (int)lampPosts.size()) {
            const LampPost& lp = lampPosts[b.lampIdx];
            float r = lp.armAngle * 3.14159f / 180.0f;
            GLfloat lampLight[4] = {
                lp.x + sinf(r) * 1.8f,
                lp.lightY,
                lp.z + cosf(r) * 1.8f,
                1.0f
            };
            DrawBenchShadow(b, lampLight, 0.52f, 0.62f);
        }
    }

    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_LIGHTING);
}

// ===================== DESENARE BANCA =====================
void DrawObjMesh(const ObjMesh& mesh, float scale) {
    glDisable(GL_TEXTURE_2D);
    glColor3f(0.55f, 0.35f, 0.18f); 
    for (const auto& face : mesh.faces) {
        bool skip = false;
        for (int i = 0; i < face.vcount; i++) {
            const auto& v = mesh.verts[face.vi[i]];
            if (fabsf(v.x) > 2.0f || fabsf(v.z) > 2.0f || v.y > 1.5f) { skip = true; break; }
        }
        if (skip) continue;
        GLenum mode = (face.vcount == 4) ? GL_QUADS : GL_TRIANGLES;
        glBegin(mode);
        for (int i = 0; i < face.vcount; i++) {
            if (face.ni[i] >= 0) { const auto& n = mesh.normals[face.ni[i]]; glNormal3f(n.x, n.y, n.z); }
            const auto& v = mesh.verts[face.vi[i]];
            glVertex3f(v.x * scale, v.y * scale, v.z * scale);
        }
        glEnd();
    }
}

void DrawBench(const Bench& b) {
    if (!benchLoaded) return;
    float y = HeightFunc(b.x, b.z);
    glPushMatrix();
    glTranslatef(b.x, y, b.z);
    glRotatef(b.rot, 0, 1, 0);
    DrawObjMesh(benchMesh, 5.0f);
    glPopMatrix();
}
void DrawAllBenches() { for (const auto& b : benches) DrawBench(b); }

// ===================== UMBRA PROPRIE STALP =====================
void DrawLampSelfShadow(const LampPost& lp) {
    float rad = deg2rad(lp.armAngle);
    float lx = lp.x + sinf(rad) * 1.8f, lz = lp.z + cosf(rad) * 1.8f;
    float dx = lp.x - lx, dz = lp.z - lz;
    float len = sqrtf(dx * dx + dz * dz); if (len < 0.0001f)return;
    dx /= len; dz /= len;
    float nx = -dz, nz = dx;
    float y = lp.y + 0.57f, bw = 0.45f, sl = 7.5f;
    float bx1 = lp.x + nx * bw, bz1 = lp.z + nz * bw;
    float bx2 = lp.x - nx * bw, bz2 = lp.z - nz * bw;
    float ex1 = lp.x + dx * sl + nx * 0.15f, ez1 = lp.z + dz * sl + nz * 0.15f;
    float ex2 = lp.x + dx * sl - nx * 0.15f, ez2 = lp.z + dz * sl - nz * 0.15f;
    glDisable(GL_LIGHTING); glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glDepthMask(GL_FALSE);
    glBegin(GL_QUADS);
    glColor4f(0, 0, 0, 0.32f); glVertex3f(bx1, y, bz1); glVertex3f(bx2, y, bz2);
    glColor4f(0, 0, 0, 0.0f); glVertex3f(ex2, y, ez2); glVertex3f(ex1, y, ez1);
    glEnd();
    glDepthMask(GL_TRUE); glDisable(GL_BLEND); glEnable(GL_LIGHTING);
}
void DrawAllLampSelfShadows() { for (const auto& lp : lampPosts) DrawLampSelfShadow(lp); }

// ===================== TOATE UMBRELE =====================
void DrawAllShadows() {
    glDisable(GL_LIGHTING); glDisable(GL_TEXTURE_2D);
    // PASS 1: umbre stalpi (stencil)
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 1, 0xFF); glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    glColorMask(0, 0, 0, 0); glDepthMask(0);
    for (const auto& lp : lampPosts) DrawLampPostShadow(lp, sunPos);
    glColorMask(1, 1, 1, 1); glDepthMask(1);
    glStencilFunc(GL_EQUAL, 1, 0xFF); glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glColor4f(0, 0, 0, 0.42f);
    glBegin(GL_QUADS);
    glVertex3f(-500, 1.5f, -500); glVertex3f(500, 1.5f, -500);
    glVertex3f(500, 1.5f, 500);  glVertex3f(-500, 1.5f, 500);
    glEnd();
    glEnable(GL_DEPTH_TEST); glDisable(GL_BLEND);
    // PASS 2: umbre cladiri
    glDisable(GL_STENCIL_TEST); glClear(GL_STENCIL_BUFFER_BIT);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(0); glEnable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(-4, -4);
    glColor4f(0, 0, 0, 0.35f);
    float d = 22, h = 35, w = 12;
    struct B { float x, z, r, w, h, d; };
    B bs[] = {
        {-d,-120,90,w,h,8},{d,-160,-90,w,h,8},
        {-d, 120,90,w,h,8},{d, 160,-90,w,h,8},
        {140,d,0,8,h,w},{180,-d,180,8,h,w},
        {-140,d,0,8,h,w},{-180,-d,180,8,h,w}
    };
    for (auto& b : bs) DrawBuildingShadowDirect(b.x, b.z, b.r, b.w, b.h, b.d, sunPos);
    glDisable(GL_POLYGON_OFFSET_FILL); glDisable(GL_BLEND);
    glDepthMask(1); glColorMask(1, 1, 1, 1); glEnable(GL_DEPTH_TEST); glEnable(GL_LIGHTING);
}

// ===================== CAMERA =====================
void UpdateCamera() {
    float fx = cosf(deg2rad(yaw)) * cosf(deg2rad(pitch));
    float fy = sinf(deg2rad(pitch));
    float fz = sinf(deg2rad(yaw)) * cosf(deg2rad(pitch));
    float rx = fz, rz = -fx;
    if (keys['w'] || keys['W']) { camX += fx * moveSpeed; camY += fy * moveSpeed; camZ += fz * moveSpeed; }
    if (keys['s'] || keys['S']) { camX -= fx * moveSpeed; camY -= fy * moveSpeed; camZ -= fz * moveSpeed; }
    if (keys['a'] || keys['A']) { camX += rx * moveSpeed; camZ -= rz * moveSpeed; }
    if (keys['d'] || keys['D']) { camX -= rx * moveSpeed; camZ += rz * moveSpeed; }
    if (keys['q'] || keys['Q'])camY += moveSpeed;
    if (keys['e'] || keys['E'])camY -= moveSpeed;
    camY = clampf(camY, 2, 150);
}

// ===================== DISPLAY =====================
void display() {
    UpdateCamera();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    float fx = cosf(deg2rad(yaw)) * cosf(deg2rad(pitch));
    float fy = sinf(deg2rad(pitch));
    float fz = sinf(deg2rad(yaw)) * cosf(deg2rad(pitch));
    gluLookAt(camX, camY, camZ, camX + fx, camY + fy, camZ + fz, 0, 1, 0);
    glEnable(GL_LIGHTING);
    GLfloat ga[] = { 0.10f,0.10f,0.12f,1 };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ga);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    ActivateLampLights();
    glPushMatrix(); glTranslatef(camX, camY, camZ); DrawSkybox2D(520); glPopMatrix();
    DrawGround(260, 0, 45);
    DrawRelief(90, 150, 24);
    DrawRoads();
    float d = 22, h = 35, w = 12;
    DrawRealisticBlock(-d, -120, 90, w, h, 8); DrawRealisticBlock(d, -160, -90, w, h, 8);
    DrawRealisticBlock(-d, 120, 90, w, h, 8); DrawRealisticBlock(d, 160, -90, w, h, 8);
    DrawRealisticBlock(140, d, 0, 8, h, w);    DrawRealisticBlock(180, -d, 180, 8, h, w);
    DrawRealisticBlock(-140, d, 0, 8, h, w);   DrawRealisticBlock(-180, -d, 180, 8, h, w);
    DrawAllShadows();
    DrawAllLampSelfShadows();
    DrawAllBenchShadows();
    DrawAllBenches();
    DrawAllLampPosts();
    DrawAllLampGlows();
    float to = 13, ts = 7;
    for (int z = -220; z <= 220; z += 35) {
        if (abs(z) < 65)continue;
        bool zn = (z > -180 && z < -100) || (z > 100 && z < 180);
        if (!zn) { DrawTree(-to, (float)z, ts); DrawTree(to, (float)z, ts); }
    }
    for (int x = -220; x <= 220; x += 35) {
        if (abs(x) < 65)continue;
        bool xn = (x > 120 && x < 200) || (x > -200 && x < -120);
        if (!xn) { DrawTree((float)x, -to, ts); DrawTree((float)x, to, ts); }
    }
    glutSwapBuffers();
}

// ===================== RESHAPE =====================
void reshape(int w, int h) {
    if (!h)h = 1; glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    gluPerspective(60, (double)w / h, 0.1, 2000);
    glMatrixMode(GL_MODELVIEW);
}
void keyboardDown(unsigned char k, int, int) { keys[k] = true; if (k == 27)exit(0); }
void keyboardUp(unsigned char k, int, int) { keys[k] = false; }
void mouseButton(int btn, int state, int x, int y) {
    if (state == GLUT_DOWN) {
        float fx = cosf(deg2rad(yaw)) * cosf(deg2rad(pitch));
        float fy = sinf(deg2rad(pitch));
        float fz = sinf(deg2rad(yaw)) * cosf(deg2rad(pitch));
        if (btn == 3) { camX += fx * 5; camY += fy * 5; camZ += fz * 5; }
        if (btn == 4) { camX -= fx * 5; camY -= fy * 5; camZ -= fz * 5; }
        camY = clampf(camY, 2, 400);
    }
    if (btn == GLUT_RIGHT_BUTTON) { rightMouseDown = (state == GLUT_DOWN); lastMouseX = x; lastMouseY = y; }
}
void mouseMove(int x, int y) {
    if (!rightMouseDown)return;
    yaw += (x - lastMouseX) * mouseSensitivity;
    pitch -= (y - lastMouseY) * mouseSensitivity;
    pitch = clampf(pitch, -89, 89);
    lastMouseX = x; lastMouseY = y;
}

// ===================== INIT =====================
void InitScene() {
    glEnable(GL_DEPTH_TEST); glEnable(GL_STENCIL_TEST); glEnable(GL_NORMALIZE);
    glClearColor(0.08f, 0.08f, 0.12f, 1);
    texPX = LoadTexture2D("px.png", false, false);   texNX = LoadTexture2D("nx.png", false, false);
    texPY = LoadTexture2D("py.png", false, false);   texNY = LoadTexture2D("ny.png", false, false);
    texPZ = LoadTexture2D("nz.png", false, false);   texNZ = LoadTexture2D("pz.png", false, false);
    texGround = LoadTexture2D("aerial_grass_rock_diff_4k.jpg", true, true);
    texRoad = LoadTexture2D("road.jpg", true, true);
    texBuilding = LoadTexture2D("14_old building texture.jpg", true, true);
    texTree = LoadTexture2D("tree.png", false, true);
    texStalp = LoadTexture2D("stalp.jpg", true, true);
    if (!texPX || !texGround || !texStalp)std::cout << "Atentie: Lipsesc texturi!\n";
    BuildLampPosts();
    benchLoaded = LoadOBJ("bankblender.obj", benchMesh);
    BuildBenches();
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_STENCIL);
    glutInitWindowSize(1200, 700);
    glutCreateWindow("Circuit Stradal - Lumina si Umbre");
    InitScene();
    glutDisplayFunc(display); glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboardDown); glutKeyboardUpFunc(keyboardUp);
    glutMouseFunc(mouseButton); glutMotionFunc(mouseMove);
    glutIdleFunc(display); glutMainLoop(); return 0;
}

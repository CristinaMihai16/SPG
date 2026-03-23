#include <windows.h>
#include <freeglut.h>
#include <iostream>
#include <cmath>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

// Texturi
GLuint texPX = 0, texNX = 0, texPY = 0, texNY = 0, texPZ = 0, texNZ = 0;
GLuint texGround = 0;
GLuint texRoad = 0;
GLuint texBuilding = 0; 
GLuint texTree = 0;

// Camera
float camX = 0.0f, camY = 18.0f, camZ = 80.0f;
float yaw = -90.0f;
float pitch = -12.0f;
float moveSpeed = 1.2f;
float mouseSensitivity = 0.22f;
bool rightMouseDown = false;
int lastMouseX = -1, lastMouseY = -1;
bool keys[256] = { false };

static float deg2rad(float d) { return d * 3.1415926535f / 180.0f; }
static float clampf(float v, float a, float b) { return (v < a) ? a : (v > b ? b : v); }

// ===================== INCARCARE TEXTURA =====================
GLuint LoadTexture2D(const char* filename, bool repeat, bool flipVertical)
{
    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);

    int w, h, c;
    stbi_set_flip_vertically_on_load(flipVertical ? 1 : 0);

    unsigned char* data = stbi_load(filename, &w, &h, &c, 0);
    if (!data)
    {
        std::cout << "Nu pot incarca textura: " << filename << "\n";
        return 0;
    }

    GLenum format = (c == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, data);
    stbi_image_free(data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    if (repeat)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }
    else
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    return texID;
}

// ===================== RELIEF =====================
static float hash2(int x, int y)
{
    unsigned int h = (unsigned int)(x * 374761393u + y * 668265263u);
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= (h >> 16);
    return (h & 0x00FFFFFF) / 16777215.0f;
}
static float lerp(float a, float b, float t) { return a + (b - a) * t; }
static float smooth(float t) { return t * t * (3.0f - 2.0f * t); }

static float valueNoise(float x, float z)
{
    int x0 = (int)floorf(x);
    int z0 = (int)floorf(z);
    int x1 = x0 + 1;
    int z1 = z0 + 1;
    float fx = x - x0;
    float fz = z - z0;
    float v00 = hash2(x0, z0);
    float v10 = hash2(x1, z0);
    float v01 = hash2(x0, z1);
    float v11 = hash2(x1, z1);
    float sx = smooth(fx);
    float sz = smooth(fz);
    float ix0 = lerp(v00, v10, sx);
    float ix1 = lerp(v01, v11, sx);
    return lerp(ix0, ix1, sz) * 2.0f - 1.0f;
}

static float fbm(float x, float z)
{
    float sum = 0.0f;
    float amp = 1.0f;
    float freq = 1.0f;
    for (int i = 0; i < 5; i++)
    {
        sum += amp * valueNoise(x * freq, z * freq);
        amp *= 0.5f;
        freq *= 2.0f;
    }
    return sum;
}

static float bump(float x, float z, float cx, float cz, float amp, float radius)
{
    float dx = x - cx;
    float dz = z - cz;
    float r2 = dx * dx + dz * dz;
    return amp * expf(-r2 / (radius * radius));
}

float HeightFunc(float x, float z)
{
    float base = 3.0f * fbm(x * 0.06f, z * 0.06f) + 1.6f * fbm(x * 0.15f, z * 0.15f);
    float hills =
        bump(x, z, -35.0f, -15.0f, 7.0f, 28.0f) +
        bump(x, z, 30.0f, 20.0f, 6.0f, 24.0f) +
        bump(x, z, 5.0f, -40.0f, 5.5f, 22.0f);

    float h = base + hills;

    float roadWidth = 8.0f;
    float ringRadius = 55.0f;
    float distToCenter = sqrtf(x * x + z * z);

    if (fabs(distToCenter - ringRadius) < roadWidth) return 0.0f;
    if (fabs(x) < roadWidth && fabs(z) > (ringRadius - roadWidth)) return 0.0f;
    if (fabs(z) < roadWidth && fabs(x) > (ringRadius - roadWidth)) return 0.0f;

    return clampf(h, -2.0f, 14.0f);
}

// ===================== DESENARE ELEMENTE =====================

void DrawSkybox2D(float s = 520.0f)
{
    glDepthMask(GL_FALSE);
    glDisable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);
    glColor3f(1, 1, 1);
    const float eps = 0.0018f;
    const float u0 = eps, u1 = 1.0f - eps;
    const float v0 = eps, v1 = 1.0f - eps;

    auto BindSky = [&](GLuint t) {
        glBindTexture(GL_TEXTURE_2D, t);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        };

    BindSky(texPZ); glBegin(GL_QUADS); glTexCoord2f(u0, v0); glVertex3f(-s, -s, s); glTexCoord2f(u1, v0); glVertex3f(s, -s, s); glTexCoord2f(u1, v1); glVertex3f(s, s, s); glTexCoord2f(u0, v1); glVertex3f(-s, s, s); glEnd();
    BindSky(texNZ); glBegin(GL_QUADS); glTexCoord2f(u0, v0); glVertex3f(s, -s, -s); glTexCoord2f(u1, v0); glVertex3f(-s, -s, -s); glTexCoord2f(u1, v1); glVertex3f(-s, s, -s); glTexCoord2f(u0, v1); glVertex3f(s, s, -s); glEnd();
    BindSky(texPX); glBegin(GL_QUADS); glTexCoord2f(u0, v0); glVertex3f(s, -s, s); glTexCoord2f(u1, v0); glVertex3f(s, -s, -s); glTexCoord2f(u1, v1); glVertex3f(s, s, -s); glTexCoord2f(u0, v1); glVertex3f(s, s, s); glEnd();
    BindSky(texNX); glBegin(GL_QUADS); glTexCoord2f(u0, v0); glVertex3f(-s, -s, -s); glTexCoord2f(u1, v0); glVertex3f(-s, -s, s); glTexCoord2f(u1, v1); glVertex3f(-s, s, s); glTexCoord2f(u0, v1); glVertex3f(-s, s, -s); glEnd();
    BindSky(texPY); glBegin(GL_QUADS); glTexCoord2f(u0, v0); glVertex3f(-s, s, s); glTexCoord2f(u1, v0); glVertex3f(s, s, s); glTexCoord2f(u1, v1); glVertex3f(s, s, -s); glTexCoord2f(u0, v1); glVertex3f(-s, s, -s); glEnd();
    BindSky(texNY); glBegin(GL_QUADS); glTexCoord2f(u0, v0); glVertex3f(-s, -s, -s); glTexCoord2f(u1, v0); glVertex3f(s, -s, -s); glTexCoord2f(u1, v1); glVertex3f(s, -s, s); glTexCoord2f(u0, v1); glVertex3f(-s, -s, s); glEnd();

    glDisable(GL_TEXTURE_2D);
    glDepthMask(GL_TRUE);
}

void DrawGround(float size = 260.0f, float y = 0.0f, float tile = 45.0f)
{
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texGround);
    glColor3f(1, 1, 1);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex3f(-size, y, -size);
    glTexCoord2f(tile, 0); glVertex3f(size, y, -size);
    glTexCoord2f(tile, tile); glVertex3f(size, y, size);
    glTexCoord2f(0, tile); glVertex3f(-size, y, size);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

void DrawRelief(float half = 90.0f, int steps = 150, float uvTiling = 24.0f)
{
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texGround);
    glColor3f(1, 1, 1);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.0f, -1.0f);

    float step = (2.0f * half) / steps;
    for (int z = 0; z < steps; z++)
    {
        float z0 = -half + z * step;
        float z1 = z0 + step;
        float v0 = (float)z / steps * uvTiling;
        float v1 = (float)(z + 1) / steps * uvTiling;

        glBegin(GL_TRIANGLE_STRIP);
        for (int x = 0; x <= steps; x++)
        {
            float x0 = -half + x * step;
            float baseY = 0.45f;
            float y0 = baseY + HeightFunc(x0, z0);
            float y1 = baseY + HeightFunc(x0, z1);
            float u = (float)x / steps * uvTiling;
            glTexCoord2f(u, v0); glVertex3f(x0, y0, z0);
            glTexCoord2f(u, v1); glVertex3f(x0, y1, z1);
        }
        glEnd();
    }
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_TEXTURE_2D);
}

void DrawRoads()
{
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texRoad);
    glColor3f(1.0f, 1.0f, 1.0f);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-3.0f, -3.0f);

    float roadWidth = 7.0f;
    float ringRadius = 55.0f;
    int segments = 150;
    float roadHeight = 0.52f;

    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= segments; i++) {
        float angle = 2.0f * 3.14159f * (float)i / (float)segments;
        float xIn = cosf(angle) * (ringRadius - roadWidth);
        float zIn = sinf(angle) * (ringRadius - roadWidth);
        float xOut = cosf(angle) * (ringRadius + roadWidth);
        float zOut = sinf(angle) * (ringRadius + roadWidth);
        float u = (float)i / (float)segments * 12.0f;
        glTexCoord2f(0.0f, u); glVertex3f(xIn, roadHeight, zIn);
        glTexCoord2f(1.0f, u); glVertex3f(xOut, roadHeight, zOut);
    }
    glEnd();

    auto DrawFlatStraight = [&](float sX, float sZ, float eX, float eZ, bool horiz) {
        int detailSteps = 50;
        glBegin(GL_TRIANGLE_STRIP);
        for (int i = 0; i <= detailSteps; i++) {
            float t = (float)i / detailSteps;
            float currX = sX + (eX - sX) * t;
            float currZ = sZ + (eZ - sZ) * t;
            float xL, zL, xR, zR;
            if (!horiz) { xL = currX - roadWidth; zL = currZ; xR = currX + roadWidth; zR = currZ; }
            else { xL = currX; zL = currZ - roadWidth; xR = currX; zR = currZ + roadWidth; }
            float u_straight = t * 15.0f;
            glTexCoord2f(0.0f, u_straight); glVertex3f(xL, roadHeight, zL);
            glTexCoord2f(1.0f, u_straight); glVertex3f(xR, roadHeight, zR);
        }
        glEnd();
        };

    float extent = 240.0f;
    DrawFlatStraight(0, ringRadius, 0, extent, false);
    DrawFlatStraight(0, -ringRadius, 0, -extent, false);
    DrawFlatStraight(ringRadius, 0, extent, 0, true);
    DrawFlatStraight(-ringRadius, 0, -extent, 0, true);

    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_TEXTURE_2D);
}

void DrawRealisticBlock(float x, float z, float rot, float width, float height, float depth) {
    float y = HeightFunc(x, z);
    glPushMatrix();
    glTranslatef(x, y, z);
    glRotatef(rot, 0, 1, 0);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texBuilding);
    glColor3f(1.0f, 1.0f, 1.0f);

    float texRepeatW = width / 8.0f;
    float texRepeatH = height / 12.0f;
    float texRepeatD = depth / 8.0f;

    glBegin(GL_QUADS);
    // Fat
    glTexCoord2f(0.0f, 0.0f);           glVertex3f(-width, 0, depth);
    glTexCoord2f(texRepeatW, 0.0f);     glVertex3f(width, 0, depth);
    glTexCoord2f(texRepeatW, texRepeatH); glVertex3f(width, height, depth);
    glTexCoord2f(0.0f, texRepeatH);     glVertex3f(-width, height, depth);

    // Spate
    glTexCoord2f(0.0f, 0.0f);           glVertex3f(-width, 0, -depth);
    glTexCoord2f(texRepeatW, 0.0f);     glVertex3f(width, 0, -depth);
    glTexCoord2f(texRepeatW, texRepeatH); glVertex3f(width, height, -depth);
    glTexCoord2f(0.0f, texRepeatH);     glVertex3f(-width, height, -depth);

    // Dreapta
    glTexCoord2f(0.0f, 0.0f);           glVertex3f(width, 0, -depth);
    glTexCoord2f(texRepeatD, 0.0f);     glVertex3f(width, 0, depth);
    glTexCoord2f(texRepeatD, texRepeatH); glVertex3f(width, height, depth);
    glTexCoord2f(0.0f, texRepeatH);     glVertex3f(width, height, -depth);

    // Stanga
    glTexCoord2f(0.0f, 0.0f);           glVertex3f(-width, 0, -depth);
    glTexCoord2f(texRepeatD, 0.0f);     glVertex3f(-width, 0, depth);
    glTexCoord2f(texRepeatD, texRepeatH); glVertex3f(-width, height, depth);
    glTexCoord2f(0.0f, texRepeatH);     glVertex3f(-width, height, -depth);
    glEnd();

    glDisable(GL_TEXTURE_2D);

    // Acoperis 
    glColor3f(0.2f, 0.2f, 0.2f);
    glBegin(GL_QUADS);
    glVertex3f(-width, height, depth);
    glVertex3f(width, height, depth);
    glVertex3f(width, height, -depth);
    glVertex3f(-width, height, -depth);
    glEnd();

    glPopMatrix();
}

void DrawTree(float x, float z, float scale) {
    float y = HeightFunc(x, z);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texTree);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.1f);
    glPushMatrix();
    glTranslatef(x, y, z);

    for (int i = 0; i < 2; i++) {
        glRotatef(i * 90.0f, 0, 1, 0);
        glBegin(GL_QUADS);
        glColor3f(1, 1, 1);
        glTexCoord2f(0, 0); glVertex3f(-scale, 0, 0);
        glTexCoord2f(1, 0); glVertex3f(scale, 0, 0);
        glTexCoord2f(1, 1); glVertex3f(scale, scale * 2, 0);
        glTexCoord2f(0, 1); glVertex3f(-scale, scale * 2, 0);
        glEnd();
    }

    glPopMatrix();
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
}


void UpdateCamera()
{
    float fx = cosf(deg2rad(yaw)) * cosf(deg2rad(pitch));
    float fy = sinf(deg2rad(pitch));
    float fz = sinf(deg2rad(yaw)) * cosf(deg2rad(pitch));
    float rx = fz; float rz = -fx;

    if (keys['w'] || keys['W']) { camX += fx * moveSpeed; camY += fy * moveSpeed; camZ += fz * moveSpeed; }
    if (keys['s'] || keys['S']) { camX -= fx * moveSpeed; camY -= fy * moveSpeed; camZ -= fz * moveSpeed; }
    if (keys['a'] || keys['A']) { camX += rx * moveSpeed; camZ -= rz * moveSpeed; }
    if (keys['d'] || keys['D']) { camX -= rx * moveSpeed; camZ += rz * moveSpeed; }
    if (keys['q'] || keys['Q']) { camY += moveSpeed; }
    if (keys['e'] || keys['E']) { camY -= moveSpeed; }
    camY = clampf(camY, 2.0f, 150.0f);
}

void display()
{
    UpdateCamera();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    float fx = cosf(deg2rad(yaw)) * cosf(deg2rad(pitch));
    float fy = sinf(deg2rad(pitch));
    float fz = sinf(deg2rad(yaw)) * cosf(deg2rad(pitch));

    gluLookAt(camX, camY, camZ, camX + fx, camY + fy, camZ + fz, 0, 1, 0);

    glPushMatrix();
    glTranslatef(camX, camY, camZ);
    DrawSkybox2D(520.0f);
    glPopMatrix();

    DrawGround(260.0f, 0.0f, 45.0f);
    DrawRelief(90.0f, 150, 24.0f);
    DrawRoads();

    float dist = 22.0f;
    float h = 35.0f;
    float w = 12.0f;

    DrawRealisticBlock(-dist, -120, 90, w, h, 8);
    DrawRealisticBlock(dist, -160, -90, w, h, 8);
    DrawRealisticBlock(-dist, 120, 90, w, h, 8);
    DrawRealisticBlock(dist, 160, -90, w, h, 8);
    DrawRealisticBlock(140, dist, 0, 8, h, w);
    DrawRealisticBlock(180, -dist, 180, 8, h, w);
    DrawRealisticBlock(-140, dist, 0, 8, h, w);
    DrawRealisticBlock(-180, -dist, 180, 8, h, w);


    float treeOffset = 13.0f; 
    float treeScale = 7.0f;
    for (int z = -220; z <= 220; z += 35) {
        if (abs(z) < 65) continue; 
        bool zoneCladireZ = (z > -180 && z < -100) || (z > 100 && z < 180);

        if (!zoneCladireZ) {
            DrawTree(-treeOffset, (float)z, treeScale);
            DrawTree(treeOffset, (float)z, treeScale);
        }
    }

    for (int x = -220; x <= 220; x += 35) {
        if (abs(x) < 65) continue; 


        bool zoneCladireX = (x > 120 && x < 200) || (x > -200 && x < -120);

        if (!zoneCladireX) {
            DrawTree((float)x, -treeOffset, treeScale);
            DrawTree((float)x, treeOffset, treeScale);
        }
    }
    glutSwapBuffers();
}

void reshape(int w, int h)
{
    if (h == 0) h = 1;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0, (double)w / (double)h, 0.1, 2000.0);
    glMatrixMode(GL_MODELVIEW);
}

void keyboardDown(unsigned char key, int, int) { keys[key] = true; if (key == 27) exit(0); }
void keyboardUp(unsigned char key, int, int) { keys[key] = false; }

void mouseButton(int button, int state, int x, int y)
{
    if (state == GLUT_DOWN) {
        float fx = cosf(deg2rad(yaw)) * cosf(deg2rad(pitch));
        float fy = sinf(deg2rad(pitch));
        float fz = sinf(deg2rad(yaw)) * cosf(deg2rad(pitch));
        float zoomSpeed = 5.0f; 

        if (button == 3) { 
            camX += fx * zoomSpeed;
            camY += fy * zoomSpeed;
            camZ += fz * zoomSpeed;
        }
        else if (button == 4) { 
            camX -= fx * zoomSpeed;
            camY -= fy * zoomSpeed;
            camZ -= fz * zoomSpeed;
        }
        camY = clampf(camY, 2.0f, 400.0f);
    }

    if (button == GLUT_RIGHT_BUTTON) {
        rightMouseDown = (state == GLUT_DOWN);
        lastMouseX = x;
        lastMouseY = y;
    }
}

void mouseMove(int x, int y)
{
    if (!rightMouseDown) return;
    int dx = x - lastMouseX; int dy = y - lastMouseY;
    lastMouseX = x; lastMouseY = y;
    yaw += dx * mouseSensitivity;
    pitch -= dy * mouseSensitivity;
    pitch = clampf(pitch, -89.0f, 89.0f);
}

void InitScene()
{
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.08f, 0.08f, 0.12f, 1.0f);

    texPX = LoadTexture2D("px.png", false, false);
    texNX = LoadTexture2D("nx.png", false, false);
    texPY = LoadTexture2D("py.png", false, false);
    texNY = LoadTexture2D("ny.png", false, false);
    texPZ = LoadTexture2D("nz.png", false, false);
    texNZ = LoadTexture2D("pz.png", false, false);
    texGround = LoadTexture2D("aerial_grass_rock_diff_4k.jpg", true, true);
    texRoad = LoadTexture2D("road.jpg", true, true);
    texBuilding = LoadTexture2D("14_old building texture.jpg", true, true);
    texTree = LoadTexture2D("tree.png", false, true);


    if (!texPX || !texGround) std::cout << "Atentie: Lipsesc unele texturi!\n";
}

int main(int argc, char** argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(1200, 700);
    glutCreateWindow("Circuit Stradal Adaptat pe Relief");

    InitScene();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboardDown);
    glutKeyboardUpFunc(keyboardUp);
    glutMouseFunc(mouseButton);
    glutMotionFunc(mouseMove);
    glutIdleFunc(display);

    glutMainLoop();
    return 0;
}
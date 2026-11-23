#include "game.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

namespace {

const char* WINDOW_TITLE = "PwAG RPG";
Input gInput;
int gLastTime = 0, gFpsTime = 0, gFpsFrames = 0;
bool gMouseCaptured = false;

void setMouseCapture(bool capture) {
    if (capture == gMouseCaptured) return;
    gMouseCaptured = capture;
    glutSetCursor(capture ? GLUT_CURSOR_NONE : GLUT_CURSOR_LEFT_ARROW);
    if (capture) glutWarpPointer(gInput.width / 2, gInput.height / 2);
}

void display() {
    int now = glutGet(GLUT_ELAPSED_TIME);
    // limit 0.1 s: po zacięciu (np. przy przeciąganiu okna) gracz nie przeskoczy przez ścianę
    float dt = std::min((now - gLastTime) / 1000.0f, 0.1f);
    gLastTime = now;

    gameUpdate(gInput, dt);
    if (game.quit) {
        glutLeaveMainLoop();
        return;
    }
    gameDraw(gInput);
    glutSwapBuffers();
    setMouseCapture(gameWantsMouseCapture());

    std::memset(gInput.pressed, 0, sizeof(gInput.pressed));
    std::memset(gInput.specialPressed, 0, sizeof(gInput.specialPressed));
    gInput.mouseDX = gInput.mouseDY = 0.0f;
    gInput.lmbClick = gInput.rmbClick = false;

    ++gFpsFrames;
    if (now - gFpsTime >= 1000) {
        char title[128];
        std::snprintf(title, sizeof(title), "%s | FPS: %d", WINDOW_TITLE, gFpsFrames * 1000 / (now - gFpsTime));
        glutSetWindowTitle(title);
        gFpsTime = now;
        gFpsFrames = 0;
    }
}

void idle() {
    // ok. 150 klatek/s - bez tego gra zjada cały rdzeń, a laptop robi za suszarkę (przynajmniej u mnie)
    if (glutGet(GLUT_ELAPSED_TIME) - gLastTime < 6) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        return;
    }
    glutPostRedisplay();
}

void reshape(int width, int height) {
    gInput.width = std::max(width, 1);
    gInput.height = std::max(height, 1);
}

void keyDown(unsigned char key, int, int) {
    key = (unsigned char)std::tolower(key);
    if (!gInput.down[key]) gInput.pressed[key] = true;
    gInput.down[key] = true;
}

void keyUp(unsigned char key, int, int) { gInput.down[(unsigned char)std::tolower(key)] = false; }

void specialDown(int key, int, int) {
    if (key < 0 || key > 255) return;
    if (!gInput.specialDown[key]) gInput.specialPressed[key] = true;
    gInput.specialDown[key] = true;
}

void specialUp(int key, int, int) {
    if (key >= 0 && key <= 255) gInput.specialDown[key] = false;
}

void mouseButton(int button, int state, int x, int y) {
    bool down = state == GLUT_DOWN;
    if (button == GLUT_LEFT_BUTTON) {
        if (down && !gInput.lmb) gInput.lmbClick = true;
        gInput.lmb = down;
    } else if (button == GLUT_RIGHT_BUTTON) {
        if (down && !gInput.rmb) gInput.rmbClick = true;
        gInput.rmb = down;
    }
    gInput.mouseX = x;
    gInput.mouseY = y;
}

void mouseMove(int x, int y) {
    gInput.mouseX = x;
    gInput.mouseY = y;
    if (!gMouseCaptured) return;
    int cx = gInput.width / 2, cy = gInput.height / 2;
    if (x == cx && y == cy) return;   // to zdarzenie wywołał nasz własny glutWarpPointer
    gInput.mouseDX += float(x - cx);
    gInput.mouseDY += float(y - cy);
    glutWarpPointer(cx, cy);
}

} // namespace

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitContextVersion(3, 3);
    glutInitContextProfile(GLUT_CORE_PROFILE);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(1280, 720);
    glutCreateWindow(WINDOW_TITLE);
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);

    if (!initRenderer()) {
        std::printf("[BLAD] Nie udalo sie przygotowac grafiki (wymagany OpenGL 3.3).\n");
        return 1;
    }
    gameInit();

    glutIgnoreKeyRepeat(1);
    glutDisplayFunc(display);
    glutIdleFunc(idle);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyDown);
    glutKeyboardUpFunc(keyUp);
    glutSpecialFunc(specialDown);
    glutSpecialUpFunc(specialUp);
    glutMouseFunc(mouseButton);
    glutMotionFunc(mouseMove);
    glutPassiveMotionFunc(mouseMove);

    gLastTime = gFpsTime = glutGet(GLUT_ELAPSED_TIME);
    glutMainLoop();
    return 0;
}

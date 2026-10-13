#include "glut.h"
#include "Game.h"
#include <iostream>
#include <cstdlib>
#include <ctime>

Game* game = nullptr;

void init() {
    glClearColor(0.12f, 0.13f, 0.17f, 1.0f);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, MSZ + PANEL_W, 0, MSZ);   // map + side panel

    game = new Game();
    game->init();
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT);

    if (game) {
        game->draw();
    }

    glutSwapBuffers();
}

// fixed-rate game loop: same speed on every computer
void timer(int) {
    if (game) game->update(0.016f);
    glutPostRedisplay();
    glutTimerFunc(TICK_MS, timer, 0);
}

void keyboard(unsigned char key, int x, int y) {
    switch (key) {
    case 27:  // ESC
        std::cout << "Exiting...\n";
        exit(0);
        break;
    case 'v':
    case 'V':
        if (game) game->cycleViewMode();
        break;
    case 'p':
    case 'P':
    case ' ':
        if (game) game->togglePause();
        break;
    case 'r':
    case 'R':                         // new random battle
        delete game;
        game = new Game();
        game->init();
        break;
    }
    glutPostRedisplay();
}

void mouse(int button, int state, int x, int y) {
    if (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN) {
        if (game) {
            game->cycleViewMode();
        }
    }
}

int main(int argc, char** argv) {
    std::srand((unsigned int)std::time(nullptr));
    std::cout << "=================================\n";
    std::cout << "  AI Strategy Game - Project\n";
    std::cout << "=================================\n\n";
    std::cout << "Controls:\n";
    std::cout << "  V or RIGHT CLICK - Cycle view modes (Game/Visibility/Safety)\n";
    std::cout << "  P or SPACE - Pause\n";
    std::cout << "  R - New battle\n";
    std::cout << "  ESC - Exit\n\n";
    std::cout << "Note: Visibility mode shows BOTH teams together!\n\n";
    std::cout << "Starting game...\n\n";

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(W, H);
    glutInitWindowPosition(100, 50);
    glutCreateWindow("Tactical AI Battle");

    init();

    glutDisplayFunc(display);
    glutTimerFunc(TICK_MS, timer, 0);
    glutKeyboardFunc(keyboard);
    glutMouseFunc(mouse);  // Register mouse callback

    glutMainLoop();

    return 0;
}
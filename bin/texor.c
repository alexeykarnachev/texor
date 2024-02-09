#include "raylib.h"

#define SCREEN_WIDTH 1024
#define SCREEN_HEIGHT 768

int main(void) {
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "texor");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        DrawCircle(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, 100.0, RED);
        EndDrawing();
    }
}

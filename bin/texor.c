#include "raylib.h"
#include "raymath.h"

#define SCREEN_WIDTH 1024
#define SCREEN_HEIGHT 768

typedef struct Player {
    Transform transform;
} Player;

typedef struct World {
    Player player;
    Camera3D camera;
} World;

static World WORLD;

static World init_world();

int main(void) {
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "texor");
    SetTargetFPS(60);

    WORLD = init_world();

    while (!WindowShouldClose()) {
        BeginDrawing();

        BeginMode3D(WORLD.camera);
        DrawSphere(WORLD.player.transform.translation, 1.0, RED);
        EndMode3D();

        EndDrawing();
    }
}

static World init_world(void) {
    World world = {0};

    world.player.transform.rotation = QuaternionIdentity();
    world.player.transform.scale = Vector3One();
    world.player.transform.translation = Vector3Zero();

    world.camera.fovy = 40.0;
    world.camera.position = (Vector3){0.0, 0.0, 50.0};
    world.camera.target = Vector3Zero();
    world.camera.up = (Vector3){0.0, 1.0, 0.0};
    world.camera.projection = CAMERA_PERSPECTIVE;

    return world;
}

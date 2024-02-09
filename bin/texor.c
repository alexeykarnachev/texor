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
static void update_world(World *WORLD);

int main(void) {
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "texor");
    SetTargetFPS(60);

    WORLD = init_world();

    while (!WindowShouldClose()) {
        update_world(&WORLD);

        BeginDrawing();

        BeginMode3D(WORLD.camera);
        ClearBackground(BLANK);
        DrawSphere(WORLD.player.transform.translation, 1.0, RED);
        EndMode3D();

        EndDrawing();
    }
}

static World init_world(void) {
    World world = {0};

    // -------------------------------------------------------------------
    // Player
    world.player.transform.rotation = QuaternionIdentity();
    world.player.transform.scale = Vector3One();
    world.player.transform.translation = Vector3Zero();

    // -------------------------------------------------------------------
    // Camera
    Vector3 player_position = world.player.transform.translation;
    world.camera.fovy = 40.0;
    world.camera.position = player_position;
    world.camera.position.z = 50.0;
    world.camera.target = player_position;
    world.camera.up = (Vector3){0.0, 1.0, 0.0};
    world.camera.projection = CAMERA_PERSPECTIVE;

    return world;
}


static void update_world(World *world) {
    float dt = GetFrameTime();

    // -------------------------------------------------------------------
    // Player
    Vector2 dir = Vector2Zero();
    dir.y += IsKeyDown(KEY_W);
    dir.y -= IsKeyDown(KEY_S);
    dir.x -= IsKeyDown(KEY_A);
    dir.x += IsKeyDown(KEY_D);
    if (Vector2Length(dir) > EPSILON) {
        Vector2 step = Vector2Scale(
            Vector2Normalize(dir),
            20.0 * dt
        );
        world->player.transform.translation.x += step.x;
        world->player.transform.translation.y += step.y;
    }
}

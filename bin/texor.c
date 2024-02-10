#include "raylib.h"
#include "raymath.h"
#include "rcamera.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define print_vec(v) (printf("%f, %f, %f\n", v.x, v.y, v.z))

#define SCREEN_WIDTH 1024
#define SCREEN_HEIGHT 768

#define MAX_N_ENEMIES 256

typedef struct Player {
    Transform transform;
} Player;

typedef struct Enemy {
    Transform transform;
    float speed;
} Enemy;

typedef struct World {
    Player player;

    int n_enemies;
    Enemy enemies[MAX_N_ENEMIES];

    float time;
    float spawn_countdown;
    float spawn_radius;
    Camera3D camera;
} World;

static World WORLD;

static void init_world(World *world);
static void update_world(World *world);
static void update_free_orbit_camera(Camera3D *camera);

int main(void) {
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "texor");
    SetTargetFPS(60);

    init_world(&WORLD);

    while (!WindowShouldClose()) {
        update_world(&WORLD);

        BeginDrawing();

        BeginMode3D(WORLD.camera);
        ClearBackground(BLANK);
        DrawSphere(WORLD.player.transform.translation, 1.0, RAYWHITE);
        for (int i = 0; i < WORLD.n_enemies; ++i) {
            Enemy enemy = WORLD.enemies[i];
            DrawSphere(enemy.transform.translation, 1.0, RED);
        }
        DrawCircle3D(
            Vector3Zero(), WORLD.spawn_radius, (Vector3){0.0, 0.0, 1.0}, 0.0, WHITE
        );
        EndMode3D();

        EndDrawing();
    }
}

static void init_world(World *world) {
    memset(world, 0, sizeof(World));

    // -------------------------------------------------------------------
    // Player
    world->player.transform.rotation = QuaternionIdentity();
    world->player.transform.scale = Vector3One();
    world->player.transform.translation = Vector3Zero();

    // -------------------------------------------------------------------
    // Camera
    Vector3 player_position = world->player.transform.translation;
    world->camera.fovy = 60.0;
    world->camera.position = player_position;
    world->camera.position.z = 50.0;
    world->camera.target = player_position;
    world->camera.up = (Vector3){0.0, 1.0, 0.0};
    world->camera.projection = CAMERA_PERSPECTIVE;

    // -------------------------------------------------------------------
    // Game
    world->spawn_radius = 28.0;
}

static void update_world(World *world) {
    float dt = GetFrameTime();
    world->time += dt;
    world->spawn_countdown -= dt;

    Player *player = &world->player;

    // -------------------------------------------------------------------
    // Camera
    update_free_orbit_camera(&world->camera);

    // -------------------------------------------------------------------
    // Player
    Vector2 dir = Vector2Zero();
    dir.y += IsKeyDown(KEY_W);
    dir.y -= IsKeyDown(KEY_S);
    dir.x -= IsKeyDown(KEY_A);
    dir.x += IsKeyDown(KEY_D);
    if (Vector2Length(dir) > EPSILON) {
        Vector2 step = Vector2Scale(Vector2Normalize(dir), 20.0 * dt);
        player->transform.translation.x += step.x;
        player->transform.translation.y += step.y;
    }

    // -------------------------------------------------------------------
    // Enemies

    // Spawn
    if (world->spawn_countdown < 0.0 && world->n_enemies < MAX_N_ENEMIES) {
        world->spawn_countdown = fmaxf(5.0 * expf(world->time * 0.001), 1.0);

        float angle = ((float)rand() / RAND_MAX) * 2 * PI;
        Enemy enemy = {
            .transform={
                .translation = {
                    .x = world->spawn_radius * cos(angle),
                    .y = world->spawn_radius * sin(angle),
                    .z = 0.0,
                },
                .rotation = QuaternionIdentity(),
                .scale = Vector3One(),
            },
            .speed=5.0
        };
        world->enemies[world->n_enemies++] = enemy;
    }

    // Move towards player
    for (int i = 0; i < world->n_enemies; ++i) {
        Enemy *enemy = &world->enemies[i];
        Vector3 dir = Vector3Normalize(
            Vector3Subtract(player->transform.translation, enemy->transform.translation)
        );
        Vector3 step = Vector3Scale(dir, enemy->speed * dt);
        enemy->transform.translation = Vector3Add(enemy->transform.translation, step);
    }
}

static void update_free_orbit_camera(Camera3D *camera) {
    static float rot_speed = 0.003f;
    static float move_speed = 0.01f;
    static float zoom_speed = 1.0f;

    bool is_mmb_down = IsMouseButtonDown(MOUSE_MIDDLE_BUTTON);
    bool is_shift_down = IsKeyDown(KEY_LEFT_SHIFT);
    float mouse_wheel_move = GetMouseWheelMove();
    Vector2 mouse_delta = GetMouseDelta();

    if (is_mmb_down && is_shift_down) {
        // Shift + MMB + mouse move -> change the camera position in the
        // right-direction plane
        CameraMoveRight(camera, -move_speed * mouse_delta.x, true);

        Vector3 right = GetCameraRight(camera);
        Vector3 up = Vector3CrossProduct(
            Vector3Subtract(camera->position, camera->target), right
        );
        up = Vector3Scale(Vector3Normalize(up), move_speed * mouse_delta.y);
        camera->position = Vector3Add(camera->position, up);
        camera->target = Vector3Add(camera->target, up);
    } else if (is_mmb_down) {
        // Rotate the camera around the look-at point
        CameraYaw(camera, -rot_speed * mouse_delta.x, true);
        CameraPitch(camera, -rot_speed * mouse_delta.y, true, true, false);
    }

    // Bring camera closer (or move away), to the look-at point
    CameraMoveToTarget(camera, -mouse_wheel_move * zoom_speed);
}

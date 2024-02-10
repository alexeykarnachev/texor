#include "../src/shader.h"
#include "raylib.h"
#include "raymath.h"
#include "rcamera.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define print_vec3(v) (printf("%f, %f, %f\n", v.x, v.y, v.z))
#define print_vec2(v) (printf("%f, %f\n", v.x, v.y))

#define SCREEN_WIDTH 1024
#define SCREEN_HEIGHT 768

#define MAX_N_ENEMIES 256
#define MAX_WORD_LEN 33

typedef struct Word {
    Font font;
    Rectangle rec;
    Vector2 text_pos;
    char chars[MAX_WORD_LEN];
    Color char_colors[MAX_WORD_LEN];
} Word;

typedef struct Player {
    Transform transform;
} Player;

typedef struct Enemy {
    Transform transform;
    float speed;
    Word word;
} Enemy;

typedef struct World {
    Player player;

    int n_enemies;
    Enemy enemies[MAX_N_ENEMIES];

    char text_input[MAX_WORD_LEN];

    float time;
    float spawn_countdown;
    float spawn_radius;
    Camera3D camera;

    Font enemy_word_font;
    Font prompt_font;
} World;

static World WORLD;

static void init_world(World *world);
static void update_world(World *world);
static void update_free_orbit_camera(Camera3D *camera);
static void draw_world(World *world);
static void draw_text(Font font, const char *text, Vector2 position, Color *colors);

int main(void) {
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "texor");
    SetTargetFPS(60);

    init_world(&WORLD);

    while (!WindowShouldClose()) {
        update_world(&WORLD);
        draw_world(&WORLD);
    }
}

static void init_world(World *world) {
    memset(world, 0, sizeof(World));

    // -------------------------------------------------------------------
    // fonts
    const char *font_file_path = "./resources/fonts/ShareTechMono-Regular.ttf";
    world->enemy_word_font = LoadFontEx(font_file_path, 30, 0, 0);
    SetTextureFilter(world->enemy_word_font.texture, TEXTURE_FILTER_BILINEAR);

    world->prompt_font = world->enemy_word_font;

    // -------------------------------------------------------------------
    // player
    world->player.transform.rotation = QuaternionIdentity();
    world->player.transform.scale = Vector3One();
    world->player.transform.translation = Vector3Zero();

    // -------------------------------------------------------------------
    // camera
    Vector3 player_position = world->player.transform.translation;
    world->camera.fovy = 60.0;
    world->camera.position = player_position;
    world->camera.position.z = 50.0;
    world->camera.target = player_position;
    world->camera.up = (Vector3){0.0, 1.0, 0.0};
    world->camera.projection = CAMERA_PERSPECTIVE;

    // -------------------------------------------------------------------
    // game
    world->spawn_radius = 28.0;
}

static void update_world(World *world) {
    float dt = GetFrameTime();
    world->time += dt;
    world->spawn_countdown -= dt;

    int text_input_len = strlen(WORLD.text_input);
    int pressed_char = GetCharPressed();

    Player *player = &world->player;

    // -------------------------------------------------------------------
    // update camera
    update_free_orbit_camera(&world->camera);

    // -------------------------------------------------------------------
    // update enemy words
    for (int i = 0; i < world->n_enemies; ++i) {
        Enemy *enemy = &world->enemies[i];
        Word *word = &enemy->word;

        Vector2 screen_pos = GetWorldToScreen(
            enemy->transform.translation, world->camera
        );
        Vector2 text_size = MeasureTextEx(
            word->font, word->chars, word->font.baseSize, 0
        );

        Vector2 rec_size = Vector2Scale(text_size, 1.2);
        Vector2 rec_center = {screen_pos.x, screen_pos.y - 35.0};
        Vector2 rec_pos = Vector2Subtract(rec_center, Vector2Scale(rec_size, 0.5));

        Rectangle rec = {rec_pos.x, rec_pos.y, rec_size.x, rec_size.y};
        Vector2 text_pos = {
            rec_center.x - 0.5 * text_size.x, rec_center.y - 0.5 * word->font.baseSize};

        word->rec = rec;
        word->text_pos = text_pos;
        bool is_combo = true;
        for (int i = 0; i < strlen(word->chars); ++i) {
            char name_c = word->chars[i];
            char text_input_c = world->text_input[i];
            is_combo = text_input_c != '\0' && is_combo && name_c == text_input_c;

            Color color;
            if (is_combo) {
                color = GREEN;
            } else if (i < text_input_len) {
                color = RED;
            } else {
                color = WHITE;
            }

            word->char_colors[i] = color;
        }
    }

    // -------------------------------------------------------------------
    // update keyboard input

    Vector2 dir = Vector2Zero();
    dir.y += IsKeyDown(KEY_UP);
    dir.y -= IsKeyDown(KEY_DOWN);
    dir.x -= IsKeyDown(KEY_LEFT);
    dir.x += IsKeyDown(KEY_RIGHT);

    if (Vector2Length(dir) > EPSILON) {
        Vector2 step = Vector2Scale(Vector2Normalize(dir), 20.0 * dt);
        player->transform.translation.x += step.x;
        player->transform.translation.y += step.y;
    } else if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && text_input_len > 0) {
        WORLD.text_input[--text_input_len] = '\0';
    } else if (IsKeyPressed(KEY_ENTER) && text_input_len > 0) {
        // kill the enemy
        int kill_enemy_idx = -1;
        for (int i = 0; i < world->n_enemies; ++i) {
            Enemy *enemy = &world->enemies[i];
            if (strcmp(world->text_input, enemy->word.chars) == 0) {
                kill_enemy_idx = i;
                break;
            }
        }

        if (kill_enemy_idx >= 0) {
            world->n_enemies -= 1;
            memmove(
                &world->enemies[kill_enemy_idx],
                &world->enemies[kill_enemy_idx + 1],
                sizeof(Enemy) * (world->n_enemies - kill_enemy_idx)
            );
        }

        world->text_input[0] = '\0';
    } else if (text_input_len < MAX_WORD_LEN - 1 && isprint(pressed_char)) {
        WORLD.text_input[text_input_len++] = pressed_char;
        WORLD.text_input[text_input_len] = '\0';
    }

    // -------------------------------------------------------------------
    // update enemies spawn
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
            .speed = 5.0,
        };
        enemy.word.font = world->enemy_word_font;
        strcpy(enemy.word.chars, TextFormat("enemy_%d", world->n_enemies));
        world->enemies[world->n_enemies++] = enemy;
    }

    // -------------------------------------------------------------------
    // update enemies action
    for (int i = 0; i < world->n_enemies; ++i) {
        Enemy *enemy = &world->enemies[i];
        Vector3 vec = Vector3Subtract(
            player->transform.translation, enemy->transform.translation
        );
        if (Vector3Length(vec) > 2.0) {
            // move towards the player
            Vector3 dir = Vector3Normalize(vec);
            Vector3 step = Vector3Scale(dir, enemy->speed * dt);
            enemy->transform.translation = Vector3Add(enemy->transform.translation, step);
        } else {
            // attack the player
        }
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

static void draw_world(World *world) {
    BeginDrawing();
    {
        // scene
        BeginMode3D(world->camera);
        {
            ClearBackground(BLANK);
            DrawSphere(world->player.transform.translation, 1.0, RAYWHITE);
            for (int i = 0; i < world->n_enemies; ++i) {
                Enemy enemy = world->enemies[i];
                DrawSphere(enemy.transform.translation, 1.0, RED);
            }
            DrawCircle3D(
                Vector3Zero(), world->spawn_radius, (Vector3){0.0, 0.0, 1.0}, 0.0, WHITE
            );
        }
        EndMode3D();

        // enemy words
        for (int i = 0; i < world->n_enemies; ++i) {
            Word word = world->enemies[i].word;
            DrawRectangleRounded(word.rec, 0.3, 16, (Color){20, 20, 20, 255});
            draw_text(word.font, word.chars, word.text_pos, word.char_colors);
        }

        // text input
        static char prompt[3] = {'>', ' ', '\0'};
        Font font = world->prompt_font;
        Vector2 prompt_size = MeasureTextEx(font, prompt, font.baseSize, 0);
        Vector2 text_size = MeasureTextEx(font, world->text_input, font.baseSize, 0);
        float y = GetScreenHeight() - font.baseSize - 5;
        DrawRectangle(
            5.0 + prompt_size.x + text_size.x,
            GetScreenHeight() - font.baseSize - 5,
            2,
            font.baseSize,
            WHITE
        );
        draw_text(font, prompt, (Vector2){5.0, y}, 0);
        draw_text(font, world->text_input, (Vector2){prompt_size.x, y}, 0);
    }
    EndDrawing();
}

static void draw_text(Font font, const char *text, Vector2 position, Color *colors) {
    int n_chars = strlen(text);
    float offset = 0.0f;
    float scale = (float)font.baseSize / font.baseSize;

    for (int i = 0; i < n_chars; i++) {
        int ch = text[i];
        int index = GetGlyphIndex(font, ch);
        Color color;
        if (colors) color = colors[i];
        else color = WHITE;

        if (ch != ' ') {
            DrawTextCodepoint(
                font, ch, (Vector2){position.x + offset, position.y}, font.baseSize, color
            );
        }

        if (font.glyphs[index].advanceX == 0) {
            offset += ((float)font.recs[index].width * scale);
        } else {
            offset += ((float)font.glyphs[index].advanceX * scale);
        }
    }
}

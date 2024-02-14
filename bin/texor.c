#include "../src/shader.h"
#include "raylib.h"
#include "raymath.h"
#include "rcamera.h"
#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))
#define print_vec3(v) (printf("%f, %f, %f\n", v.x, v.y, v.z))
#define print_vec2(v) (printf("%f, %f\n", v.x, v.y))

#define SCREEN_WIDTH 1024
#define SCREEN_HEIGHT 768

#define MAX_N_ENEMIES 32
#define MAX_WORD_LEN 32
#define MAX_N_ENEMY_NAMES 20000
#define MAX_N_ENEMY_EFFECTS 16

#define DIFFICULTY 6.0
#define BASE_SPAWN_PERIOD 5.0
#define BASE_ENEMY_SPEED_FACTOR 0.3
#define MAX_ENEMY_SPEED_FACTOR 1.1
#define BOSS_SPAWN_PERIOD 10  // in number of enemies
#define PLAYER_SPEED 6.0

// puase
#define PAUSE_COOLDOWN 5.0
// cryonics
#define CRYONICS_COOLDOWN 40.0
#define CRYONICS_DURATION 10.0
// repulse
#define REPULSE_COOLDOWN 20.0
#define REPULSE_SPEED 80.0
#define REPULSE_DECELERATION 150.0
#define REPULSE_RADIUS 30.0
// decay
#define DECAY_COOLDOWN 20.0
#define DECAY_STRENGTH 0.5

#define UI_BACKGROUND_COLOR ((Color){20, 20, 20, 255})
#define UI_OUTLINE_COLOR ((Color){0, 40, 0, 255})

typedef enum EffectType {
    EFFECT_FREEZE,
    EFFECT_IMPULSE,
} EffectType;

typedef struct Effect {
    EffectType type;
    union {
        struct {
            float time;
        } freeze;

        struct {
            float speed;
            float deceleration;
            Vector3 direction;
        } impulse;
    };
} Effect;

typedef enum CommandType {
    COMMAND_PAUSE,
    COMMAND_CRYONICS,
    COMMAND_REPULSE,
    COMMAND_DECAY,

    COMMAND_RESTART_GAME,
    COMMAND_EXIT_GAME,
    N_COMMANDS,
} CommandType;

typedef struct Command {
    float cooldown;
    float time;
    char name[MAX_WORD_LEN];

    CommandType type;

    union {
        struct {
            float duration;
        } cryonics;

        struct {
            float radius;
            float speed;
            float deceleration;
        } repulse;

        struct {
            float strength;
        } decay;
    };
} Command;

typedef struct Player {
    Transform transform;
    float max_health;
    float health;
} Player;

typedef struct Enemy {
    Transform transform;
    float speed;
    float attack_strength;
    float attack_radius;
    float attack_cooldown;
    float recent_attack_time;
    char name[MAX_WORD_LEN];

    int n_effects;
    Effect effects[MAX_N_ENEMY_EFFECTS];
} Enemy;

typedef enum WorldState {
    STATE_PLAYING,
    STATE_PAUSE,
    STATE_GAME_OVER,
} WorldState;

typedef struct World {
    Player player;

    int n_commands;
    Command commands[N_COMMANDS];

    int n_enemies_spawned; // in total
    int n_enemies;
    Enemy enemies[MAX_N_ENEMIES];

    char prompt[MAX_WORD_LEN];
    char submit_word[MAX_WORD_LEN];

    bool should_exit;
    float dt;
    float time;
    float freeze_time;
    float spawn_countdown;
    float spawn_radius;
    Vector3 spawn_position;
    Camera3D camera;
    WorldState state;
} World;

typedef struct Resources {
    Font word_font;

    int n_enemy_names;
    char enemy_names[MAX_N_ENEMY_NAMES][MAX_WORD_LEN];

    int n_boss_names;
    char boss_names[MAX_N_ENEMY_NAMES][MAX_WORD_LEN];
} Resources;

static Resources RESOURCES;
static World WORLD;

static void init_resources(Resources *resources);
static void init_world(World *world);
static void init_spawn_position(World *world);
static void update_world(World *world, Resources *resources);
static void update_prompt(World *world);
static void update_enemies_spawn(World *world, Resources *resources);
static void update_commands(World *world);
static void update_enemies(World *world);
static void update_player(World *world);
static void update_free_orbit_camera(Camera3D *camera);
static void draw_world(World *world, Resources *resources);
static void draw_text(
    Font font, const char *text, Vector2 position, const char *match_prompt
);

int main(void) {
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "texor");
    SetTargetFPS(60);

    init_resources(&RESOURCES);
    init_world(&WORLD);

    while (!WORLD.should_exit) {
        update_world(&WORLD, &RESOURCES);
        draw_world(&WORLD, &RESOURCES);
    }
}

static void init_resources(Resources *resources) {
    // -------------------------------------------------------------------
    // init fonts
    const char *font_file_path = "./resources/fonts/ShareTechMono-Regular.ttf";
    resources->word_font = LoadFontEx(font_file_path, 30, 0, 0);
    SetTextureFilter(resources->word_font.texture, TEXTURE_FILTER_BILINEAR);

    // -------------------------------------------------------------------
    // init names
    FILE *f = fopen("./resources/words/enemy_names.txt", "r");
    while (fgets(resources->enemy_names[resources->n_enemy_names], MAX_WORD_LEN, f)) {
        char *name = resources->enemy_names[resources->n_enemy_names];
        name[strcspn(name, "\n")] = 0;
        if (++resources->n_enemy_names >= MAX_N_ENEMY_NAMES) break;
    }
    fclose(f);

    f = fopen("./resources/words/boss_names.txt", "r");
    while (fgets(resources->boss_names[resources->n_boss_names], MAX_WORD_LEN, f)) {
        char *name = resources->boss_names[resources->n_boss_names];
        name[strcspn(name, "\n")] = 0;
        if (++resources->n_boss_names >= MAX_N_ENEMY_NAMES) break;
    }
    fclose(f);
}

static void init_world(World *world) {
    SetRandomSeed(time(NULL));
    memset(world, 0, sizeof(World));

    // -------------------------------------------------------------------
    // init commands

    // pause command
    Command command = {0};
    command.cooldown = PAUSE_COOLDOWN;
    command.time = command.cooldown;
    command.type = COMMAND_PAUSE;
    strcpy(command.name, "pause");
    world->commands[world->n_commands++] = command;

    // cryonics command
    memset(&command, 0, sizeof(Command));
    command.cooldown = CRYONICS_COOLDOWN;
    command.time = command.cooldown;
    command.type = COMMAND_CRYONICS;
    command.cryonics.duration = CRYONICS_DURATION;
    strcpy(command.name, "cryonics");
    world->commands[world->n_commands++] = command;

    // repulse command
    memset(&command, 0, sizeof(Command));
    command.cooldown = REPULSE_COOLDOWN;
    command.time = command.cooldown;
    command.type = COMMAND_REPULSE;
    command.repulse.speed = REPULSE_SPEED;
    command.repulse.deceleration = REPULSE_DECELERATION;
    command.repulse.radius = REPULSE_RADIUS;
    strcpy(command.name, "repulse");
    world->commands[world->n_commands++] = command;

    // decay command
    memset(&command, 0, sizeof(Command));
    command.cooldown = DECAY_COOLDOWN;
    command.time = command.cooldown;
    command.type = COMMAND_DECAY;
    command.decay.strength = DECAY_STRENGTH;
    strcpy(command.name, "decay");
    world->commands[world->n_commands++] = command;

    // exit command
    memset(&command, 0, sizeof(Command));
    command.cooldown = 0.0;
    command.time = command.cooldown;
    command.type = COMMAND_EXIT_GAME;
    strcpy(command.name, "exit");
    world->commands[world->n_commands++] = command;

    // -------------------------------------------------------------------
    // init player
    world->player.transform.rotation = QuaternionIdentity();
    world->player.transform.scale = Vector3One();
    world->player.transform.translation = Vector3Zero();
    world->player.max_health = 100.0;
    world->player.health = world->player.max_health;

    // -------------------------------------------------------------------
    // init camera
    Vector3 player_position = world->player.transform.translation;
    world->camera.fovy = 60.0;
    world->camera.position = player_position;
    world->camera.position.z = 50.0;
    world->camera.position.x -= 10.0;
    world->camera.target = world->camera.position;
    world->camera.target.z -= 1.0;
    world->camera.up = (Vector3){0.0, 1.0, 0.0};
    world->camera.projection = CAMERA_PERSPECTIVE;

    // -------------------------------------------------------------------
    // init game parameters
    world->state = STATE_PLAYING;
    world->spawn_radius = 28.0;
    init_spawn_position(world);
}

static void init_spawn_position(World *world) {
    float angle = ((float)GetRandomValue(0, RAND_MAX) / RAND_MAX) * 2 * PI;
    world->spawn_position = (Vector3){
        .x = world->spawn_radius * cos(angle),
        .y = world->spawn_radius * sin(angle),
        .z = 0.0,
    };
}

static void update_world(World *world, Resources *resources) {
    world->dt = world->state == STATE_PLAYING ? GetFrameTime() : 0.0;
    world->freeze_time = fmaxf(0.0, world->freeze_time - world->dt);
    world->time += world->dt;

    bool is_altf4_pressed = IsKeyDown(KEY_LEFT_ALT) && IsKeyPressed(KEY_F4);
    world->should_exit = (WindowShouldClose() || is_altf4_pressed)
                         && !IsKeyPressed(KEY_ESCAPE);

    update_free_orbit_camera(&world->camera);
    update_prompt(world);
    update_commands(world);

    if (world->state == STATE_PLAYING) {
        update_enemies_spawn(world, resources);
        update_enemies(world);
        update_player(world);
    }

    world->submit_word[0] = '\0';
}

static void update_prompt(World *world) {
    int prompt_len = strlen(world->prompt);
    int pressed_char = GetCharPressed();
    if (IsKeyPressed(KEY_ENTER) > 0) {
        strcpy(world->submit_word, world->prompt);
        world->prompt[0] = '\0';
    } else if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && prompt_len > 0) {
        world->prompt[--prompt_len] = '\0';
    } else if (prompt_len < MAX_WORD_LEN - 1 && isprint(pressed_char)) {
        world->prompt[prompt_len++] = pressed_char;
        world->prompt[prompt_len] = '\0';
    }
}

static void update_enemies_spawn(World *world, Resources *resources) {
    // don't update spawn_countdown if the world is frozen
    if (world->freeze_time < EPSILON) {
        world->spawn_countdown -= world->dt;
    }

    if (world->spawn_countdown > 0.0 || world->n_enemies == MAX_N_ENEMIES) return;

    // https://www.desmos.com/calculator/jp6dgyycwn
    world->spawn_countdown = fmaxf(
        BASE_SPAWN_PERIOD * expf(-world->time * 0.001 * DIFFICULTY), 1.0
    );
    float speed_factor = BASE_ENEMY_SPEED_FACTOR
                         + (MAX_ENEMY_SPEED_FACTOR - BASE_ENEMY_SPEED_FACTOR)
                               * (1.0 - expf(-world->time * 0.001 * DIFFICULTY));
    float speed = PLAYER_SPEED * speed_factor;

    TraceLog(
        LOG_INFO,
        "\nspawn countdown -> %.3f\nspeed factor-> %.3f",
        world->spawn_countdown,
        speed_factor
    );

    Vector3 position = world->spawn_position;
    init_spawn_position(world);

    Enemy enemy = {
        .transform={
            .translation = position,
            .rotation = QuaternionIdentity(),
            .scale = Vector3One(),
        },
        .speed = speed,
        .attack_strength = 10.0,
        .attack_radius = 2.0,
        .attack_cooldown = 1.0,
        .recent_attack_time = 0.0,
    };

    if (++world->n_enemies_spawned % BOSS_SPAWN_PERIOD == 0) {
        int idx = GetRandomValue(0, resources->n_boss_names - 1);
        strcpy(enemy.name, resources->boss_names[idx]);
    } else {
        int idx = GetRandomValue(0, resources->n_enemy_names - 1);
        strcpy(enemy.name, resources->enemy_names[idx]);
    }
    world->enemies[world->n_enemies++] = enemy;
}

static void update_commands(World *world) {
    float dt = world->dt;
    const char *submit_word = world->submit_word;

    for (int i = 0; i < world->n_commands; ++i) {
        Command *command = &world->commands[i];
        command->time += dt;

        bool is_match = strcmp(submit_word, command->name) == 0;
        bool is_ready = command->time > command->cooldown;
        if (is_match && is_ready) {
            if (command->type == COMMAND_PAUSE && world->state == STATE_PLAYING) {
                command->time = command->cooldown + 1.0;
                strcpy(command->name, "continue");
                world->state = STATE_PAUSE;
            } else if (command->type == COMMAND_PAUSE && world->state == STATE_PAUSE) {
                command->time = 0.0;
                strcpy(command->name, "pause");
                world->state = STATE_PLAYING;
            } else if (command->type == COMMAND_RESTART_GAME) {
                init_world(world);
            } else if (command->type == COMMAND_EXIT_GAME) {
                world->should_exit = true;
            } else if (command->type == COMMAND_CRYONICS && world->state == STATE_PLAYING) {
                world->freeze_time = command->cryonics.duration;
                command->time = 0.0;
                for (int i = 0; i < world->n_enemies; ++i) {
                    Enemy *enemy = &world->enemies[i];
                    Effect effect = {
                        .type = EFFECT_FREEZE,
                        .freeze = {.time = command->cryonics.duration}};
                    if (enemy->n_effects < MAX_N_ENEMY_EFFECTS) {
                        enemy->effects[enemy->n_effects++] = effect;
                    }
                }
            } else if (command->type == COMMAND_REPULSE && world->state == STATE_PLAYING) {
                command->time = 0.0;
                for (int i = 0; i < world->n_enemies; ++i) {
                    Enemy *enemy = &world->enemies[i];
                    Vector3 vec = Vector3Subtract(
                        enemy->transform.translation, world->player.transform.translation
                    );
                    float dist = Vector3Length(vec);
                    Vector3 dir = Vector3Normalize(vec);
                    if (dist < command->repulse.radius) {
                        Effect effect = {
                            .type = EFFECT_IMPULSE,
                            .impulse = {
                                .speed = command->repulse.speed,
                                .deceleration = command->repulse.deceleration,
                                .direction = dir}};
                        if (enemy->n_effects < MAX_N_ENEMY_EFFECTS) {
                            enemy->effects[enemy->n_effects++] = effect;
                        }
                    }
                }
            } else if (command->type == COMMAND_DECAY && world->state == STATE_PLAYING) {
                command->time = 0.0;
                for (int i = 0; i < world->n_enemies; ++i) {
                    Enemy *enemy = &world->enemies[i];
                    int len = max(1, strlen(enemy->name) / 2);
                    enemy->name[len] = '\0';
                }
            }
        }
    }
}

static void update_enemies(World *world) {
    const char *submit_word = world->submit_word;
    int kill_enemy_idx = -1;

    for (int i = 0; i < world->n_enemies; ++i) {
        Enemy *enemy = &world->enemies[i];

        if (strcmp(submit_word, enemy->name) == 0) {
            kill_enemy_idx = i;
            continue;
        }

        // update enemy effects
        bool can_move = true;
        bool can_attack = true;
        Vector3 step = {0};

        int n_active_effects = 0;
        static Effect active_effects[MAX_N_ENEMY_EFFECTS];
        for (int effect_i = 0; effect_i < enemy->n_effects; ++effect_i) {
            Effect effect = enemy->effects[effect_i];
            if (effect.type == EFFECT_FREEZE && effect.freeze.time > 0) {
                effect.freeze.time -= world->dt;
                can_move = false;
                can_attack = false;

                active_effects[n_active_effects++] = effect;
            } else if (effect.type == EFFECT_IMPULSE && effect.impulse.speed > 0.0) {
                Vector3 dir = Vector3Normalize(effect.impulse.direction);
                step = Vector3Scale(dir, effect.impulse.speed * world->dt);
                effect.impulse.speed -= effect.impulse.deceleration * world->dt;
                can_move = false;
                can_attack = false;

                active_effects[n_active_effects++] = effect;
            }
        }

        enemy->n_effects = n_active_effects;
        memcpy(enemy->effects, active_effects, sizeof(Effect) * n_active_effects);

        // apply enemy movements and attacks
        enemy->transform.translation = Vector3Add(enemy->transform.translation, step);
        Vector3 dir_to_player = Vector3Subtract(
            world->player.transform.translation, enemy->transform.translation
        );
        float dist_to_player = Vector3Length(dir_to_player);
        dir_to_player = Vector3Normalize(dir_to_player);
        float time_since_last_attack = world->time - enemy->recent_attack_time;
        can_attack &= dist_to_player < enemy->attack_radius
                      && time_since_last_attack > enemy->attack_cooldown;
        can_move &= dist_to_player > enemy->attack_radius;

        if (can_attack) {
            enemy->recent_attack_time = world->time;
            world->player.health -= enemy->attack_strength;
        } else if (can_move) {
            Vector3 step = Vector3Scale(dir_to_player, enemy->speed * world->dt);
            enemy->transform.translation = Vector3Add(enemy->transform.translation, step);
        }
    }

    if (kill_enemy_idx != -1) {
        world->n_enemies -= 1;
        memmove(
            &world->enemies[kill_enemy_idx],
            &world->enemies[kill_enemy_idx + 1],
            sizeof(Enemy) * (world->n_enemies - kill_enemy_idx)
        );
    }
}

static void update_player(World *world) {
    Player *player = &world->player;
    if (player->health <= 0.0) {
        world->state = STATE_GAME_OVER;

        Command *command = &world->commands[0];
        command->type = COMMAND_RESTART_GAME;
        command->cooldown = 0.0;
        strcpy(command->name, "restart");

        command = &world->commands[1];
        command->type = COMMAND_EXIT_GAME;
        command->cooldown = 0.0;
        strcpy(command->name, "exit");

        world->n_commands = 2;
        return;
    }

    Vector2 dir = Vector2Zero();
    dir.y += IsKeyDown(KEY_UP);
    dir.y -= IsKeyDown(KEY_DOWN);
    dir.x -= IsKeyDown(KEY_LEFT);
    dir.x += IsKeyDown(KEY_RIGHT);

    int pressed_char = GetCharPressed();
    if (Vector2Length(dir) > EPSILON) {
        Vector2 step = Vector2Scale(Vector2Normalize(dir), PLAYER_SPEED * world->dt);
        player->transform.translation.x += step.x;
        player->transform.translation.y += step.y;
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

static void draw_world(World *world, Resources *resources) {
    BeginDrawing();
    {
        // scene
        BeginMode3D(world->camera);
        {
            ClearBackground(BLANK);

            // draw player
            DrawSphere(world->player.transform.translation, 1.0, RAYWHITE);

            // draw enemies
            for (int i = 0; i < world->n_enemies; ++i) {
                Enemy enemy = world->enemies[i];

                Color color = RED;
                for (int effect_i = 0; effect_i < enemy.n_effects; ++effect_i) {
                    Effect effect = enemy.effects[effect_i];
                    if (effect.type == EFFECT_FREEZE) {
                        color = BLUE;
                        break;
                    }
                }

                DrawSphere(enemy.transform.translation, 1.0, color);
            }

            // draw next spawn enemy ghost
            float alpha = 1.0 - world->spawn_countdown / BASE_SPAWN_PERIOD;
            DrawSphere(world->spawn_position, 1.0, ColorAlpha(RED, alpha));

            // draw arena boundary
            DrawCircle3D(
                Vector3Zero(),
                world->spawn_radius,
                (Vector3){0.0, 0.0, 1.0},
                0.0,
                UI_OUTLINE_COLOR
            );
        }
        EndMode3D();

        // draw enemy words
        {
            for (int i = 0; i < world->n_enemies; ++i) {
                Enemy enemy = world->enemies[i];

                Vector2 screen_pos = GetWorldToScreen(
                    enemy.transform.translation, world->camera
                );
                Vector2 text_size = MeasureTextEx(
                    resources->word_font, enemy.name, resources->word_font.baseSize, 0
                );

                Vector2 rec_size = Vector2Scale(text_size, 1.2);
                Vector2 rec_center = {screen_pos.x, screen_pos.y - 35.0};
                Vector2 rec_pos = Vector2Subtract(
                    rec_center, Vector2Scale(rec_size, 0.5)
                );

                Rectangle rec = {rec_pos.x, rec_pos.y, rec_size.x, rec_size.y};
                Vector2 text_pos = {
                    rec_center.x - 0.5 * text_size.x,
                    rec_center.y - 0.5 * resources->word_font.baseSize};

                DrawRectangleRounded(rec, 0.3, 16, (Color){20, 20, 20, 255});
                draw_text(resources->word_font, enemy.name, text_pos, world->prompt);
            }
        }

        // draw prompt
        {
            static char prompt[3] = {'>', ' ', '\0'};
            Font font = resources->word_font;
            Vector2 prompt_size = MeasureTextEx(font, prompt, font.baseSize, 0);
            Vector2 text_size = MeasureTextEx(font, world->prompt, font.baseSize, 0);
            float y = GetScreenHeight() - font.baseSize - 5;
            DrawRectangle(
                5.0 + prompt_size.x + text_size.x,
                GetScreenHeight() - font.baseSize - 5,
                2,
                font.baseSize,
                WHITE
            );
            draw_text(font, prompt, (Vector2){5.0, y}, 0);
            draw_text(font, world->prompt, (Vector2){prompt_size.x, y}, 0);
        }

        // draw in-game ui
        {
            Rectangle rec = {2.0, 2.0, 200.0, 400.0};
            DrawRectangleRounded(rec, 0.05, 16, UI_BACKGROUND_COLOR);
            DrawRectangleRoundedLines(rec, 0.05, 16, 2.0, UI_OUTLINE_COLOR);

            // draw player health
            float ratio = fmaxf(0.0, world->player.health / world->player.max_health);
            Color color = ColorFromNormalized((Vector4){
                .x = 1.0 - ratio,
                .y = ratio,
                .z = 0.0,
                .w = 1.0,
            });
            rec = (Rectangle){8.0, 8.0, 190.0, 20.0};
            DrawRectangleRoundedLines(rec, 0.5, 16, 2.0, UI_OUTLINE_COLOR);
            rec.width *= ratio;
            DrawRectangleRounded(rec, 0.5, 16, color);

            // draw commands
            for (int i = 0; i < world->n_commands; ++i) {
                Command *command = &world->commands[i];
                float y = 40.0 + 1.8 * i * resources->word_font.baseSize;

                float ratio;
                ratio = fminf(1.0, command->time / command->cooldown);
                Color color = ColorFromNormalized((Vector4){
                    .x = 1.0 - ratio,
                    .y = ratio,
                    .z = 0.0,
                    .w = 1.0,
                });
                float width = 190.0 * ratio;
                draw_text(
                    resources->word_font, command->name, (Vector2){8.0, y}, world->prompt
                );
                Rectangle rec = {8.0, y + resources->word_font.baseSize, width, 5.0};
                DrawRectangleRec(rec, color);
            }
        }
    }
    EndDrawing();
}

static void draw_text(
    Font font, const char *text, Vector2 position, const char *match_prompt
) {
    int n_chars = strlen(text);
    float offset = 0.0f;
    float scale = (float)font.baseSize / font.baseSize;

    int prompt_len = match_prompt != 0 ? strlen(match_prompt) : 0;
    bool is_combo = prompt_len > 0;

    for (int i = 0; i < n_chars; i++) {
        int ch = text[i];
        is_combo = is_combo && match_prompt[i] != '\0' && match_prompt[i] == ch;

        Color color;
        if (is_combo) {
            color = GREEN;
        } else if (i < prompt_len) {
            color = RED;
        } else {
            color = WHITE;
        }

        if (ch != ' ') {
            DrawTextCodepoint(
                font, ch, (Vector2){position.x + offset, position.y}, font.baseSize, color
            );
        }

        int index = GetGlyphIndex(font, ch);
        if (font.glyphs[index].advanceX == 0) {
            offset += ((float)font.recs[index].width * scale);
        } else {
            offset += ((float)font.glyphs[index].advanceX * scale);
        }
    }
}

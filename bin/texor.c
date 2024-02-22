#include "../src/shader.h"
#include "raylib.h"
#include "raymath.h"
#include "rcamera.h"
#include "rlgl.h"
#include <asm-generic/errno.h>
#include <ctype.h>
#include <dirent.h>
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

#define MAX_N_ENEMIES 5
#define MAX_WORD_LEN 32
#define MAX_N_ENEMY_NAMES 20000
#define MAX_N_ENEMY_EFFECTS 16

// camera
#define CAMERA_INIT_POSITION ((Vector3){-10.0, 0.0, 70.0})
#define CAMERA_SHAKE_TIME 0.2

// shot
#define SHOT_TRACE_DURATION 0.08

// drop
#define MAX_N_DROPS 4
#define DROP_RADIUS 2.0
#define DROP_PROBABILITY 1.0
#define DROP_DURATION 30.0
#define DROP_HEAL_VALUE 30.0

#define SPAWN_RADIUS 35.0
#define ENEMY_RADIUS 2.0
#define PLAYER_RADIUS 2.0
// #define BASE_SPAWN_PERIOD 5.0
#define BASE_SPAWN_PERIOD 0.1
#define BASE_ENEMY_SPEED_FACTOR 0.3
#define MAX_ENEMY_SPEED_FACTOR 1.1
#define BOSS_SPAWN_PERIOD 10  // in number of enemies
#define PLAYER_SPEED 20.0
#define PLAYER_MAX_HEALTH 100.0
#define BACKSPACE_DAMAGE 1.0
#define WRONG_COMMAND_DAMAGE 10.0

// difficulties
#define DIFFICULTY_EASY 1
#define DIFFICULTY_MEDIUM 3
#define DIFFICULTY_HARD 6
#define DIFFICULTY_MONKEYTYPE 10

// puase
#define PAUSE_COOLDOWN 5.0
// cryonics
#define CRYONICS_COOLDOWN 30.0
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

typedef struct Shot {
    float time;
    float trace_duration;
    Vector3 start_position;
    Vector3 end_position;
} Shot;

typedef enum DropType {
    DROP_HEAL,
    DROP_REFRESH,
    N_DROPS,
} DropType;

typedef struct Drop {
    float time;
    Vector3 position;

    DropType type;

    union {
        struct {
            float value;
        } heal;

        struct {
        } refresh;
    };
} Drop;

typedef struct CameraShake {
    float duration;
    float time;
    float strength;
} CameraShake;

typedef enum CommandType {
    COMMAND_START_EASY,
    COMMAND_START_MEDIUM,
    COMMAND_START_HARD,
    COMMAND_START_MONKEYTYPE,

    COMMAND_EXIT_GAME,

    COMMAND_PAUSE,
    COMMAND_CRYONICS,
    COMMAND_REPULSE,
    COMMAND_DECAY,

    COMMAND_RESTART_GAME,
    N_COMMANDS,
} CommandType;

typedef struct Command {
    float cooldown;
    float time;
    bool show_separator;
    bool show_cooldown;
    char name[MAX_WORD_LEN];

    Texture2D icon_texture;

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

typedef struct AnimatedSprite {
    Texture2D texture;
    int n_frames;
    int frame_width;
    int frame_idx;
    int fps;

    float time;
    bool is_repeat;
} AnimatedSprite;

typedef enum PlayerState {
    PLAYER_IDLE,
    PLAYER_RUN,
    PLAYER_SHOOT,
    PLAYER_HURT,
    PLAYER_DEATH,
} PlayerState;

typedef struct Player {
    Transform transform;
    float max_health;
    float health;

    PlayerState state;
    PlayerState next_state;
    AnimatedSprite animated_sprite;
} Player;

typedef enum EnemyState {
    ENEMY_IDLE,
    ENEMY_RUN,
    ENEMY_ATTACK,
    ENEMY_FREEZE,
    ENEMY_EXPLODE,
} EnemyState;

typedef struct Enemy {
    Transform transform;
    float speed;
    float attack_strength;
    float attack_cooldown;
    float recent_attack_time;
    char name[MAX_WORD_LEN];

    struct {
        float speed;
        float deceleration;
        Vector3 direction;
    } impulse;

    EnemyState state;
    EnemyState next_state;
    AnimatedSprite animated_sprite;

    // for sorting
    int n_matched_chars;
} Enemy;

#define MAX_N_ROULETTE_SOUNDS 8
typedef struct SoundsRoulette {
    int n;
    int i;
    Sound sounds[MAX_N_ROULETTE_SOUNDS];
} SoundsRoulette;

typedef enum WorldState {
    STATE_MENU,
    STATE_PLAYING,
    STATE_PAUSE,
    STATE_GAME_OVER,
} WorldState;

typedef struct World {
    Player player;
    Shot shot;

    int n_drops;
    Drop drops[MAX_N_DROPS];

    int n_commands;
    Command commands[N_COMMANDS];

    int n_enemies_spawned;  // in total
    int n_enemies_killed;
    int n_enemies;
    Enemy enemies[MAX_N_ENEMIES];

    char prompt[MAX_WORD_LEN];
    char submit_word[MAX_WORD_LEN];
    bool is_command_matched;

    bool should_exit;
    float dt;
    float time;
    float freeze_time;
    float spawn_period;
    float spawn_countdown;
    float spawn_radius;
    int difficulty;
    char difficulty_str[MAX_WORD_LEN];
    int n_backspaces_typed;  // in total
    int n_keystrokes_typed;  // in total
    Vector3 spawn_position;
    Camera3D camera;
    CameraShake camera_shake;
    WorldState state;
} World;

typedef struct Resources {
    Font command_font;
    Font stats_font;

    Shader ground_shader;

    Mesh sprite_plane;
    Material sprite_material;

    int n_enemy_names;
    char enemy_names[MAX_N_ENEMY_NAMES][MAX_WORD_LEN];

    int n_boss_names;
    char boss_names[MAX_N_ENEMY_NAMES][MAX_WORD_LEN];

    SoundsRoulette enemy_death_sounds;
    SoundsRoulette pickup_sounds;
    SoundsRoulette shot_sounds;
    SoundsRoulette cryonics_sounds;

    Texture2D pause_icon_texture;
    Texture2D cryonics_icon_texture;
    Texture2D repulse_icon_texture;
    Texture2D decay_icon_texture;
    Texture2D health_icon_texture;
    Texture2D enemy_icon_texture;

    Texture2D player_idle_texture;
    Texture2D player_run_texture;
    Texture2D player_shoot_texture;
    Texture2D player_hurt_texture;
    Texture2D player_death_texture;

    Texture2D enemy_idle_texture;
    Texture2D enemy_run_texture;
    Texture2D enemy_attack_texture;
    Texture2D enemy_freeze_texture;
    Texture2D enemy_explode_texture;
} Resources;

static Resources RESOURCES;
static World WORLD;

static void init_resources(Resources *resources);
static void init_world(World *world, Resources *resources);
static void init_menu_commands(World *world);
static void init_playing_commands(World *world, Resources *resources);
static void init_game_over_commands(World *world);
static void init_spawn_position(World *world);
static void update_world(World *world, Resources *resources);
static void update_prompt(World *world);
static void update_enemies_spawn(World *world, Resources *resources);
static void update_commands(World *world, Resources *resources);
static void update_enemies(World *world, Resources *resources);
static void update_drops(World *world, Resources *resources);
static void update_player(World *world, Resources *resources);
static void update_camera(World *world);
static void update_animated_sprite(AnimatedSprite *animated_sprite, float dt);
static void draw_world(World *world, Resources *resources);
static void draw_text(
    Font font, const char *text, Vector2 position, const char *match_prompt
);
static void draw_sprite_2d(Texture2D texture, Vector2 position, Resources *resources);
static void draw_animated_sprite(
    AnimatedSprite animated_sprite, Transform transform, Resources *resources
);
static Texture2D load_icon(const char *fp);
static Texture2D load_sprite(const char *fp);
static SoundsRoulette load_sounds_roulette(char *prefix);
static int sort_enemies(const void *enemy1, const void *enemy2);
static float frand_01(void);
static float frand_centered(void);
static AnimatedSprite get_animated_sprite(Texture2D texture, bool is_repeat);
static bool is_animated_sprite_finished(AnimatedSprite animated_sprite);
static void play_sounds_roulette(SoundsRoulette *sounds);

int main(void) {
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "texor");
    InitAudioDevice();
    SetTargetFPS(60);

    init_resources(&RESOURCES);
    init_world(&WORLD, &RESOURCES);

    while (!WORLD.should_exit) {
        update_world(&WORLD, &RESOURCES);
        draw_world(&WORLD, &RESOURCES);
    }
}

static void init_resources(Resources *resources) {
    // -------------------------------------------------------------------
    // Audio
    resources->enemy_death_sounds = load_sounds_roulette("enemy_death");
    resources->pickup_sounds = load_sounds_roulette("pickup");
    resources->shot_sounds = load_sounds_roulette("shot");
    resources->cryonics_sounds = load_sounds_roulette("cryonics");

    // -------------------------------------------------------------------
    // init models, meshes and materials
    resources->sprite_plane = GenMeshPlane(6.0, 6.0, 2, 2);
    resources->sprite_material = LoadMaterialDefault();
    resources->sprite_material.shader = load_shader(0, "sprite.frag");
    resources->ground_shader = load_shader(0, "ground.frag");

    // -------------------------------------------------------------------
    // init sprites
    // icons
    resources->pause_icon_texture = load_icon("pause_icon");
    resources->cryonics_icon_texture = load_icon("cryonics_icon");
    resources->repulse_icon_texture = load_icon("repulse_icon");
    resources->decay_icon_texture = load_icon("decay_icon");
    resources->health_icon_texture = load_icon("health_icon");
    resources->enemy_icon_texture = load_icon("enemy_icon");

    // player
    resources->player_idle_texture = load_sprite("player_idle");
    resources->player_run_texture = load_sprite("player_run");
    resources->player_shoot_texture = load_sprite("player_shoot");
    resources->player_hurt_texture = load_sprite("player_hurt");
    resources->player_death_texture = load_sprite("player_death");
    // enemy
    resources->enemy_idle_texture = load_sprite("enemy_idle");
    resources->enemy_run_texture = load_sprite("enemy_run");
    resources->enemy_attack_texture = load_sprite("enemy_attack");
    resources->enemy_freeze_texture = load_sprite("enemy_freeze");
    resources->enemy_explode_texture = load_sprite("enemy_explode");
    // -------------------------------------------------------------------
    // init fonts
    const char *font_file_path = "./resources/fonts/ShareTechMono-Regular.ttf";
    resources->command_font = LoadFontEx(font_file_path, 30, 0, 0);
    SetTextureFilter(resources->command_font.texture, TEXTURE_FILTER_BILINEAR);

    resources->stats_font = LoadFontEx(font_file_path, 20, 0, 0);
    SetTextureFilter(resources->stats_font.texture, TEXTURE_FILTER_BILINEAR);

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

static void init_world(World *world, Resources *resources) {
    SetRandomSeed(time(NULL));
    memset(world, 0, sizeof(World));

    // -------------------------------------------------------------------
    // init commands
    init_menu_commands(world);

    // -------------------------------------------------------------------
    // init player
    world->player.transform.rotation = QuaternionIdentity();
    world->player.transform.scale = Vector3One();
    world->player.transform.translation = Vector3Zero();
    world->player.max_health = PLAYER_MAX_HEALTH;
    world->player.health = world->player.max_health;
    world->player.animated_sprite = get_animated_sprite(
        resources->player_idle_texture, true
    );

    // -------------------------------------------------------------------
    // init camera
    world->camera.fovy = 60.0;
    world->camera.position = CAMERA_INIT_POSITION;
    world->camera.target = world->camera.position;
    world->camera.target.z -= 1.0;
    world->camera.up = (Vector3){0.0, 1.0, 0.0};
    world->camera.projection = CAMERA_PERSPECTIVE;

    // -------------------------------------------------------------------
    // init game parameters
    world->state = STATE_MENU;
    world->spawn_radius = SPAWN_RADIUS;
    init_spawn_position(world);
}

static void init_menu_commands(World *world) {
    world->n_commands = 0;
    Command command = {0};

    // start easy command
    command.type = COMMAND_START_EASY;
    strcpy(command.name, "easy");
    world->commands[world->n_commands++] = command;

    // start medium command
    command.type = COMMAND_START_MEDIUM;
    strcpy(command.name, "medium");
    world->commands[world->n_commands++] = command;

    // start hard command
    command.type = COMMAND_START_HARD;
    strcpy(command.name, "hard");
    world->commands[world->n_commands++] = command;

    // start monkeytype command
    command.type = COMMAND_START_MONKEYTYPE;
    strcpy(command.name, "monkeytype");
    world->commands[world->n_commands++] = command;

    // exit command
    command.type = COMMAND_EXIT_GAME;
    command.show_separator = true;
    strcpy(command.name, "exit");
    world->commands[world->n_commands++] = command;
}

static void init_playing_commands(World *world, Resources *resources) {
    world->n_commands = 0;
    Command command = {0};

    // pause command
    command.cooldown = PAUSE_COOLDOWN;
    command.time = command.cooldown;
    command.type = COMMAND_PAUSE;
    command.show_cooldown = true;
    command.icon_texture = resources->pause_icon_texture;
    strcpy(command.name, "pause");
    world->commands[world->n_commands++] = command;

    // cryonics command
    memset(&command, 0, sizeof(Command));
    command.cooldown = CRYONICS_COOLDOWN;
    command.time = command.cooldown;
    command.type = COMMAND_CRYONICS;
    command.show_cooldown = true;
    command.cryonics.duration = CRYONICS_DURATION;
    command.icon_texture = resources->cryonics_icon_texture;
    strcpy(command.name, "cryonics");
    world->commands[world->n_commands++] = command;

    // repulse command
    memset(&command, 0, sizeof(Command));
    command.cooldown = REPULSE_COOLDOWN;
    command.time = command.cooldown;
    command.type = COMMAND_REPULSE;
    command.show_cooldown = true;
    command.repulse.speed = REPULSE_SPEED;
    command.repulse.deceleration = REPULSE_DECELERATION;
    command.repulse.radius = REPULSE_RADIUS;
    command.icon_texture = resources->repulse_icon_texture;
    strcpy(command.name, "repulse");
    world->commands[world->n_commands++] = command;

    // decay command
    memset(&command, 0, sizeof(Command));
    command.cooldown = DECAY_COOLDOWN;
    command.time = command.cooldown;
    command.type = COMMAND_DECAY;
    command.show_cooldown = true;
    command.decay.strength = DECAY_STRENGTH;
    command.icon_texture = resources->decay_icon_texture;
    strcpy(command.name, "decay");
    world->commands[world->n_commands++] = command;

    // exit command
    memset(&command, 0, sizeof(Command));
    command.type = COMMAND_EXIT_GAME;
    command.show_cooldown = false;
    strcpy(command.name, "exit");
    world->commands[world->n_commands++] = command;
}

static void init_game_over_commands(World *world) {
    world->n_commands = 0;
    Command command = {0};

    // restart command
    command.type = COMMAND_RESTART_GAME;
    strcpy(command.name, "restart");
    world->commands[world->n_commands++] = command;

    // exit command
    command.type = COMMAND_EXIT_GAME;
    strcpy(command.name, "exit");
    world->commands[world->n_commands++] = command;
}

static void init_spawn_position(World *world) {
    float angle = frand_01() * 2 * PI;
    world->spawn_position = (Vector3){
        .x = world->spawn_radius * cos(angle),
        .y = world->spawn_radius * sin(angle),
        .z = 0.0,
    };
}

static void update_world(World *world, Resources *resources) {
    bool is_altf4_pressed = IsKeyDown(KEY_LEFT_ALT) && IsKeyPressed(KEY_F4);
    world->should_exit = (WindowShouldClose() || is_altf4_pressed)
                         && !IsKeyPressed(KEY_ESCAPE);

    world->dt = world->state == STATE_PLAYING ? GetFrameTime() : 0.0;
    world->time += world->dt;
    world->freeze_time = fmaxf(0.0, world->freeze_time - world->dt);
    world->is_command_matched = false;

    update_prompt(world);
    update_commands(world, resources);
    update_camera(world);
    update_enemies_spawn(world, resources);
    update_enemies(world, resources);
    update_drops(world, resources);
    update_player(world, resources);
    world->shot.time += world->dt;

    if (world->state == STATE_PLAYING && world->submit_word[0] != '\0'
        && !world->is_command_matched) {
        world->n_backspaces_typed += strlen(world->submit_word);
    }

    world->submit_word[0] = '\0';
}

static void update_prompt(World *world) {
    int prompt_len = strlen(world->prompt);
    int pressed_char = GetCharPressed();
    if (IsKeyPressed(KEY_ENTER) > 0) {
        if (world->state == STATE_PLAYING) {
            world->n_keystrokes_typed += strlen(world->prompt);
        }

        strcpy(world->submit_word, world->prompt);
        world->prompt[0] = '\0';
    } else if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && prompt_len > 0) {
        if (world->state == STATE_PLAYING) {
            world->n_backspaces_typed += 1;
            world->n_keystrokes_typed += 1;
        }

        world->prompt[--prompt_len] = '\0';
    } else if (prompt_len < MAX_WORD_LEN - 1 && isprint(pressed_char)) {
        world->prompt[prompt_len++] = pressed_char;
        world->prompt[prompt_len] = '\0';
    }
}

static void update_enemies_spawn(World *world, Resources *resources) {
    if (world->state != STATE_PLAYING || world->n_enemies == MAX_N_ENEMIES) return;

    // don't update spawn_countdown if the world is frozen
    if (world->freeze_time <= EPSILON) {

        bool is_any_alive = false;
        for (int i = 0; i < world->n_enemies; ++i) {
            if (world->enemies[i].state != ENEMY_EXPLODE) {
                is_any_alive = true;
                break;
            }
        }
        if (is_any_alive) {
            world->spawn_countdown -= world->dt;
        } else {
            world->spawn_countdown = 0.0;
        }
    }

    if (world->spawn_countdown > 0.0) return;

    // https://www.desmos.com/calculator/jp6dgyycwn
    world->spawn_period = fmaxf(
        BASE_SPAWN_PERIOD * expf(-world->time * 0.001 * world->difficulty), 1.0
    );
    world->spawn_countdown = world->spawn_period;
    float speed_factor = BASE_ENEMY_SPEED_FACTOR
                         + (MAX_ENEMY_SPEED_FACTOR - BASE_ENEMY_SPEED_FACTOR)
                               * (1.0 - expf(-world->time * 0.001 * world->difficulty));
    float speed = PLAYER_SPEED * speed_factor;

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
        .attack_cooldown = 1.0,
        .recent_attack_time = 0.0,
        .animated_sprite = get_animated_sprite(resources->enemy_run_texture, true),
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

static void update_commands(World *world, Resources *resources) {
    float dt = world->dt;
    const char *submit_word = world->submit_word;

    for (int i = 0; i < world->n_commands; ++i) {
        Command *command = &world->commands[i];
        command->time += dt;

        if (strcmp("unfreeze", command->name) == 0 && world->freeze_time <= EPSILON) {
            command->time = 0.0;
            strcpy(command->name, "cryonics");
            world->freeze_time = 0.0;
        }

        bool is_ready = command->time >= command->cooldown;
        bool is_command_matched = strcmp(submit_word, command->name) == 0;
        world->is_command_matched |= is_command_matched;
        if (is_command_matched && is_ready) {
            if (command->type == COMMAND_EXIT_GAME) {
                world->should_exit = true;
            } else if (command->type == COMMAND_START_EASY) {
                world->state = STATE_PLAYING;
                world->difficulty = DIFFICULTY_EASY;
                strcpy(world->difficulty_str, command->name);
                init_playing_commands(world, resources);
            } else if (command->type == COMMAND_START_MEDIUM) {
                world->state = STATE_PLAYING;
                world->difficulty = DIFFICULTY_MEDIUM;
                strcpy(world->difficulty_str, command->name);
                init_playing_commands(world, resources);
            } else if (command->type == COMMAND_START_HARD) {
                world->state = STATE_PLAYING;
                world->difficulty = DIFFICULTY_HARD;
                strcpy(world->difficulty_str, command->name);
                init_playing_commands(world, resources);
            } else if (command->type == COMMAND_START_MONKEYTYPE) {
                world->state = STATE_PLAYING;
                world->difficulty = DIFFICULTY_MONKEYTYPE;
                strcpy(world->difficulty_str, command->name);
                init_playing_commands(world, resources);
            } else if (command->type == COMMAND_PAUSE && world->state == STATE_PLAYING) {
                command->time = command->cooldown + 1.0;
                strcpy(command->name, "continue");
                world->state = STATE_PAUSE;
            } else if (command->type == COMMAND_PAUSE && world->state == STATE_PAUSE) {
                command->time = 0.0;
                strcpy(command->name, "pause");
                world->state = STATE_PLAYING;
            } else if (command->type == COMMAND_RESTART_GAME) {
                init_world(world, resources);
            } else if (command->type == COMMAND_CRYONICS && world->state == STATE_PLAYING && world->freeze_time <= EPSILON) {
                command->time = command->cooldown + 1.0;
                strcpy(command->name, "unfreeze");
                world->freeze_time = command->cryonics.duration;
                play_sounds_roulette(&resources->cryonics_sounds);
            } else if (command->type == COMMAND_CRYONICS && world->freeze_time >= EPSILON) {
                command->time = 0.0;
                strcpy(command->name, "cryonics");
                world->freeze_time = 0.0;
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
                        enemy->impulse.deceleration = command->repulse.deceleration;
                        enemy->impulse.speed = command->repulse.speed;
                        enemy->impulse.direction = dir;
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

static void update_enemies(World *world, Resources *resources) {
    if (world->state != STATE_PLAYING) return;

    const char *submit_word = world->submit_word;
    int kill_enemy_idx = -1;

    for (int i = 0; i < world->n_enemies; ++i) {
        Enemy *enemy = &world->enemies[i];
        update_animated_sprite(&enemy->animated_sprite, world->dt);

        if (enemy->state != enemy->next_state) {
            enemy->state = enemy->next_state;
            if (enemy->state == ENEMY_IDLE) {
                enemy->animated_sprite = get_animated_sprite(
                    resources->enemy_idle_texture, true
                );
            } else if (enemy->state == ENEMY_RUN) {
                enemy->animated_sprite = get_animated_sprite(
                    resources->enemy_run_texture, true
                );
            } else if (enemy->state == ENEMY_ATTACK) {
                enemy->animated_sprite = get_animated_sprite(
                    resources->enemy_attack_texture, false
                );
            } else if (enemy->state == ENEMY_FREEZE) {
                enemy->animated_sprite = get_animated_sprite(
                    resources->enemy_freeze_texture, true
                );
            } else if (enemy->state == ENEMY_EXPLODE) {
                enemy->animated_sprite = get_animated_sprite(
                    resources->enemy_explode_texture, false
                );
            }
        }

        // count number of matched chars with prompt
        enemy->n_matched_chars = 0;
        char *str1 = enemy->name;
        char *str2 = world->prompt;
        while (*str1 && *str2 && *str1 == *str2) {
            enemy->n_matched_chars++;
            str1++;
            str2++;
        }

        if (strcmp(submit_word, enemy->name) == 0) {
            world->shot = (Shot
            ){.time = 0.0,
              .trace_duration = SHOT_TRACE_DURATION,
              .start_position = world->player.transform.translation,
              .end_position = enemy->transform.translation};
            enemy->next_state = ENEMY_EXPLODE;
            world->is_command_matched = true;
            play_sounds_roulette(&resources->shot_sounds);
            play_sounds_roulette(&resources->enemy_death_sounds);
            continue;
        }

        if (enemy->state == ENEMY_EXPLODE) {
            if (is_animated_sprite_finished(enemy->animated_sprite)) {
                kill_enemy_idx = i;
            }
            continue;
        }

        bool can_move = true;
        bool can_attack = true;
        Vector3 step = {0};
        if (enemy->impulse.speed > 0.0) {
            Vector3 dir = Vector3Normalize(enemy->impulse.direction);
            step = Vector3Scale(dir, enemy->impulse.speed * world->dt);
            enemy->impulse.speed -= enemy->impulse.deceleration * world->dt;
            can_move = false;
            can_attack = false;
        }
        enemy->transform.translation = Vector3Add(enemy->transform.translation, step);

        if (world->freeze_time > EPSILON) {
            enemy->next_state = ENEMY_FREEZE;
            continue;
        }

        // apply enemy movements and attacks
        Vector3 dir = Vector3Subtract(
            world->player.transform.translation, enemy->transform.translation
        );
        float dist_to_player = Vector3Length(dir);
        dir = Vector3Normalize(dir);
        float time_since_last_attack = world->time - enemy->recent_attack_time;
        can_attack &= dist_to_player <= (ENEMY_RADIUS + PLAYER_RADIUS)
                      && time_since_last_attack > enemy->attack_cooldown;
        can_move &= dist_to_player > (ENEMY_RADIUS + PLAYER_RADIUS);

        if (can_attack) {
            enemy->recent_attack_time = world->time;
            world->player.health -= enemy->attack_strength;
            world->player.next_state = PLAYER_HURT;
            world->camera_shake = (CameraShake
            ){.time = 0.0,
              .duration = CAMERA_SHAKE_TIME,
              .strength = enemy->attack_strength};
            enemy->next_state = ENEMY_ATTACK;
        } else if (can_move) {
            Vector3 step = Vector3Scale(dir, enemy->speed * world->dt);
            enemy->transform.translation = Vector3Add(enemy->transform.translation, step);
            enemy->next_state = ENEMY_RUN;
        } else if (enemy->state == ENEMY_ATTACK && is_animated_sprite_finished(enemy->animated_sprite)) {
            enemy->next_state = ENEMY_IDLE;
        }

        // resolve enemy collisions with each other
        for (int i = 0; i < world->n_enemies; ++i) {
            Enemy *enemy0 = &world->enemies[i];
            for (int j = 0; j < world->n_enemies; ++j) {
                if (i == j) continue;
                Enemy *enemy1 = &world->enemies[j];
                Vector3 v = Vector3Subtract(
                    enemy1->transform.translation, enemy0->transform.translation
                );
                if (Vector3Length(v) < ENEMY_RADIUS * 2.0) {
                    v = Vector3Scale(Vector3Normalize(v), ENEMY_RADIUS * 2.0);
                    enemy1->transform.translation = Vector3Add(
                        enemy0->transform.translation, v
                    );
                }
            }
        }

        // rotate enemies towards the player
        if (can_attack || can_move) {
            enemy->transform.rotation = QuaternionFromVector3ToVector3(
                (Vector3){0.0, 1.0, 0.0}, (Vector3){dir.x, dir.y, 0.0}
            );
        }
    }

    if (kill_enemy_idx != -1) {
        Vector3 position = world->enemies[kill_enemy_idx].transform.translation;

        world->n_enemies -= 1;
        world->n_enemies_killed += 1;
        memmove(
            &world->enemies[kill_enemy_idx],
            &world->enemies[kill_enemy_idx + 1],
            sizeof(Enemy) * (world->n_enemies - kill_enemy_idx)
        );

        float p = frand_01();
        if (DROP_PROBABILITY >= p && world->n_drops < MAX_N_DROPS) {
            int idx = GetRandomValue(0, N_DROPS - 1);
            Drop drop = {0};
            drop.position = position;
            drop.time = DROP_DURATION;
            if (idx == DROP_HEAL) {
                drop.type = DROP_HEAL;
                drop.heal.value = DROP_HEAL_VALUE;
            } else if (idx == DROP_REFRESH) {
                drop.type = DROP_REFRESH;
            }
            world->drops[world->n_drops++] = drop;
        }
    }

    // sort enemies
    qsort(world->enemies, world->n_enemies, sizeof(Enemy), sort_enemies);
}

static void update_drops(World *world, Resources *resources) {

    Vector3 player_position = world->player.transform.translation;

    int n_alive_drops = 0;
    for (int i = 0; i < world->n_drops; ++i) {
        Drop *drop = &world->drops[i];
        drop->time -= world->dt;
        if (drop->time > EPSILON) {
            float dist = Vector3Distance(drop->position, player_position);
            if (dist <= PLAYER_RADIUS + DROP_RADIUS) {
                if (drop->type == DROP_HEAL) {
                    world->player.health += drop->heal.value;
                } else if (drop->type == DROP_REFRESH) {
                    for (int command_i = 0; command_i < world->n_commands; ++command_i) {
                        Command *command = &world->commands[command_i];
                        command->time = command->cooldown;
                    }
                }
                play_sounds_roulette(&resources->pickup_sounds);
            } else {
                world->drops[n_alive_drops++] = *drop;
            }
        }
    }

    world->n_drops = n_alive_drops;
}

static void update_player(World *world, Resources *resources) {
    Player *player = &world->player;
    update_animated_sprite(&player->animated_sprite, world->dt);

    // -------------------------------------------------------------------
    // apply player next state
    if (player->state != player->next_state && player->state != PLAYER_DEATH) {
        player->state = player->next_state;
        if (player->state == PLAYER_IDLE) {
            player->animated_sprite = get_animated_sprite(
                resources->player_idle_texture, true
            );
        } else if (player->state == PLAYER_RUN) {
            player->animated_sprite = get_animated_sprite(
                resources->player_run_texture, true
            );
        } else if (player->state == PLAYER_SHOOT) {
            player->animated_sprite = get_animated_sprite(
                resources->player_shoot_texture, false
            );
        } else if (player->state == PLAYER_HURT) {
            player->animated_sprite = get_animated_sprite(
                resources->player_hurt_texture, false
            );
        } else if (player->state == PLAYER_DEATH) {
            player->animated_sprite = get_animated_sprite(
                resources->player_death_texture, false
            );
        }
    }

    if (player->health <= 0.0) {
        if (player->state != PLAYER_DEATH) {
            player->next_state = PLAYER_DEATH;
        } else if (is_animated_sprite_finished(player->animated_sprite)) {
            world->state = STATE_GAME_OVER;
            init_game_over_commands(world);
        }
        return;
    }

    bool is_just_shot = world->shot.time == 0.0 && world->shot.trace_duration > 0.0;
    if (is_just_shot) {
        Vector3 dir = Vector3Normalize(
            Vector3Subtract(world->shot.end_position, world->shot.start_position)
        );
        player->transform.rotation = QuaternionFromVector3ToVector3(
            (Vector3){0.0, 1.0, 0.0}, (Vector3){dir.x, dir.y, 0.0}
        );
        player->next_state = PLAYER_SHOOT;
    }

    Vector2 dir = Vector2Zero();
    dir.y += IsKeyDown(KEY_UP);
    dir.y -= IsKeyDown(KEY_DOWN);
    dir.x -= IsKeyDown(KEY_LEFT);
    dir.x += IsKeyDown(KEY_RIGHT);

    if (Vector2Length(dir) >= EPSILON) {
        dir = Vector2Normalize(dir);
        Vector2 step = Vector2Scale(dir, PLAYER_SPEED * world->dt);

        Vector3 position = player->transform.translation;
        position.x += step.x;
        position.y += step.y;

        // resolve collision with enemies
        for (int i = 0; i < world->n_enemies; ++i) {
            Enemy *enemy = &world->enemies[i];
            if (enemy->state == ENEMY_EXPLODE) continue;
            Vector3 v = Vector3Subtract(position, enemy->transform.translation);
            if (Vector3Length(v) < ENEMY_RADIUS + PLAYER_RADIUS) {
                v = Vector3Scale(Vector3Normalize(v), ENEMY_RADIUS + PLAYER_RADIUS);
                position = Vector3Add(enemy->transform.translation, v);
            }
        }

        if (Vector3Length(position) > world->spawn_radius) {
            position = Vector3Scale(Vector3Normalize(position), world->spawn_radius);
        }

        player->transform.translation = position;
        player->transform.rotation = QuaternionFromVector3ToVector3(
            (Vector3){0.0, 1.0, 0.0}, (Vector3){dir.x, dir.y, 0.0}
        );
        player->next_state = PLAYER_RUN;
    } else if (player->state == PLAYER_RUN) {
        player->next_state = PLAYER_IDLE;
    }

    if (world->state == STATE_PLAYING) {
        // damage player if submitted command doesn't exist
        if (world->submit_word[0] != '\0' && !world->is_command_matched) {
            world->player.health -= WRONG_COMMAND_DAMAGE;
            world->camera_shake = (CameraShake
            ){.time = 0.0,
              .duration = CAMERA_SHAKE_TIME,
              .strength = WRONG_COMMAND_DAMAGE};
            player->next_state = PLAYER_HURT;
        }

        // damage player if backspace is pressed
        int prompt_len = strlen(world->prompt);
        if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE))
            && prompt_len > 0) {
            world->player.health -= BACKSPACE_DAMAGE;
        }
    }

    world->player.health = Clamp(world->player.health, 0.0, PLAYER_MAX_HEALTH);

    if (is_animated_sprite_finished(player->animated_sprite)) {
        player->next_state = PLAYER_IDLE;
    }
}

static void update_camera(World *world) {
    Camera3D *camera = &world->camera;
    CameraShake *shake = &world->camera_shake;
    camera->position = CAMERA_INIT_POSITION;

    if (shake->time <= shake->duration && world->state == STATE_PLAYING) {
        float x_shake = frand_centered();
        float y_shake = frand_centered();
        float k = shake->time / shake->duration;
        x_shake *= k * shake->strength * 0.001;
        y_shake *= k * shake->strength * 0.001;

        camera->position.x += x_shake;
        camera->position.y += y_shake;

        shake->time += world->dt;
    }
}

static void update_animated_sprite(AnimatedSprite *animated_sprite, float dt) {
    animated_sprite->time += dt;
    float frame_duration = 1.0 / animated_sprite->fps;
    animated_sprite->frame_idx = animated_sprite->time / frame_duration;
    if (animated_sprite->frame_idx >= animated_sprite->n_frames
        && animated_sprite->is_repeat) {
        animated_sprite->frame_idx %= animated_sprite->n_frames;
    } else {
        animated_sprite->frame_idx = min(
            animated_sprite->frame_idx, animated_sprite->n_frames - 1
        );
    }
}

static void draw_world(World *world, Resources *resources) {
    BeginDrawing();
    ClearBackground(BLANK);

    // scene
    if (world->state > STATE_MENU) {
        BeginMode3D(world->camera);

        // draw arena boundary
        float light_pos[2] = {
            world->player.transform.translation.x, world->player.transform.translation.y};
        SetShaderValue(
            resources->ground_shader,
            GetShaderLocation(resources->ground_shader, "u_light_pos"),
            light_pos,
            SHADER_UNIFORM_VEC2
        );
        SetShaderValue(
            resources->ground_shader,
            GetShaderLocation(resources->ground_shader, "u_radius"),
            &world->spawn_radius,
            SHADER_UNIFORM_FLOAT
        );
        BeginShaderMode(resources->ground_shader);
        DrawCylinderEx(
            (Vector3){0.0, 0.0, -1.0},
            (Vector3){0.0, 0.0, -0.1},
            world->spawn_radius,
            world->spawn_radius,
            64,
            WHITE
        );
        EndShaderMode();

        // draw player
        draw_animated_sprite(
            world->player.animated_sprite, world->player.transform, resources
        );

        // draw drops
        for (int i = 0; i < world->n_drops; ++i) {
            Drop drop = world->drops[i];
            Vector3 start = drop.position;
            Vector3 end = start;
            end.z += 2.0;
            Color color;
            if (drop.type == DROP_HEAL) {
                color = MAGENTA;
            } else if (drop.type == DROP_REFRESH) {
                color = GREEN;
            }

            float r = 1.0 + (sinf(world->time * 4.0) + 1.0) * 0.5 * 0.25;

            DrawCapsule(start, end, r, 16, 16, color);

            start.z -= 1.0;
            end.z += 1.0;
            DrawCapsule(start, end, r * 1.5, 16, 16, ColorAlpha(color, 0.3));
        }

        // draw enemies
        for (int i = 0; i < world->n_enemies; ++i) {
            Enemy enemy = world->enemies[i];
            if (Vector3Length(enemy.transform.translation) <= world->spawn_radius) {
                draw_animated_sprite(enemy.animated_sprite, enemy.transform, resources);
            }
        }

        // draw shot
        Shot *shot = &world->shot;
        if (shot->time < shot->trace_duration) {
            Vector3 a = shot->start_position;
            Vector3 b = shot->end_position;
            Vector3 d = Vector3Normalize(Vector3Subtract(b, a));
            a = Vector3Add(a, Vector3Scale(d, 2.0));
            float alpha = 1.0 - shot->time / shot->trace_duration;
            Color color = {255, 240, 50};
            DrawCylinderEx(a, b, 0.2, 0.4, 8, ColorAlpha(color, alpha));
        }

        EndMode3D();

        if (world->state < STATE_GAME_OVER) {
            // draw enemy names
            for (int i = 0; i < world->n_enemies; ++i) {
                Enemy enemy = world->enemies[i];
                if (enemy.state == ENEMY_EXPLODE
                    || Vector3Length(enemy.transform.translation) > world->spawn_radius)
                    continue;

                Vector2 screen_pos = GetWorldToScreen(
                    enemy.transform.translation, world->camera
                );
                Vector2 text_size = MeasureTextEx(
                    resources->command_font,
                    enemy.name,
                    resources->command_font.baseSize,
                    0
                );

                Vector2 rec_size = Vector2Scale(text_size, 1.2);
                Vector2 rec_center = {screen_pos.x, screen_pos.y - 35.0};
                Vector2 rec_pos = Vector2Subtract(
                    rec_center, Vector2Scale(rec_size, 0.5)
                );

                Rectangle rec = {rec_pos.x, rec_pos.y, rec_size.x, rec_size.y};
                Vector2 text_pos = {
                    rec_center.x - 0.5 * text_size.x,
                    rec_center.y - 0.5 * resources->command_font.baseSize};

                DrawRectangleRounded(rec, 0.3, 16, (Color){20, 20, 20, 190});
                draw_text(resources->command_font, enemy.name, text_pos, world->prompt);
            }

            // commands pane
            Rectangle rec = {2.0, 2.0, 200.0, 400.0};
            DrawRectangleRounded(rec, 0.05, 16, UI_BACKGROUND_COLOR);
            DrawRectangleRoundedLines(rec, 0.05, 16, 2.0, UI_OUTLINE_COLOR);

            // stats pane
            rec = (Rectangle){2.0, 408.0, 200.0, 180.0};
            DrawRectangleRounded(rec, 0.05, 16, UI_BACKGROUND_COLOR);
            DrawRectangleRoundedLines(rec, 0.05, 16, 2.0, UI_OUTLINE_COLOR);

            // draw health
            float ratio = fmaxf(0.0, world->player.health / world->player.max_health);
            Color color = ColorFromNormalized((Vector4){
                .x = 1.0 - ratio,
                .y = ratio,
                .z = 0.0,
                .w = 1.0,
            });
            rec = (Rectangle){8.0, 8.0, 190.0, 10.0};
            DrawRectangleRoundedLines(rec, 0.5, 16, 2.0, UI_OUTLINE_COLOR);
            rec.width *= ratio;
            DrawRectangleRounded(rec, 0.5, 16, color);

            BeginShaderMode(resources->sprite_material.shader);
            DrawTextureEx(
                resources->health_icon_texture,
                (Vector2){rec.x, rec.y - 10.0},
                0.0,
                1.0,
                WHITE
            );
            EndShaderMode();

            // draw enemies spawn progress bar
            if (world->freeze_time >= EPSILON) {
                ratio = world->freeze_time / CRYONICS_DURATION;
                color = BLUE;
            } else {
                ratio = 1.0 - fmaxf(0.0, world->spawn_countdown / world->spawn_period);
                color = ColorFromNormalized((Vector4){
                    .x = ratio,
                    .y = 1.0 - ratio,
                    .z = 0.0,
                    .w = 1.0,
                });
            }
            rec = (Rectangle){8.0, 414.0, 190.0, 10.0};
            DrawRectangleRoundedLines(rec, 0.5, 16, 2.0, UI_OUTLINE_COLOR);
            rec.width *= ratio;
            DrawRectangleRounded(rec, 0.5, 16, color);

            BeginShaderMode(resources->sprite_material.shader);
            DrawTextureEx(
                resources->enemy_icon_texture,
                (Vector2){rec.x, rec.y - 10.0},
                0.0,
                1.0,
                WHITE
            );
            EndShaderMode();
        }

        // ---------------------------------------------------------------
        // draw stats
        // TODO: draw stats only on the STATE_GAME_OVER
        // if (world->state == STATE_GAME_OVER) {

        float accuracy = 1.0;
        int cpm = 0;
        if (world->n_keystrokes_typed > 0) {
            accuracy = 1.0 - (float)world->n_backspaces_typed / world->n_keystrokes_typed;
            cpm = accuracy * world->n_keystrokes_typed * 60.0 / world->time;
        }

        int y = 448;
        draw_text(
            resources->stats_font,
            TextFormat("Kills: %d", world->n_enemies_killed),
            (Vector2){8.0, y},
            0
        );

        y += resources->stats_font.baseSize;
        draw_text(
            resources->stats_font,
            TextFormat("Play time: %d s", (int)world->time),
            (Vector2){8.0, y},
            0
        );

        y += resources->stats_font.baseSize;
        draw_text(
            resources->stats_font,
            TextFormat("Keystrokes: %d", (int)world->n_keystrokes_typed),
            (Vector2){8.0, y},
            0
        );

        y += resources->stats_font.baseSize;
        draw_text(
            resources->stats_font, TextFormat("CPM: %d", cpm), (Vector2){8.0, y}, 0
        );

        y += resources->stats_font.baseSize;
        draw_text(
            resources->stats_font,
            TextFormat("Accuracy: %.2f", accuracy),
            (Vector2){8.0, y},
            0
        );

        y += resources->stats_font.baseSize;
        draw_text(
            resources->stats_font,
            TextFormat("Difficulty: %s", world->difficulty_str),
            (Vector2){8.0, y},
            0
        );
        // }
    }

    // draw commands
    int n = 0;
    for (int i = 0; i < world->n_commands; ++i) {
        Command *command = &world->commands[i];
        float y = 40.0 + 1.8 * (n++) * resources->command_font.baseSize;

        float ratio;
        ratio = fminf(1.0, command->time / command->cooldown);
        Color color = ColorFromNormalized((Vector4){
            .x = 1.0 - ratio,
            .y = ratio,
            .z = 0.0,
            .w = 1.0,
        });

        if (command->show_separator) {
            DrawLineEx(
                (Vector2){8.0, y - 4.0},
                (Vector2){200.0, y - 4.0},
                2,
                ColorAlpha(WHITE, 0.3)
            );
        }

        float text_x = 8.0;
        if (IsTextureReady(command->icon_texture)) {
            text_x += command->icon_texture.width + 2.0;
            BeginShaderMode(resources->sprite_material.shader);
            float alpha = 1.0;
            if (ratio < 1.0 - EPSILON) {
                float min_alpha = 0.2;
                float max_alpha = 0.4;
                alpha = ((sinf(GetTime() * 8.0) + 1.0) / 2.0) * (max_alpha - min_alpha)
                        + min_alpha;
            }
            DrawTextureEx(
                command->icon_texture,
                (Vector2){8.0, y - 2.0},
                0.0,
                1.0,
                ColorAlpha(GREEN, alpha)
            );
            EndShaderMode();
        }

        draw_text(
            resources->command_font, command->name, (Vector2){text_x, y}, world->prompt
        );

        // draw command cooldown progress bar
        if (command->show_cooldown) {
            float width = 190.0 * ratio;
            Rectangle rec = {8.0, y + resources->command_font.baseSize, width, 5.0};
            DrawRectangleRec(rec, color);
        }
    }

    // draw prompt
    static char prompt[3] = {'>', ' ', '\0'};
    Font font = resources->command_font;
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

static void draw_sprite_2d(Texture2D texture, Vector2 position, Resources *resources) {
    int loc = GetShaderLocation(resources->sprite_material.shader, "src");
    float src[4] = {0.0, 0.0, texture.width, texture.height};
    SetShaderValue(resources->sprite_material.shader, loc, src, SHADER_UNIFORM_VEC4);
    resources->sprite_material.maps[0].texture = texture;

    rlPushMatrix();
    rlTranslatef(position.x, position.y, 0.0);
    DrawMesh(resources->sprite_plane, resources->sprite_material, MatrixIdentity());
    rlPopMatrix();
}

static void draw_animated_sprite(
    AnimatedSprite animated_sprite, Transform transform, Resources *resources
) {
    int loc = GetShaderLocation(resources->sprite_material.shader, "src");

    float x = animated_sprite.frame_idx * animated_sprite.frame_width;

    float src[4] = {x, 0.0, animated_sprite.frame_width, animated_sprite.texture.height};
    SetShaderValue(resources->sprite_material.shader, loc, src, SHADER_UNIFORM_VEC4);
    resources->sprite_material.maps[0].texture = animated_sprite.texture;

    Vector3 axis;
    float angle;
    QuaternionToAxisAngle(transform.rotation, &axis, &angle);

    rlPushMatrix();
    rlTranslatef(transform.translation.x, transform.translation.y, 0.0);
    rlRotatef(90.0, 1.0, 0.0, 0.0);
    rlRotatef(RAD2DEG * angle, axis.x, axis.z, axis.y);
    DrawMesh(resources->sprite_plane, resources->sprite_material, MatrixIdentity());
    rlPopMatrix();
}

static Texture2D load_icon(const char *name) {
    Texture2D texture = LoadTexture(TextFormat("./resources/sprites/%s.png", name));
    return texture;
}

static Texture2D load_sprite(const char *name) {
    Texture2D texture = LoadTexture(TextFormat("./resources/sprites/%s.png", name));
    SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
    return texture;
}

static int pstrcmp(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

static SoundsRoulette load_sounds_roulette(char *prefix) {
    SoundsRoulette sounds = {0};

    int n_file_names;
    const char *path = "./resources/audio";
    DIR *dir = opendir(path);
    if (dir == NULL) {
        TraceLog(LOG_ERROR, "Failed to open directory %s", path);
        exit(1);
    }

    char **file_names = (char **)malloc(256 * sizeof(char *));

    int i = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            file_names[i] = strdup(entry->d_name);
            i += 1;
        }
    }

    n_file_names = i;
    closedir(dir);
    qsort(file_names, n_file_names, sizeof(file_names[0]), pstrcmp);

    for (int i = 0; i < n_file_names; ++i) {
        if (sounds.n == MAX_N_ROULETTE_SOUNDS) break;

        char *file_name = file_names[i];
        if (strncmp(file_name, prefix, strlen(prefix)) == 0) {
            const char *file_path_parts[2] = {path, file_name};
            const char *file_path = TextJoin(file_path_parts, 2, "/");
            sounds.sounds[sounds.n++] = LoadSound(file_path);
        }
    }

    return sounds;
}

static int sort_enemies(const void *enemy1, const void *enemy2) {
    int n1 = ((Enemy *)enemy1)->n_matched_chars;
    int n2 = ((Enemy *)enemy2)->n_matched_chars;
    if (n1 > n2) return 1;
    else if (n1 < n2) return -1;
    else return 0;
}

static float frand_01(void) {
    return ((float)GetRandomValue(0, RAND_MAX) / RAND_MAX);
}

static float frand_centered(void) {
    return (frand_01() * 2.0) - 1.0;
}

static AnimatedSprite get_animated_sprite(Texture2D texture, bool is_repeat) {
    int frame_width = 32;
    int fps = 10;

    AnimatedSprite sprite = {0};
    sprite.is_repeat = is_repeat;
    sprite.texture = texture;
    sprite.n_frames = texture.width / frame_width;
    sprite.frame_width = frame_width;
    sprite.fps = fps;

    return sprite;
}

static bool is_animated_sprite_finished(AnimatedSprite animated_sprite) {
    if (animated_sprite.is_repeat) return false;
    float frame_duration = 1.0 / animated_sprite.fps;
    float total_duration = frame_duration * animated_sprite.n_frames;
    return animated_sprite.time >= total_duration;
}

static void play_sounds_roulette(SoundsRoulette *sounds) {
    if (sounds->n == 0) return;
    PlaySound(sounds->sounds[sounds->i++]);
    if (sounds->i >= sounds->n) sounds->i = 0;
}

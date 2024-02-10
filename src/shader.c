#include "shader.h"

#include <stdlib.h>
#include <string.h>

static char *load_shader_src(const char *file_name);

Shader load_shader(const char *vs_file_name, const char *fs_file_name) {
    char *vs = NULL;
    char *fs = NULL;

    if (vs_file_name) vs = load_shader_src(vs_file_name);
    else vs = load_shader_src("base.vert");

    if (fs_file_name) fs = load_shader_src(fs_file_name);
    Shader shader = LoadShaderFromMemory(vs, fs);

    if (vs) free(vs);
    if (fs) free(fs);

    return shader;
}

static char *load_shader_src(const char *file_name) {

    const char *version;
    version = "#version 460 core";

    char *common = LoadFileText("resources/shaders/common.glsl");
    char *text = LoadFileText(TextFormat("resources/shaders/%s", file_name));

    char *src = malloc(strlen(version) + strlen(common) + strlen(text) + 6);

    int p = 0;

    strcpy(&src[p], version);
    p += strlen(version);
    src[p++] = '\n';

    strcpy(&src[p], common);
    p += strlen(common);
    src[p++] = '\n';

    strcpy(&src[p], text);

    UnloadFileText(common);
    UnloadFileText(text);

    return src;
}

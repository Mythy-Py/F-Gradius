#include <raylib.h>
#include <vector>
#include <sys/types.h>
#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "raylib.h"
#include "raymath.h"

const int screenWidth = 800;
const int screenHeight = 600;
Font font;

enum _ANIM
{
    ONCE,
    LOOP
};

enum _CONTAINER
{
    ASTEROIDS,
    PROJECTILES,
    ENEMYPROJECTILES,
    EXPLOSIONS
};

struct asteroid_t
{
    Texture2D *texture;
    Vector2 pos;
    int hp;
};

struct projectile_t
{
    Texture2D *texture;
    Vector2 pos;
    Vector2 direction;
    float speed;
    int damage;
};

struct ship_t
{
    Texture2D *texture;
    Vector2 pos;
    float speed;
    int hp;
    int max_hp;
    int shield;
    int max_shield;
    float shooting_cooldown;
    float last_shot;
    std::vector<projectile_t> weaponarsenal;
    float shieldrecovertime;
    uint8_t weapon;
    uint8_t player;
    float powerup_cd;
};

struct enemy_t
{
    Texture2D *texture;
    Vector2 pos;
    std::vector<Vector2> *path;
    int pathpos;
    float speed;
    int hp;
    float shooting_cooldown;
    float last_shot;
};

struct animation_t
{
    Texture2D *texture;
    Rectangle framerec;
    Vector2 position;
    int frames;
    int cols;
    int rows;
    float framespeed;
    int currentframe;
    float framescounter;
    _ANIM style;
};

struct game
{
    Vector2 ship_startpos;
    Vector2 screencenter;
    double gametime = 0;
    float delta = 0;
    uint32_t highscore = 0;
    bool pause = false;
    bool exit = false;
    int bg_scollspeed;
    float bg_scrollpos;
    int textsize;
    int window_height;
    int window_width;

    std::vector<Vector2> collisions;
    std::vector<asteroid_t> asteroids;
    std::vector<projectile_t> projectiles;
    std::vector<projectile_t> enemy_projectiles;
    std::vector<enemy_t> enemies;
    std::vector<animation_t> explosions;
    std::vector<animation_t> powerups;

    struct
    {
        Texture2D bg_tex;
        Texture2D ship_tex;
        Texture2D ufo_tex;
        Texture2D torpedo_tex;
        Texture2D explosion_tex;
        Texture2D explosion2_tex;
        Texture2D big_boom_tex;
        Texture2D shield_tex;
        Texture2D boost_text;
        Texture2D orb_red;
        Texture2D ui_bar_f;
        Texture2D ui_bar_b;
        Texture2D ui_bar_red;
        Texture2D ui_bar_blue;
        Texture2D powup_life_tex;
        Texture2D powup_shield_tex;
        Texture2D powup_weapon_tex;

        std::vector<Texture2D> asteroid_textures;
    } textures;

    struct
    {
        animation_t explosion;
        animation_t explosion2;
        animation_t big_boom;
        animation_t boost;
        animation_t powup_life;
        animation_t powup_shield;
        animation_t powup_weapon;
    } animations;

    struct
    {
        Music bg_music;
        Music opening;
        Sound explosion_sound;
        Sound gunloop_sound;
        Sound click;
        Sound select;
    } sound;

    struct
    {
        ship_t ship;
        projectile_t weapon1;
        projectile_t enemy_attack;
        enemy_t enemy;
        std::vector<Vector2> enemy_path;
    } var;

} game;

void load_textures_from_dir(std::vector<Texture2D> &vec, const char *path)
{
    auto dir = opendir(path);
    if (!dir)
    {
        printf("couldnt find assets");
        return;
    }
    dirent *entry = readdir(dir);
    char *text = (char *)malloc(1024);
    while (entry != nullptr)
    {
        if (strstr(entry->d_name, ".png") != nullptr)
        {
            memset(text, 0, 1024);
            strcpy(text, path);
            strcat(text, "/");
            strcat(text, entry->d_name);
            printf("TEXTPATH : %.*s \n", 1024, text);
            vec.push_back(LoadTexture(text));
        }
        entry = readdir(dir);
    }

    free(entry);
    free(text);
}

void playerdamage(ship_t &player, int damage)
{
    static double last_hit = 0;
    // invincibility time

    if (damage < 0 && player.hp < player.max_hp)
    {
        player.hp -= damage;
        if (player.hp > player.max_hp)
            player.hp = player.max_hp;
        return;
    }

    if (game.gametime - last_hit > player.shieldrecovertime / 2)
        return;

    last_hit = game.gametime;
    player.shield -= damage;
    if (player.shield < 0)
    {
        player.hp += player.shield;
        player.shield = 0;
    }
}

void shieldrecover(ship_t &player)
{
    static double last_hit = game.gametime;
    static int last_shield = player.shield;
    static int last_hp = player.hp;

    if (player.shield == player.max_shield)
        return;

    if (last_shield > player.shield || last_hp > player.hp)
    {
        last_shield = player.shield;
        last_hp = player.hp;
        last_hit = game.gametime;
        return;
    }
    else if (game.gametime - last_hit > player.shieldrecovertime)
    {
        player.shield += 10;
        if (player.shield > player.max_shield)
            player.shield = player.max_shield;
        last_shield = player.shield;
    }
}

void asteroids_update(std::vector<asteroid_t> &asteroids)
{
    auto height = GetScreenHeight();
    for (int i = 0; i < asteroids.size(); i++)
    {
        auto &asteroid = asteroids[i];
        asteroid.pos.y += game.delta * 100;
        // out of vision
        if (asteroid.pos.y > height)
            asteroids.erase(std::next(asteroids.begin(), i));
    }
}

void asteroids_spawn(std::vector<asteroid_t> &asteroids, Texture2D *texture, int num)
{
    auto height = GetScreenHeight();
    auto width = GetScreenWidth();
    for (int i = 0; i < num; i++)
    {
        asteroids.push_back((asteroid_t){texture, {(float)(rand() % (2 * width) - width), (float)(rand() % (height)-1.5f * height)}});
    }
}

void projectiles_update(std::vector<projectile_t> &projectiles)
{
    auto height = GetScreenHeight();
    auto width = GetScreenWidth();

    for (int i = 0; i < projectiles.size(); i++)
    {
        auto &projectile = projectiles[i];
        projectile.pos.x += (game.delta * projectile.speed) * projectile.direction.x;
        projectile.pos.y += (game.delta * projectile.speed) * projectile.direction.y;
        // boooost!!!
        projectile.speed += game.delta * 1000;
        // out of Vision
        if (projectile.pos.y > height + 10 || projectile.pos.y < -10 || projectile.pos.x < -10 || projectile.pos.x > width + 10)
        {
            projectiles.erase(std::next(projectiles.begin(), i));
        }
    }
}

void collision_handler(std::vector<projectile_t> &projectiles, std::vector<asteroid_t> &asteroids, std::vector<enemy_t> &enemies, ship_t &player, std::vector<projectile_t> &enemy_projectiles, std::vector<animation_t> &powerups)
{
    int damage = 0;
    // DrawCircleV(player.pos,player.texture->height/2,BLUE);

    // Iterate trough Projectiles
    for (int i = 0; i < projectiles.size(); i++)
    {
        Vector2 projectile_hitbox = {projectiles[i].pos.x + projectiles[i].texture->width / 2, projectiles[i].pos.y};
        // DrawCircleV(projectile_hitbox,5,BLUE);

        // collision between asteroid and projectile
        for (int j = 0; j < asteroids.size(); j++)
        {
            Vector2 asteroids_hitbox = {asteroids[j].pos.x + asteroids[j].texture->width / 2, asteroids[j].pos.y + asteroids[j].texture->height / 2};
            // DrawCircleV(asteroids_hitbox,asteroids[j].texture->height/2,RED);
            if (CheckCollisionPointCircle(projectile_hitbox, asteroids_hitbox, asteroids[j].texture->height / 2))
            {
                game.collisions.push_back(projectile_hitbox);
                projectiles.erase(std::next(projectiles.begin(), i));
                asteroids.erase(std::next(asteroids.begin(), j));
                game.highscore += 100;
                goto LOOP_END;
            }
        }

        // collision between enemy and projectile
        for (int j = 0; j < enemies.size(); j++)
        {
            Vector2 enemies_hitbox = {enemies[j].pos.x + enemies[j].texture->width / 2, enemies[j].pos.y + enemies[j].texture->height / 2};
            // DrawCircleV(enemies_hitbox,enemies[j].texture->height/2,ORANGE);
            if (CheckCollisionPointCircle(projectile_hitbox, enemies_hitbox, enemies[j].texture->height / 2))
            {
                game.collisions.push_back(projectile_hitbox);
                projectiles.erase(std::next(projectiles.begin(), i));
                enemies.erase(std::next(enemies.begin(), j));
                game.highscore += 250;
                goto LOOP_END;
            }
        }
    LOOP_END:
    }

    // check if player is hit
    for (int i = 0; i < asteroids.size(); i++)
    {
        Vector2 asteroids_hitbox = {asteroids[i].pos.x + asteroids[i].texture->width / 2, asteroids[i].pos.y + asteroids[i].texture->height / 2};
        if (CheckCollisionCircles(asteroids_hitbox, asteroids[i].texture->width / 2, player.pos, player.texture->height / 2))
        {
            damage += 500;
            asteroids.erase(std::next(asteroids.begin(), i));
        }
    }
    for (int i = 0; i < enemies.size(); i++)
    {
        Vector2 enemies_hitbox = {enemies[i].pos.x + enemies[i].texture->width / 2, enemies[i].pos.y + enemies[i].texture->height / 2};
        if (CheckCollisionCircles(enemies_hitbox, enemies[i].texture->width / 2, player.pos, player.texture->height / 2))
        {
            damage += 100;
            enemies.erase(std::next(enemies.begin(), i));
        }
    }

    for (int i = 0; i < enemy_projectiles.size(); i++)
    {
        Vector2 enemies_hitbox = {enemy_projectiles[i].pos.x + enemy_projectiles[i].texture->width / 2, enemy_projectiles[i].pos.y + enemy_projectiles[i].texture->height / 2};
        if (CheckCollisionCircles(enemies_hitbox, enemy_projectiles[i].texture->width / 2, player.pos, player.texture->height / 2))
        {
            damage += enemy_projectiles[i].damage;
            game.collisions.push_back(enemies_hitbox);
            enemy_projectiles.erase(std::next(enemy_projectiles.begin(), i));
        }
    }

    for (int i = 0; i < powerups.size(); i++)
    {
        Vector2 powerup_hitbox = {powerups[i].position.x + powerups[i].framerec.width / 2, powerups[i].position.y + powerups[i].framerec.height / 2};
        if (CheckCollisionCircles(powerup_hitbox, powerups[i].framerec.width / 2, player.pos, player.texture->height / 2))
        {
            if (powerups[i].texture == &game.textures.powup_life_tex)
            {
                damage -= 500;
                game.var.ship.max_hp += 200;
            }
            else if (powerups[i].texture == &game.textures.powup_shield_tex)
            {
                game.var.ship.max_shield += 1000;
            }
            else
            {
                game.var.ship.powerup_cd = 6;
                game.var.ship.weapon++;
            }

            powerups.erase(std::next(powerups.begin(), i));
        }
    }

    playerdamage(player, damage);
}

void playerinput_handler(ship_t &ship)
{
    const static float origin_speed = ship.speed;

    if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A))
    {
        ship.pos.x -= game.delta * ship.speed;
        if (ship.pos.x < ship.texture->width / 2)
            ship.pos.x = ship.texture->width / 2;
    }
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D))
    {
        ship.pos.x += game.delta * ship.speed;
        if (ship.pos.x > GetScreenWidth() - ship.texture->width / 2)
            ship.pos.x = GetScreenWidth() - ship.texture->width / 2;
    }
    if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W))
    {
        ship.pos.y -= game.delta * ship.speed;
        if (ship.pos.y < ship.texture->height / 2)
            ship.pos.y = ship.texture->height / 2;
    }
    if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S))
    {
        ship.pos.y += game.delta * ship.speed;
        if (ship.pos.y > GetScreenHeight() - ship.texture->height / 2)
            ship.pos.y = GetScreenHeight() - ship.texture->height / 2;
    }
    if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
    {
        if (ship.speed <= origin_speed + 200)
        {
            ship.speed += 50;
        }
    }
    if (IsKeyReleased(KEY_LEFT_SHIFT) || IsKeyReleased(KEY_RIGHT_SHIFT))
    {
        ship.speed = origin_speed;
    }
    if (IsKeyDown(KEY_SPACE))
    {
        if (game.gametime - game.var.ship.last_shot >= game.var.ship.shooting_cooldown)
        {
            game.var.ship.last_shot = game.gametime;
            for (int i = 0; i < game.var.ship.weaponarsenal.size(); i++)
                game.var.ship.weaponarsenal[i].pos = {game.var.ship.pos.x - game.var.ship.weaponarsenal[i].texture->width / 2, game.var.ship.pos.y - game.var.ship.texture->height / 2};

            game.projectiles.push_back(game.var.ship.weaponarsenal[0]);
            if (game.var.ship.weapon)
            {
                game.projectiles.push_back(game.var.ship.weaponarsenal[1]);
                game.projectiles.push_back(game.var.ship.weaponarsenal[2]);
            }
            PlaySound(game.sound.gunloop_sound);
        }
    }
    if (IsKeyPressed(KEY_I))
    {
        game.var.ship.weapon = ++game.var.ship.weapon % 2;
    }
}

void animation_play(animation_t &animation)
{
    if (animation.currentframe > animation.frames)
        return;

    if (animation.framescounter += game.delta, animation.framescounter >= (animation.framespeed))
    {
        animation.framescounter = 0;
        animation.framerec.x = (float)(animation.currentframe % animation.rows) * (float)animation.texture->width / animation.rows;
        animation.framerec.y = (float)(animation.currentframe / animation.rows) * (float)animation.texture->height / animation.cols;
        animation.currentframe++;
    }
    if (animation.currentframe > animation.frames)
        if (animation.style == LOOP)
            animation.currentframe = 0;
}

void animations_update(std::vector<animation_t> &animations)
{
    for (int i = 0; i < animations.size(); i++)
    {
        animation_play(animations[i]);
        if (animations[i].currentframe > animations[i].frames)
        {
            // if (animations[i].style == LOOP)
            //     animations[i].currentframe = 0;
            // else
            animations.erase(std::next(animations.begin(), i));
        }
    }
}

bool update_pos(Vector2 &pos, const Vector2 &dst, float speed)
{
    Vector2 diff = Vector2Subtract(dst, pos);
    float diff_length = Vector2Length(diff);
    if (diff_length < 0.05f)
    {
        pos = dst;
        return true;
    }

    Vector2 direction = Vector2Normalize(diff);
    Vector2 walking_distance = Vector2Scale(direction, speed);
    Vector2 walking_distance_delta = Vector2Scale(walking_distance, game.delta);

    float wdd_length = Vector2Length(walking_distance_delta);
    if (wdd_length >= diff_length)
    {
        pos = dst;
        return true;
    }
    else
    {
        pos = Vector2Add(pos, walking_distance_delta);
        return false;
    }
}

void enemy_update(std::vector<enemy_t> &enemies)
{
    for (int i = 0; i < enemies.size(); i++)
    {
        auto &enemy = enemies[i];
        Vector2 approaching = (*enemy.path)[enemy.pathpos];
        auto diff = Vector2Subtract(approaching, enemy.pos);

        float diff_length = Vector2Length(diff);
        if (diff_length < 0.05f)
        {
            enemy.pos = approaching;
            enemy.pathpos++;

            if (enemy.pathpos >= (*enemy.path).size())
            {
                enemy.path->push_back({(float)(rand() % GetScreenWidth()), (float)(rand() % GetScreenHeight())});
                // enemy.pathpos = rand() % ((*enemy.path).size() - 1);
            }
            continue;
        }

        auto direction = Vector2Normalize(diff);
        auto walking_distance = Vector2Scale(direction, enemy.speed);
        auto walking_distance_delta = Vector2Scale(walking_distance, game.delta);

        float wdd_length = Vector2Length(walking_distance_delta);
        if (wdd_length >= diff_length)
        {
            enemy.pos = approaching;
            enemy.pathpos++;
            if (enemy.pathpos >= (*enemy.path).size())
            {
                enemy.pathpos = (*enemy.path).size() - 1;
                return;
            }
        }
        else
        {
            enemy.pos = Vector2Add(enemy.pos, walking_distance_delta);
        }
    }
}

void multi_send()
{

    uint8_t state = 0;
    if (game.pause)
        state |= 1 << 7;
    if (game.exit)
        state |= 1 << 6;
    if (game.var.ship.player)
        state |= 1 << 0;

    uint8_t inputs = 0;
    if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A))
        inputs |= 1 << 7;
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D))
        inputs |= 1 << 6;
    if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W))
        inputs |= 1 << 5;
    if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S))
        inputs |= 1 << 4;
    if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
        inputs |= 1 << 3;
    if (IsKeyDown(KEY_SPACE))
        inputs |= 1 << 2;
    if (IsKeyDown(KEY_P))
        inputs |= 1 << 1;
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        inputs |= 1 << 0;

    struct ship_multi_t
    {
        int hp;
        int shield;
        Vector2 pos;
    } player;

    struct container_t
    {
        enum _CONTAINER type;
        int size;
        std::vector<Vector2> positions;
    };

    struct send_t
    {
        uint8_t inputs;
        uint8_t state;
        ship_multi_t player;
        container_t asteroids;
        container_t projectiles;
        container_t enemy_projectiles;
        container_t explosions;
    } datablock;

    datablock.player.hp = game.var.ship.hp;
    datablock.player.shield = game.var.ship.shield;
    datablock.player.pos = game.var.ship.pos;

    datablock.asteroids = {ASTEROIDS};
    datablock.projectiles = {PROJECTILES};
    datablock.enemy_projectiles = {ENEMYPROJECTILES};
    datablock.explosions = {EXPLOSIONS};

    datablock.asteroids.size = game.asteroids.size();
    datablock.projectiles.size = game.projectiles.size();
    datablock.enemy_projectiles.size = game.enemy_projectiles.size();
    datablock.explosions.size = game.explosions.size();

    for (auto &e : game.asteroids)
        datablock.asteroids.positions.push_back(e.pos);
    for (auto &e : game.projectiles)
        datablock.projectiles.positions.push_back(e.pos);
    for (auto &e : game.enemy_projectiles)
        datablock.enemy_projectiles.positions.push_back(e.pos);
    for (auto &e : game.explosions)
        datablock.explosions.positions.push_back(e.position);

    // send datablock , sizeof(datablock)
}

void DrawHealthbar(const Vector2 &pos, float value)
{
    Vector2 position = {pos.x - game.textures.ui_bar_b.width / 2, pos.y};
    Rectangle fill = {0, 0, (float)game.textures.ui_bar_red.width * value, (float)game.textures.ui_bar_red.height};

    DrawTextureV(game.textures.ui_bar_b, position, WHITE);
    DrawTextureRec(game.textures.ui_bar_red, fill, position, RED);
    DrawTextureV(game.textures.ui_bar_f, position, WHITE);
}

void DrawShieldbar(const Vector2 &pos, float value)
{
    Vector2 position = {pos.x - game.textures.ui_bar_b.width / 2, pos.y};
    Rectangle fill = {0, 0, (float)game.textures.ui_bar_blue.width * value, (float)game.textures.ui_bar_blue.height};

    DrawTextureV(game.textures.ui_bar_b, position, WHITE);
    DrawTextureRec(game.textures.ui_bar_blue, fill, position, BLUE);
    DrawTextureV(game.textures.ui_bar_f, position, WHITE);
}

void DrawAnimation(const animation_t &animation)
{
    DrawTextureRec(*animation.texture, animation.framerec, animation.position, WHITE);
}

void startscreen()
{
    float height = GetScreenHeight();
    float width = GetScreenWidth();
    // node_t<Rectangle> box0 = {{width * 0.1f, height * 0.1f, width * 0.8f, height * 0.8f}};
    std::vector<Rectangle> box = {{width * 0.1f, height * 0.2f, width * 0.8f, height * 0.6f}};
    box.reserve(15);

    // int textsize = (width + height) / 28;
    Vector2 title_pos = {width / 3.5f, (float)game.textsize};
    Rectangle exit_pos = {width / 3, box[0].height + game.textsize + box[0].y, (float)5.5 * game.textsize, (float)game.textsize};
    int bordersize = game.textsize / 10;

    for (size_t i = 1; i < 15; i++)
    {
        box.push_back({box[i - 1].x + 2 * bordersize, box[i - 1].y + 2 * bordersize, box[i - 1].width - 4 * bordersize, box[i - 1].height - 4 * bordersize});
    }

    Vector2 center = {width / 2, height / 2};
    bool back;
    bool change = false;

    Color hover;
    float hue = 295;
    float opa = 0.3;
    float hovertime = 0;

    int exit_opa = 255;

    std::vector<Vector2> pos = {center, {center.x / 2, center.y}, {center.x * 1.5f, center.y}, {center.x, center.y * 1.5f}, {center.x, center.y * 1.5f}};
    int pos_pos = 0;
    game.var.ship.pos = {width / 2, height + 50};

    SeekMusicStream(game.sound.opening, 0);
    PlayMusicStream(game.sound.opening);
    while (!WindowShouldClose())
    {

        UpdateMusicStream(game.sound.opening);
        game.delta = GetFrameTime();
        float maxtime = box.size() * 0.05f;

        hover = ColorFromHSV(hue, 0.8f, opa);

        auto mouse = GetMousePosition();
        if (CheckCollisionPointRec(mouse, box[0]))
        {
            if (opa < 1)
                opa += 0.5f * game.delta;

            hue += game.delta * 10;
            if (hue > 355)
                hue = 0;
            if (hovertime < maxtime)
                hovertime += game.delta;

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                PlaySound(game.sound.select);
                return;
            }

            if (hovertime < maxtime && fmod(hovertime, 0.05f) <= game.delta)
                PlaySound(game.sound.click);
        }
        else
        {
            if (opa > 0.3f)
                opa -= 0.5f * game.delta;
            if (hovertime > 0)
                hovertime -= game.delta;
            else if (hovertime < 0)
                hovertime = 0;

            exit_opa = 80;
        }

        if (CheckCollisionPointRec(mouse, exit_pos))
        {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                PlaySound(game.sound.select);
                exit(0);
            }
            exit_opa = 120;
        }

        if (game.bg_scrollpos -= game.delta * game.bg_scollspeed, game.bg_scrollpos <= -game.textures.bg_tex.height * 2)
            game.bg_scrollpos = 0;

        if (update_pos(game.var.ship.pos, pos[pos_pos], 20))
            pos_pos = ++pos_pos % pos.size();

        Color title = ColorFromHSV(hue, 1, 1);

        animation_play(game.animations.boost);
        game.animations.boost.position = {game.var.ship.pos.x - game.animations.boost.framerec.width / 2, game.var.ship.pos.y + game.var.ship.texture->height / 2};

        BeginDrawing();
        ClearBackground(BLACK);
        DrawTextureEx(game.textures.bg_tex, {-20, -game.bg_scrollpos}, 0, 2, WHITE);
        DrawTextureEx(game.textures.bg_tex, {20, -game.textures.bg_tex.height * 2 - game.bg_scrollpos}, 0, 2, RAYWHITE);

        DrawTexture(*game.var.ship.texture, game.var.ship.pos.x - game.var.ship.texture->width / 2, game.var.ship.pos.y - game.var.ship.texture->height / 2, WHITE);
        DrawTextureRec(*game.animations.boost.texture, game.animations.boost.framerec, game.animations.boost.position, WHITE);

        for (size_t i = 0; i < box.size(); i++)
        {
            if (hovertime > i * 0.05f)
            {
                DrawRectangleLinesEx(box[i], bordersize, hover);
            }
            else
                break;
        }

        if (hovertime <= 0)
            DrawRectangleLinesEx(box[0], bordersize, hover);
        else
            // DrawText("S  T  A  R  T", box.back().x + 2 * bordersize, box.back().height / 2 + box.back().y - 25, textsize, hover);
            DrawTextEx(font, " S T A R T", {box.back().x + (2 * bordersize), box.back().height / 2 + box.back().y - 20}, game.textsize, 1, hover);

        // DrawText("F-GRADIUS", title_pos.x, title_pos.y, textsize, title);
        DrawTextEx(font, "F GRADIUS", {title_pos.x, title_pos.y}, game.textsize, 1, title);
        // DrawText("    EXIT   ", exit_pos.x, exit_pos.y, textsize, title);
        DrawTextEx(font, "   EXIT   ", {exit_pos.x, exit_pos.y + 5}, game.textsize, 1, title);
        title.a = exit_opa;
        DrawRectangleRec(exit_pos, title);

        EndDrawing();
    }
}

void fly_to_start()
{
    bool reached = false;

    float speed = game.var.ship.speed * 1.1f;
    Vector2 destination = {game.screencenter.x, game.screencenter.y};
    Vector2 pos_overscreen = {game.screencenter.x, game.screencenter.y * 1.2f};

    while (!reached && !WindowShouldClose())
    {
        game.delta = GetFrameTime();
        if (game.bg_scrollpos -= game.delta * game.bg_scollspeed, game.bg_scrollpos <= -game.textures.bg_tex.height * 2)
            game.bg_scrollpos = 0;

        if (update_pos(game.var.ship.pos, destination, speed))
        {
            if (destination == game.screencenter)
                destination = pos_overscreen;
            else if (destination == pos_overscreen)
                destination = game.ship_startpos;
            else
                reached = true;
        }

        if (speed > 600)
        {
            speed -= (speed * game.delta);
        }
        else if (speed > 400)
        {
            speed -= (speed * game.delta / 2);
        }
        else if (speed > 300)
        {
            speed -= 100 * game.delta;
        }
        else
            speed = 300;

        if (destination != game.ship_startpos)
        {
            animation_play(game.animations.boost);
            game.animations.boost.position = {game.var.ship.pos.x - game.animations.boost.framerec.width / 2, game.var.ship.pos.y + game.var.ship.texture->height / 2};
        }

        BeginDrawing();
        ClearBackground(BLACK);
        DrawTextureEx(game.textures.bg_tex, {-20, -game.bg_scrollpos}, 0, 2, WHITE);
        DrawTextureEx(game.textures.bg_tex, {20, -game.textures.bg_tex.height * 2 - game.bg_scrollpos}, 0, 2, RAYWHITE);
        DrawTexture(*game.var.ship.texture, game.var.ship.pos.x - game.var.ship.texture->width / 2, game.var.ship.pos.y - game.var.ship.texture->height / 2, WHITE);
        if (destination != game.ship_startpos)
            DrawTextureRec(*game.animations.boost.texture, game.animations.boost.framerec, game.animations.boost.position, WHITE);
        EndDrawing();
    }
}

void mainloop()
{

    game.explosions.clear();
    game.asteroids.clear();
    game.projectiles.clear();
    game.enemy_projectiles.clear();
    game.enemies.clear();
    int enemy_spawner = 10;
    double asteroid_spawntimer = 0;
    double enemy_spawntimer = 2;
    int asteroid_spawns = 1;
    double enemy_spawnspeed = 1;
    game.var.enemy.speed = 250;

    game.var.ship.pos = game.ship_startpos;
    game.var.ship.hp = game.var.ship.max_hp;
    game.var.ship.shield = game.var.ship.max_shield;
    game.gametime = 0;
    game.var.ship.last_shot = 0;
    game.highscore = 0;

    SeekMusicStream(game.sound.bg_music, 0);
    PlayMusicStream(game.sound.bg_music);

    //game.powerups.push_back(game.animations.powup_life);
    //game.powerups.push_back(game.animations.powup_shield);
    //game.powerups.push_back(game.animations.powup_weapon);
    float event_timer = 0;

    while (!WindowShouldClose())
    {
        if (game.var.ship.hp <= 0)
            return;

        if (IsKeyPressed(KEY_P))
            game.pause = game.pause ? false : true;

        if (!game.pause)
        {
            UpdateMusicStream(game.sound.bg_music);
            // fire events!
            for (auto e : game.collisions)
            {
                game.animations.explosion2.position = {e.x - game.animations.explosion2.framerec.width / 2, e.y - game.animations.explosion2.framerec.height / 2};
                game.explosions.push_back(game.animations.explosion2);
                PlaySound(game.sound.explosion_sound);
            }
            game.collisions.clear();

            // update times
            game.delta = GetFrameTime();
            game.gametime += game.delta;
            // update_game
            enemy_update(game.enemies);
            asteroids_update(game.asteroids);
            projectiles_update(game.projectiles);
            projectiles_update(game.enemy_projectiles);
            collision_handler(game.projectiles, game.asteroids, game.enemies, game.var.ship, game.enemy_projectiles, game.powerups);
            playerinput_handler(game.var.ship);
            shieldrecover(game.var.ship);
            animations_update(game.explosions);
            animations_update(game.powerups);

            for (int i = 0; i < game.powerups.size(); i++)
                update_pos(game.powerups[i].position, {game.powerups[i].position.x, (float)game.window_height + 10}, 100);

            // RANDOM SPAWN TIME!!!!
            if (game.gametime - event_timer > 1)
            {
                event_timer = game.gametime;
                if(game.var.ship.powerup_cd > 0)
                    game.var.ship.powerup_cd -= 1;

                if (game.var.ship.powerup_cd == 0)
                    game.var.ship.weapon = 0;

                int enemyshoot = rand() % (40);
                if (game.enemies.size() > 0 && game.enemies.size() > enemyshoot)
                {
                    game.var.enemy_attack.pos = game.enemies[enemyshoot].pos;
                    Vector2 diff = Vector2Subtract(game.var.ship.pos, game.enemies[enemyshoot].pos);
                    game.var.enemy_attack.direction = Vector2Normalize(diff);

                    game.enemy_projectiles.push_back(game.var.enemy_attack);
                    PlaySound(game.sound.gunloop_sound);
                }

                if(enemyshoot == 1 || enemyshoot == 40)
                {
                    game.animations.powup_life.position = {(float)(rand()%game.window_width), 0};
                    game.powerups.push_back(game.animations.powup_life);
                }
                else if (enemyshoot == 2|| enemyshoot == 20)
                {
                    game.animations.powup_shield.position = {(float)(rand()%game.window_width), 0};
                    game.powerups.push_back(game.animations.powup_shield);
                }
                else if (enemyshoot == 3|| enemyshoot == 30)
                {
                    game.animations.powup_weapon.position = {(float)(rand()%game.window_width), 0};
                    game.powerups.push_back(game.animations.powup_weapon);
                }
            }

            // spawntime!
            if (game.gametime - asteroid_spawntimer > 0.3)
            {
                asteroid_spawntimer = game.gametime;
                asteroids_spawn(game.asteroids, &game.textures.asteroid_textures[rand() % game.textures.asteroid_textures.size()], asteroid_spawns);
            }
            if (game.gametime - enemy_spawntimer > enemy_spawnspeed)
            {
                enemy_spawntimer = game.gametime;
                if (enemy_spawner-- > 0)
                    game.enemies.push_back(game.var.enemy);

                else if (game.enemies.size() == 0)
                {
                    game.var.enemy.speed += 40;
                    Vector2 spawnpos = game.var.enemy_path[0];
                    spawnpos.x = rand() % screenWidth;
                    game.var.enemy_path.clear();
                    game.var.enemy_path.push_back(spawnpos);

                    enemy_spawner = rand() % 10;
                    enemy_spawnspeed -= enemy_spawnspeed * 0.08;
                    asteroid_spawns += rand() % 2;
                }
            }

            if (game.bg_scrollpos -= game.delta * game.bg_scollspeed, game.bg_scrollpos <= -game.textures.bg_tex.height * 2)
                game.bg_scrollpos = 0;
        }

        BeginDrawing();
        ClearBackground(BLACK);

        // Draw background
        DrawTextureEx(game.textures.bg_tex, {-20, -game.bg_scrollpos}, 0, 2, WHITE);
        DrawTextureEx(game.textures.bg_tex, {20, -game.textures.bg_tex.height * 2 - game.bg_scrollpos}, 0, 2, RAYWHITE);

        // Draw asteroids
        for (int i = 0; i < game.asteroids.size(); i++)
            DrawTexture(*game.asteroids[i].texture, game.asteroids[i].pos.x, game.asteroids[i].pos.y, WHITE);

        // Draw enemies
        for (int i = 0; i < game.enemies.size(); i++)
            DrawTexture(*game.enemies[i].texture, game.enemies[i].pos.x, game.enemies[i].pos.y, WHITE);

        // Draw projectiles
        for (int i = 0; i < game.projectiles.size(); i++)
            DrawTexture(*game.projectiles[i].texture, game.projectiles[i].pos.x, game.projectiles[i].pos.y, WHITE);
        for (int i = 0; i < game.enemy_projectiles.size(); i++)
            DrawTexture(*game.enemy_projectiles[i].texture, game.enemy_projectiles[i].pos.x, game.enemy_projectiles[i].pos.y, WHITE);

        // Draw the spaceship
        DrawTexture(*game.var.ship.texture, game.var.ship.pos.x - game.var.ship.texture->width / 2, game.var.ship.pos.y - game.var.ship.texture->height / 2, WHITE);
        if (game.var.ship.shield > 0)
            DrawTexture(game.textures.shield_tex, game.var.ship.pos.x - game.textures.shield_tex.width / 2, game.var.ship.pos.y - game.textures.shield_tex.height / 2, WHITE);

        for (int i = 0; i < game.powerups.size(); i++)
            DrawAnimation(game.powerups[i]);

        // Draw animations
        for (int i = 0; i < game.explosions.size(); i++)
            DrawAnimation(game.explosions[i]);
        // DrawTextureRec(*game.explosions[i].texture, game.explosions[i].framerec, game.explosions[i].position, WHITE);

        // auto s_hp = "HP: " + std::to_string(game.var.ship.hp);
        // auto s_shield = "Shield: " + std::to_string(game.var.ship.shield);
        // DrawText(h_size.c_str(), screenWidth / 3, 10, 20, RED);
        DrawTextEx(font, TextFormat("Score: %d", game.highscore), {screenWidth / 3, 10}, 20, 1, RED);
        // DrawText(s_hp.c_str(), 10, 10, 20, RED);
        // DrawText(s_shield.c_str(), 10, 40, 20, RED);
        DrawShieldbar({(float)game.textures.ui_bar_b.width / 2, 0}, (float)game.var.ship.shield / game.var.ship.max_shield);
        DrawHealthbar({(float)game.textures.ui_bar_b.width / 2, (float)game.textures.ui_bar_red.height * 0.7f}, (float)game.var.ship.hp / game.var.ship.max_hp);
        if (game.pause)
        {
            // DrawText("P A U S E", screenWidth / 2 - screenWidth * 0.1, screenHeight / 2, 40, WHITE);
            DrawTextEx(font, "P A U S E", {screenWidth / 2 - screenWidth * 0.1, screenHeight / 2}, 40, 1, WHITE);
            // DrawTextEx(font , "    EXIT   ", {exit_pos.x, exit_pos.y}, textsize, 5.5, title);
        }

        EndDrawing();
    }

    StopMusicStream(game.sound.bg_music);
}

void gameover()
{
    std::vector<animation_t> animations;
    animations.reserve(12);
    float height = game.window_height;
    float width = game.window_width;
    std::vector<Vector2> pos = {{width / 2, height / 2}};
    int pos_pos = 0;
    game.gametime = 0;
    float timestamp = 0;
    bool reached = false;
    Color text_color;
    float hue = 295;
    float opa = 0.0;

    while (!WindowShouldClose())
    {
        if (game.bg_scrollpos -= game.delta * game.bg_scollspeed, game.bg_scrollpos <= -game.textures.bg_tex.height * 2)
            game.bg_scrollpos = 0;
        game.delta = GetFrameTime();
        game.gametime += game.delta;
        if (update_pos(game.var.ship.pos, pos[pos_pos], 100))
            reached = true;

        if (!reached && game.gametime - timestamp > 0.2f)
        {
            timestamp = game.gametime;
            game.animations.explosion2.position = {(game.var.ship.pos.x - game.animations.explosion2.framerec.width / 2) + (rand() % game.var.ship.texture->width - game.var.ship.texture->width / 2), (game.var.ship.pos.y - game.animations.explosion2.framerec.height / 2) + (rand() % game.var.ship.texture->height - game.var.ship.texture->height / 2)};
            animations.push_back(game.animations.explosion2);
            PlaySound(game.sound.explosion_sound);
        }

        if (reached && game.var.ship.pos != Vector2{width / 2, height + 100})
        {
            game.animations.big_boom.position = {(game.var.ship.pos.x - game.animations.big_boom.framerec.width / 2), (game.var.ship.pos.y - game.animations.big_boom.framerec.width / 2)};
            game.var.ship.pos = {width / 2, height + 100};
            pos.clear();
            pos.push_back({width / 2, height + 100});
            animations.push_back(game.animations.big_boom);
            PlaySound(game.sound.explosion_sound);
        }

        if (reached && opa < 0.9)
            opa += 0.5f * game.delta;

        if (reached && opa >= 0.9 && game.gametime - timestamp > 0.2f)
        {
            timestamp = game.gametime;
            hue++;
        }

        if (reached && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            return;

        animations_update(animations);
        text_color = ColorFromHSV(hue, 0.8f, opa);

        BeginDrawing();
        ClearBackground(BLACK);
        DrawTextureEx(game.textures.bg_tex, {-20, -game.bg_scrollpos}, 0, 2, WHITE);
        DrawTextureEx(game.textures.bg_tex, {20, -game.textures.bg_tex.height * 2 - game.bg_scrollpos}, 0, 2, RAYWHITE);

        DrawTexture(*game.var.ship.texture, game.var.ship.pos.x - game.var.ship.texture->width / 2, game.var.ship.pos.y - game.var.ship.texture->height / 2, WHITE);
        // Draw animations
        for (int i = 0; i < animations.size(); i++)
        {
            DrawTextureRec(*animations[i].texture, animations[i].framerec, animations[i].position, WHITE);
        }

        if (reached)
        {
            // DrawText(TextFormat("%d",game.highscore), width / 5, height / 2 + textsize, textsize, text_color);
            // DrawText("G A M E   O V E R", width / 5, height / 2 - textsize / 2, textsize, text_color);
            DrawTextEx(font, "GAME OVER", {(float)game.window_width / 4, height / 2 - game.textsize / 2}, game.textsize, 1, text_color);
            DrawTextEx(font, "YOUR SCORE:", {(float)game.window_width / 4, height / 2 + game.textsize}, game.textsize, 1, text_color);
            DrawTextEx(font, TextFormat("%d", game.highscore), {(float)game.window_width / 4, height / 2 + game.textsize * 2}, game.textsize, 1, text_color);
        }
        EndDrawing();
    }
    animations.clear();
}

void init_assets()
{
    game.textures.bg_tex = LoadTexture("assets/background/spr_stars02.png");

    Image ship_image = LoadImage("assets/ships/spiked ship 3.PNG");
    ImageResize(&ship_image, screenWidth / 10, screenHeight / 10);
    game.textures.ship_tex = LoadTextureFromImage(ship_image);

    Image boost_image = LoadImage("assets/ships/boost_high.png");
    ImageResize(&boost_image, screenWidth / 3, screenHeight / 2);
    game.textures.boost_text = LoadTextureFromImage(boost_image);

    Image ufo_image = LoadImage("assets/ships/ufo.png");
    ImageResize(&ufo_image, screenWidth / 20, screenWidth / 20);
    game.textures.ufo_tex = LoadTextureFromImage(ufo_image);

    Image torpedo_image = LoadImage("assets/projectiles/torpedo.png");
    ImageResize(&torpedo_image, screenWidth / 20, screenWidth / 20);
    game.textures.torpedo_tex = LoadTextureFromImage(torpedo_image);

    Image orb_red_image = LoadImage("assets/projectiles/orb_red.png");
    ImageResize(&orb_red_image, screenWidth / 35, screenWidth / 35);
    game.textures.orb_red = LoadTextureFromImage(orb_red_image);

    Image explosion_atlas = LoadImage("assets/projectiles/explosion2.png");
    Image explosion_1 = ImageFromImage(explosion_atlas, {373, 8, 236, 35});
    game.textures.explosion_tex = LoadTextureFromImage(explosion_1);
    Image shield_img = LoadImage("assets/ships/shield.png");
    ImageResize(&shield_img, game.textures.ship_tex.width * 1.1, game.textures.ship_tex.width * 1.1);
    game.textures.shield_tex = LoadTextureFromImage(shield_img);

    load_textures_from_dir(game.textures.asteroid_textures, "./assets/asteroids");

    game.textures.explosion2_tex = LoadTexture("assets/projectiles/exp2.png");

    Image big_boom_imgage = LoadImage("assets/projectiles/exp2.png");
    ImageResize(&big_boom_imgage, big_boom_imgage.width * 3, big_boom_imgage.height * 3);
    game.textures.big_boom_tex = LoadTextureFromImage(big_boom_imgage);

    int bar_w = screenWidth / 5;
    int bar_h = screenHeight / 15;
    Image bar_b = LoadImage("assets/misc/BarBackground.png");
    ImageResize(&bar_b, bar_w, bar_h);
    game.textures.ui_bar_b = LoadTextureFromImage(bar_b);

    Image bar_f = LoadImage("assets/misc/BarGlass.png");
    ImageResize(&bar_f, bar_w, bar_h);
    game.textures.ui_bar_f = LoadTextureFromImage(bar_f);

    Image bar_red = LoadImage("assets/misc/RedBar.png");
    ImageResize(&bar_red, bar_w, bar_h);
    game.textures.ui_bar_red = LoadTextureFromImage(bar_red);

    Image bar_blue = LoadImage("assets/misc/BlueBar.png");
    ImageResize(&bar_blue, bar_w, bar_h);
    game.textures.ui_bar_blue = LoadTextureFromImage(bar_blue);

    Image powup_life = LoadImage("assets/powerup/life.png");
    Image powup_shield = LoadImage("assets/powerup/shield.png");
    Image powup_weapon = LoadImage("assets/powerup/multi.png");

    int pow_w = screenWidth / 8;
    int pow_h = screenHeight / 10;
    ImageResize(&powup_life, pow_w, pow_h);
    ImageResize(&powup_shield, pow_w, pow_h);
    ImageResize(&powup_weapon, pow_w, pow_h);
    game.textures.powup_life_tex = LoadTextureFromImage(powup_life);
    game.textures.powup_shield_tex = LoadTextureFromImage(powup_shield);
    game.textures.powup_weapon_tex = LoadTextureFromImage(powup_weapon);

    font = LoadFont("assets/misc/sterion.ttf");
    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);

    // end load assets

    UnloadImage(big_boom_imgage);
    UnloadImage(ship_image);
    UnloadImage(boost_image);
    UnloadImage(ufo_image);
    UnloadImage(torpedo_image);
    UnloadImage(explosion_atlas);
    UnloadImage(explosion_1);
    UnloadImage(shield_img);
    UnloadImage(orb_red_image);
    UnloadImage(bar_b);
    UnloadImage(bar_f);
    UnloadImage(bar_red);
    UnloadImage(bar_blue);
    UnloadImage(powup_life);
    UnloadImage(powup_shield);
    UnloadImage(powup_weapon);
}

void init_sound()
{
    game.sound.bg_music = LoadMusicStream("sound/music.mp3");

    game.sound.explosion_sound = LoadSound("sound/explosion.wav");
    game.sound.gunloop_sound = LoadSound("sound/gunloop.wav");
    SetSoundVolume(game.sound.gunloop_sound, 0.1);
    SetSoundVolume(game.sound.explosion_sound, 0.6);

    game.sound.opening = LoadMusicStream("sound/opening.mp3");
    game.sound.click = LoadSound("sound/menu_click.wav");
    game.sound.select = LoadSound("sound/menu_select.wav");
}

void init_types()
{

    game.asteroids.reserve(100);
    game.projectiles.reserve(100);
    game.enemy_projectiles.reserve(100);
    game.enemies.reserve(100);
    game.explosions.reserve(100);

    game.screencenter = {(float)screenWidth / 2, (float)screenHeight / 2};
    Vector2 spawnposition = {(float)screenWidth / 2, -50};
    game.ship_startpos = {(float)screenWidth / 2, (float)screenHeight * 0.95};
    game.window_width = GetScreenWidth();
    game.window_height = GetScreenHeight();
    game.textsize = (game.window_height + game.window_width) / 28;

    // weapon1
    game.var.weapon1.texture = &game.textures.torpedo_tex;
    game.var.weapon1.direction = {0, -1};
    game.var.weapon1.speed = 300;
    game.var.weapon1.damage = 50;

    // weapon2
    projectile_t weapon2;
    weapon2.texture = &game.textures.torpedo_tex;
    weapon2.direction = {0.3f, -1};
    weapon2.speed = 300;
    weapon2.damage = 50;

    projectile_t weapon3;
    weapon3.texture = &game.textures.torpedo_tex;
    weapon3.direction = {-0.3f, -1};
    weapon3.speed = 300;
    weapon3.damage = 50;

    // enemy attack
    game.var.enemy_attack.texture = &game.textures.orb_red;
    game.var.enemy_attack.damage = 100;
    game.var.enemy_attack.speed = 190;

    // ship
    game.var.ship.texture = &game.textures.ship_tex;
    game.var.ship.pos = game.ship_startpos;
    game.var.ship.speed = 300;
    game.var.ship.shooting_cooldown = 0.1f;
    game.var.ship.last_shot = 0;
    game.var.ship.hp = 3000;
    game.var.ship.max_hp = 3000;
    game.var.ship.shield = 1500;
    game.var.ship.max_shield = 1500;
    game.var.ship.shieldrecovertime = 2;
    game.var.ship.weaponarsenal.push_back(game.var.weapon1);
    game.var.ship.weaponarsenal.push_back(weapon2);
    game.var.ship.weaponarsenal.push_back(weapon3);

    // enemy
    game.var.enemy_path = {spawnposition};
    game.var.enemy.texture = &game.textures.ufo_tex;
    game.var.enemy.hp = 100;
    game.var.enemy.pos = spawnposition;
    game.var.enemy.speed = 250;
    game.var.enemy.shooting_cooldown = 0.8f;
    game.var.enemy.last_shot = 0;
    game.var.enemy.path = &game.var.enemy_path;

    // animations
    game.animations.explosion.texture = &game.textures.explosion_tex;
    game.animations.explosion.frames = 7;
    game.animations.explosion.rows = 7;
    game.animations.explosion.cols = 1;
    game.animations.explosion.framerec = {0.0f, 0.0f, (float)game.textures.explosion_tex.width / game.animations.explosion.rows, (float)game.textures.explosion_tex.height / game.animations.explosion.cols};
    game.animations.explosion.framespeed = 0.02f;
    game.animations.explosion.style = ONCE;

    // the main-explosion
    game.animations.explosion2.texture = &game.textures.explosion2_tex;
    game.animations.explosion2.rows = 4;
    game.animations.explosion2.cols = 4;
    game.animations.explosion2.frames = game.animations.explosion2.rows * game.animations.explosion2.cols;
    game.animations.explosion2.framerec = {0.0f, 0.0f, (float)game.textures.explosion2_tex.width / game.animations.explosion2.rows, (float)game.textures.explosion2_tex.height / game.animations.explosion2.cols};
    game.animations.explosion2.framespeed = 0.02f;
    game.animations.explosion2.style = ONCE;

    // big boom
    game.animations.big_boom.texture = &game.textures.big_boom_tex;
    game.animations.big_boom.currentframe = 0;
    game.animations.big_boom.framespeed = 0.015f;
    game.animations.big_boom.rows = 4;
    game.animations.big_boom.cols = 4;
    game.animations.big_boom.framerec = {0.0f, 0.0f, (float)game.textures.big_boom_tex.width / game.animations.big_boom.rows, (float)game.textures.big_boom_tex.height / game.animations.big_boom.cols};
    game.animations.big_boom.frames = game.animations.big_boom.rows * game.animations.big_boom.cols;
    game.animations.big_boom.style = ONCE;

    game.animations.boost.texture = &game.textures.boost_text;
    game.animations.boost.currentframe = 0;
    game.animations.boost.framespeed = 0.008f;
    game.animations.boost.rows = 8;
    game.animations.boost.cols = 8;
    game.animations.boost.framerec = {0.0f, 0.0f, (float)game.textures.boost_text.width / game.animations.boost.rows, (float)game.textures.boost_text.height / game.animations.boost.cols};
    game.animations.boost.frames = game.animations.boost.rows * game.animations.boost.cols;
    game.animations.boost.style = LOOP;

    game.animations.powup_life.texture = &game.textures.powup_life_tex;
    game.animations.powup_life.currentframe = 0;
    game.animations.powup_life.framespeed = 0.1f;
    game.animations.powup_life.rows = 2;
    game.animations.powup_life.cols = 1;
    game.animations.powup_life.framerec = {0.0f, 0.0f, (float)game.textures.powup_life_tex.width / game.animations.powup_life.rows, (float)game.textures.powup_life_tex.height / game.animations.powup_life.cols};
    game.animations.powup_life.frames = game.animations.powup_life.rows * game.animations.powup_life.cols;
    game.animations.powup_life.style = LOOP;

    game.animations.powup_shield.texture = &game.textures.powup_shield_tex;
    game.animations.powup_shield.currentframe = 0;
    game.animations.powup_shield.framespeed = 0.1f;
    game.animations.powup_shield.rows = 2;
    game.animations.powup_shield.cols = 1;
    game.animations.powup_shield.framerec = {0.0f, 0.0f, (float)game.textures.powup_shield_tex.width / game.animations.powup_shield.rows, (float)game.textures.powup_shield_tex.height / game.animations.powup_shield.cols};
    game.animations.powup_shield.frames = game.animations.powup_life.rows * game.animations.powup_life.cols;
    game.animations.powup_shield.style = LOOP;

    game.animations.powup_weapon.texture = &game.textures.powup_weapon_tex;
    game.animations.powup_weapon.currentframe = 0;
    game.animations.powup_weapon.framespeed = 0.1f;
    game.animations.powup_weapon.rows = 2;
    game.animations.powup_weapon.cols = 1;
    game.animations.powup_weapon.framerec = {0.0f, 0.0f, (float)game.textures.powup_weapon_tex.width / game.animations.powup_weapon.rows, (float)game.textures.powup_life_tex.height / game.animations.powup_weapon.cols};
    game.animations.powup_weapon.frames = game.animations.powup_weapon.rows * game.animations.powup_weapon.cols;
    game.animations.powup_weapon.style = LOOP;
    // bg variables
    game.bg_scollspeed = 100;
    game.bg_scrollpos = 0.0f;
}

int main(void)
{
    InitWindow(screenWidth, screenHeight, "FGradius");
    //SetTargetFPS(60);

    InitAudioDevice();
    init_assets();
    init_sound();
    init_types();

    while (1)
    {
        startscreen();
        if (game.var.ship.player == 0)
        {
            fly_to_start();
            mainloop();
            gameover();
        }
        if (WindowShouldClose())
            return 0;
        BeginDrawing();
        EndDrawing();
    }

    CloseWindow();
    return 0;
}

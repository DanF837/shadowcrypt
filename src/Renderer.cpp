#include "Renderer.h"
#include "Item.h"
#include <cstdio>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
static void enableANSI() {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode;
    GetConsoleMode(h, &mode);
    SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}
static bool ansiEnabled = (enableANSI(), true);
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

void Renderer::clearScreen() {
    buffer.clear();
    buffer += "\033[2J\033[H";
}

void Renderer::moveCursor(int x, int y) {
    buffer += "\033[" + std::to_string(y + 1) + ";" + std::to_string(x + 1) + "H";
}

void Renderer::setColor(int fg, int bg) {
    buffer += "\033[";
    if (bg >= 0) {
        buffer += std::to_string(40 + bg) + ";";
    }
    buffer += std::to_string(30 + fg) + "m";
}

void Renderer::resetColor() {
    buffer += "\033[0m";
}

void Renderer::getTerminalSize(int& cols, int& rows) {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    } else {
        cols = 80; rows = 24;
    }
#else
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        cols = ws.ws_col;
        rows = ws.ws_row;
    } else {
        cols = 80; rows = 24;
    }
#endif
}

std::string Renderer::getMessageColor(const std::string& msg) const {
    if (msg.find("CRITICAL") != std::string::npos || msg.find("Critical") != std::string::npos)
        return "\033[1;31m";
    if (msg.find("dies!") != std::string::npos || msg.find("BOOM") != std::string::npos)
        return "\033[1;33m";
    if (msg.find("damage") != std::string::npos || msg.find("burns you") != std::string::npos ||
        msg.find("shoots you") != std::string::npos || msg.find("lava") != std::string::npos ||
        msg.find("enraged") != std::string::npos || msg.find("Burning") != std::string::npos)
        return "\033[31m";
    if (msg.find("hits ") != std::string::npos)
        return "\033[33m";
    if (msg.find("gold") != std::string::npos || msg.find("Gold") != std::string::npos ||
        msg.find("Sold") != std::string::npos || msg.find("Bought") != std::string::npos)
        return "\033[33m";
    if (msg.find("heal") != std::string::npos || msg.find("restore") != std::string::npos ||
        msg.find("drains life") != std::string::npos)
        return "\033[32m";
    if (msg.find("Level up") != std::string::npos || msg.find("XP") != std::string::npos)
        return "\033[36m";
    if (msg.find("poison") != std::string::npos || msg.find("Poison") != std::string::npos ||
        msg.find("blind") != std::string::npos || msg.find("stun") != std::string::npos ||
        msg.find("trap") != std::string::npos || msg.find("Slow") != std::string::npos ||
        msg.find("sluggish") != std::string::npos)
        return "\033[35m";
    if (msg.find("descend") != std::string::npos || msg.find("Descend") != std::string::npos ||
        msg.find("Floor") != std::string::npos)
        return "\033[36m";
    return "\033[37m";
}

void Renderer::printCentered(int y, int termW, const std::string& text) {
    int x = std::max(0, (termW - (int)text.size()) / 2);
    moveCursor(x, y);
    buffer += text;
}

char Renderer::tileChar(Tile t, Biome biome) const {
    switch (t) {
        case Tile::Wall:
            if (biome == Biome::Cave) return '%';
            return '#';
        case Tile::Floor:
            if (biome == Biome::Cave) return ',';
            return '.';
        case Tile::StairsDown: return '>';
        case Tile::StairsUp:   return '<';
        case Tile::Water:      return '~';
        case Tile::Lava:       return '~';
        case Tile::Fountain:   return '*';
    }
    return '?';
}

void Renderer::getTileColor(Tile t, bool visible, int& fg, Biome biome) const {
    if (!visible) {
        fg = 0;
        return;
    }
    switch (t) {
        case Tile::Wall:
            if (biome == Biome::Inferno) fg = 1; // red
            else fg = 7;
            break;
        case Tile::Floor:
            if (biome == Biome::Cave) fg = 3; // yellow
            else fg = 7;
            break;
        case Tile::StairsDown: fg = 3; break;
        case Tile::StairsUp:   fg = 3; break;
        case Tile::Water:      fg = 4; break;
        case Tile::Lava:       fg = 1; break;
        case Tile::Fountain:   fg = 6; break;
    }
}

void Renderer::render(const Map& map, const Player& player,
                      const std::vector<Enemy>& enemies,
                      const std::vector<Item>& items,
                      const std::vector<Trap>& traps,
                      const std::vector<std::string>& log,
                      int floor, Biome biome,
                      Vec2 merchantPos, int difficulty) {
    clearScreen();

    // Draw map
    for (int y = 0; y < Map::HEIGHT; y++) {
        moveCursor(0, y);
        for (int x = 0; x < Map::WIDTH; x++) {
            if (!map.isExplored(x, y)) {
                buffer += ' ';
                continue;
            }
            bool vis = map.isVisible(x, y);
            Tile t = map.getTile(x, y);
            int fg = 7;
            getTileColor(t, vis, fg, biome);

            if (!vis) {
                buffer += "\033[90m";
            } else {
                setColor(fg);
            }
            buffer += tileChar(t, biome);
            resetColor();
        }
    }

    // Draw items (only visible)
    for (auto& item : items) {
        if (!item.onGround) continue;
        if (!map.isVisible(item.pos.x, item.pos.y)) continue;
        moveCursor(item.pos.x, item.pos.y);
        switch (item.type) {
            case ItemType::HealthPotion: setColor(1); break;
            case ItemType::AttackBoost:  setColor(6); break;
            case ItemType::DefenseBoost: setColor(6); break;
            case ItemType::Weapon:       setColor(3); break;
            case ItemType::Armor:        setColor(2); break;
            case ItemType::Gold:           setColor(3); break;
            case ItemType::TeleportScroll: setColor(6); break;
            case ItemType::Bomb:           setColor(1); break;
            case ItemType::ShieldPotion:   setColor(4); break;
        }
        buffer += item.glyph;
        resetColor();
    }

    // Draw revealed traps (only visible)
    for (auto& trap : traps) {
        if (!trap.revealed) continue;
        if (!map.isVisible(trap.pos.x, trap.pos.y)) continue;
        moveCursor(trap.pos.x, trap.pos.y);
        setColor(5);
        buffer += '^';
        resetColor();
    }

    // Draw merchant
    if (merchantPos.x >= 0 && map.isVisible(merchantPos.x, merchantPos.y)) {
        moveCursor(merchantPos.x, merchantPos.y);
        buffer += "\033[1;33m"; // bold yellow
        buffer += 'M';
        resetColor();
    }

    // Draw enemies (only visible)
    for (auto& e : enemies) {
        if (!e.isAlive()) continue;
        if (!map.isVisible(e.pos.x, e.pos.y)) continue;
        moveCursor(e.pos.x, e.pos.y);
        switch (e.type) {
            case EnemyType::Rat:         setColor(2); break;
            case EnemyType::Skeleton:    setColor(7); break;
            case EnemyType::Ghost:       setColor(5); break;
            case EnemyType::Demon:       setColor(1); break;
            case EnemyType::Dragon:      setColor(1); break;
            case EnemyType::Archer:      setColor(3); break;
            case EnemyType::Necromancer: buffer += "\033[35m"; break; // magenta
            case EnemyType::Lich:        buffer += "\033[1;35m"; break; // bold magenta
        }
        buffer += e.glyph;
        resetColor();
    }

    // Draw player
    moveCursor(player.pos.x, player.pos.y);
    buffer += "\033[1;33m";
    buffer += '@';
    resetColor();

    // HUD area — separated from map with divider
    int hudY = Map::HEIGHT;

    // Divider line
    moveCursor(0, hudY);
    buffer += "\033[90m";
    for (int i = 0; i < Map::WIDTH; i++) buffer += '\xC4'; // ─
    resetColor();

    // HP bar + core stats on one line
    moveCursor(0, hudY + 1);
    buffer += "\033[1;37m";
    buffer += "HP: ";
    int barLen = 20;
    int filled = (player.hp * barLen) / std::max(1, player.maxHp);
    buffer += "\033[41m";
    for (int i = 0; i < filled; i++) buffer += ' ';
    buffer += "\033[40m";
    for (int i = filled; i < barLen; i++) buffer += ' ';
    resetColor();
    buffer += " \033[1;37m" + std::to_string(player.hp) + "/" + std::to_string(player.maxHp);
    buffer += "  ATK:" + std::to_string(player.totalAttack()) +
              " DEF:" + std::to_string(player.totalDefense()) +
              " LVL:" + std::to_string(player.level) +
              " XP:" + std::to_string(player.xp) + "/" + std::to_string(player.xpToNextLevel()) +
              " Floor:" + std::to_string(floor) +
              " \033[33mGold:" + std::to_string(player.gold);
    if (difficulty == 0) buffer += " \033[32m[EASY]";
    else if (difficulty == 2) buffer += " \033[31m[HARD]";
    resetColor();

    // Status effects + ability (own line, only meaningful content)
    moveCursor(0, hudY + 2);
    buffer += "\033[90m";
    buffer += "[R] " + player.abilityName();
    if (player.abilityBuffActive) {
        buffer += " \033[1;32m[ACTIVE]\033[90m";
    } else if (player.abilityCooldown > 0) {
        buffer += " (" + std::to_string(player.abilityCooldown) + " turns)";
    } else {
        buffer += " \033[33m(Ready)\033[90m";
    }
    if (player.poisonTurns > 0) {
        buffer += "  \033[32m[POISON:" + std::to_string(player.poisonTurns) + "]\033[90m";
    }
    if (player.burningTurns > 0) {
        buffer += "  \033[31m[BURN:" + std::to_string(player.burningTurns) + "]\033[90m";
    }
    if (player.blindTurns > 0) {
        buffer += "  \033[35m[BLIND:" + std::to_string(player.blindTurns) + "]\033[90m";
    }
    if (player.slowTurns > 0) {
        buffer += "  \033[36m[SLOW:" + std::to_string(player.slowTurns) + "]\033[90m";
    }
    if (player.hasteTurns > 0) {
        buffer += "  \033[33m[HASTE:" + std::to_string(player.hasteTurns) + "]\033[90m";
    }
    if (player.shieldTurns > 0) {
        buffer += "  \033[34m[SHIELD:" + std::to_string(player.shieldTurns) + "]\033[90m";
    }
    resetColor();

    // Controls hint
    moveCursor(0, hudY + 3);
    buffer += "\033[90m";
    buffer += "[WASD]Move [E]Pick up [I]Inv [>]Stairs [X]Look [Z]Explore [T]Wait [Q]Quit";
    resetColor();

    // Divider before messages
    moveCursor(0, hudY + 4);
    buffer += "\033[90m";
    for (int i = 0; i < Map::WIDTH; i++) buffer += '\xC4'; // ─
    resetColor();

    // Visible enemies on right side of status line
    {
        std::string enemyInfo;
        int shown = 0;
        for (auto& e : enemies) {
            if (!e.isAlive() || !map.isVisible(e.pos.x, e.pos.y)) continue;
            if (shown > 0) enemyInfo += "  ";
            // Apply enemy color
            switch (e.type) {
                case EnemyType::Rat:         enemyInfo += "\033[32m"; break;
                case EnemyType::Skeleton:    enemyInfo += "\033[37m"; break;
                case EnemyType::Ghost:       enemyInfo += "\033[35m"; break;
                case EnemyType::Demon:       enemyInfo += "\033[31m"; break;
                case EnemyType::Dragon:      enemyInfo += "\033[1;31m"; break;
                case EnemyType::Archer:      enemyInfo += "\033[33m"; break;
                case EnemyType::Necromancer: enemyInfo += "\033[35m"; break;
                case EnemyType::Lich:        enemyInfo += "\033[1;35m"; break;
            }
            enemyInfo += e.glyph;
            enemyInfo += ":" + std::to_string(e.hp) + "/" + std::to_string(e.maxHp);
            enemyInfo += "\033[0m";
            shown++;
            if (shown >= 5) break;
        }
        if (shown > 0) {
            moveCursor(55, hudY + 2);
            buffer += enemyInfo;
        }
    }

    // Message log (last 4 messages) with colored text
    int logStart = hudY + 5;
    int logCount = std::min((int)log.size(), 4);
    for (int i = 0; i < logCount; i++) {
        moveCursor(0, logStart + i);
        int age = logCount - 1 - i; // 0=newest, 1=second newest, etc.
        std::string msg = log[log.size() - logCount + i];
        if ((int)msg.size() > Map::WIDTH) msg = msg.substr(0, Map::WIDTH);
        if (age == 0) {
            buffer += "\033[1m"; // bold
            buffer += getMessageColor(msg);
        } else if (age == 1) {
            buffer += getMessageColor(msg);
        } else {
            buffer += "\033[90m";
        }
        buffer += msg;
        resetColor();
    }

    std::fputs(buffer.c_str(), stdout);
    std::fflush(stdout);
}

void Renderer::renderWithCursor(const Map& map, const Player& player,
                const std::vector<Enemy>& enemies,
                const std::vector<Item>& items,
                const std::vector<Trap>& traps,
                const std::vector<std::string>& log,
                int floor, Vec2 cursor, const std::string& desc,
                Biome biome, Vec2 merchantPos) {
    render(map, player, enemies, items, traps, log, floor, biome, merchantPos);

    // Overlay cursor with reverse video
    moveCursor(cursor.x, cursor.y);
    buffer += "\033[7m"; // reverse video

    // Show what's at cursor position
    bool found = false;
    for (auto& e : enemies) {
        if (e.isAlive() && e.pos == cursor && map.isVisible(cursor.x, cursor.y)) {
            buffer += e.glyph; found = true; break;
        }
    }
    if (!found && cursor == player.pos) {
        buffer += '@'; found = true;
    }
    if (!found) {
        for (auto& item : items) {
            if (item.onGround && item.pos == cursor && map.isVisible(cursor.x, cursor.y)) {
                buffer += item.glyph; found = true; break;
            }
        }
    }
    if (!found && merchantPos.x >= 0 && cursor.x == merchantPos.x && cursor.y == merchantPos.y
        && map.isVisible(cursor.x, cursor.y)) {
        buffer += 'M'; found = true;
    }
    if (!found) {
        if (map.isExplored(cursor.x, cursor.y)) {
            buffer += tileChar(map.getTile(cursor.x, cursor.y), biome);
        } else {
            buffer += ' ';
        }
    }
    resetColor();

    // Show description on HUD — overwrite the controls line
    int hudY = Map::HEIGHT;
    moveCursor(0, hudY + 3);
    buffer += "\033[K"; // clear line
    buffer += "\033[1;36m";
    buffer += "[LOOK] " + desc + " (ESC/X=exit)";
    resetColor();

    std::fputs(buffer.c_str(), stdout);
    std::fflush(stdout);
}

void Renderer::renderTitle(const std::vector<std::string>& topScores, bool hasSaveFile) {
    clearScreen();

    int tw, th;
    getTerminalSize(tw, th);

    // Logo lines (kept as-is)
    const char* logo[] = {
        R"( _____  _               _                              _   )",
        R"(/ ____|| |             | |                            | |  )",
        R"(| (___ | |__   __ _  __| | _____      _____ _ __ _   _| |_ )",
        R"( \___ \| '_ \ / _` |/ _` |/ _ \ \ /\ / / __|  __| | | | __|)",
        R"( ____) | | | | (_| | (_| | (_) \ V  V / (__| |  | |_| | |_ )",
        R"(|_____/|_| |_|\__,_|\__,_|\___/ \_/\_/ \___|_|   \__, |\__|)",
        R"(                                                  __/ |    )",
        R"(                                                 |___/     )",
    };
    int logoW = 62;
    int logoH = 8;

    // Calculate vertical layout to center everything
    int scoreBlock = topScores.empty() ? 0 : (int)topScores.size() + 2;
    int totalH = logoH + 2 + scoreBlock + 3 + 2; // logo + subtitle + scores + menu/divider/tagline + padding
    int startY = std::max(1, (th - totalH) / 2);

    // Draw logo centered
    buffer += "\033[1;31m";
    for (int i = 0; i < logoH; i++) {
        int lx = std::max(0, (tw - logoW) / 2);
        moveCursor(lx, startY + i);
        buffer += logo[i];
    }
    resetColor();

    int y = startY + logoH + 1;

    // Subtitle
    buffer += "\033[1;33m";
    printCentered(y, tw, "A Roguelike Dungeon Crawler");
    resetColor();
    y += 2;

    // High scores
    if (!topScores.empty()) {
        buffer += "\033[90m";
        std::string divider(std::min(30, tw - 4), '-');
        printCentered(y, tw, divider);
        resetColor();
        y++;

        buffer += "\033[1;37m";
        printCentered(y, tw, "HIGH SCORES");
        resetColor();
        y++;

        for (int i = 0; i < (int)topScores.size(); i++) {
            buffer += "\033[33m";
            printCentered(y, tw, topScores[i]);
            resetColor();
            y++;
        }

        buffer += "\033[90m";
        printCentered(y, tw, divider);
        resetColor();
        y++;
    }

    y++;

    // Menu options
    buffer += "\033[1;37m";
    if (hasSaveFile) {
        printCentered(y, tw, "[ENTER] New Game    [L] Continue    [H] Help    [Q] Quit");
    } else {
        printCentered(y, tw, "[ENTER] Play    [H] How to Play    [Q] Quit");
    }
    resetColor();
    y += 2;

    // Tagline
    buffer += "\033[90m";
    printCentered(y, tw, "Descend 8 floors. Slay the Lich.");
    resetColor();
    y += 2;

    buffer += "\033[90m";
    printCentered(y, tw, "v1.0");
    resetColor();

    std::fputs(buffer.c_str(), stdout);
    std::fflush(stdout);
}

void Renderer::renderGameOver(int floor, int level) {
    clearScreen();

    int tw, th;
    getTerminalSize(tw, th);
    int y = std::max(1, th / 2 - 4);

    buffer += "\033[1;31m";
    printCentered(y, tw, "=== YOU HAVE DIED ===");
    resetColor();

    y += 3;
    buffer += "\033[37m";
    printCentered(y, tw, "Reached Floor " + std::to_string(floor) + ", Level " + std::to_string(level));
    resetColor();

    y += 3;
    buffer += "\033[90m";
    printCentered(y, tw, "The dungeon claims another soul...");
    resetColor();

    y += 3;
    buffer += "\033[1;37m";
    printCentered(y, tw, "Press [R] to retry or [Q] to quit");
    resetColor();

    std::fputs(buffer.c_str(), stdout);
    std::fflush(stdout);
}

void Renderer::renderWin(const Player& player, int floor, int score) {
    clearScreen();

    int tw, th;
    getTerminalSize(tw, th);
    int pad = std::max(2, (tw - 50) / 2);
    int y = std::max(1, (th - 22) / 2);

    buffer += "\033[1;33m";
    printCentered(y, tw, "=== VICTORY! ===");
    resetColor();
    y += 2;

    buffer += "\033[1;37m";
    printCentered(y, tw, "You have slain the Lich and conquered the dungeon!");
    resetColor();
    y += 1;

    buffer += "\033[33m";
    printCentered(y, tw, "The dungeon trembles as light returns...");
    resetColor();
    y += 2;

    auto statLine = [&](const std::string& label, const std::string& val) {
        moveCursor(pad, y);
        buffer += "\033[90m" + label;
        moveCursor(pad + 18, y);
        buffer += "\033[1;37m" + val;
        resetColor();
        y++;
    };

    auto classStr = [](PlayerClass c) -> std::string {
        switch (c) {
            case PlayerClass::Warrior: return "Warrior";
            case PlayerClass::Rogue:   return "Rogue";
            case PlayerClass::Mage:    return "Mage";
            case PlayerClass::Cleric:  return "Cleric";
        }
        return "???";
    };

    statLine("Class:", classStr(player.playerClass));
    statLine("Level:", std::to_string(player.level));
    statLine("Floor:", std::to_string(floor));
    statLine("Turns:", std::to_string(player.turnsPlayed));
    statLine("Kills:", std::to_string(player.killCount));
    statLine("Gold:", std::to_string(player.gold));
    statLine("Damage Dealt:", std::to_string(player.damageDealt));
    statLine("Damage Taken:", std::to_string(player.damageTaken));
    statLine("Potions Used:", std::to_string(player.potionsUsed));
    y++;

    moveCursor(pad, y);
    buffer += "\033[90mWeapon: \033[33m";
    if (player.hasEquippedWeapon()) {
        buffer += player.getWeaponSlot().name + " (+" + std::to_string(player.getWeaponSlot().value) + " ATK)";
    } else {
        buffer += "(none)";
    }
    resetColor();
    y++;

    moveCursor(pad, y);
    buffer += "\033[90mArmor:  \033[32m";
    if (player.hasEquippedArmor()) {
        buffer += player.getArmorSlot().name + " (+" + std::to_string(player.getArmorSlot().value) + " DEF)";
    } else {
        buffer += "(none)";
    }
    resetColor();
    y += 2;

    buffer += "\033[1;33m";
    printCentered(y, tw, "Score: " + std::to_string(score));
    resetColor();
    y += 2;

    buffer += "\033[1;37m";
    printCentered(y, tw, "Press [R] to play again or [Q] to quit");
    resetColor();

    std::fputs(buffer.c_str(), stdout);
    std::fflush(stdout);
}

void Renderer::renderHelp() {
    clearScreen();

    int tw, th;
    getTerminalSize(tw, th);
    int pad = std::max(2, (tw - 72) / 2); // left padding to center a ~72 char block
    int y = std::max(0, (th - 24) / 2);

    // Title
    buffer += "\033[1;33m";
    printCentered(y, tw, "=== HOW TO PLAY ===");
    resetColor();
    y += 2;

    // Movement
    moveCursor(pad, y);
    buffer += "\033[1;37mMovement:\033[0m";
    moveCursor(pad + 2, y + 1);
    buffer += "\033[37mWASD or Arrow Keys to move\033[0m";
    y += 3;

    // Actions
    moveCursor(pad, y);
    buffer += "\033[1;37mActions:\033[0m";
    moveCursor(pad + 2, y + 1);
    buffer += "\033[37mE = Pick up/Trade    I = Open inventory    R = Use ability\033[0m";
    moveCursor(pad + 2, y + 2);
    buffer += "\033[37m> or . = Descend stairs    X = Examine    Z = Auto-explore    Q = Quit\033[0m";
    moveCursor(pad + 2, y + 3);
    buffer += "\033[37mS = Sell items (at merchant)    M = Message log    T = Wait/Rest\033[0m";
    moveCursor(pad + 2, y + 4);
    buffer += "\033[37mItems and gold are auto-picked up on walk\033[0m";
    y += 6;

    // Inventory
    moveCursor(pad, y);
    buffer += "\033[1;37mInventory:\033[0m";
    moveCursor(pad + 2, y + 1);
    buffer += "\033[37m1-9 to use/equip item    ESC or I to close\033[0m";
    y += 3;

    // Combat
    moveCursor(pad, y);
    buffer += "\033[1;37mCombat:\033[0m";
    moveCursor(pad + 2, y + 1);
    buffer += "\033[37mBump into enemies to attack. They strike back when adjacent.\033[0m";
    y += 3;

    // Goal
    moveCursor(pad, y);
    buffer += "\033[1;37mGoal:\033[0m";
    moveCursor(pad + 2, y + 1);
    buffer += "\033[37mDescend 8 floors and defeat the Lich!\033[0m";
    y += 3;

    // Symbols
    moveCursor(pad, y);
    buffer += "\033[1;37mSymbols:\033[0m";
    moveCursor(pad + 2, y + 1);
    buffer += "\033[1;33m@\033[37m You   ";
    buffer += "\033[32mr\033[37m Rat   ";
    buffer += "\033[37ms\033[37m Skeleton   ";
    buffer += "\033[35mg\033[37m Ghost   ";
    buffer += "\033[31mD\033[37m Demon/Dragon   ";
    buffer += "\033[33ma\033[37m Archer";
    moveCursor(pad + 2, y + 2);
    buffer += "\033[35mn\033[37m Necromancer   ";
    buffer += "\033[1;35mL\033[37m Lich   ";
    buffer += "\033[1;33mM\033[37m Merchant   ";
    buffer += "\033[33m$\033[37m Gold";
    moveCursor(pad + 2, y + 3);
    buffer += "\033[31m!\033[37m Potion   ";
    buffer += "\033[36m?\033[37m Scroll   ";
    buffer += "\033[33m/\033[37m Weapon   ";
    buffer += "\033[32m[\033[37m Armor   ";
    buffer += "\033[33m>\033[37m Stairs";
    resetColor();
    y += 5;

    // Footer
    buffer += "\033[90m";
    printCentered(y + 1, tw, "Press any key to return...");
    resetColor();

    std::fputs(buffer.c_str(), stdout);
    std::fflush(stdout);
}

void Renderer::renderClassSelect() {
    clearScreen();

    int tw, th;
    getTerminalSize(tw, th);
    int pad = std::max(2, (tw - 64) / 2);
    int y = std::max(0, (th - 22) / 2);

    buffer += "\033[1;33m";
    printCentered(y, tw, "=== CHOOSE YOUR CLASS ===");
    resetColor();
    y += 3;

    moveCursor(pad, y);
    buffer += "\033[1;31m[1] Warrior\033[0m";
    moveCursor(pad + 4, y + 1);
    buffer += "\033[37mHP: 40  ATK: 4  DEF: 4\033[0m";
    moveCursor(pad + 4, y + 2);
    buffer += "\033[37mAbility: Shield Bash - Bonus damage + stun enemy (8 turn CD)\033[0m";
    y += 4;

    moveCursor(pad, y);
    buffer += "\033[1;32m[2] Rogue\033[0m";
    moveCursor(pad + 4, y + 1);
    buffer += "\033[37mHP: 25  ATK: 7  DEF: 1\033[0m";
    moveCursor(pad + 4, y + 2);
    buffer += "\033[37mAbility: Backstab - Next attack deals 3x damage (6 turn CD)\033[0m";
    y += 4;

    moveCursor(pad, y);
    buffer += "\033[1;34m[3] Mage\033[0m";
    moveCursor(pad + 4, y + 1);
    buffer += "\033[37mHP: 20  ATK: 8  DEF: 1\033[0m";
    moveCursor(pad + 4, y + 2);
    buffer += "\033[37mAbility: Fireball - AoE damage to nearby enemies (10 turn CD)\033[0m";
    y += 4;

    moveCursor(pad, y);
    buffer += "\033[1;33m[4] Cleric\033[0m";
    moveCursor(pad + 4, y + 1);
    buffer += "\033[37mHP: 35  ATK: 3  DEF: 3\033[0m";
    moveCursor(pad + 4, y + 2);
    buffer += "\033[37mAbility: Divine Heal - Restore 50% HP (12 turn CD)\033[0m";
    moveCursor(pad + 4, y + 3);
    buffer += "\033[37mPassive: Potions heal 50% more\033[0m";
    y += 5;

    y++;
    buffer += "\033[90m";
    printCentered(y, tw, "Press [1-4] to select, [Q] to go back");
    resetColor();

    std::fputs(buffer.c_str(), stdout);
    std::fflush(stdout);
}

void Renderer::renderInventory(const Player& player) {
    clearScreen();

    moveCursor(5, 1);
    buffer += "\033[1;33m";
    buffer += "=== INVENTORY ===";
    resetColor();

    moveCursor(5, 3);
    buffer += "\033[1;37m";
    buffer += "Equipped:";
    resetColor();

    moveCursor(7, 4);
    buffer += "\033[37m";
    if (player.equippedWeapon) {
        buffer += "Weapon: " + player.equippedWeapon->name +
                  " (+" + std::to_string(player.equippedWeapon->value) + " ATK)";
    } else {
        buffer += "Weapon: (none)";
    }

    moveCursor(7, 5);
    if (player.equippedArmor) {
        buffer += "Armor:  " + player.equippedArmor->name +
                  " (+" + std::to_string(player.equippedArmor->value) + " DEF)";
    } else {
        buffer += "Armor:  (none)";
    }
    resetColor();

    moveCursor(5, 7);
    buffer += "\033[1;37m";
    buffer += "Items (" + std::to_string(player.inventory.size()) + "/" +
              std::to_string(Inventory::MAX_ITEMS) + "):";
    resetColor();

    if (player.inventory.size() == 0) {
        moveCursor(7, 8);
        buffer += "\033[90m";
        buffer += "(empty)";
        resetColor();
    } else {
        for (int i = 0; i < player.inventory.size(); i++) {
            moveCursor(7, 8 + i);
            const Item& item = player.inventory.get(i);
            buffer += "\033[37m";
            buffer += "[" + std::to_string(i + 1) + "] " + item.description();
            resetColor();
        }
    }

    moveCursor(5, 20);
    buffer += "\033[90m";
    buffer += "Press [1-9] to use/equip item, [ESC/I] to close";
    resetColor();

    std::fputs(buffer.c_str(), stdout);
    std::fflush(stdout);
}

void Renderer::renderShop(const std::vector<ShopItem>& shopItems, int playerGold) {
    clearScreen();

    moveCursor(5, 1);
    buffer += "\033[1;33m";
    buffer += "=== MERCHANT'S WARES ===";
    resetColor();

    moveCursor(5, 3);
    buffer += "\033[1;37m";
    buffer += "Your Gold: " + std::to_string(playerGold);
    resetColor();

    for (int i = 0; i < (int)shopItems.size(); i++) {
        moveCursor(7, 5 + i);
        if (shopItems[i].sold) {
            buffer += "\033[90m";
            buffer += "[" + std::to_string(i + 1) + "] (SOLD)";
        } else {
            buffer += "\033[37m";
            buffer += "[" + std::to_string(i + 1) + "] " +
                      shopItems[i].item.description() +
                      " - \033[33m" + std::to_string(shopItems[i].price) + " gold\033[37m";
        }
        resetColor();
    }

    moveCursor(5, 18);
    buffer += "\033[90m";
    buffer += "Press [1-5] to buy, [S] to sell items, [ESC] to leave";
    resetColor();

    std::fputs(buffer.c_str(), stdout);
    std::fflush(stdout);
}

void Renderer::renderSellMenu(const Player& player) {
    clearScreen();

    moveCursor(5, 1);
    buffer += "\033[1;33m";
    buffer += "=== SELL ITEMS ===";
    resetColor();

    moveCursor(5, 3);
    buffer += "\033[1;37m";
    buffer += "Your Gold: " + std::to_string(player.gold);
    resetColor();

    if (player.inventory.size() == 0) {
        moveCursor(7, 5);
        buffer += "\033[90m";
        buffer += "(no items to sell)";
        resetColor();
    } else {
        for (int i = 0; i < player.inventory.size(); i++) {
            moveCursor(7, 5 + i);
            const Item& item = player.inventory.get(i);
            int price = item.sellPrice();
            buffer += "\033[37m";
            buffer += "[" + std::to_string(i + 1) + "] " + item.description();
            if (price > 0) {
                buffer += " - \033[33m" + std::to_string(price) + " gold\033[37m";
            } else {
                buffer += " - \033[90mno value\033[37m";
            }
            resetColor();
        }
    }

    moveCursor(5, 18);
    buffer += "\033[90m";
    buffer += "Press [1-9] to sell, [ESC] to go back";
    resetColor();

    std::fputs(buffer.c_str(), stdout);
    std::fflush(stdout);
}

void Renderer::renderDeathRecap(const Player& player, int floor, int score) {
    clearScreen();

    int tw, th;
    getTerminalSize(tw, th);
    int pad = std::max(2, (tw - 50) / 2);
    int y = std::max(1, (th - 20) / 2);

    buffer += "\033[1;31m";
    printCentered(y, tw, "=== YOU HAVE DIED ===");
    resetColor();
    y += 2;

    // Cause of death
    buffer += "\033[37m";
    printCentered(y, tw, "Slain by " + player.lastDamageSource);
    resetColor();
    y += 2;

    // Stats table
    auto statLine = [&](const std::string& label, const std::string& val) {
        moveCursor(pad, y);
        buffer += "\033[90m" + label;
        moveCursor(pad + 18, y);
        buffer += "\033[1;37m" + val;
        resetColor();
        y++;
    };

    auto classStr = [](PlayerClass c) -> std::string {
        switch (c) {
            case PlayerClass::Warrior: return "Warrior";
            case PlayerClass::Rogue:   return "Rogue";
            case PlayerClass::Mage:    return "Mage";
            case PlayerClass::Cleric:  return "Cleric";
        }
        return "???";
    };

    statLine("Class:", classStr(player.playerClass));
    statLine("Level:", std::to_string(player.level));
    statLine("Floor:", std::to_string(floor));
    statLine("Turns:", std::to_string(player.turnsPlayed));
    statLine("Kills:", std::to_string(player.killCount));
    statLine("Gold:", std::to_string(player.gold));
    statLine("Damage Dealt:", std::to_string(player.damageDealt));
    statLine("Damage Taken:", std::to_string(player.damageTaken));
    statLine("Potions Used:", std::to_string(player.potionsUsed));
    y++;

    // Equipment
    moveCursor(pad, y);
    buffer += "\033[90mWeapon: \033[33m";
    if (player.hasEquippedWeapon()) {
        buffer += player.getWeaponSlot().name + " (+" + std::to_string(player.getWeaponSlot().value) + " ATK)";
    } else {
        buffer += "(none)";
    }
    resetColor();
    y++;

    moveCursor(pad, y);
    buffer += "\033[90mArmor:  \033[32m";
    if (player.hasEquippedArmor()) {
        buffer += player.getArmorSlot().name + " (+" + std::to_string(player.getArmorSlot().value) + " DEF)";
    } else {
        buffer += "(none)";
    }
    resetColor();
    y += 2;

    // Score
    buffer += "\033[1;33m";
    printCentered(y, tw, "Score: " + std::to_string(score));
    resetColor();
    y += 2;

    buffer += "\033[1;37m";
    printCentered(y, tw, "Press [R] to retry or [Q] to quit");
    resetColor();

    std::fputs(buffer.c_str(), stdout);
    std::fflush(stdout);
}

void Renderer::renderLevelUp(const Player& player) {
    clearScreen();

    int tw, th;
    getTerminalSize(tw, th);
    int pad = std::max(2, (tw - 50) / 2);
    int y = std::max(1, (th - 14) / 2);

    buffer += "\033[1;33m";
    printCentered(y, tw, "=== LEVEL UP! ===");
    resetColor();
    y += 2;

    buffer += "\033[1;37m";
    printCentered(y, tw, "You reached Level " + std::to_string(player.level) + "!");
    resetColor();
    y += 2;

    buffer += "\033[37m";
    printCentered(y, tw, "Choose a bonus:");
    resetColor();
    y += 2;

    moveCursor(pad, y);
    buffer += "\033[1;32m[1] Vitality\033[0m";
    moveCursor(pad + 16, y);
    buffer += "\033[37m+" + std::to_string(player.lvlHp) + " Max HP\033[0m";
    y += 2;

    moveCursor(pad, y);
    buffer += "\033[1;31m[2] Power\033[0m";
    moveCursor(pad + 16, y);
    buffer += "\033[37m+" + std::to_string(std::max(1, player.lvlAtk)) + " ATK\033[0m";
    y += 2;

    moveCursor(pad, y);
    buffer += "\033[1;34m[3] Fortitude\033[0m";
    moveCursor(pad + 16, y);
    buffer += "\033[37m+" + std::to_string(std::max(1, player.lvlDef)) + " DEF\033[0m";
    y += 2;

    buffer += "\033[90m";
    printCentered(y, tw, "Press [1-3] to choose");
    resetColor();

    std::fputs(buffer.c_str(), stdout);
    std::fflush(stdout);
}

void Renderer::renderDifficultySelect() {
    clearScreen();

    int tw, th;
    getTerminalSize(tw, th);
    int pad = std::max(2, (tw - 56) / 2);
    int y = std::max(1, (th - 16) / 2);

    buffer += "\033[1;33m";
    printCentered(y, tw, "=== SELECT DIFFICULTY ===");
    resetColor();
    y += 3;

    moveCursor(pad, y);
    buffer += "\033[1;32m[1] Easy\033[0m";
    moveCursor(pad + 4, y + 1);
    buffer += "\033[37mEnemies have less health and deal less damage.\033[0m";
    y += 3;

    moveCursor(pad, y);
    buffer += "\033[1;37m[2] Normal\033[0m";
    moveCursor(pad + 4, y + 1);
    buffer += "\033[37mThe standard dungeon experience.\033[0m";
    y += 3;

    moveCursor(pad, y);
    buffer += "\033[1;31m[3] Hard\033[0m";
    moveCursor(pad + 4, y + 1);
    buffer += "\033[37mMore enemies, stronger and more aggressive.\033[0m";
    y += 3;

    buffer += "\033[90m";
    printCentered(y, tw, "Press [1-3] to select, [Q] to go back");
    resetColor();

    std::fputs(buffer.c_str(), stdout);
    std::fflush(stdout);
}

void Renderer::renderMessageLog(const std::vector<std::string>& log, int scrollOffset) {
    clearScreen();

    int tw, th;
    getTerminalSize(tw, th);
    int y = 1;

    moveCursor(5, y);
    buffer += "\033[1;33m";
    buffer += "=== MESSAGE LOG ===";
    resetColor();
    y += 2;

    int maxLines = th - 5;
    int total = (int)log.size();
    // scrollOffset 0 = most recent at bottom, increasing = scroll up
    int endIdx = total - scrollOffset;
    int startIdx = std::max(0, endIdx - maxLines);

    for (int i = startIdx; i < endIdx; i++) {
        moveCursor(3, y);
        if (i == total - 1 && scrollOffset == 0) {
            buffer += "\033[1;37m"; // latest message bright
        } else {
            buffer += "\033[37m";
        }
        std::string msg = log[i];
        if ((int)msg.size() > tw - 4) msg = msg.substr(0, tw - 4);
        buffer += msg;
        resetColor();
        y++;
    }

    moveCursor(5, th - 1);
    buffer += "\033[90m";
    buffer += "[UP/DOWN] Scroll  [ESC/M] Close";
    if (total > 0) {
        buffer += "  (" + std::to_string(startIdx + 1) + "-" + std::to_string(endIdx) + " of " + std::to_string(total) + ")";
    }
    resetColor();

    std::fputs(buffer.c_str(), stdout);
    std::fflush(stdout);
}

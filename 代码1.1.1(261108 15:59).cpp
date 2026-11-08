#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <string>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <cmath>

const int WORLD_W = 22;
const int WORLD_H = 18;
const int TILE_W = 46;
const int TILE_H = 24;
const UINT_PTR GAME_TIMER = 1;

enum TileType { GRASS, SAND, WATER };
enum ObjectType { NOTHING, TREE, ROCK, BERRY, CAMPFIRE };

struct Tile {
    TileType ground;
    ObjectType object;
    int shade;
};

Tile world[WORLD_W][WORLD_H];
int playerX = 10;
int playerY = 10;
int wood = 0;
int stone = 0;
int berries = 0;
int health = 100;
int hunger = 100;
int selectedSlot = 0;
int screenWidth = 1200;
int screenHeight = 760;
bool keys[256] = {};
bool gameOver = false;
DWORD startedAt = 0;
DWORD lastMoveAt = 0;
DWORD lastHungerAt = 0;
DWORD lastRecoveryAt = 0;
HWND mainWindow = NULL;

void FillRectColor(HDC dc, int x, int y, int w, int h, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    RECT rect = { x, y, x + w, y + h };
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
}

void DrawTextAt(HDC dc, const std::wstring& text, int x, int y, COLORREF color,
                int size = 16, bool bold = false) {
    HFONT font = CreateFontW(-size, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL,
                             FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS,
                             L"Segoe UI");
    HFONT oldFont = (HFONT)SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    TextOutW(dc, x, y, text.c_str(), (int)text.length());
    SelectObject(dc, oldFont);
    DeleteObject(font);
}

void DrawCenteredText(HDC dc, const std::wstring& text, int x, int y,
                      COLORREF color, int size = 16, bool bold = false) {
    SIZE extent = { 0, 0 };
    HFONT font = CreateFontW(-size, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL,
                             FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS,
                             L"Segoe UI");
    HFONT oldFont = (HFONT)SelectObject(dc, font);
    GetTextExtentPoint32W(dc, text.c_str(), (int)text.length(), &extent);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    TextOutW(dc, x - extent.cx / 2, y, text.c_str(), (int)text.length());
    SelectObject(dc, oldFont);
    DeleteObject(font);
}

void PolygonFill(HDC dc, POINT* points, int count, COLORREF color,
                 COLORREF outline = RGB(0, 0, 0)) {
    HBRUSH brush = CreateSolidBrush(color);
    HPEN pen = CreatePen(PS_SOLID, 1, outline);
    HBRUSH oldBrush = (HBRUSH)SelectObject(dc, brush);
    HPEN oldPen = (HPEN)SelectObject(dc, pen);
    Polygon(dc, points, count);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void EllipseFill(HDC dc, int left, int top, int right, int bottom,
                 COLORREF color, COLORREF outline) {
    HBRUSH brush = CreateSolidBrush(color);
    HPEN pen = CreatePen(PS_SOLID, 1, outline);
    HBRUSH oldBrush = (HBRUSH)SelectObject(dc, brush);
    HPEN oldPen = (HPEN)SelectObject(dc, pen);
    Ellipse(dc, left, top, right, bottom);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void RoundedPanel(HDC dc, int x, int y, int w, int h, COLORREF color,
                  COLORREF border) {
    HBRUSH brush = CreateSolidBrush(color);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HBRUSH oldBrush = (HBRUSH)SelectObject(dc, brush);
    HPEN oldPen = (HPEN)SelectObject(dc, pen);
    RoundRect(dc, x, y, x + w, y + h, 12, 12);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void WorldOrigin(int& ox, int& oy) {
    ox = screenWidth / 2;
    oy = 150;
}

void TileCenter(int tx, int ty, int& x, int& y) {
    int ox, oy;
    WorldOrigin(ox, oy);
    x = ox + (tx - ty) * (TILE_W / 2);
    y = oy + (tx + ty) * (TILE_H / 2);
}

bool IsWalkable(int x, int y) {
    return x >= 0 && x < WORLD_W && y >= 0 && y < WORLD_H &&
           world[x][y].ground != WATER;
}

void InitWorld() {
    srand(7);
    for (int x = 0; x < WORLD_W; ++x) {
        for (int y = 0; y < WORLD_H; ++y) {
            world[x][y].ground = GRASS;
            world[x][y].object = NOTHING;
            world[x][y].shade = rand() % 3;
            if ((x < 3 || y < 2 || x > WORLD_W - 4 || y > WORLD_H - 3) &&
                rand() % 5 == 0) {
                world[x][y].ground = SAND;
            }
            if (x >= 13 && x <= 15 && y >= 6 && y <= 9) {
                world[x][y].ground = WATER;
            }
            if (world[x][y].ground != WATER && rand() % 100 < 15) {
                world[x][y].object = TREE;
            } else if (world[x][y].ground != WATER && rand() % 100 < 7) {
                world[x][y].object = ROCK;
            } else if (world[x][y].ground != WATER && rand() % 100 < 7) {
                world[x][y].object = BERRY;
            }
        }
    }

    for (int x = 8; x <= 12; ++x) {
        for (int y = 8; y <= 12; ++y) {
            if (world[x][y].ground == WATER) world[x][y].ground = GRASS;
            world[x][y].object = NOTHING;
        }
    }
    world[7][9].object = TREE;
    world[8][7].object = TREE;
    world[12][8].object = ROCK;
    world[11][12].object = ROCK;
    world[9][7].object = BERRY;
    playerX = 10;
    playerY = 10;
}

void DrawGround(HDC dc, int x, int y, int cx, int cy) {
    POINT diamond[4] = {
        { cx, cy - TILE_H / 2 }, { cx + TILE_W / 2, cy },
        { cx, cy + TILE_H / 2 }, { cx - TILE_W / 2, cy }
    };
    COLORREF grass[3] = { RGB(105, 174, 87), RGB(111, 181, 91), RGB(99, 168, 82) };
    COLORREF sand[3] = { RGB(210, 190, 126), RGB(218, 198, 137), RGB(204, 184, 120) };
    COLORREF water[3] = { RGB(63, 151, 185), RGB(71, 160, 193), RGB(58, 143, 177) };
    Tile& tile = world[x][y];
    COLORREF color = tile.ground == WATER ? water[tile.shade] :
                     tile.ground == SAND ? sand[tile.shade] : grass[tile.shade];
    PolygonFill(dc, diamond, 4, color,
                tile.ground == WATER ? RGB(50, 127, 158) : RGB(81, 144, 72));
    if (tile.ground == GRASS && (x * 3 + y * 5) % 7 == 0) {
        EllipseFill(dc, cx - 2, cy - 1, cx + 1, cy + 2,
                    RGB(128, 191, 97), RGB(128, 191, 97));
    }
    if (tile.ground == WATER && (x + y) % 3 == 0) {
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(126, 201, 218));
        HPEN oldPen = (HPEN)SelectObject(dc, pen);
        MoveToEx(dc, cx - 8, cy, NULL);
        LineTo(dc, cx + 4, cy);
        SelectObject(dc, oldPen);
        DeleteObject(pen);
    }
}

void DrawTree(HDC dc, int cx, int cy) {
    FillRectColor(dc, cx - 5, cy - 25, 10, 28, RGB(112, 75, 43));
    POINT leaves[6] = {
        { cx, cy - 59 }, { cx + 22, cy - 39 }, { cx + 15, cy - 20 },
        { cx, cy - 15 }, { cx - 17, cy - 24 }, { cx - 22, cy - 40 }
    };
    PolygonFill(dc, leaves, 6, RGB(49, 126, 66), RGB(41, 106, 55));
    EllipseFill(dc, cx - 16, cy - 52, cx + 7, cy - 28,
                RGB(69, 150, 73), RGB(69, 150, 73));
}

void DrawRock(HDC dc, int cx, int cy) {
    POINT rock[6] = {
        { cx - 13, cy - 4 }, { cx - 10, cy - 16 }, { cx + 1, cy - 23 },
        { cx + 14, cy - 13 }, { cx + 17, cy - 3 }, { cx + 4, cy + 3 }
    };
    PolygonFill(dc, rock, 6, RGB(137, 145, 145), RGB(93, 102, 102));
    HPEN pen = CreatePen(PS_SOLID, 2, RGB(186, 192, 187));
    HPEN oldPen = (HPEN)SelectObject(dc, pen);
    MoveToEx(dc, cx - 5, cy - 13, NULL);
    LineTo(dc, cx + 1, cy - 19);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

void DrawBerryBush(HDC dc, int cx, int cy) {
    EllipseFill(dc, cx - 15, cy - 19, cx + 15, cy + 2,
                RGB(56, 132, 66), RGB(45, 107, 53));
    EllipseFill(dc, cx - 8, cy - 11, cx - 3, cy - 6,
                RGB(205, 55, 75), RGB(205, 55, 75));
    EllipseFill(dc, cx + 4, cy - 15, cx + 9, cy - 10,
                RGB(220, 65, 81), RGB(220, 65, 81));
    EllipseFill(dc, cx + 1, cy - 5, cx + 6, cy,
                RGB(192, 49, 68), RGB(192, 49, 68));
}

void DrawCampfire(HDC dc, int cx, int cy) {
    POINT logs1[4] = { { cx - 13, cy - 2 }, { cx - 9, cy - 7 },
                       { cx + 13, cy + 1 }, { cx + 9, cy + 6 } };
    POINT logs2[4] = { { cx + 12, cy - 2 }, { cx + 8, cy - 7 },
                       { cx - 12, cy + 1 }, { cx - 8, cy + 6 } };
    PolygonFill(dc, logs1, 4, RGB(111, 69, 39), RGB(82, 50, 28));
    PolygonFill(dc, logs2, 4, RGB(131, 79, 41), RGB(82, 50, 28));
    POINT flame[5] = { { cx, cy - 23 }, { cx + 8, cy - 11 }, { cx + 5, cy - 4 },
                       { cx - 5, cy - 4 }, { cx - 8, cy - 12 } };
    PolygonFill(dc, flame, 5, RGB(255, 164, 54), RGB(220, 105, 39));
}

void DrawPlayer(HDC dc, int cx, int cy) {
    EllipseFill(dc, cx - 12, cy - 4, cx + 12, cy + 5,
                RGB(52, 89, 49), RGB(52, 89, 49));
    FillRectColor(dc, cx - 8, cy - 26, 16, 23, RGB(67, 117, 204));
    FillRectColor(dc, cx - 7, cy - 42, 14, 17, RGB(238, 191, 143));
    FillRectColor(dc, cx - 8, cy - 46, 16, 7, RGB(91, 61, 43));
    FillRectColor(dc, cx - 7, cy - 4, 5, 8, RGB(68, 51, 42));
    FillRectColor(dc, cx + 2, cy - 4, 5, 8, RGB(68, 51, 42));
    FillRectColor(dc, cx + 2, cy - 36, 2, 2, RGB(48, 48, 48));
}

void DrawObject(HDC dc, int x, int y, int cx, int cy) {
    switch (world[x][y].object) {
    case TREE: DrawTree(dc, cx, cy); break;
    case ROCK: DrawRock(dc, cx, cy); break;
    case BERRY: DrawBerryBush(dc, cx, cy); break;
    case CAMPFIRE: DrawCampfire(dc, cx, cy); break;
    default: break;
    }
}

std::wstring NumberText(int value) {
    std::wstringstream stream;
    stream << value;
    return stream.str();
}

void DrawBar(HDC dc, int x, int y, int w, int h, int value, COLORREF fill) {
    RoundedPanel(dc, x, y, w, h, RGB(48, 53, 56), RGB(83, 91, 94));
    int inner = (w - 4) * value / 100;
    if (inner > 0) {
        RoundedPanel(dc, x + 2, y + 2, inner, h - 4, fill, fill);
    }
}

void DrawHUD(HDC dc) {
    RoundedPanel(dc, 18, 15, 350, 91, RGB(28, 36, 39), RGB(77, 94, 92));
    DrawTextAt(dc, L"BLOCKLAND  /  SURVIVAL", 34, 24, RGB(240, 232, 204), 17, true);
    DrawTextAt(dc, L"HEALTH", 34, 53, RGB(221, 226, 218), 11, true);
    DrawBar(dc, 94, 52, 155, 15, health, RGB(210, 75, 71));
    DrawTextAt(dc, NumberText(health), 256, 52, RGB(240, 232, 204), 12, true);
    DrawTextAt(dc, L"HUNGER", 34, 77, RGB(221, 226, 218), 11, true);
    DrawBar(dc, 94, 76, 155, 15, hunger, RGB(224, 166, 59));
    DrawTextAt(dc, NumberText(hunger), 256, 76, RGB(240, 232, 204), 12, true);

    DWORD elapsed = (GetTickCount() - startedAt) / 1000;
    int day = (int)(elapsed / 120) + 1;
    int phase = (int)(elapsed % 120);
    std::wstring time = phase < 80 ? L"DAY" : L"NIGHT";
    RoundedPanel(dc, screenWidth - 186, 18, 166, 46, RGB(28, 36, 39), RGB(77, 94, 92));
    DrawTextAt(dc, L"DAY " + NumberText(day) + L"   " + time, screenWidth - 170, 32,
               RGB(240, 232, 204), 15, true);

    RoundedPanel(dc, screenWidth - 250, 76, 230, 88, RGB(28, 36, 39), RGB(77, 94, 92));
    DrawTextAt(dc, L"BACKPACK", screenWidth - 232, 88, RGB(240, 232, 204), 13, true);
    DrawTextAt(dc, L"Wood   " + NumberText(wood), screenWidth - 230, 111,
               RGB(224, 211, 176), 14);
    DrawTextAt(dc, L"Stone  " + NumberText(stone), screenWidth - 230, 132,
               RGB(203, 213, 216), 14);
    DrawTextAt(dc, L"Berries " + NumberText(berries), screenWidth - 130, 111,
               RGB(237, 167, 174), 14);
}

void DrawHotbar(HDC dc) {
    const int box = 54;
    const int gap = 7;
    int count = 4;
    int total = count * box + (count - 1) * gap;
    int start = screenWidth / 2 - total / 2;
    int y = screenHeight - 92;
    for (int i = 0; i < count; ++i) {
        COLORREF border = i == selectedSlot ? RGB(247, 215, 120) : RGB(104, 119, 114);
        RoundedPanel(dc, start + i * (box + gap), y, box, box,
                     i == selectedSlot ? RGB(69, 76, 65) : RGB(39, 47, 47), border);
        DrawTextAt(dc, NumberText(i + 1), start + i * (box + gap) + 6, y + 4,
                   RGB(196, 203, 190), 11, true);
        std::wstring icon = i == 0 ? L"W" : i == 1 ? L"S" : i == 2 ? L"B" : L"F";
        COLORREF iconColor = i == 0 ? RGB(193, 139, 80) :
                             i == 1 ? RGB(174, 187, 190) :
                             i == 2 ? RGB(222, 84, 101) : RGB(255, 169, 60);
        DrawCenteredText(dc, icon, start + i * (box + gap) + box / 2, y + 15,
                         iconColor, 24, true);
        if (i < 3) {
            int amount = i == 0 ? wood : i == 1 ? stone : berries;
            DrawCenteredText(dc, NumberText(amount), start + i * (box + gap) + box / 2,
                             y + 37, RGB(239, 238, 224), 12, true);
        }
    }
    DrawCenteredText(dc, L"1-4 SELECT     F EAT BERRIES     E GATHER     C BUILD CAMPFIRE",
                     screenWidth / 2, screenHeight - 25, RGB(227, 232, 214), 13, true);
}

void DrawWorld(HDC dc) {
    FillRectColor(dc, 0, 0, screenWidth, screenHeight, RGB(121, 183, 199));
    FillRectColor(dc, 0, 117, screenWidth, screenHeight - 117, RGB(75, 113, 90));

    for (int depth = 0; depth < WORLD_W + WORLD_H - 1; ++depth) {
        for (int x = 0; x < WORLD_W; ++x) {
            int y = depth - x;
            if (y < 0 || y >= WORLD_H) continue;
            int cx, cy;
            TileCenter(x, y, cx, cy);
            DrawGround(dc, x, y, cx, cy);
        }
    }

    for (int depth = 0; depth < WORLD_W + WORLD_H - 1; ++depth) {
        for (int x = 0; x < WORLD_W; ++x) {
            int y = depth - x;
            if (y < 0 || y >= WORLD_H) continue;
            int cx, cy;
            TileCenter(x, y, cx, cy);
            DrawObject(dc, x, y, cx, cy);
            if (x == playerX && y == playerY) DrawPlayer(dc, cx, cy);
        }
    }
}

void DrawGame(HDC dc) {
    DrawWorld(dc);
    DrawHUD(dc);
    DrawHotbar(dc);
    RoundedPanel(dc, 18, screenHeight - 104, 255, 77, RGB(28, 36, 39), RGB(77, 94, 92));
    DrawTextAt(dc, L"WASD / ARROWS  MOVE", 32, screenHeight - 91,
               RGB(232, 232, 217), 12, true);
    DrawTextAt(dc, L"Find trees, rocks and berry bushes.", 32, screenHeight - 69,
               RGB(199, 211, 196), 12);
    DrawTextAt(dc, L"Stand on a campfire to heal.", 32, screenHeight - 50,
               RGB(199, 211, 196), 12);

    if (gameOver) {
        FillRectColor(dc, 0, 0, screenWidth, screenHeight, RGB(20, 25, 26));
        DrawCenteredText(dc, L"YOU DID NOT SURVIVE", screenWidth / 2,
                         screenHeight / 2 - 36, RGB(246, 216, 173), 30, true);
        DrawCenteredText(dc, L"Press R to start a new day", screenWidth / 2,
                         screenHeight / 2 + 12, RGB(222, 227, 214), 17);
    }
}

void MovePlayer(int dx, int dy) {
    int nx = playerX + dx;
    int ny = playerY + dy;
    if (IsWalkable(nx, ny)) {
        playerX = nx;
        playerY = ny;
    }
}

bool IsNear(int x, int y) {
    return abs(x - playerX) <= 1 && abs(y - playerY) <= 1 &&
           (x != playerX || y != playerY);
}

void Gather() {
    int bestX = -1;
    int bestY = -1;
    int bestDistance = 100;
    for (int x = 0; x < WORLD_W; ++x) {
        for (int y = 0; y < WORLD_H; ++y) {
            if (!IsNear(x, y) || world[x][y].object == NOTHING ||
                world[x][y].object == CAMPFIRE) continue;
            int distance = abs(x - playerX) + abs(y - playerY);
            if (distance < bestDistance) {
                bestDistance = distance;
                bestX = x;
                bestY = y;
            }
        }
    }
    if (bestX < 0) return;
    ObjectType object = world[bestX][bestY].object;
    if (object == TREE) wood += 3;
    if (object == ROCK) stone += 2;
    if (object == BERRY) berries += 3;
    world[bestX][bestY].object = NOTHING;
}

void EatBerries() {
    if (berries <= 0 || hunger >= 100) return;
    --berries;
    hunger += 30;
    if (hunger > 100) hunger = 100;
}

void BuildCampfire() {
    if (wood < 4 || stone < 2 || world[playerX][playerY].object != NOTHING) return;
    wood -= 4;
    stone -= 2;
    world[playerX][playerY].object = CAMPFIRE;
}

void NewGame() {
    InitWorld();
    wood = 0;
    stone = 0;
    berries = 2;
    health = 100;
    hunger = 100;
    gameOver = false;
    startedAt = GetTickCount();
    lastHungerAt = startedAt;
    lastRecoveryAt = startedAt;
}

void GameTick() {
    if (gameOver) return;
    DWORD now = GetTickCount();
    if (now - lastMoveAt >= 135) {
        int dx = 0;
        int dy = 0;
        if (keys['W'] || keys[VK_UP]) dy = -1;
        else if (keys['S'] || keys[VK_DOWN]) dy = 1;
        else if (keys['A'] || keys[VK_LEFT]) dx = -1;
        else if (keys['D'] || keys[VK_RIGHT]) dx = 1;
        if (dx != 0 || dy != 0) {
            MovePlayer(dx, dy);
            lastMoveAt = now;
        }
    }
    if (now - lastHungerAt >= 12000) {
        if (hunger > 0) --hunger;
        else if (health > 0) --health;
        lastHungerAt = now;
    }
    if (now - lastRecoveryAt >= 2500) {
        if (world[playerX][playerY].object == CAMPFIRE && health < 100) {
            health += 2;
            if (health > 100) health = 100;
        }
        lastRecoveryAt = now;
    }
    if (health <= 0) gameOver = true;
    InvalidateRect(mainWindow, NULL, FALSE);
}

void PaintWindow(HWND hwnd) {
    PAINTSTRUCT paint;
    HDC dc = BeginPaint(hwnd, &paint);
    RECT client;
    GetClientRect(hwnd, &client);
    screenWidth = client.right - client.left;
    screenHeight = client.bottom - client.top;
    HDC memory = CreateCompatibleDC(dc);
    HBITMAP bitmap = CreateCompatibleBitmap(dc, screenWidth, screenHeight);
    HBITMAP oldBitmap = (HBITMAP)SelectObject(memory, bitmap);
    DrawGame(memory);
    BitBlt(dc, 0, 0, screenWidth, screenHeight, memory, 0, 0, SRCCOPY);
    SelectObject(memory, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(memory);
    EndPaint(hwnd, &paint);
}

LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_PAINT:
        PaintWindow(hwnd);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_TIMER:
        if (wParam == GAME_TIMER) GameTick();
        return 0;
    case WM_KEYDOWN:
        if (wParam < 256) keys[wParam] = true;
        if (gameOver && (wParam == 'R')) NewGame();
        if (!gameOver) {
            if (wParam == 'E') Gather();
            if (wParam == 'F') EatBerries();
            if (wParam == 'C') BuildCampfire();
            if (wParam >= '1' && wParam <= '4') selectedSlot = (int)wParam - '1';
        }
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_KEYUP:
        if (wParam < 256) keys[wParam] = false;
        return 0;
    case WM_LBUTTONDOWN: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        int box = 54;
        int gap = 7;
        int start = screenWidth / 2 - (4 * box + 3 * gap) / 2;
        int barY = screenHeight - 92;
        if (y >= barY && y <= barY + box && x >= start &&
            x < start + 4 * (box + gap)) {
            int slot = (x - start) / (box + gap);
            if (slot < 4) selectedSlot = slot;
        }
        SetFocus(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    case WM_KILLFOCUS:
        for (int i = 0; i < 256; ++i) keys[i] = false;
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, GAME_TIMER);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int showCommand) {
    const wchar_t CLASS_NAME[] = L"BlocklandSurvivalWindow";
    WNDCLASSEXW windowClass;
    ZeroMemory(&windowClass, sizeof(windowClass));
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProcedure;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    windowClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    windowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    windowClass.lpszClassName = CLASS_NAME;
    if (!RegisterClassExW(&windowClass)) return 1;

    RECT bounds = { 0, 0, 1200, 760 };
    AdjustWindowRect(&bounds, WS_OVERLAPPEDWINDOW, FALSE);
    mainWindow = CreateWindowExW(
        0, CLASS_NAME, L"Blockland: Survival",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        bounds.right - bounds.left, bounds.bottom - bounds.top,
        NULL, NULL, instance, NULL);
    if (!mainWindow) return 1;

    NewGame();
    ShowWindow(mainWindow, showCommand);
    UpdateWindow(mainWindow);
    SetTimer(mainWindow, GAME_TIMER, 33, NULL);

    MSG message;
    while (GetMessageW(&message, NULL, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return (int)message.wParam;
}

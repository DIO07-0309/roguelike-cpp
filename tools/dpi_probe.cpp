// G10.9-fix 探针 v7: 全量坐标审计 + GPU 原点色块
#include "raylib.h"
#include <cstdio>

int main() {
    const int W = 960, H = 640;
    SetConfigFlags(FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE);
    InitWindow(W, H, "probe v7");
    SetTargetFPS(10);

    FILE* f = nullptr;
    for (int frame = 0; frame < 45; frame++) {
        BeginDrawing();
        ClearBackground(BLACK);
        // 原点红块 200x200 + x=800..960 顶蓝条 + x=960..1440 绿条(界外宽度探测)
        DrawRectangle(0, 0, 200, 200, RED);
        DrawRectangle(800, 0, 160, 200, BLUE);
        DrawRectangle(960, 0, 480, 200, GREEN);
        // y 探测: y=600..640 蓝条(界内) + y=640..960 绿条(界外)
        DrawRectangle(0, 440, 200, 200, BLUE);
        DrawRectangle(0, 640, 200, 320, GREEN);
        EndDrawing();

        if (frame == 5) {
            f = fopen("dpi_probe.txt", "w");
            fprintf(f, "Screen=%dx%d Render=%dx%d Monitor=%dx%d DPI=%.2f WinPos=%d,%d\n",
                    GetScreenWidth(), GetScreenHeight(), GetRenderWidth(), GetRenderHeight(),
                    GetMonitorWidth(0), GetMonitorHeight(0),
                    GetWindowScaleDPI().x, GetWindowPosition().x, GetWindowPosition().y);
        }
        if (frame == 30) { TakeScreenshot("probe_v7.png"); }
    }
    if (f) fclose(f);
    CloseWindow();
    return 0;
}

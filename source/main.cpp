#include <switch.h>
#include <cstdio>

#include "app.hpp"

int main(int argc, char** argv) {
    FILE* boot = fopen("sdmc:/switch/stremnx-boot.log", "w");
    if (boot) { fputs("StremNX libounce startup\n", boot); fflush(boot); }
    Result net = socketInitializeDefault();
    if (boot) { fprintf(boot, "socketInitializeDefault: 0x%x\n", net); fflush(boot); }
    Result font = plInitialize(PlServiceType_User);
    if (boot) { fprintf(boot, "plInitialize: 0x%x\n", font); fflush(boot); }
    Result assets = romfsInit();
    if (boot) { fprintf(boot, "romfsInit: 0x%x\n", assets); fflush(boot); }

    {
        App app;
        if (boot) { fputs("App::init starting\n", boot); fflush(boot); }
        if (app.init()) {
            if (boot) { fputs("App::run starting\n", boot); fflush(boot); }
            app.run();
            if (boot) { fputs("App::run returned\n", boot); fflush(boot); }

            // hbmenu also uses + to exit. Let go of the button before handing
            // control back, or its first input scan can close the menu again.
            bool plusReleased = false;
            int releasePolls = 0;
            for (; releasePolls < 250; ++releasePolls) {
                app.in.update();
                if (!app.in.held(K_PLUS)) { plusReleased = true; break; }
                svcSleepThread(20'000'000);
            }
            svcSleepThread(100'000'000);
            if (boot) {
                fprintf(boot, "Plus release before shutdown: %d (polls: %d)\n", plusReleased, releasePolls);
                fflush(boot);
            }
        }
        app.shutdown();
        if (boot) { fputs("App::shutdown returned\n", boot); fflush(boot); }
    }

    if (R_SUCCEEDED(assets)) romfsExit();
    if (R_SUCCEEDED(font)) plExit();
    if (R_SUCCEEDED(net)) socketExit();

    bool nextLoad = envHasNextLoad();
    if (boot) {
        fprintf(boot, "envHasNextLoad: %d, exitFunc: %s\n", nextLoad,
                envGetExitFuncPtr() ? "set" : "missing");
        fflush(boot);
    }
    if (nextLoad) {
        Result rc = envSetNextLoad("sdmc:/hbmenu.nro", "sdmc:/hbmenu.nro");
        if (boot) { fprintf(boot, "envSetNextLoad hbmenu: 0x%x\n", rc); fflush(boot); }
    }
    if (boot) { fputs("StremNX exited\n", boot); fclose(boot); }
    return 0;
}

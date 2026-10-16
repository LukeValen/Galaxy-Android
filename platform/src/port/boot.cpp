// Brings up the emulated Wii environment and starts the game's main().
#include "port/port.h"
#include <sys/stat.h>
#include <string>

extern "C" void port_os_time_init(void);
extern "C" void port_gx_init(void);
extern "C" void port_gx_use_recorder_backend(void);
extern "C" void port_dvd_set_root(const char* root);
extern "C" void port_dvd_load_dol_data(const char* root);
extern "C" void port_nand_set_root(const char* root);

// GameSystem.cpp's main(), renamed for the port.
void port_game_main(void);

static void gameEntry() { port_game_main(); }

extern "C" void port_boot(const char* dataRoot, const char* saveRoot) {
    port_log("port_boot: data=%s save=%s", dataRoot, saveRoot);
    port_mem_init();
    port_os_time_init();
    port_dvd_set_root(dataRoot);
    const PortDisc* disc = port_dvd_disc();
    if (!disc) {
        port_fatal("%s: not the files of a Super Mario Galaxy disc the port knows (no EuEnglish, UsEnglish, JpJapanese or KrKorean "
                   "folder with the game's texts in sys/fst.bin and files/)",
                   dataRoot);
    }
    port_log("disc: %s (%s), %d languages", disc->id, disc->region, port_language_count());
    port_mem_set_disc_id(disc->id);
    port_dvd_load_dol_data(dataRoot);
    port_nand_set_root(saveRoot);

    // Auto-restore save data from persistent external mirror if internal NAND directory is empty
    const char* titles[] = {"524d4750", "524d4745", "524d474a", "524d474b"};
    for (const char* t : titles) {
        std::string internalSave = std::string(saveRoot) + "/title/00010000/" + t + "/data/GameData.bin";
        FILE* checkSave = fopen(internalSave.c_str(), "rb");
        if (!checkSave) {
            FILE* mirror = fopen("/sdcard/GalaxyData/saves/GameData.bin", "rb");
            if (mirror) {
                std::string dir = std::string(saveRoot) + "/title/00010000/" + t + "/data";
                mkdir((std::string(saveRoot) + "/title").c_str(), 0770);
                mkdir((std::string(saveRoot) + "/title/00010000").c_str(), 0770);
                mkdir((std::string(saveRoot) + "/title/00010000/" + t).c_str(), 0770);
                mkdir(dir.c_str(), 0770);
                FILE* restore = fopen(internalSave.c_str(), "wb");
                if (restore) {
                    char buf[4096];
                    size_t n;
                    while ((n = fread(buf, 1, sizeof(buf), mirror)) > 0) {
                        fwrite(buf, 1, n, restore);
                    }
                    fclose(restore);
                    port_log("nand: auto-restored GameData.bin from external mirror -> %s", internalSave.c_str());
                }
                fclose(mirror);
            }
        } else {
            fclose(checkSave);
        }
    }

    port_gx_init();
    port_gx_use_recorder_backend();
    port_os_boot(gameEntry);
}

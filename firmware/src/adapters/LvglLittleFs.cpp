#include "adapters/LvglLittleFs.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <lvgl.h>

namespace feedme::adapters {

namespace {

// LVGL passes paths like "/cats/c2_130.png" (the drive letter has been
// stripped) to open_cb. LittleFS expects the same leading slash, so
// just forward verbatim.
//
// File handles: each lv_fs_open allocates a `File` on the heap so the
// pointer stays stable across reads — the Arduino `File` object owns
// an internal stream that we can't safely move/copy. The PNG decoder
// opens a file, reads it sequentially, then closes — no concurrent
// opens — so heap allocation cost is negligible (one malloc per cat
// first-display).
void* fs_open_cb(lv_fs_drv_t*, const char* path, lv_fs_mode_t /*mode*/) {
    File* f = new (std::nothrow) File(LittleFS.open(path, "r"));
    if (!f) {
        Serial.printf("[lvfs] open '%s' — alloc failed\n", path);
        return nullptr;
    }
    if (!*f) {
        Serial.printf("[lvfs] open '%s' — LittleFS open failed\n", path);
        delete f;
        return nullptr;
    }
    return f;
}

lv_fs_res_t fs_close_cb(lv_fs_drv_t*, void* file_p) {
    File* f = static_cast<File*>(file_p);
    f->close();
    delete f;
    return LV_FS_RES_OK;
}

lv_fs_res_t fs_read_cb(lv_fs_drv_t*, void* file_p, void* buf,
                       uint32_t btr, uint32_t* br) {
    File* f = static_cast<File*>(file_p);
    *br = f->read(static_cast<uint8_t*>(buf), btr);
    return LV_FS_RES_OK;
}

lv_fs_res_t fs_seek_cb(lv_fs_drv_t*, void* file_p, uint32_t pos,
                       lv_fs_whence_t whence) {
    File* f = static_cast<File*>(file_p);
    SeekMode mode = SeekSet;
    if      (whence == LV_FS_SEEK_CUR) mode = SeekCur;
    else if (whence == LV_FS_SEEK_END) mode = SeekEnd;
    return f->seek(pos, mode) ? LV_FS_RES_OK : LV_FS_RES_FS_ERR;
}

lv_fs_res_t fs_tell_cb(lv_fs_drv_t*, void* file_p, uint32_t* pos_p) {
    File* f = static_cast<File*>(file_p);
    *pos_p = f->position();
    return LV_FS_RES_OK;
}

// Static so its lifetime spans the whole program — lv_fs holds a
// pointer to this struct after register.
lv_fs_drv_t fs_drv;

}  // namespace

void registerLvglLittleFs() {
    lv_fs_drv_init(&fs_drv);
    fs_drv.letter   = 'L';
    fs_drv.open_cb  = fs_open_cb;
    fs_drv.close_cb = fs_close_cb;
    fs_drv.read_cb  = fs_read_cb;
    fs_drv.seek_cb  = fs_seek_cb;
    fs_drv.tell_cb  = fs_tell_cb;
    lv_fs_drv_register(&fs_drv);
    Serial.println("[lvgl] LittleFS registered as drive 'L:'");
}

}  // namespace feedme::adapters

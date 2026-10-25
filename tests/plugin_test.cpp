#include "SPF_Plugin.h"
#include "SPF_Manifest_API.h"
#include "SPF_Telemetry_API.h"
#include "SPF_UI_API.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>
extern "C" bool SPF_GetManifestAPI(SPF_Manifest_API*);
namespace {
SPF_GameState game{};
SPF_Timestamps time{};
SPF_TruckData truck{};
SPF_DrawCallback draw = nullptr;
float progress = -1;
bool disclaimer = false;
SPF_Telemetry_Handle* context(const char*) { return reinterpret_cast<SPF_Telemetry_Handle*>(1); }
void get_game(SPF_Telemetry_Handle*, SPF_GameState* out, size_t) { *out = game; }
void get_time(SPF_Telemetry_Handle*, SPF_Timestamps* out, size_t) { *out = time; }
void get_truck(SPF_Telemetry_Handle*, SPF_TruckData* out, size_t) { *out = truck; }
void get_constants(SPF_Telemetry_Handle*, SPF_TruckConstants* out, size_t) { out->rpm_limit = 2200; }
void get_job(SPF_Telemetry_Handle*, SPF_JobConstants*, size_t) {}
void get_job_data(SPF_Telemetry_Handle*, SPF_JobData*, size_t) {}
void register_draw(const char*, const char*, SPF_DrawCallback cb, void*) { draw = cb; }
void text(const char* message) { if (std::strstr(message, u8"ещё не подтверждена")) disclaimer = true; }
void bar(float f, float, float, const char*) { progress = f; }
}
int main() {
    assert(!SPF_GetPlugin(nullptr));
    assert(!SPF_GetManifestAPI(nullptr));
    SPF_Plugin_Exports exports{};
    assert(SPF_GetPlugin(&exports));
    exports.OnLoad(nullptr);
    exports.OnActivated(nullptr);
    exports.OnUpdate();
    exports.OnUnload();
    SPF_Telemetry_API tel{};
    tel.Tel_GetContext = context;
    tel.Tel_GetGameState = get_game;
    tel.Tel_GetTimestamps = get_time;
    tel.Tel_GetTruckData = get_truck;
    tel.Tel_GetTruckConstants = get_constants;
    tel.Tel_GetJobConstants = get_job;
    tel.Tel_GetJobData = get_job_data;
    SPF_Core_API core{}; core.telemetry = &tel;
    SPF_UI_API ui{};
    ui.UI_RegisterDrawCallback = register_draw;
    ui.UI_Text = text; ui.UI_TextWrapped = text; ui.UI_ProgressBar = bar;
    exports.OnActivated(&core);
    exports.OnRegisterUI(&ui);
    assert(draw);
    std::memcpy(game.game_id, "ats", 4);
    truck.engine_enabled = true; truck.engine_rpm = 1500;
    truck.effective_throttle = 1; truck.displayed_gear = 5;
    for (int i=0; i<180; ++i) { time.paused_simulation += 16667; exports.OnUpdate(); }
    draw(&ui, nullptr);
    assert(disclaimer && progress > 0 && progress <= 1 && std::isfinite(progress));
    game.paused = true; exports.OnUpdate(); draw(&ui,nullptr); assert(progress == 0);
    game.paused = false;
    time.paused_simulation = 1; exports.OnUpdate(); draw(&ui,nullptr); assert(progress == 0);
    truck.engine_rpm = std::numeric_limits<float>::quiet_NaN();
    time.paused_simulation += 16667; exports.OnUpdate(); draw(&ui,nullptr); assert(progress == 0);
    exports.OnWorldUnloaded(); draw(&ui,nullptr); assert(progress == 0);
    exports.OnUnload(); exports.OnUpdate();
}

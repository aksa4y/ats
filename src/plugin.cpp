#include "SPF_Plugin.h"
#include "SPF_Manifest_API.h"
#include "SPF_Telemetry_API.h"
#include "SPF_UI_API.h"
#include "SPF_Logger_API.h"
#include "exhaust_model.hpp"
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
constexpr auto name = "ATSExhaust";
const SPF_Core_API* core = nullptr;
SPF_Telemetry_Handle* telemetry = nullptr;
modguard::ExhaustModel model;
std::uint64_t previous = 0;
std::atomic<float> demand{0.0F};
std::atomic<bool> ready{false};
void reset() { model.reset(); previous = 0; demand.store(0.0F); ready.store(false); }
void load(const SPF_Load_API* api) {
    reset();
    if (api && api->logger) {
        auto h = api->logger->Log_GetContext(name);
        if (h) api->logger->Log(h, SPF_LOG_WARN,
            "ATSExhaust: diagnostic preview. Windows particle backend is NOT implemented; no visual smoke control.");
    }
}
void activate(const SPF_Core_API* api) {
    core = api;
    telemetry = core && core->telemetry ? core->telemetry->Tel_GetContext(name) : nullptr;
    reset();
}
void unload() { reset(); telemetry = nullptr; core = nullptr; }
void update() {
    if (!core || !core->telemetry || !telemetry) { reset(); return; }
    auto* api = core->telemetry;
    SPF_GameState game{};
    SPF_Timestamps time{};
    api->Tel_GetGameState(telemetry, &game, sizeof(game));
    api->Tel_GetTimestamps(telemetry, &time, sizeof(time));
    if (std::strncmp(game.game_id, "ats", sizeof(game.game_id)) != 0 || game.paused) { reset(); return; }
    // SCS simulation timestamps are microseconds. Paused simulation excludes pauses.
    const auto now = time.paused_simulation;
    if (!previous || now < previous || now - previous > 1000000) { reset(); previous = now; return; }
    if (now == previous) return;
    const float dt = static_cast<float>(now - previous) / 1000000.0F;
    previous = now;
    SPF_TruckData truck{};
    SPF_TruckConstants constants{};
    SPF_JobConstants job{};
    api->Tel_GetTruckData(telemetry, &truck, sizeof(truck));
    api->Tel_GetTruckConstants(telemetry, &constants, sizeof(constants));
    api->Tel_GetJobConstants(telemetry, &job, sizeof(job));
    modguard::ExhaustInputs input;
    input.engine_enabled = truck.engine_enabled;
    input.throttle = truck.effective_throttle;
    input.clutch = truck.effective_clutch;
    input.rpm = truck.engine_rpm;
    input.rpm_limit = constants.rpm_limit > 0 ? constants.rpm_limit : 2500.0F;
    input.speed = truck.speed;
    input.gear = truck.displayed_gear;
    input.acceleration = -truck.local_linear_acceleration.z;
    input.cargo_mass = job.cargo_mass;
    input.cargo_loaded = job.is_cargo_loaded;
    for (const float v : {input.throttle, input.clutch, input.rpm, input.rpm_limit,
                         input.speed, input.acceleration, input.cargo_mass}) {
        if (!std::isfinite(v)) { reset(); return; }
    }
    demand.store(model.update(input, dt).demand);
    ready.store(true);
}
void draw(SPF_UI_API* ui, void*) {
    if (!ui) return;
    ui->UI_TextWrapped(u8"ATSExhaust — диагностика выхлопа");
    ui->UI_TextWrapped(u8"Управление видимым дымом НЕ реализовано. Нужен проверенный Windows-модуль частиц для вашей сборки ATS.");
    ui->UI_Text(ready.load() ? u8"Телеметрия поступает" : u8"Ожидание поездки в ATS");
    ui->UI_ProgressBar(demand.load(), -1.0F, 20.0F, u8"Расчётная интенсивность");
    ui->UI_TextWrapped(u8"update.bat получает опубликованные обновления. Он не создаёт поддержку неизвестной версии игры.");
}
void register_ui(SPF_UI_API* ui) {
    if (ui) ui->UI_RegisterDrawCallback(name, "Status", draw, nullptr);
}
void manifest(SPF_Manifest_Builder_Handle* h, const SPF_Manifest_Builder_API* api) {
    if (!h || !api) return;
    api->Info_SetName(h, name);
    api->Info_SetVersion(h, "0.1.0");
    api->Info_SetMinFrameworkVersion(h, "1.2.6");
    api->Info_SetAuthor(h, "aksa4y");
    api->Info_SetGithubUrl(h, "https://github.com/aksa4y/ats");
    api->Info_SetDescriptionLiteral(h, "Exhaust telemetry preview. Visual smoke backend unavailable.");
    api->Defaults_SetLogging(h, "info", true);
    api->Defaults_AddWindow(h, "Status", true, true, 80, 80, 540, 240, false, false);
}
}
extern "C" SPF_PLUGIN_EXPORT bool SPF_GetManifestAPI(SPF_Manifest_API* api) {
    if (!api) return false;
    api->BuildManifest = manifest;
    return true;
}
extern "C" SPF_PLUGIN_EXPORT bool SPF_GetPlugin(SPF_Plugin_Exports* exports) {
    if (!exports) return false;
    *exports = {};
    exports->OnLoad = load;
    exports->OnActivated = activate;
    exports->OnUnload = unload;
    exports->OnUpdate = update;
    exports->OnRegisterUI = register_ui;
    exports->OnGameWorldReady = reset;
    exports->OnWorldUnloaded = reset;
    return true;
}

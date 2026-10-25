#pragma once
#include <cstdint>
struct SPF_Core_API;
namespace ats_exhaust {
enum class BackendState { unavailable, unsupported_build, bytes_mismatch, waiting, active, disabled, failed };
struct ParticleStats { std::uint64_t matched, admitted, suppressed; };
void initialize_particles(const SPF_Core_API* api);
void shutdown_particles();
void set_smoke_demand(float demand);
BackendState particle_state();
ParticleStats particle_stats();
}

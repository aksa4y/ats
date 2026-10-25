#include "particle_backend.hpp"
#include "particle_logic.hpp"
#include "SPF_Plugin.h"
#include "SPF_Logger_API.h"
#include <atomic>
#include <algorithm>
#include <string>
#if defined(_WIN32) && defined(_MSC_VER) && defined(_M_X64)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <intrin.h>
#include <vector>
#endif

namespace ats_exhaust {
namespace {
static_assert(std::atomic<float>::is_always_lock_free);
static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
std::atomic<float> smoke_demand{0.0F};
std::atomic<bool> filtering{false};
std::atomic<std::uint64_t> matched{0}, accepted{0}, suppressed{0};
std::atomic<std::uint32_t> sequence{0};
std::atomic<BackendState> state{BackendState::unavailable};
const SPF_Core_API* services = nullptr;
SPF_Hook_Handle* hook = nullptr;
[[maybe_unused]] void log(SPF_LogLevel level, const char* text) {
    if (services && services->logger) {
        auto* context=services->logger->Log_GetContext("ATSExhaust");
        if (context) services->logger->Log(context,level,text);
    }
}
#if defined(_WIN32) && defined(_MSC_VER) && defined(_M_X64)
using Allocate = void* (*)(void*);
Allocate original = nullptr;
std::uintptr_t image_base = 0;

// This function contains no C++ objects with destructors. SEH protects reads from
// unrelated containers that share the allocator. Never write into these objects.
bool inspect_container(void* container, ParticleFields* fields) {
    __try {
        auto address=reinterpret_cast<std::uintptr_t>(container);
        if (address<container_offset ||
            *reinterpret_cast<const std::uintptr_t*>(address) != image_base+container_vtable_rva) return false;
        *fields=read_fields(reinterpret_cast<const unsigned char*>(address-container_offset));
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
__declspec(noinline) void* allocate_detour(void* container) {
    const auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    // Keep all allocation paths except the verified particle-spawn call site unchanged.
    if (filtering.load(std::memory_order_acquire) && caller==image_base+caller_return_rva) {
        ParticleFields fields{};
        if (inspect_container(container,&fields) && eligible(fields,image_base,caller)) {
            matched.fetch_add(1,std::memory_order_relaxed);
            const auto n=sequence.fetch_add(1,std::memory_order_relaxed);
            if (!admit(smoke_demand.load(std::memory_order_relaxed),n)) {
                suppressed.fetch_add(1,std::memory_order_relaxed);
                return nullptr; // Caller tests RAX before linking/writing the particle node.
            }
            accepted.fetch_add(1,std::memory_order_relaxed);
        }
    }
    return original(container);
}
std::string executable_hash() {
    std::vector<wchar_t> path(32768);
    const DWORD length=GetModuleFileNameW(nullptr,path.data(),static_cast<DWORD>(path.size()));
    if (!length || length>=path.size()) return {};
    HANDLE file=CreateFileW(path.data(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if (file==INVALID_HANDLE_VALUE) return {};
    BCRYPT_ALG_HANDLE algorithm=nullptr;
    BCRYPT_HASH_HANDLE hash=nullptr;
    unsigned char digest[32]{};
    unsigned char buffer[65536];
    bool ok=BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0;
    if (ok) ok=BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)>=0;
    while (ok) {
        DWORD bytes=0;
        if (!ReadFile(file,buffer,sizeof(buffer),&bytes,nullptr)) { ok=false; break; }
        if (!bytes) break;
        ok=BCryptHashData(hash,buffer,bytes,0)>=0;
    }
    if (ok) ok=BCryptFinishHash(hash,digest,sizeof(digest),0)>=0;
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm,0);
    CloseHandle(file);
    if (!ok) return {};
    const char digits[]="0123456789abcdef";
    std::string result;
    for (const auto byte : digest) { result+=digits[byte>>4]; result+=digits[byte&15]; }
    return result;
}
bool runtime_bytes_match() {
    __try {
        const auto* base=reinterpret_cast<const unsigned char*>(image_base);
        const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 || dos->e_lfanew>0x1000) return false;
        const auto* pe=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
        if (pe->Signature!=IMAGE_NT_SIGNATURE || pe->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64 ||
            pe->OptionalHeader.SizeOfImage!=0x3975000) return false;
        if (std::memcmp(base+allocator_rva,allocator_bytes.data(),allocator_bytes.size())!=0) return false;
        const unsigned char call[]={0xff,0x50,0x08};
        const unsigned char null_guard[]={0x48,0x85,0xc0,0x74,0x4a};
        if (std::memcmp(base+caller_return_rva-3,call,sizeof(call))!=0 ||
            std::memcmp(base+0x1584457,null_guard,sizeof(null_guard))!=0) return false;
        return *reinterpret_cast<const std::uintptr_t*>(base+container_vtable_rva+8)==image_base+allocator_rva;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
#endif
}
void initialize_particles(const SPF_Core_API* api) {
    filtering.store(false);
    services=api;
    state.store(BackendState::unavailable);
#if defined(_WIN32) && defined(_MSC_VER) && defined(_M_X64)
    if (!api || !api->hooks || !api->hooks->Hook_Register || !api->hooks->Hook_FindPattern) return;
    if (executable_hash()!=game_sha256) {
        state.store(BackendState::unsupported_build);
        log(SPF_LOG_WARN,"Unsupported amtrucks.exe SHA-256. Particle hook not installed."); return;
    }
    image_base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if (!runtime_bytes_match() || api->hooks->Hook_FindPattern(allocator_signature)!=image_base+allocator_rva) {
        state.store(BackendState::bytes_mismatch);
        log(SPF_LOG_WARN,"Particle target bytes differ, or another mod already hooks this function. Hook not installed."); return;
    }
    matched.store(0); accepted.store(0); suppressed.store(0); sequence.store(0);
    // SPF owns MinHook installation and removes this hook before unloading the DLL.
    hook=api->hooks->Hook_Register("ATSExhaust","SmokeAllocation","ATS exhaust smoke",
        reinterpret_cast<void*>(&allocate_detour),reinterpret_cast<void**>(&original),allocator_signature,true);
    if (!hook) { state.store(BackendState::failed); return; }
    filtering.store(true,std::memory_order_release);
    state.store(BackendState::waiting);
    log(SPF_LOG_INFO,"ATS 1.61.3.18 Windows particle controller registered. Compatible smoke emitters required. In-game validation pending.");
#else
    (void)api;
#endif
}
void shutdown_particles() {
    filtering.store(false,std::memory_order_release);
    smoke_demand.store(0.0F);
    // Keep the trampoline valid until SPF removes the hook after OnUnload.
    services=nullptr; hook=nullptr; state.store(BackendState::unavailable);
}
void set_smoke_demand(float value) {
    smoke_demand.store(std::isfinite(value)?std::clamp(value,0.0F,1.0F):0.0F,std::memory_order_relaxed);
}
BackendState particle_state() {
    if (hook && services && services->hooks) {
        if (!services->hooks->Hook_IsEnabled(hook)) return BackendState::disabled;
        return services->hooks->Hook_IsInstalled(hook)?BackendState::active:BackendState::waiting;
    }
    return state.load();
}
ParticleStats particle_stats() { return {matched.load(),accepted.load(),suppressed.load()}; }
}

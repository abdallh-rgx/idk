// ============================================================
// Hook sub_55B8AB0 — تتبّع GameState
// ============================================================
#define ADDR_GetGameState  0x055B8AB0

typedef void* (*GetGameState_t)(void* gameMode);
static GetGameState_t g_orig_GetGameState = nullptr;
static uint64_t g_call_count = 0;

static void* Hook_GetGameState(void* gameMode) {
    g_call_count++;
    void* result = g_orig_GetGameState(gameMode);
    
    // اطبع أول 10 مرات + كل 100
    if (g_call_count <= 10 || g_call_count % 100 == 0) {
        LOGF("[Diag #%llu] sub_55B8AB0(GameMode=%p) → GameState=%p",
             (unsigned long long)g_call_count, gameMode, result);
        
        if (result) {
            uint8_t matchState = *(uint8_t*)((char*)result + 0x2150);
            LOGF("         MatchState = %d", matchState);
        }
    }
    
    return result;
}

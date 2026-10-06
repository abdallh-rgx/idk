#define _GNU_SOURCE
#include <jni.h>
#include <pthread.h>
#include <dlfcn.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <android/log.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

using int8  = int8_t;
using int16 = int16_t;
using int32 = int32_t;
using int64 = int64_t;
using uint8 = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
using uint64 = uint64_t;
using TCHAR = char16_t;

static FILE* g_logFile = nullptr;
static void InitLogFile() {
    const char* paths[] = {
        "/storage/emulated/0/Android/data/com.epicgames.fortnite/files/era_log.txt",
        "/sdcard/Android/data/com.epicgames.fortnite/files/era_log.txt",
        "/data/local/tmp/era_log.txt",
        "/sdcard/era_log.txt", nullptr
    };
    for (int i = 0; paths[i]; ++i) { g_logFile = fopen(paths[i], "w"); if (g_logFile) break; }
}
#define LOGI(...) do { __android_log_print(ANDROID_LOG_INFO,"EraCH3",__VA_ARGS__); if(g_logFile){fprintf(g_logFile,"[I] ");fprintf(g_logFile,__VA_ARGS__);fprintf(g_logFile,"\n");fflush(g_logFile);} } while(0)
#define LOGE(...) do { __android_log_print(ANDROID_LOG_ERROR,"EraCH3",__VA_ARGS__); if(g_logFile){fprintf(g_logFile,"[E] ");fprintf(g_logFile,__VA_ARGS__);fprintf(g_logFile,"\n");fflush(g_logFile);} } while(0)

// ============================================================
// Dobby (inline)
// ============================================================
using addr_t = uintptr_t;
using addr32_t = uint32_t;
using addr64_t = uint64_t;

typedef union _FPReg { __int128_t q; struct { double d1, d2; } d; struct { float f1,f2,f3,f4; } f; } FPReg;
typedef struct {
  uint64_t dmmpy_0; uint64_t sp; uint64_t dmmpy_1;
  union { uint64_t x[29]; struct { uint64_t x0,x1,x2,x3,x4,x5,x6,x7,x8,x9,x10,x11,x12,x13,x14,x15,x16,x17,x18,x19,x20,x21,x22,x23,x24,x25,x26,x27,x28; } regs; } general;
  uint64_t fp; uint64_t lr;
  union { FPReg q[32]; } floating;
} DobbyRegisterContext;

#define ARM64_TMP_REG_NDX_0 17
#define ALIGN_FLOOR(address, range) ((uintptr_t)(address) & ~((uintptr_t)(range) - 1))
#define ALIGN_CEIL(address, range) (((uintptr_t)(address) + (uintptr_t)(range) - 1) & ~((uintptr_t)(range) - 1))
#define ALIGN(address, range) ALIGN_FLOOR(address, range)
#define LeftShift(a,b,c) (((a) & ((1 << (b)) - 1)) << (c))
#define RightShift(a,b,c) (((a) >> (c)) & ((1 << (b)) - 1))
#define submask(x) ((1L << ((x) + 1)) - 1)
#define bits_(obj, st, fn) (((obj) >> (st)) & submask((fn) - (st)))
#define bit_(obj, st) (((obj) >> (st)) & 1)
#define set_bit_(obj, st, b) (obj) = ((~(1 << (st))) & (obj)) | ((b) << (st))
#define set_bits_(obj, st, fn, b) (obj) = ((~(submask((fn) - (st)) << (st))) & (obj)) | ((b) << (st))
#define TMP_REG_0 17
#define kRdShift 0
#define kRnShift 5
#define kRtShift 0
#define kRmShift 16
#define Rd_(rd) ((rd) << kRdShift)
#define Rt_(rt) ((rt) << kRtShift)
#define Rn_(rn) ((rn) << kRnShift)
#define Rm_(rm) ((rm) << kRmShift)
#define UnconditionalBranchFixed 0x14000000
#define UnconditionalBranchFixedMask 0x7C000000
#define UnconditionalBranchMask 0xFC000000
#define B_ 0x14000000
#define BL 0x94000000
#define BR 0xD61F0000
#define BLR 0xD63F0000
#define RET 0xD65F0000
#define LoadRegLiteralFixed 0x18000000
#define LoadRegLiteralFixedMask 0x3B000000
#define LoadRegLiteralMask 0xFF000000
#define PCRelAddressingFixed 0x10000000
#define PCRelAddressingFixedMask 0x1F000000
#define PCRelAddressingMask 0x9F000000
#define ADR 0x10000000
#define ADRP 0x90000000
#define CompareBranchFixed 0x34000000
#define CompareBranchFixedMask 0x7E000000
#define CompareBranchMask 0xFF000000
#define ConditionalBranchFixed 0x54000000
#define ConditionalBranchFixedMask 0xFE000000
#define ConditionalBranchMask 0xFF000010
#define TestBranchFixed 0x36000000
#define TestBranchFixedMask 0x7E000000
#define TestBranchMask 0x7F000000
#define MoveWideImmediateFixed 0x12800000
#define MOVZ 0x40000000
#define MOVK 0x60000000
#define MOVN 0x00000000
#define SixtyFourBits 0x80000000
#define AddSubImmediateFixed 0x11000000
#define ADD_X_IMM 0x91000000
#define LoadStoreUnsignedOffsetFixed 0x39000000
#define LDR_X 0xF9400000
#define LDR_W 0xB9400000
#define STR_X 0xF9000000
#define LogicalShiftedFixed 0x0A000000
#define ORR 0x20000000
#define NOP 0xD503201F
#define kNoAccess 0
#define kRead 1
#define kWrite 2
#define kExecute 4
#define kReadExecute (kRead | kExecute)
#define kReadWrite (kRead | kWrite)
#define MEM_PERM_R 0x1
#define MEM_PERM_W 0x2
#define MEM_PERM_X 0x4
#define ARM64_B_XXX_RANGE ((1ULL << 25) << 2)

static inline int64_t SignExtend(unsigned long x, int M, int N) {
  char sb = bit_(x, M-1); unsigned long sm = 0 - sb; x |= ((sm >> M) << M); return (int64_t)x;
}
static inline int64_t decode_imm14_offset(uint32_t instr) { int64_t i = bits_(instr,5,18); return SignExtend(i<<2, 16, 64); }
static inline int64_t decode_imm19_offset(uint32_t instr) { int64_t i = bits_(instr,5,23); return SignExtend(i<<2, 21, 64); }
static inline uint32_t encode_imm19_offset(uint32_t instr, int64_t offset) { uint32_t i = bits_((offset>>2),0,18); set_bits_(instr,5,23,i); return instr; }
static inline int64_t decode_imm26_offset(uint32_t instr) { int64_t i = bits_(instr,0,25); return SignExtend(i<<2, 28, 64); }
static inline int64_t decode_immhi_immlo_offset(uint32_t instr) {
  uint32_t lo = bits_(instr,29,30); uint32_t hi = bits_(instr,5,23);
  int64_t i = (int64_t)lo | ((int64_t)hi<<2); return SignExtend(i, 21, 64);
}
static inline int64_t decode_immhi_immlo_zero12_offset(uint32_t instr) { return decode_immhi_immlo_offset(instr) << 12; }
static inline int decode_rt(uint32_t instr) { return bits_(instr,0,4); }
static inline int decode_rd(uint32_t instr) { return bits_(instr,0,4); }
static inline int inst_is_b_bl(uint32_t i) { return (i & UnconditionalBranchFixedMask) == UnconditionalBranchFixed; }
static inline int inst_is_ldr_literal(uint32_t i) { return (i & LoadRegLiteralFixedMask) == LoadRegLiteralFixed; }
static inline int inst_is_adr(uint32_t i) { return (i & PCRelAddressingFixedMask) == PCRelAddressingFixed && (i & PCRelAddressingMask) == ADR; }
static inline int inst_is_adrp(uint32_t i) { return (i & PCRelAddressingFixedMask) == PCRelAddressingFixed && (i & PCRelAddressingMask) == ADRP; }
static inline int inst_is_b_cond(uint32_t i) { return (i & ConditionalBranchFixedMask) == ConditionalBranchFixed; }
static inline int inst_is_compare_b(uint32_t i) { return (i & CompareBranchFixedMask) == CompareBranchFixed; }
static inline int inst_is_test_b(uint32_t i) { return (i & TestBranchFixedMask) == TestBranchFixed; }
static int os_page_size(void) { return (int)sysconf(_SC_PAGESIZE); }

static void clear_cache_arm64(void* start, void* end) {
  uint64_t xs = (uint64_t)(uintptr_t)start, xe = (uint64_t)(uintptr_t)end;
  static uint64_t ctr = 0; if (!ctr) __asm__ __volatile__("mrs %0, ctr_el0" : "=r"(ctr));
  uint64_t a;
  if (((ctr>>28)&1)==0) { size_t l = 4 << ((ctr>>16)&15); for (a=xs&~(l-1); a<xe; a+=l) __asm__ __volatile__("dc cvau, %0"::"r"(a)); }
  __asm__ __volatile__("dsb ish");
  if (((ctr>>29)&1)==0) { size_t l = 4 << ((ctr>>0)&15); for (a=xs&~(l-1); a<xe; a+=l) __asm__ __volatile__("ic ivau, %0"::"r"(a)); __asm__ __volatile__("dsb ish"); }
  __asm__ __volatile__("isb sy");
}

typedef struct { addr_t start_; size_t size; } MemRange;
static inline void MemRange_init(MemRange* r, addr_t s, size_t sz) { r->start_=s; r->size=sz; }
static inline addr_t MemRange_addr(const MemRange* r){return r->start_;}
static MemRange MemRange_intersect(const MemRange* a, const MemRange* b) {
  MemRange r; addr_t s = a->start_>b->start_?a->start_:b->start_;
  addr_t e = (a->start_+a->size)<(b->start_+b->size)?(a->start_+a->size):(b->start_+b->size);
  if (s<e) MemRange_init(&r,s,e-s); else MemRange_init(&r,0,0); return r;
}
typedef MemRange MemBlock;
static inline void MemBlock_init(MemBlock* b, addr_t s, size_t sz){ b->start_=s; b->size=sz; }

typedef struct { uint8_t* buffer; uint32_t buffer_size; uint32_t buffer_capacity; } MemBuffer;
static void MemBuffer_init(MemBuffer* b){ b->buffer_capacity=64; b->buffer=(uint8_t*)malloc(64); b->buffer_size=0; }
static void MemBuffer_destroy(MemBuffer* b){ if(b->buffer){free(b->buffer);b->buffer=NULL;} b->buffer_size=0; b->buffer_capacity=0; }
static inline uint8_t* MemBuffer_data(MemBuffer* b){return b->buffer;}
static inline uint32_t MemBuffer_size(MemBuffer* b){return b->buffer_size;}
static void MemBuffer_ensure(MemBuffer* b, uint32_t s){ if(b->buffer_size+s>b->buffer_capacity){ uint32_t nc=b->buffer_capacity*2; while(nc<b->buffer_size+s)nc*=2; uint8_t* nb=(uint8_t*)malloc(nc); memcpy(nb,b->buffer,b->buffer_size); free(b->buffer); b->buffer=nb; b->buffer_capacity=nc; } }
static void MemBuffer_emit(MemBuffer* b, const void* d, uint32_t s){ MemBuffer_ensure(b,s); memcpy(b->buffer+b->buffer_size,d,s); b->buffer_size+=s; }
static void MemBuffer_emit_u32(MemBuffer* b, uint32_t v){ MemBuffer_emit(b,&v,4); }
static void MemBuffer_emit_u64(MemBuffer* b, uint64_t v){ MemBuffer_emit(b,&v,8); }
static void MemBuffer_rewrite_inst(MemBuffer* b, uint32_t off, uint32_t inst){ *(uint32_t*)(b->buffer+off)=inst; }
static uint32_t MemBuffer_load_inst(MemBuffer* b, uint32_t off){ return *(uint32_t*)(b->buffer+off); }
static MemBlock MemBuffer_dup(MemBuffer* b){ uint8_t* c=(uint8_t*)malloc(b->buffer_size); memcpy(c,b->buffer,b->buffer_size); MemBlock m; MemBlock_init(&m,(addr_t)c,b->buffer_size); return m; }

typedef struct { uint8_t* buffer; uint32_t size; uint32_t capacity; uint32_t builtin_alignment; } simple_linear_allocator_t;
static void sla_init(simple_linear_allocator_t* a, uint8_t* b, uint32_t c, uint32_t al){ a->buffer=b; a->capacity=c; a->builtin_alignment=al?al:1; a->size=0; }
static uint8_t* sla_cursor(simple_linear_allocator_t* a){ return a->buffer+a->size; }
static uint8_t* sla_alloc(simple_linear_allocator_t* a, uint32_t s, uint32_t al){
  uint32_t align = al?al:a->builtin_alignment;
  uint32_t gap = ALIGN_CEIL((uintptr_t)sla_cursor(a),align)-(uintptr_t)sla_cursor(a);
  a->size += gap;
  if (a->size+s>a->capacity) return NULL;
  uint8_t* d = sla_cursor(a); a->size += s; return d;
}
#define MAX_PAGE_ALLOCATORS 64
static struct { simple_linear_allocator_t* code[MAX_PAGE_ALLOCATORS]; int code_n; simple_linear_allocator_t* data[MAX_PAGE_ALLOCATORS]; int data_n; } gMemAlloc;

static void* os_mem_alloc(size_t s, int ac){ int p=0; if(ac&kRead)p|=PROT_READ; if(ac&kWrite)p|=PROT_WRITE; if(ac&kExecute)p|=PROT_EXEC; void* r=mmap(NULL,s,p,MAP_PRIVATE|MAP_ANONYMOUS,-1,0); return r==MAP_FAILED?NULL:r; }
static int os_mem_perm(void* a, size_t s, int ac){ int p=0; if(ac&kRead)p|=PROT_READ; if(ac&kWrite)p|=PROT_WRITE; if(ac&kExecute)p|=PROT_EXEC; return mprotect(a,s,p)==0; }
static MemBlock alloc_mem_block(size_t sz, int exec){
  if (sz > (size_t)os_page_size()) { MemBlock b; MemBlock_init(&b,0,0); return b; }
  uint8_t* res = NULL; int cnt; simple_linear_allocator_t** als; int* cp;
  if (exec) { cnt=gMemAlloc.code_n; als=gMemAlloc.code; cp=&gMemAlloc.code_n; }
  else { cnt=gMemAlloc.data_n; als=gMemAlloc.data; cp=&gMemAlloc.data_n; }
  for (int i=0;i<cnt;i++){ res = sla_alloc(als[i],(uint32_t)sz,0); if(res) break; }
  if (!res) {
    if (*cp>=MAX_PAGE_ALLOCATORS){ MemBlock b; MemBlock_init(&b,0,0); return b; }
    void* pg = os_mem_alloc(os_page_size(), kNoAccess);
    if (!pg){ MemBlock b; MemBlock_init(&b,0,0); return b; }
    os_mem_perm(pg, os_page_size(), exec?kReadExecute:kReadWrite);
    simple_linear_allocator_t* pa=(simple_linear_allocator_t*)malloc(sizeof(simple_linear_allocator_t));
    sla_init(pa,(uint8_t*)pg,os_page_size(),8); als[*cp]=pa; (*cp)++;
    res = sla_alloc(pa,(uint32_t)sz,0);
  }
  MemBlock b; MemBlock_init(&b,(addr_t)res,sz); return b;
}
static MemBlock alloc_exec_block(size_t s){ return alloc_mem_block(s,1); }
static MemBlock alloc_data_block(size_t s){ return alloc_mem_block(s,0); }

static int DobbyCodePatch(void* address, uint8_t* buffer, uint32_t buffer_size){
  int ps = os_page_size();
  uintptr_t pp = ALIGN_FLOOR(address,ps), pe = ALIGN_FLOOR((uintptr_t)address+buffer_size,ps);
  mprotect((void*)pp,ps,PROT_READ|PROT_WRITE|PROT_EXEC);
  if (pp!=pe) mprotect((void*)pe,ps,PROT_READ|PROT_WRITE|PROT_EXEC);
  memcpy(address,buffer,buffer_size);
  mprotect((void*)pp,ps,PROT_READ|PROT_EXEC);
  if (pp!=pe) mprotect((void*)pe,ps,PROT_READ|PROT_EXEC);
  addr_t cs=(addr_t)address; clear_cache_arm64((void*)cs,(void*)(cs+buffer_size)); return 0;
}
static void make_memory_readable(void* a, size_t s){ void* p=(void*)ALIGN_FLOOR(a,os_page_size()); os_mem_perm(p,os_page_size(),kReadExecute); }

typedef struct RelocDataLabel { addr_t pos; uint8_t data[8]; uint8_t data_size; struct { int lt; uintptr_t io; } ref[32]; int ref_n; } RelocDataLabel;
static void RDL_init(RelocDataLabel* l, uint64_t d, uint8_t sz){ l->pos=0; memcpy(l->data,&d,sz); l->data_size=sz; l->ref_n=0; }
static void RDL_link(RelocDataLabel* l, int lt, uint32_t off){ if(l->ref_n<32){l->ref[l->ref_n].lt=lt; l->ref[l->ref_n].io=off; l->ref_n++;} }
static void RDL_fixup(RelocDataLabel* l, MemBuffer* buf){
  for (int i=0;i<l->ref_n;i++){
    int64_t fo = (int64_t)l->pos - (int64_t)l->ref[i].io;
    uint32_t inst = MemBuffer_load_inst(buf,(uint32_t)l->ref[i].io);
    MemBuffer_rewrite_inst(buf,(uint32_t)l->ref[i].io,encode_imm19_offset(inst,fo));
  }
}

typedef struct { addr_t fixed_addr; MemBuffer code_buffer; RelocDataLabel* data_labels[64]; int data_labels_count; } Assembler;
static void Asm_init(Assembler* a, addr_t fa){ a->fixed_addr=fa; MemBuffer_init(&a->code_buffer); a->data_labels_count=0; }
static void Asm_destroy(Assembler* a){ for(int i=0;i<a->data_labels_count;i++) free(a->data_labels[i]); MemBuffer_destroy(&a->code_buffer); }
static inline size_t Asm_pc(Assembler* a){ return MemBuffer_size(&a->code_buffer); }
static void Asm_u32(Assembler* a, uint32_t v){ MemBuffer_emit_u32(&a->code_buffer,v); }
static void Asm_u64(Assembler* a, uint64_t v){ MemBuffer_emit_u64(&a->code_buffer,v); }
static void Asm_nop(Assembler* a){ Asm_u32(a,NOP); }
static void Asm_ret(Assembler* a){ Asm_u32(a,RET); }
static void Asm_br(Assembler* a, int rn){ Asm_u32(a,BR|Rn_(rn)); }
static void Asm_blr(Assembler* a, int rn){ Asm_u32(a,BLR|Rn_(rn)); }
static void Asm_b_imm(Assembler* a, int64_t imm){ int32_t i=bits_((imm>>2),0,25); Asm_u32(a,B_|i); }
static void Asm_adrp(Assembler* a, int rd, int64_t imm){ uint32_t lo=LeftShift(bits_((imm>>12),0,1),2,29); uint32_t hi=LeftShift(bits_((imm>>12),2,20),19,5); Asm_u32(a,ADRP|Rd_(rd)|lo|hi); }
static void Asm_add_x(Assembler* a, int rd, int rn, int64_t imm){ uint32_t i=LeftShift(imm,12,10); Asm_u32(a,ADD_X_IMM|Rd_(rd)|Rn_(rn)|i); }
static void Asm_ldr_lit_x(Assembler* a, int rt, int64_t imm){ uint32_t e=0x58000000|LeftShift((imm>>2),26,5)|Rt_(rt); Asm_u32(a,e); }
static void Asm_ldr_x(Assembler* a, int rt, int rn, int64_t off){ uint32_t i=(uint32_t)(off>>3); Asm_u32(a,LoadStoreUnsignedOffsetFixed|LDR_X|LeftShift(i,12,10)|Rn_(rn)|Rt_(rt)); }
static void Asm_ldr_w(Assembler* a, int rt, int rn, int64_t off){ uint32_t i=(uint32_t)(off>>2); Asm_u32(a,LoadStoreUnsignedOffsetFixed|LDR_W|LeftShift(i,12,10)|Rn_(rn)|Rt_(rt)); }
static void Asm_str_x(Assembler* a, int rt, int rn, int64_t off){ uint32_t i=(uint32_t)(off>>3); Asm_u32(a,LoadStoreUnsignedOffsetFixed|STR_X|LeftShift(i,12,10)|Rn_(rn)|Rt_(rt)); }
static void Asm_movz(Assembler* a, int rd, uint64_t imm, int sh){ if(sh>0)sh/=16; else sh=0; uint32_t i=LeftShift(imm,16,5); Asm_u32(a,MoveWideImmediateFixed|MOVZ|SixtyFourBits|LeftShift(sh,2,21)|i|Rd_(rd)); }
static void Asm_movk(Assembler* a, int rd, uint64_t imm, int sh){ if(sh>0)sh/=16; else sh=0; uint32_t i=LeftShift(imm,16,5); Asm_u32(a,MoveWideImmediateFixed|MOVK|SixtyFourBits|LeftShift(sh,2,21)|i|Rd_(rd)); }
static void Asm_orr_x(Assembler* a, int rd, int rn, int rm){ Asm_u32(a,LogicalShiftedFixed|ORR|SixtyFourBits|Rm_(rm)|Rn_(rn)|Rd_(rd)); }
static void Asm_mov_reg(Assembler* a, int rd, int rn){ Asm_orr_x(a,rd,rn,31); }
static void Asm_mov_imm64(Assembler* a, int rd, uint64_t imm){
  uint32_t w0=(uint32_t)imm, w1=(uint32_t)(imm>>32);
  Asm_movz(a,rd,(uint16_t)w0,0); Asm_movk(a,rd,(uint16_t)(w0>>16),16);
  Asm_movk(a,rd,(uint16_t)w1,32); Asm_movk(a,rd,(uint16_t)(w1>>16),48);
}
static void Asm_adrp_add(Assembler* a, int rd, uint64_t from, uint64_t to){
  uint64_t fp=ALIGN(from,0x1000), tp=ALIGN(to,0x1000), toff=to%0x1000;
  Asm_adrp(a,rd,(int64_t)(tp-fp)); Asm_add_x(a,rd,rd,(int64_t)toff);
}
static RelocDataLabel* Asm_create_label(Assembler* a, uint64_t d, uint8_t sz){
  RelocDataLabel* l=(RelocDataLabel*)malloc(sizeof(RelocDataLabel)); RDL_init(l,d,sz);
  if (a->data_labels_count<64) a->data_labels[a->data_labels_count++]=l; return l;
}
static void Asm_ldr_label(Assembler* a, int rt, RelocDataLabel* l){ RDL_link(l,0,(uint32_t)Asm_pc(a)); Asm_ldr_lit_x(a,rt,0); }
static void Asm_lit_branch(Assembler* a, uint64_t addr){ RelocDataLabel* l=Asm_create_label(a,addr,8); Asm_ldr_label(a,TMP_REG_0,l); Asm_br(a,TMP_REG_0); }
static void Asm_reloc_labels(Assembler* a){
  for (int i=0;i<a->data_labels_count;i++){ RelocDataLabel* l=a->data_labels[i]; l->pos=Asm_pc(a); if(l->ref_n>0) RDL_fixup(l,&a->code_buffer); MemBuffer_emit(&a->code_buffer,l->data,l->data_size); }
}
static MemBlock Asm_finalize(Assembler* a){
  size_t bs=MemBuffer_size(&a->code_buffer); MemBlock blk=alloc_exec_block(bs);
  if (MemRange_addr(&blk)==0){ MemBlock b; MemBlock_init(&b,0,0); return b; }
  a->fixed_addr=MemRange_addr(&blk);
  DobbyCodePatch((void*)a->fixed_addr, MemBuffer_data(&a->code_buffer), (uint32_t)MemBuffer_size(&a->code_buffer));
  MemBlock b; MemBlock_init(&b,a->fixed_addr,MemBuffer_size(&a->code_buffer)); return b;
}

typedef struct { int type; MemBlock buffer; addr_t forward_addr; size_t forward_size; int has_forward; } Trampoline;
static void Tramp_init(Trampoline* t, int ty, MemBlock b){ t->type=ty; t->buffer=b; t->forward_addr=0; t->forward_size=0; t->has_forward=0; }
static inline addr_t Tramp_addr(Trampoline* t){ return t->buffer.start_; }
static inline addr_t Tramp_size(Trampoline* t){ return t->buffer.size; }

static Trampoline* gen_normal_tramp(addr_t from, addr_t to){
  Assembler as; Asm_init(&as,from); int ty=0;
  uint64_t dist=(uint64_t)llabs((long long)(from-to));
  uint64_t adrp_range=((uint64_t)1<<(2+19+12-1));
  if (dist<adrp_range){ ty=3; Asm_adrp_add(&as,TMP_REG_0,from,to); Asm_br(&as,TMP_REG_0); }
  else { ty=4; Asm_lit_branch(&as,(uint64_t)to); }
  Asm_reloc_labels(&as); MemBlock blk=MemBuffer_dup(&as.code_buffer);
  Trampoline* t=(Trampoline*)malloc(sizeof(Trampoline)); Tramp_init(t,ty,blk); Asm_destroy(&as); return t;
}

typedef struct { addr_t addr; size_t size; int perm; } MemRegionInfo;
static int get_memory_layout(MemRegionInfo* out, int max){
  FILE* fp=fopen("/proc/self/maps","r"); if(!fp) return 0;
  int n=0; char line[2048];
  while (fgets(line,sizeof(line),fp)&&n<max){
    addr_t s,e,o; char p[5]={0}; uint8_t dm,dmn; long ino; int pi=0;
    if (sscanf(line,"%lx-%lx %4c %lx %hhx:%hhx %ld %n",(unsigned long*)&s,(unsigned long*)&e,p,(unsigned long*)&o,&dm,&dmn,&ino,&pi)<7) continue;
    int pm=0; if(p[0]=='r')pm|=MEM_PERM_R; if(p[1]=='w')pm|=MEM_PERM_W; if(p[2]=='x')pm|=MEM_PERM_X;
    out[n].addr=s; out[n].size=e-s; out[n].perm=pm; n++;
  }
  fclose(fp); return n;
}
static MemBlock alloc_near_in_page(addr_t pos, size_t range, uint32_t sz){
  MemRange sr; MemRange_init(&sr,pos>range?pos-range:0, range*2);
  for (int i=0;i<gMemAlloc.code_n;i++){
    simple_linear_allocator_t* al=gMemAlloc.code[i];
    addr_t cur=(addr_t)al->buffer+al->size;
    uint32_t unused=al->capacity-al->size;
    MemRange ur; MemRange_init(&ur,cur,unused);
    MemRange it=MemRange_intersect(&sr,&ur);
    if (it.size<sz) continue;
    addr_t gap=it.start_-cur;
    if (gap) sla_alloc(al,(uint32_t)gap,0);
    uint8_t* res=sla_alloc(al,sz,0);
    MemBlock b; MemBlock_init(&b,(addr_t)res,sz); return b;
  }
  MemRegionInfo rs[1024]; int rn=get_memory_layout(rs,1024);
  for (int i=0;i<rn-1;i++){
    addr_t us=rs[i].addr+rs[i].size; size_t usz=rs[i+1].addr-us;
    MemRange ur; MemRange_init(&ur,us,usz);
    MemRange it=MemRange_intersect(&sr,&ur);
    if (it.size<sz) continue;
    void* up=(void*)ALIGN_FLOOR(it.start_,os_page_size());
    void* pg=mmap(up,os_page_size(),PROT_READ|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0);
    if (pg==MAP_FAILED||pg!=up) continue;
    if (gMemAlloc.code_n>=MAX_PAGE_ALLOCATORS) break;
    simple_linear_allocator_t* pa=(simple_linear_allocator_t*)malloc(sizeof(simple_linear_allocator_t));
    sla_init(pa,(uint8_t*)pg,os_page_size(),8); gMemAlloc.code[gMemAlloc.code_n++]=pa;
    return alloc_near_in_page(pos,range,sz);
  }
  MemBlock b; MemBlock_init(&b,0,0); return b;
}
static Trampoline* gen_fast_forward_tramp(addr_t src, addr_t dst){
  Assembler as; Asm_init(&as,0);
  MemBlock blk=alloc_near_in_page(src,ARM64_B_XXX_RANGE,4*4);
  if (MemRange_addr(&blk)==0){ Asm_destroy(&as); return NULL; }
  Asm_ldr_x(&as,TMP_REG_0,TMP_REG_0,8); Asm_br(&as,TMP_REG_0); Asm_u64(&as,(uint64_t)dst);
  as.fixed_addr=MemRange_addr(&blk); MemBlock fin=Asm_finalize(&as);
  Trampoline* t=(Trampoline*)malloc(sizeof(Trampoline)); Tramp_init(t,5,fin); Asm_destroy(&as); return t;
}
static Trampoline* gen_near_tramp(addr_t src, addr_t dst){
  Assembler as; Asm_init(&as,src); int ty=0; Trampoline* fwd=NULL;
  if (llabs((long long)dst-(long long)src)<(long long)ARM64_B_XXX_RANGE){
    ty=1; Asm_b_imm(&as,(int64_t)dst-(int64_t)src);
  } else {
    ty=2; fwd=gen_fast_forward_tramp(src,dst);
    if (!fwd){ Asm_destroy(&as); return NULL; }
    Asm_b_imm(&as,(int64_t)Tramp_addr(fwd)-(int64_t)src);
  }
  MemBlock blk=MemBuffer_dup(&as.code_buffer);
  Trampoline* t=(Trampoline*)malloc(sizeof(Trampoline)); Tramp_init(t,ty,blk);
  if (fwd){ t->forward_addr=Tramp_addr(fwd); t->forward_size=Tramp_size(fwd); t->has_forward=1; free(fwd); }
  Asm_destroy(&as); return t;
}

typedef struct { uint32_t id; addr_t fake_func_addr; addr_t addr; MemBlock patched; MemBlock relocated; uint8_t* origin_code; size_t origin_code_size; } InterceptEntry;
#define MAX_INTERCEPT_ENTRIES 256
static InterceptEntry g_entries[MAX_INTERCEPT_ENTRIES];
static int g_entries_count = 0;
static InterceptEntry* find_entry(addr_t a){ for(int i=0;i<g_entries_count;i++) if(g_entries[i].patched.start_==a) return &g_entries[i]; return NULL; }
static void backup_orig(InterceptEntry* e){ uint32_t ts=(uint32_t)e->patched.size; e->origin_code=(uint8_t*)malloc(ts); memcpy(e->origin_code,(void*)e->addr,ts); e->origin_code_size=ts; }

typedef struct { addr_t origin_start; uint32_t origin_size; Assembler as; MemBlock relocated; int relocated_built; } ReloCtx;
static void RC_init(ReloCtx* c, addr_t o, uint32_t s){ c->origin_start=o; c->origin_size=s; Asm_init(&c->as,0); c->relocated_built=0; MemBlock_init(&c->relocated,0,0); }
static void RC_destroy(ReloCtx* c){ Asm_destroy(&c->as); }
static addr_t rc_cur(ReloCtx* c, uint32_t off){ return c->origin_start+off; }
static int relocate(ReloCtx* c, int branch){
  Assembler* a=&c->as; uint32_t off=0;
  while (off<c->origin_size){
    uint32_t inst=*(uint32_t*)rc_cur(c,off);
    if (inst_is_b_bl(inst)){
      int64_t o=decode_imm26_offset(inst); addr_t dst=rc_cur(c,off)+o;
      RelocDataLabel* l=Asm_create_label(a,(uint64_t)dst,8);
      Asm_ldr_label(a,TMP_REG_0,l);
      if ((inst&UnconditionalBranchMask)==BL) Asm_blr(a,TMP_REG_0); else Asm_br(a,TMP_REG_0);
    } else if (inst_is_ldr_literal(inst)){
      int64_t o=decode_imm19_offset(inst); addr_t dst=rc_cur(c,off)+o;
      int rt=decode_rt(inst); char opc=bits_(inst,30,31);
      Asm_mov_imm64(a,TMP_REG_0,(uint64_t)dst);
      if (opc==0b00) Asm_ldr_w(a,rt,TMP_REG_0,0);
      else if (opc==0b01) Asm_ldr_x(a,rt,TMP_REG_0,0);
    } else if (inst_is_adr(inst)){
      int64_t o=decode_immhi_immlo_offset(inst); addr_t dst=rc_cur(c,off)+o;
      int rd=decode_rd(inst); Asm_mov_imm64(a,rd,(uint64_t)dst);
    } else if (inst_is_adrp(inst)){
      int64_t o=decode_immhi_immlo_zero12_offset(inst); addr_t dst=rc_cur(c,off)+o;
      dst=ALIGN(dst,0x1000); int rd=decode_rd(inst); Asm_mov_imm64(a,rd,(uint64_t)dst);
    } else if (inst_is_b_cond(inst)){
      int64_t o=decode_imm19_offset(inst); addr_t dst=rc_cur(c,off)+o;
      uint32_t bi=inst; char cc=bits_(inst,0,3); cc^=1; set_bits_(bi,0,3,cc);
      int64_t no=4*3; uint32_t im=(uint32_t)(no>>2); set_bits_(bi,5,23,im);
      RelocDataLabel* l=Asm_create_label(a,(uint64_t)dst,8);
      Asm_u32(a,bi); Asm_ldr_label(a,TMP_REG_0,l); Asm_br(a,TMP_REG_0);
    } else if (inst_is_compare_b(inst)){
      int64_t o=decode_imm19_offset(inst); addr_t dst=rc_cur(c,off)+o;
      uint32_t bi=inst; char op=bit_(inst,24); op^=1; set_bit_(bi,24,op);
      int64_t no=4*3; uint32_t im=(uint32_t)(no>>2); set_bits_(bi,5,23,im);
      RelocDataLabel* l=Asm_create_label(a,(uint64_t)dst,8);
      Asm_u32(a,bi); Asm_ldr_label(a,TMP_REG_0,l); Asm_br(a,TMP_REG_0);
    } else if (inst_is_test_b(inst)){
      int64_t o=decode_imm14_offset(inst); addr_t dst=rc_cur(c,off)+o;
      uint32_t bi=inst; char op=bit_(inst,24); op^=1; set_bit_(bi,24,op);
      int64_t no=4*3; uint32_t im=(uint32_t)(no>>2); set_bits_(bi,5,18,im);
      RelocDataLabel* l=Asm_create_label(a,(uint64_t)dst,8);
      Asm_u32(a,bi); Asm_ldr_label(a,TMP_REG_0,l); Asm_br(a,TMP_REG_0);
    } else Asm_u32(a,inst);
    off += 4;
  }
  c->origin_size=off;
  if (branch) Asm_lit_branch(a,rc_cur(c,off));
  Asm_reloc_labels(a);
  c->relocated=Asm_finalize(a); c->relocated_built=1; return 0;
}

int DobbyHook(void* address, void* fake_func, void** out_origin){
  if (!address) return -1;
  make_memory_readable(address,4);
  if (find_entry((addr_t)address)) return -1;
  if (g_entries_count>=MAX_INTERCEPT_ENTRIES) return -1;
  InterceptEntry* e=&g_entries[g_entries_count]; memset(e,0,sizeof(*e));
  e->addr=(addr_t)address; e->fake_func_addr=(addr_t)fake_func;
  addr_t from=(addr_t)address, to=(addr_t)fake_func;
  Trampoline* nt=gen_near_tramp(from,to);
  Trampoline* tr=nt?NULL:gen_normal_tramp(from,to);
  addr_t ta; size_t ts;
  if (nt){ ta=Tramp_addr(nt); ts=Tramp_size(nt); }
  else if (tr){ ta=Tramp_addr(tr); ts=Tramp_size(tr); }
  else return -1;
  ReloCtx ctx; RC_init(&ctx,from,(uint32_t)ts);
  relocate(&ctx,1);
  if (ctx.relocated.size==0){ RC_destroy(&ctx); if(tr)free(tr); if(nt)free(nt); return -1; }
  e->patched.start_=from; e->patched.size=(size_t)ctx.origin_size; e->relocated=ctx.relocated;
  backup_orig(e);
  DobbyCodePatch((void*)from,(uint8_t*)ta,(uint32_t)ts);
  if (out_origin) *out_origin=(void*)e->relocated.start_;
  g_entries_count++; RC_destroy(&ctx); if(tr)free(tr); if(nt)free(nt); return 0;
}

// ============================================================
// URL / FString helpers (from Owen)
// ============================================================
struct FUEString { TCHAR* data; int32 length; int32 capacity; };

static char* FUEToCStr(const FUEString* s){
  if (!s||!s->data||s->length<=0) return NULL;
  int mb=s->length*3+1; char* o=(char*)malloc(mb); if(!o) return NULL;
  int j=0;
  for (int i=0;i<s->length-1;i++){
    uint16_t c=s->data[i];
    if (c<=0x7F) o[j++]=(char)c;
    else if (c<=0x7FF){ o[j++]=(char)(0xC0|(c>>6)); o[j++]=(char)(0x80|(c&0x3F)); }
    else { o[j++]=(char)(0xE0|(c>>12)); o[j++]=(char)(0x80|((c>>6)&0x3F)); o[j++]=(char)(0x80|(c&0x3F)); }
  }
  o[j]=0; return o;
}
static FUEString FUEFromCStr(const char* str){
  FUEString fs; memset(&fs,0,sizeof(fs)); if(!str) return fs;
  int ul=0; const unsigned char* s=(const unsigned char*)str;
  while (*s){ if((*s&0x80)==0){s+=1;ul++;} else if((*s&0xE0)==0xC0){s+=2;ul++;} else if((*s&0xF0)==0xE0){s+=3;ul++;} else {s+=4;ul++;} }
  fs.length=fs.capacity=ul+1; fs.data=(TCHAR*)malloc(fs.capacity*sizeof(TCHAR)); if(!fs.data){fs.length=fs.capacity=0; return fs;}
  int i=0; s=(const unsigned char*)str;
  while (*s){
    if ((*s&0x80)==0) fs.data[i++]=*s++;
    else if ((*s&0xE0)==0xC0){ fs.data[i++]=(uint16_t)(((s[0]&0x1F)<<6)|(s[1]&0x3F)); s+=2; }
    else if ((*s&0xF0)==0xE0){ fs.data[i++]=(uint16_t)(((s[0]&0x0F)<<12)|((s[1]&0x3F)<<6)|(s[2]&0x3F)); s+=3; }
    else { fs.data[i++]='?'; s+=4; }
  }
  fs.data[i]=0; return fs;
}

#define URL_HOST_MAX 1024
#define URL_PATH_MAX 4096
struct Url { char host[URL_HOST_MAX]; char pathAndQuery[URL_PATH_MAX]; };
static void Url_Parse(const char* url, struct Url* o){
  if(!url||!o) return; o->host[0]=0; o->pathAndQuery[0]=0; if(!url[0]) return;
  const char* pe=strstr(url,"://"); const char* rem=NULL; const char* ps=NULL;
  if (pe){ size_t hl=pe-url+3; if(hl>=URL_HOST_MAX)hl=URL_HOST_MAX-1; strncpy(o->host,url,hl); o->host[hl]=0;
    rem=pe+3; ps=strchr(rem,'/');
    if (ps){ size_t dl=ps-rem; if(strlen(o->host)+dl<URL_HOST_MAX) strncat(o->host,rem,dl); size_t pl=strlen(ps); if(pl>=URL_PATH_MAX)pl=URL_PATH_MAX-1; strncpy(o->pathAndQuery,ps,pl); o->pathAndQuery[pl]=0; }
    else { if(strlen(o->host)+strlen(rem)<URL_HOST_MAX) strcat(o->host,rem); }
  } else { ps=strchr(url,'/'); if(ps){ size_t dl=ps-url; if(dl>=URL_HOST_MAX)dl=URL_HOST_MAX-1; strncpy(o->host,url,dl); o->host[dl]=0; size_t pl=strlen(ps); if(pl>=URL_PATH_MAX)pl=URL_PATH_MAX-1; strncpy(o->pathAndQuery,ps,pl); o->pathAndQuery[pl]=0; } else { strncpy(o->host,url,URL_HOST_MAX-1); o->host[URL_HOST_MAX-1]=0; } }
}
static int ShouldRedirect(const char* host){
  char s[URL_HOST_MAX]; strncpy(s,host,sizeof(s)-1); s[sizeof(s)-1]=0;
  if (strncmp(s,"http://",7)==0) memmove(s,s+7,strlen(s+7)+1);
  else if (strncmp(s,"https://",8)==0) memmove(s,s+8,strlen(s+8)+1);
  char* c=strchr(s,':'); if(c)*c=0;
  const char* d[]={"game-social.epicgames.com","ol.epicgames.com","ol.epicgames.net","on.epicgames.com","ak.epicgames.com","epicgames.dev",".akamaized.net",".epicgames.com",".epicgames.net"};
  for (size_t i=0;i<sizeof(d)/sizeof(d[0]);i++){ size_t dl=strlen(d[i]), sl=strlen(s); if(sl>=dl && strcmp(s+sl-dl,d[i])==0) return 1; }
  return 0;
}
static void CreateUrl(const char* h, const char* pq, char* o, size_t os){
  if(!o||os==0) return; size_t hl=h?strlen(h):0, pl=pq?strlen(pq):0, t=hl+pl;
  if(t>=os)t=os-1;
  if(hl>0) memcpy(o,h,hl>t?t:hl);
  if(pl>0 && t>hl) memcpy(o+hl,pq,t-hl);
  o[t]=0;
}

static char g_backend_url[4096] = "http://127.0.0.1:3551";

// ============================================================
// Version tables (21.30 verified)
// ============================================================
static struct { const char* version; uint32_t pr_offset; uint16_t geturl_off; uint32_t seturl_off; } ue_offsets[] = {
    {"++Fortnite+Release-21.30-CL-21088273", 0x089F02F0, 0x70, 0x089EDCF8},
};
static const char* detected_version = NULL;
static uint32_t ue_pr_offset = 0; static uint16_t ue_geturl_off = 0; static uint32_t ue_seturl_off = 0;

// ============================================================
// UE SDK (from EraCH3)
// ============================================================
namespace RVAs {
    constexpr uint64_t ProcessEvent       = 0x07611B5C;
    constexpr uint64_t GObjects           = 0x0DCD6E08;
    constexpr uint64_t SpawnActor_Thunk   = 0x3217298;
    constexpr uint64_t FNameToString      = 0x74937C0;
    constexpr uint64_t FreeInternal       = 0x7355370;
    constexpr uint64_t Realloc            = 0x735E520;
    constexpr uint64_t GetDataTableRowFromName_ORIG = 0x9319C70;
    constexpr uint64_t GenericArray_Remove = 0x944B2DC;
    constexpr uint64_t curl_easy_setopt    = 0x0B1CA0B8;
}
namespace offsets {
    constexpr int32 Class=0x10, Name=0x18, Outer=0x20, Children=0x48, Next=0x28, SuperClass=0x40;
    constexpr int32 Offset=0x44, ParamsSize=0xB6, ReturnValueOffset=0xB8, StructSize=0x58;
}
static inline bool IsBadPtr(const void* p){ if(!p) return true; uintptr_t v=(uintptr_t)p; if(v<0x10000) return true; if(v>0x0000FFFFFFFFFFFFULL) return true; return false; }
template<typename T> static T& Ref0(){ static T i{}; return i; }

static void (*FNameToStringFn)(void*,void*) = nullptr;
static void (*FreeInternalFn)(void*) = nullptr;
static void* (*ReallocFn)(void*,size_t,uint32) = nullptr;
static void* (*PE)(void*,void*,void*) = nullptr;

struct FName {
    uint32 ComparisonIndex; uint32 DisplayIndex;
    std::string ToString(){
        if(!FNameToStringFn) return "";
        struct FS { TCHAR* Data; int32 Count; int32 Max; } out{};
        FNameToStringFn(this,&out); std::string s;
        if(out.Data && out.Count>0){ s.reserve(out.Count); for(int32 i=0;i<out.Count-1;i++){ TCHAR c=out.Data[i]; s.push_back(c<128?(char)c:'?'); } }
        if(out.Data && FreeInternalFn) FreeInternalFn(out.Data);
        return s;
    }
};
struct FString {
    TCHAR* Data; int32 Count; int32 Max;
    FString():Data(nullptr),Count(0),Max(0){}
    FString(const wchar_t* o){ if(!o){Data=nullptr;Count=0;Max=0;return;} int l=0; while(o[l])l++; Data=(TCHAR*)malloc((l+1)*2); for(int i=0;i<l;i++) Data[i]=o[i]; Data[l]=0; Count=Max=l+1; }
    std::string ToString() const { if(!Data) return {}; std::string out; out.reserve(Count); for(int32 i=0;i<Count-1;i++){ TCHAR c=Data[i]; out.push_back(c<128?(char)c:'?'); } return out; }
    std::wstring ToWString() const { if(!Data) return {}; std::wstring w; for(int32 i=0;i<Count-1;i++) w.push_back((wchar_t)Data[i]); return w; }
};
struct FVector { float X,Y,Z; };
struct FRotator { float Pitch,Yaw,Roll; };
struct FGuid { int32 A,B,C,D; };
struct FVector2D { float X,Y; };
struct FMargin { float Left,Top,Right,Bottom; };
struct FLinearColor { float R,G,B,A; };
struct FSlateColor { FLinearColor SpecifiedColor; uint8 ColorUseRule; uint8 _pad[0x17]; };
struct FSlateBrush { uint8 _pad[0x8]; FVector2D ImageSize; FMargin Margin; FSlateColor TintColor; class UObject* ObjectResource; };
struct FGameplayAbilitySpecDef { class UObject* Ability; uint8 _pad[0x90]; };
struct FGameplayEffectContextHandle { uint8 _pad[0x18]; };
struct FActiveGameplayEffectHandle { int32 Handle; bool bPassed; uint8 _pad[0x3]; };
struct FGameplayTagContainer { void* GT_Data; int32 GT_Count; int32 GT_Max; void* PT_Data; int32 PT_Count; int32 PT_Max; };
struct GEHard { class UObject* Effect; float Level; uint8 _pad[0x4]; };
struct FItemAndCount { int32 Count; uint8 _pad[0x4]; class UObject* Item; };
struct FKey { FName KeyName; uint8 _pad[0x10]; };
struct SpawnActorParams { uint8 Parms[0x40]; };
struct SoftObjectPtr { uint8 _pad[0x28]; };

class FFieldClass { public: FName NamePrivate; uint8 Id; void* CastFlags; uint8 ClassFlags; FFieldClass* Super; void* DefaultObj;
    std::string GetName(){ std::string n=NamePrivate.ToString(); auto p=n.rfind('/'); return p==std::string::npos?n:n.substr(p+1); } };
class FField { public: void* VTablePointer; FFieldClass* ClassPointer; uint8 _pad[0x10]; FField* Next; FName Name; uint8 Flags;
    bool IsValid(){ return !IsBadPtr(&Name); }
    std::string GetName(){ std::string n=Name.ToString(); auto p=n.rfind('/'); return p==std::string::npos?n:n.substr(p+1); } };

class UObject {
public:
    void** Vtable; int32 ObjectFlags; int32 InternalIndex; UObject* Class; FName Name; UObject* Outer;
    bool IsA(UObject* c){ return this->Class==c; }
    bool IsChild(UObject* p){ UObject* c=this->Class; while(c&&!IsBadPtr(c)){ if(c==p) return true; c=*(UObject**)((uintptr_t)c+offsets::SuperClass); } return false; }
    std::string GetName(){ std::string s=Name.ToString(); auto p=s.rfind('/'); return p==std::string::npos?s:s.substr(p+1); }
    int32 EnumIndex(const std::string& en){
        void* a=(void*)(int64(this)+0x40); if(IsBadPtr(a)) return 0;
        int32 n=*(int32*)((uintptr_t)a+8); void* d=*(void**)a; if(IsBadPtr(d)) return 0;
        for (int i=0;i<n;i++){ FName* fn=(FName*)((uintptr_t)d+i*0x10); std::string s=fn->ToString(); std::string mn=GetName(); auto p=s.find(mn+"::"); if(p!=std::string::npos) s=s.substr(p+mn.length()+2); if(s==en) return i; }
        return 0;
    }
    template<typename T=UObject*> T& Child(const std::string& name, bool bFn=false, bool bC=true){
        static std::map<std::string,int32> cache; std::string key=GetName()+"::"+name;
        if(bC){ auto it=cache.find(key); if(it!=cache.end()) return *(T*)(int64(this)+it->second); }
        UObject* k=this->Class;
        while(k&&!IsBadPtr(k)){
            UObject* ch=*(UObject**)(int64(k)+offsets::Children);
            if(ch&&!IsBadPtr(ch)){ UObject* cur=ch;
                while(cur&&!IsBadPtr(cur)){
                    std::string cn=(cur->Class&&!IsBadPtr(cur->Class))?cur->Class->GetName():"";
                    if(cn=="StructProperty"){ UObject* cc=*(UObject**)(int64(cur)+offsets::Class);
                        if(cc&&!IsBadPtr(cc)){ UObject* sc=*(UObject**)(int64(cc)+offsets::Children);
                            while(sc&&!IsBadPtr(sc)){ if(sc->GetName()==name){ int32 o1=*(uint32*)(int64(cur)+offsets::Offset); int32 o2=*(uint32*)(int64(sc)+offsets::Offset); return *(T*)(int64(this)+o1+o2); } sc=*(UObject**)(int64(sc)+offsets::Next); } } }
                    if(cur->GetName()==name){ if(bFn) return *(T*)(&*cur); int32 o=*(int32*)(int64(cur)+offsets::Offset); if(bC) cache[key]=o; return *(T*)(int64(this)+o); }
                    cur=*(UObject**)(int64(cur)+offsets::Next);
                } }
            k=*(UObject**)(int64(k)+offsets::SuperClass);
        }
        k=this->Class;
        while(k&&!IsBadPtr(k)){
            FField* fp=*(FField**)(int64(k)+0x50);
            if(fp&&fp->IsValid()){ while(fp->IsValid()){
                std::string cp=fp->ClassPointer?fp->ClassPointer->GetName():"";
                if(cp=="StructProperty"){ UObject* fc=*(UObject**)(int64(fp)+offsets::Class);
                    if(fc&&!IsBadPtr(fc)){ FField* sc=*(FField**)(int64(fc)+0x50);
                        while(sc&&sc->IsValid()){ if(sc->GetName()==name){ int32 o1=*(uint32*)(int64(fp)+0x4C); int32 o2=*(uint32*)(int64(sc)+0x4C); return *(T*)(int64(this)+o1+o2); } sc=sc->Next; } } }
                if(fp->GetName()==name){ int32 o=*(int32*)(int64(fp)+0x4C); if(bC) cache[key]=o; return *(T*)(int64(this)+o); }
                fp=fp->Next;
            } }
            k=*(UObject**)(int64(k)+offsets::SuperClass);
        }
        return Ref0<T>();
    }
    std::vector<int32> GetFunctionChildrenOffset(){
        std::vector<int32> r; UObject* c=*(UObject**)(int64(this)+offsets::Children); FField* fp=*(FField**)(int64(this)+0x50);
        while(c&&!IsBadPtr(c)){ r.push_back(*(int32*)(int64(c)+offsets::Offset)); c=*(UObject**)(int64(c)+offsets::Next); }
        while(fp&&fp->IsValid()){ r.push_back(*(int32*)(int64(fp)+0x4C)); fp=fp->Next; }
        return r;
    }
    template<typename T=int, int16 RO=-1, typename... Ps> T Call(const std::string& name, Ps... args){
        static std::map<std::string,UObject*> fc; UObject* f=nullptr; std::string key=GetName()+"::"+name;
        auto it=fc.find(key); if(it!=fc.end()) f=it->second; else { f=&(this->Child<UObject>(name,true)); fc[key]=f; }
        if(IsBadPtr(f)) return T{};
        int32 ps=*(int16*)(int64(f)+offsets::ParamsSize); int16 ro=*(int16*)(int64(f)+offsets::ReturnValueOffset);
        if(ro==-1) ro=RO; if(ps<=0||ps>0x10000) return T{};
        uint8* par=(uint8*)calloc(1,ps); auto offs=f->GetFunctionChildrenOffset(); int i=0;
        auto fill=[&](auto& v){ if(i<(int)offs.size()&&offs[i]>0&&offs[i]<ps){ size_t sz=sizeof(v); if((size_t)offs[i]+sz<=(size_t)ps) memcpy(par+offs[i],&v,sz); } i++; };
        (fill(args),...);
        PE(this,f,par); T r{}; if(ro>0&&ro<ps) r=*(T*)(par+ro); free(par); return r;
    }
};
struct FUObjectItem { UObject* Object; int32 Flags; int32 Cluster; int32 Serial; };
struct GObjects_t {
    uint8 _pad0[0x10]; void** Objects; uint8 _pad1[0x8];
    int32 MaxElements; int32 NumElements; int32 MaxChunks; int32 NumChunks;
    UObject* GetById(uint32 id){ if(!Objects) return nullptr; uint32 ci=id/0x10000, wi=id%0x10000; if((int32)ci>=NumChunks) return nullptr; auto* ch=(FUObjectItem*)Objects[ci]; if(!ch) return nullptr; return ch[wi].Object; }
};
template<class T> class TArray { public: T* Data; int32 Count; int32 Max;
    void Add(T in){ Data=(T*)ReallocFn(Data,sizeof(T)*(Count+1),0); Data[Count++]=in; Max=Count; }
    void AddWithSize(size_t sz, void* e){ Data=(T*)ReallocFn(Data,sz*(Count+1),0); memcpy((void*)((uintptr_t)Data+sz*Count),e,sz); Count++; Max=Count; }
    T& operator[](int i){ return Data[i]; } const T& operator[](int i) const { return Data[i]; }
    int MaxIndex(){ return Count-1; } T* begin(){ return Data; } T* end(){ return Data+Count; }
};
struct Struct { template<typename T=UObject*> T& Child(UObject* so, const std::string& vn){
    static std::map<std::pair<UObject*,std::string>,int32> c; auto k=std::make_pair(so,vn); auto it=c.find(k); if(it!=c.end()) return *(T*)(int64(this)+it->second);
    UObject* kl=so;
    while(kl&&!IsBadPtr(kl)){ UObject* cu=*(UObject**)(int64(kl)+offsets::Children);
        while(cu&&!IsBadPtr(cu)){ std::string cn=(cu->Class&&!IsBadPtr(cu->Class))?cu->Class->GetName():"";
            if(cn=="StructProperty"){ UObject* cc=*(UObject**)(int64(cu)+offsets::Class); if(cc&&!IsBadPtr(cc)){ UObject* sc=*(UObject**)(int64(cc)+offsets::Children);
                while(sc&&!IsBadPtr(sc)){ if(sc->GetName()==vn){ int32 o1=*(uint32*)(int64(cu)+offsets::Offset); int32 o2=*(uint32*)(int64(sc)+offsets::Offset); return *(T*)(int64(this)+o1+o2); } sc=*(UObject**)(int64(sc)+offsets::Next); } } }
            if(cu->GetName()==vn){ int32 o=*(int32*)(int64(cu)+offsets::Offset); c[k]=o; return *(T*)(int64(this)+o); }
            cu=*(UObject**)(int64(cu)+offsets::Next); }
        kl=*(UObject**)(int64(kl)+offsets::SuperClass); }
    return Ref0<T>();
} };

// ============================================================
// Globals
// ============================================================
static uintptr_t g_base = 0;
static UObject* (*SpawnActorFn)(void*,UObject*,FVector*,FRotator*,const SpawnActorParams&) = nullptr;
static UObject* (*StaticFindObjectFn)(UObject*,void*,const wchar_t*,bool) = nullptr;
static UObject* (*StaticLoadObjectFn)(UObject*,UObject*,const wchar_t*,const wchar_t*,int,void*,bool) = nullptr;
static bool (*GetDataTableRowFn)(UObject*,FName,void*) = nullptr;
static void (*GenericArray_RemoveFn)(void*,void*,int) = nullptr;
static void* (*orig_ProcessEvent)(UObject*,UObject*,void*) = nullptr;

static int (*FCurlOGProcessRequest)(void*) = nullptr;
static void (*FCurlSetUrl)(void*, const void*) = nullptr;
static int (*curlOGSetopt)(void*,int,void*) = nullptr;

static UObject* PlayerController=nullptr; static UObject* GameState=nullptr; static UObject* GameMode=nullptr;
static UObject* World=nullptr; static UObject* Pawn=nullptr; static UObject* GameViewport=nullptr;
static UObject* CheatManager=nullptr; static UObject* GameStatics=nullptr; static UObject* kismetMathLib=nullptr;
static UObject* kismetStringLib=nullptr; static UObject* kismetSystemLib=nullptr; static UObject* kismetGuidLib=nullptr;
static UObject* DataTableFunctionLibrary=nullptr; static UObject* WorldInventory=nullptr; static UObject* Quickbars=nullptr;
static UObject* WorldSettings=nullptr; static UObject* EditToolItem=nullptr; static UObject* Wall=nullptr;
static UObject* Floor=nullptr; static UObject* Stairs=nullptr; static UObject* Roof=nullptr;
static std::vector<UObject*> CharacterPartsArray; static GObjects_t* gGObjects = nullptr;
static bool bInFrontend=true, bPressedPlay=false, bLoadedInMatch=false, bDroppedFromAircraft=false, bIsEmoting=false, g_bIsVehicleVersion=false, l_bIsOutsideSafeZone=false;
static FVector EditComponentLocation; static FRotator EditComponentRotation;

static uintptr_t LibFind(const char* name){
    FILE* f=fopen("/proc/self/maps","r"); if(!f) return 0;
    char l[512]; uintptr_t lo=UINTPTR_MAX;
    while(fgets(l,sizeof(l),f)) if(strstr(l,name)){ uintptr_t a=strtoul(l,nullptr,16); if(a<lo)lo=a; }
    fclose(f); return lo==UINTPTR_MAX?0:lo;
}
static std::wstring ToWide(const std::string& s){ std::wstring w; for(char c:s) w.push_back((wchar_t)c); return w; }
static UObject* FindObject(const wchar_t* p, bool bl=false, bool ba=false){
    if(!StaticFindObjectFn) return nullptr;
    UObject* o=StaticFindObjectFn(nullptr, ba?(void*)-1:nullptr, p, false);
    if(!o && bl && StaticLoadObjectFn) o=StaticLoadObjectFn(nullptr,nullptr,p,nullptr,0,nullptr,true);
    return o;
}
static UObject* SpawnActorEasy(UObject* k, FVector l, FRotator r){
    if(!k||!SpawnActorFn) return nullptr; SpawnActorParams p{}; return SpawnActorFn((void*)World,k,&l,&r,p);
}

// ============================================================
// Athena (same as before)
// ============================================================
namespace Athena {
    void SpawnPawn(); void ShowSkin(); void DestroyLods(); void DropLoadingScreen();
    void AddToInventory(UObject*,int,char,int); void InitializeInventory();
    void OnServerExecuteInventoryItem(FGuid); void OnServerExecuteInventoryWeapon(UObject*);
    void GrantDefaultAbilities(); void InventoryUpdate(); void OnAircraftJump(); void RemoveNetDebugUI();
    void TeleportToSpawnIsland(); void ConsoleKey(); void CheatScript(const char*);
    void ServerHandlePickup(UObject*); void FixBuildingFoundations(); void Fixbus(); void Loot(UObject*);
    void OnServerCreateBuildingActor(void*); void OnBeginEditActor(UObject*);
    void OnFinishEditActor(UObject*,UObject*,int,bool,void*); void SpawnPickup(UObject*,int,FVector,bool=true);
    void GrantAbility(UObject*); void SpawnBuildPreviews(); void SpawnFloorLoot(); void GrantEffect(UObject*,float);
    void PlayEmoteItem(UObject*); void Tick(); void OnExitVehicle(); void DropInventoryItem(FGuid,int,bool=true);
    struct LootData { UObject* ItemDefinition; int Count; };
    std::vector<LootData> PickLootDrops(std::string);
}

void Athena::SpawnPawn(){
    Pawn=SpawnActorEasy(FindObject(L"/Game/Athena/PlayerPawn_Athena.PlayerPawn_Athena_C"),FVector{0,0,5000},FRotator{0,0,0});
    if(PlayerController&&Pawn) PlayerController->Call("Possess",Pawn);
}
void Athena::ShowSkin(){
    if(!PlayerController||!Pawn) return;
    static UObject* H=nullptr;
    if(auto t=&PlayerController->Child<UObject*>("MyPlayerInfo")->Child<UObject*>("AthenaMenuHeroDef"); !IsBadPtr(t)) H=*t;
    if(auto t=&PlayerController->Child<UObject*>("MyPlayerInfo")->Child<UObject*>("TempAthenaMenuHeroInstance"); !IsBadPtr(t)) H=*t;
    if(!H) return;
    static auto cp=H->Child<TArray<UObject*>>("CharacterParts");
    if(CharacterPartsArray.empty()) for(UObject* c:cp) if(c&&!IsBadPtr(c)) CharacterPartsArray.push_back(c);
    for(UObject* c:CharacterPartsArray) if(c&&!IsBadPtr(c)) Pawn->Call("ServerChoosePart",c->Child<char>("CharacterPartType"),c);
    auto ps=&PlayerController->Child<UObject*>("PlayerState");
    if(!IsBadPtr(ps)&&*ps){ (*ps)->Call("OnRep_CharacterParts"); (*ps)->Call("OnRep_CharacterData"); }
}
void Athena::DestroyLods(){ if(CheatManager&&!IsBadPtr(CheatManager)) CheatManager->Call("DestroyAll",FindObject(L"/Script/FortniteGame.FortHLODSMActor")); }
void Athena::DropLoadingScreen(){
    if(!GameState||!PlayerController) return;
    GameState->Child<char>("GamePhase")=2; GameState->Call("OnRep_GamePhase",(char)0);
    PlayerController->Call("ServerReadyToStartMatch");
    PlayerController->Child<bool>("bHasServerFinishedLoading")=true;
    PlayerController->Child<bool>("bHasClientFinishedLoading")=true;
    auto ps=&PlayerController->Child<UObject*>("PlayerState");
    if(!IsBadPtr(ps)&&*ps){ (*ps)->Call("OnRep_SquadId"); (*ps)->Child<char>("TeamIndex")=(char)2; (*ps)->Call("OnRep_TeamIndex"); }
    if(auto m=&GameState->Child<FSlateBrush>("MinimapNextCircleBrush"); !IsBadPtr(m)) *m={};
    if(auto m=&GameState->Child<FSlateBrush>("FullMapNextCircleBrush"); !IsBadPtr(m)) *m={};
    if(auto m=&GameState->Child<FSlateBrush>("MinimapSafeZoneBrush"); !IsBadPtr(m)) *m={};
    if(auto m=&GameState->Child<FSlateBrush>("MinimapCircleBrush"); !IsBadPtr(m)) *m={};
    if(auto m=&GameState->Child<FSlateBrush>("FullMapCircleBrush"); !IsBadPtr(m)) *m={};
    if(auto m=&GameState->Child<FSlateBrush>("AircraftPathBrush"); !IsBadPtr(m)) m->ObjectResource=FindObject(L"/Game/Athena/HUD/MiniMap/M_BusPath.M_BusPath");
    if(WorldSettings){ if(auto m=&GameState->Child<FSlateBrush>("MinimapBackgroundBrush"); !IsBadPtr(m)){ auto w=&WorldSettings->Child<FSlateBrush>("AthenaMapImage"); m->ObjectResource=IsBadPtr(w)?nullptr:w->ObjectResource; } }
    if(auto m=&GameState->Child<UObject*>("MinimapMaterial"); !IsBadPtr(m)){ UObject* ap=FindObject(L"/Game/Athena/Apollo/Maps/UI/M_MiniMapApollo.M_MiniMapApollo"); *m=ap?ap:FindObject(L"/Game/Athena/HUD/MiniMap/M_MiniMapAthena.M_MiniMapAthena"); }
    UObject* pl=FindObject(L"/Game/Athena/Playlists/Creative/Playlist_PlaygroundV2.Playlist_PlaygroundV2");
    if(!pl) pl=FindObject(L"/Game/Athena/Playlists/Playground/Playlist_Playground.Playlist_Playground");
    if(pl){ pl->Child<bool>("bIsLargeTeamGame")=true;
        if(auto b=&GameState->Child<UObject*>("BasePlaylist"); !IsBadPtr(b)) *b=pl;
        if(auto o=&GameState->Child<UObject*>("OverridePlaylist"); !IsBadPtr(o)) *o=pl;
        if(auto c=&GameState->Child<UObject*>("CurrentPlaylistData"); !IsBadPtr(c)) *c=pl;
        GameState->Call("OnRep_CurrentPlaylistData"); GameState->Call("OnRep_CurrentPlaylistInfo"); }
    if(auto b=FindObject(L"BBID_WinterBus",false,true); b) PlayerController->Child<UObject*>("BattleBus")=b;
    if(Pawn) Pawn->Call("OnRep_CustomizationLoadout");
}
void Athena::InventoryUpdate(){
    if(!PlayerController) return;
    PlayerController->Call("HandleWorldInventoryLocalUpdate");
    if(WorldInventory&&!IsBadPtr(WorldInventory)) WorldInventory->Call("HandleInventoryLocalUpdate");
    PlayerController->Call("OnRep_QuickBar");
    if(Quickbars&&!IsBadPtr(Quickbars)){ Quickbars->Call("OnRep_SecondaryQuickBar"); Quickbars->Call("OnRep_PrimaryQuickBar"); }
    PlayerController->Call("ClientForceUpdateQuickbar",(char)0); PlayerController->Call("ClientForceUpdateQuickbar",(char)1);
}
void Athena::AddToInventory(UObject* item,int count,char idx,int slot){
    if(IsBadPtr(item)||!WorldInventory||!Quickbars) return;
    UObject* ii=item->Call<UObject*>("CreateTemporaryItemInstanceBP",count,1);
    if(!ii||IsBadPtr(ii)) return;
    WorldInventory->Child<TArray<UObject*>>("ItemInstances").Add(ii);
    UObject* es=FindObject(L"/Script/FortniteGame.FortItemEntry");
    if(es){ int32 ss=*(int32*)(int64(es)+offsets::StructSize); if(ss>0&&ss<0x1000) WorldInventory->Child<TArray<char>>("ReplicatedEntries").AddWithSize(ss,&ii->Child<void*>("ItemEntry")); }
    ii->Call("SetOwningControllerForTemporaryItem",PlayerController);
    FGuid g=ii->Call<FGuid>("GetItemGuid");
    Quickbars->Call("ServerAddItemInternal",g,idx,slot);
}
void Athena::InitializeInventory(){
    if(!PlayerController||!GameMode) return;
    WorldInventory=PlayerController->Child<UObject*>("WorldInventory");
    if(auto q=&PlayerController->Child<UObject*>("ClientQuickBars"); !IsBadPtr(q)) Quickbars=*q;
    else { Quickbars=SpawnActorEasy(FindObject(L"/Script/FortniteGame.FortQuickBars"),FVector{0,0,0},FRotator{0,0,0}); if(Quickbars) PlayerController->Child<UObject*>("QuickBars")=Quickbars; }
    if(Quickbars&&!IsBadPtr(Quickbars)) Quickbars->Call("SetOwner",PlayerController);
    if(WorldInventory&&!IsBadPtr(WorldInventory)) WorldInventory->Call("SetOwner",PlayerController);
    auto si=GameMode->Child<TArray<FItemAndCount>>("StartingItems");
    auto pk=&PlayerController->Child<UObject*>("Pickaxe"); UObject* pw=nullptr;
    if(!IsBadPtr(pk)&&*pk) pw=(*pk)->Child<UObject*>("WeaponDefinition");
    AddToInventory(pw,1,0,0);
    if(si.Count>0&&!IsBadPtr(si.Data)){
        if(si.Count>0) AddToInventory(si[0].Item,1,1,0);
        if(si.Count>1) AddToInventory(si[1].Item,1,1,1);
        if(si.Count>2) AddToInventory(si[2].Item,1,1,2);
        if(si.Count>3) AddToInventory(si[3].Item,1,1,3);
        if(si.Count>5) AddToInventory(si[5].Item,999,3,0);
        if(si.Count>4) EditToolItem=si[4].Item;
    }
    AddToInventory(FindObject(L"/Game/Athena/Items/Ammo/AthenaAmmoDataShells.AthenaAmmoDataShells"),999,3,0);
    AddToInventory(FindObject(L"/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsMedium.AthenaAmmoDataBulletsMedium"),999,3,0);
    AddToInventory(FindObject(L"/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsLight.AthenaAmmoDataBulletsLight"),999,3,0);
    AddToInventory(FindObject(L"/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsHeavy.AthenaAmmoDataBulletsHeavy"),999,3,0);
    AddToInventory(FindObject(L"/Game/Athena/Items/Ammo/AmmoDataRockets.AmmoDataRockets"),999,3,0);
    if(kismetStringLib&&kismetSystemLib){
        FName ia=kismetStringLib->Call<FName>("Conv_StringToName",FString(L"bInfiniteAmmo"));
        FName bf=kismetStringLib->Call<FName>("Conv_StringToName",FString(L"bBuildFree"));
        kismetSystemLib->Call("SetBoolPropertyByName",PlayerController,ia,true);
        kismetSystemLib->Call("SetBoolPropertyByName",PlayerController,bf,true);
    }
    InventoryUpdate();
}
void Athena::OnServerExecuteInventoryItem(FGuid g){
    if(!WorldInventory||!Pawn) return;
    auto ins=WorldInventory->Child<TArray<UObject*>>("ItemInstances");
    for(UObject* i:ins){ if(!i||IsBadPtr(i)) continue;
        if(kismetGuidLib->Call<bool>("EqualEqual_GuidGuid",i->Call<FGuid>("GetItemGuid"),g)){
            UObject* d=i->Call<UObject*>("GetItemDefinitionBP"); if(!d) continue;
            if(d->IsA(FindObject(L"/Script/FortniteGame.FortTrapItemDefinition"))){ Pawn->Call<bool>("PickUpActor",nullptr,d); Pawn->Child<UObject*>("CurrentWeapon")->Child<FGuid>("ItemEntryGuid")=i->Call<FGuid>("GetItemGuid"); }
            else if(d->IsA(FindObject(L"AthenaGadgetItemDefinition",false,true))||d->IsA(FindObject(L"FortGadgetItemDefinition",false,true))){
                auto ps=&PlayerController->Child<UObject*>("PlayerState");
                if(!IsBadPtr(ps)&&*ps){ (*ps)->Call("OnRep_CharacterParts"); (*ps)->Call("OnRep_CharacterData"); }
                UObject* as=kismetSystemLib->Call<UObject*>("Conv_SoftObjectReferenceToObject",d->Child<SoftObjectPtr>("AbilitySet"));
                if(as){ for(UObject* a:as->Child<TArray<UObject*>>("GameplayAbilities")) GrantAbility(a);
                    for(auto& e:as->Child<TArray<GEHard>>("GrantedGameplayEffects")) GrantEffect(e.Effect,e.Level); }
                Pawn->Child<UObject*>("Mesh")->Call("SetAnimInstanceClass",d->Child<UObject*>("AnimBPOverride"));
                Pawn->Call<UObject*>("EquipWeaponDefinition",d->Call<UObject*>("GetDecoItemDefinition"),i->Call<FGuid>("GetItemGuid"));
            } else Pawn->Call<UObject*>("EquipWeaponDefinition",d,i->Call<FGuid>("GetItemGuid"));
        }
    }
}
void Athena::OnServerExecuteInventoryWeapon(UObject* w){
    if(!w||!WorldInventory||!Pawn) return;
    FGuid g=w->Child<FGuid>("ItemEntryGuid");
    auto ins=WorldInventory->Child<TArray<UObject*>>("ItemInstances");
    for(UObject* i:ins){ if(!i||IsBadPtr(i)) continue; if(kismetGuidLib->Call<bool>("EqualEqual_GuidGuid",i->Call<FGuid>("GetItemGuid"),g)) Pawn->Call<UObject*>("EquipWeaponDefinition",i->Call<UObject*>("GetItemDefinitionBP"),i->Call<FGuid>("GetItemGuid")); }
}
void Athena::RemoveNetDebugUI(){ UObject* w=FindObject(L"/Script/UMG.Default__WidgetBlueprintLibrary"); if(!w) return; TArray<UObject*> e{}; auto a=w->Call<TArray<UObject*>,0x8>("GetAllWidgetsOfClass",World,e,FindObject(L"/Game/Athena/HUD/NetDebugUI.NetDebugUI_C"),false); if(a.Count>0&&a[0]&&!IsBadPtr(a[0])) a[0]->Call("RemoveFromViewport"); }
void Athena::TeleportToSpawnIsland(){ UObject* c=FindObject(L"/Script/FortniteGame.FortPlayerStartWarmup"); if(!c||!GameStatics||!Pawn) return; TArray<UObject*> e{}; auto a=GameStatics->Call<TArray<UObject*>,0x10>("GetAllActorsOfClass",World,c,e); if(a.Count==0) return; int i=kismetMathLib->Call<int>("RandomIntegerInRange",0,a.MaxIndex()); Pawn->Call("K2_TeleportTo",a[i]->Call<FVector>("K2_GetActorLocation"),FRotator{0,0,0}); }
void Athena::ConsoleKey(){ UObject* is=FindObject(L"/Script/Engine.Default__InputSettings"); if(!is||!kismetStringLib) return; FName f6=kismetStringLib->Call<FName>("Conv_StringToName",FString(L"F6")); auto& k=is->Child<TArray<FKey>>("ConsoleKeys"); if(k.Count>1) k[1]=FKey{f6}; }
void Athena::CheatScript(const char* s){
    std::string t=s; std::transform(t.begin(),t.end(),t.begin(),::tolower);
    if(t.find("spawnweapon")!=std::string::npos){ std::string wn=t.substr(t.find(" ")+1); UObject* d=FindObject(ToWide(wn).c_str(),false,true); if(d) SpawnPickup(d,1,Pawn->Call<FVector>("K2_GetActorLocation")); }
}
void Athena::ServerHandlePickup(UObject* p){
    if(!p||!WorldInventory) return;
    UObject* d=p->Child<UObject*>("ItemDefinition"); int c=p->Child<int>("Count"); if(!d) return;
    UObject* e=FindObject(L"EFortItemType",false,true);
    char it=d->Child<char>("ItemType");
    if(it==(char)(e?e->EnumIndex("Ammo"):0)||it==(char)(e?e->EnumIndex("Trap"):0)||it==(char)(e?e->EnumIndex("WorldResource"):0)){
        AddToInventory(d,c,3,10); InventoryUpdate(); p->Call("K2_DestroyActor"); return;
    }
    UObject* bl=FindObject(L"/Script/BlueprintContext.Default__BlueprintContextLibrary");
    if(!bl||!GameViewport) return;
    UObject* lp=GameViewport->Child<UObject*>("GameInstance")->Child<TArray<UObject*>>("LocalPlayers")[0];
    UObject* ic=bl->Call<UObject*>("GetContext",lp,FindObject(L"/Script/FortniteUI.FortInventoryContext"));
    if(ic){ for(int i=0;i<6;i++){ if(!ic->Call<UObject*>("GetQuickBarSlottedItem",(char)0,i)){ AddToInventory(d,c,0,i); InventoryUpdate(); p->Call("K2_DestroyActor"); return; } } }
}
void Athena::FixBuildingFoundations(){ if(!GameStatics) return; auto a=GameStatics->Call<TArray<UObject*>,0x10>("GetAllActorsOfClass",World,FindObject(L"/Script/FortniteGame.BuildingFoundation"),TArray<UObject*>{}); for(UObject* b:a) if(b&&!IsBadPtr(b)) b->Child<char>("DynamicFoundationType")=0; }
void Athena::Fixbus(){ if(!GameState) return; GameState->Child<char>("GamePhase")=3; GameState->Call("OnRep_GamePhase",(char)2); if(!Pawn) return; Pawn->Call<bool>("K2_TeleportTo",FVector{0,0,22500},FRotator{0,0,0}); Pawn->Child<bool>("bIsSkydiving")=true; Pawn->Call("OnRep_IsSkydiving",false); }
void Athena::Loot(UObject* a){
    if(!a) return;
    std::string tier="None";
    UObject* bc=FindObject(L"BuildingContainer",false,true);
    if(bc&&a->IsChild(bc)){ FName n=a->Child<FName>("SearchLootTierGroup"); tier=n.ToString(); }
    if(tier.find("None")!=std::string::npos) return;
    auto r=PickLootDrops(tier);
    for(auto& x:r){ FVector rv=kismetMathLib->Call<FVector>("GetRightVector",a->Call<FRotator>("K2_GetActorRotation")); rv.X*=64; rv.Y*=64;
        FVector l=kismetMathLib->Call<FVector>("Add_VectorVector",a->Call<FVector>("K2_GetActorLocation"),rv); SpawnPickup(x.ItemDefinition,x.Count,l); }
}
void Athena::OnServerCreateBuildingActor(void* p){
    struct OldP { UObject* C; uint8 _p[0x8]; FVector L; FRotator R; };
    struct NewP { int32 CH; FVector L; FRotator R; uint8 _p[0xC]; UObject* C; };
    UObject* c=nullptr; FVector l{}; FRotator r{};
    auto* f=FindObject(L"/Script/FortniteGame.FortPlayerController.ServerCreateBuildingActor");
    if(f&&f->GetFunctionChildrenOffset().size()==1){ auto x=*(NewP*)p; c=x.C; l=x.L; r=x.R; } else { auto x=*(OldP*)p; c=x.C; l=x.L; r=x.R; }
    UObject* ac=SpawnActorEasy(c,l,r); if(!ac) return;
    auto* i=FindObject(L"/Script/FortniteGame.BuildingActor.InitializeKismetSpawnedBuildingActor");
    if(i&&i->GetFunctionChildrenOffset().size()==3) ac->Call("InitializeKismetSpawnedBuildingActor",ac,PlayerController,true);
    else ac->Call("InitializeKismetSpawnedBuildingActor",ac,PlayerController);
}
void Athena::OnBeginEditActor(UObject* pc){
    if(!EditToolItem||!Pawn) return;
    UObject* t=EditToolItem->Call<UObject*>("CreateTemporaryItemInstanceBP",1,1); if(!t) return;
    UObject* e=Pawn->Call<UObject*>("EquipWeaponDefinition",EditToolItem,t->Call<FGuid>("GetItemGuid")); if(!e) return;
    e->Child<UObject*>("EditActor")=pc; e->Call("OnRep_EditActor");
}
void Athena::OnFinishEditActor(UObject* a,UObject* nc,int ri,bool m,void* p){
    struct FP { UObject* BA; UObject* NC; int8 R; bool M; }; auto fp=*(FP*)p; if(!a) return;
    a->Child<bool>("bPlayDestructionEffects")=false; a->Call("SetActorScale3D",FVector{0,0,0}); a->Call("K2_DestroyActor");
    UObject* e=SpawnActorEasy(nc,EditComponentLocation,EditComponentRotation); if(!e) return;
    e->Call("SetMirrored",m);
    auto* i=FindObject(L"/Script/FortniteGame.BuildingActor.InitializeKismetSpawnedBuildingActor");
    if(i&&i->GetFunctionChildrenOffset().size()==3) e->Call("InitializeKismetSpawnedBuildingActor",e,PlayerController,true);
    else e->Call("InitializeKismetSpawnedBuildingActor",e,PlayerController);
}
void Athena::SpawnPickup(UObject* d,int c,FVector l,bool t){
    if(!d) return; UObject* p=SpawnActorEasy(FindObject(L"/Script/FortniteGame.FortPickupAthena"),l,FRotator{0,0,0}); if(!p) return;
    p->Child<UObject*>("ItemDefinition")=d; p->Child<int>("Count")=c; p->Call("OnRep_PrimaryPickupItemEntry"); p->Call("TossPickup",l,Pawn,1,t);
}
void Athena::GrantEffect(UObject* e,float l){ if(!e||!Pawn) return; UObject* a=Pawn->Child<UObject*>("AbilitySystemComponent"); if(!a) return; FGameplayEffectContextHandle c{}; a->Call<FActiveGameplayEffectHandle>("BP_ApplyGameplayEffectToSelf",e,l,c); }
void Athena::GrantAbility(UObject* k){
    if(!k||!Pawn) return;
    UObject* ge=FindObject(L"/Game/Athena/Items/Consumables/PurpleStuff/GE_Athena_PurpleStuff.GE_Athena_PurpleStuff_C");
    if(!ge) ge=FindObject(L"/Game/Athena/Items/Consumables/PurpleStuff/GE_Athena_PurpleStuff_Health.GE_Athena_PurpleStuff_Health_C");
    if(!ge) return;
    UObject* du=FindObject(L"/Script/FortniteGame.Default__FortAbilitySystemUI"); if(!du) return;
    UObject* de=du->Call<UObject*>("GetDefaultObjectOfGameplayEffectType",ge); if(!de) return;
    auto& ab=de->Child<TArray<FGameplayAbilitySpecDef>>("GrantedAbilities"); if(ab.Count>0) ab[0].Ability=k;
    de->Child<char>("DurationPolicy")=1;
    UObject* a=Pawn->Child<UObject*>("AbilitySystemComponent"); if(!a) return; FGameplayEffectContextHandle c{}; a->Call<FActiveGameplayEffectHandle>("BP_ApplyGameplayEffectToSelf",ge,1.f,c);
}
void Athena::GrantDefaultAbilities(){ GrantAbility(FindObject(L"/Script/FortniteGame.FortGameplayAbility_Sprint")); GrantAbility(FindObject(L"/Script/FortniteGame.FortGameplayAbility_Jump")); GrantAbility(FindObject(L"/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GA_DefaultPlayer_InteractUse.GA_DefaultPlayer_InteractUse_C")); GrantAbility(FindObject(L"/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GA_DefaultPlayer_InteractSearch.GA_DefaultPlayer_InteractSearch_C")); }
void Athena::OnAircraftJump(){
    if(!PlayerController) return;
    FVector l=PlayerController->Call<FVector>("GetFocalLocation"); FRotator r=PlayerController->Call<FRotator>("GetControlRotation"); r.Pitch=0; r.Roll=0;
    Pawn=SpawnActorEasy(FindObject(L"/Game/Athena/PlayerPawn_Athena.PlayerPawn_Athena_C"),l,r); if(!Pawn) return;
    PlayerController->Call("Possess",Pawn); Pawn->Call("OnRep_CustomizationLoadout"); ShowSkin(); bDroppedFromAircraft=true;
}
std::vector<Athena::LootData> Athena::PickLootDrops(std::string cat){
    std::vector<LootData> r;
    UObject* lt=FindObject(L"/Game/Items/Datatables/AthenaLootTierData_Client.AthenaLootTierData_Client",true);
    UObject* lp=FindObject(L"/Game/Items/Datatables/AthenaLootPackages_Client.AthenaLootPackages_Client",true);
    if(!lt||!lp||!GetDataTableRowFn||!DataTableFunctionLibrary) return r;
    UObject* ltr=lt->Child<UObject*>("RowStruct"); UObject* lpr=lp->Child<UObject*>("RowStruct"); if(!ltr||!lpr) return r;
    int32 ltsz=*(int32*)(int64(ltr)+offsets::StructSize); int32 lpsz=*(int32*)(int64(lpr)+offsets::StructSize);
    auto ln=DataTableFunctionLibrary->Call<TArray<FName>,0x8>("GetDataTableRowNames",lt,TArray<FName>{});
    auto pn=DataTableFunctionLibrary->Call<TArray<FName>,0x8>("GetDataTableRowNames",lp,TArray<FName>{});
    auto fo=[](UObject* rs,const std::string& n)->int32{ UObject* k=rs; while(k&&!IsBadPtr(k)){ UObject* c=*(UObject**)(int64(k)+offsets::Children);
        while(c&&!IsBadPtr(c)){ if(c->GetName()==n) return *(int32*)(int64(c)+offsets::Offset); c=*(UObject**)(int64(c)+offsets::Next); } k=*(UObject**)(int64(k)+offsets::SuperClass); } return -1; };
    int32 tg=fo(ltr,"TierGroup"), w=fo(ltr,"Weight"), nd=fo(ltr,"NumLootPackageDrops"), lpo=fo(ltr,"LootPackage");
    int32 lpid=fo(lpr,"LootPackageID"), lpc=fo(lpr,"LootPackageCall"), ido=fo(lpr,"ItemDefinition"), cnt=fo(lpr,"Count");
    std::map<std::string,void*> cr; float mw=0;
    for(FName n:ln){ void* row=calloc(1,ltsz); GetDataTableRowFn(lt,n,row); FName* t=(FName*)((uintptr_t)row+tg);
        if(tg>=0&&t->ToString()==cat){ float ww=*(float*)((uintptr_t)row+w); if(ww!=0.f){ cr[n.ToString()]=row; mw+=ww; } else free(row); } else free(row); }
    if(cr.empty()||mw==0){ for(auto&p:cr) free(p.second); return r; }
    float rv=kismetMathLib->Call<float>("RandomFloatInRange",0.f,mw); void* pr=nullptr;
    for(auto&p:cr){ float ww=*(float*)((uintptr_t)p.second+w); if(rv<ww){ pr=p.second; break; } rv-=ww; }
    if(!pr){ for(auto&p:cr) free(p.second); return r; }
    float n2=*(float*)((uintptr_t)pr+nd); std::string pk=((FName*)((uintptr_t)pr+lpo))->ToString();
    for(auto&p:cr) free(p.second); cr.clear();
    std::map<std::string,void*> pkR;
    for(FName n:pn){ void* row=calloc(1,lpsz); GetDataTableRowFn(lp,n,row); if(((FName*)((uintptr_t)row+lpid))->ToString()==pk) pkR[n.ToString()]=row; else free(row); }
    for(auto&p:pkR){ auto* cs=(FUEString*)((uintptr_t)p.second+lpc); std::string call;
        if(cs->data&&cs->length>0){ char* t=FUEToCStr(cs); if(t){ call=t; free(t); } }
        if(pk.find(".Empty")!=std::string::npos) continue;
        std::map<std::string,void*> tc; float tm=0;
        if(!strstr(pk.c_str(),"WorldList")){ for(FName n:pn){ void* row=calloc(1,lpsz); GetDataTableRowFn(lp,n,row); if(((FName*)((uintptr_t)row+lpid))->ToString()==call) tc[n.ToString()]=row; else free(row); } }
        else for(auto&x:pkR) tc[x.first]=x.second;
        for(auto&x:tc) tm+=*(float*)((uintptr_t)x.second+w);
        float tr=kismetMathLib->Call<float>("RandomFloatInRange",0.f,tm);
        for(auto&x:tc){ float ww=*(float*)((uintptr_t)x.second+w); if(tr<ww){ SoftObjectPtr* ref=(SoftObjectPtr*)((uintptr_t)x.second+ido);
            UObject* d=kismetSystemLib->Call<UObject*>("Conv_SoftObjectReferenceToObject",*ref); if(d) r.push_back(LootData{d,*(int32*)((uintptr_t)x.second+cnt)}); break; } tr-=ww; }
        if(!strstr(pk.c_str(),"WorldList")) for(auto&x:tc) free(x.second);
    }
    for(auto&p:pkR) free(p.second);
    return r;
}
void Athena::SpawnFloorLoot(){
    if(!GameStatics) return;
    auto a=GameStatics->Call<TArray<UObject*>,0x10>("GetAllActorsOfClass",World,FindObject(L"Tiered_Athena_FloorLoot_01_C",false,true),TArray<UObject*>{});
    for(UObject* e:a) if(e&&!IsBadPtr(e)){ auto l=PickLootDrops("Loot_AthenaFloorLoot"); for(auto& x:l) SpawnPickup(x.ItemDefinition,x.Count,e->Call<FVector>("K2_GetActorLocation"),false); }
}
void Athena::SpawnBuildPreviews(){ UObject* pc=FindObject(L"BuildingPlayerPrimitivePreview",false,true); if(!pc||!PlayerController) return;
    auto b=[&](const wchar_t* m,char t)->UObject*{ UObject* a=SpawnActorEasy(pc,FVector{0,0,0},FRotator{0,0,0}); if(!a) return nullptr;
        UObject* mc=a->Call<UObject*>("GetBuildingMeshComponent"); if(mc){ mc->Call<bool>("SetStaticMesh",FindObject(m,false,true)); mc->Call("SetMaterial",0,PlayerController->Child<UObject*>("BuildPreviewMarkerMID")); }
        a->Call("OnBuildingActorInitialized",(char)5,t); a->Call("SetActorHiddenInGame",true); return a; };
    Wall=b(L"PBW_W1_Solid",0); Floor=b(L"PBW_W1_Floor",1); Stairs=b(L"PBW_W1_StairW",5); Roof=b(L"PBW_W1_RoofC",6); }
void Athena::PlayEmoteItem(UObject* i){ if(!i||!Pawn) return; if(!FindObject(L"GetAnimationHardReference",false,true)) return; UObject* m=i->Call<UObject*>("GetAnimationHardReference",(char)6,(char)3); if(IsBadPtr(m)||!m) return; UObject* ai=Pawn->Child<UObject*>("Mesh")->Call<UObject*>("GetAnimInstance"); if(!ai) return; ai->Call<float>("Montage_Play",m,1.f,(char)0,0.f,true); bIsEmoting=true; }
void Athena::Tick(){
    if(bIsEmoting&&PlayerController&&!PlayerController->Call<bool>("IsInAircraft")){ bool mv=PlayerController->Child<bool>("bIsPlayerActivelyMoving");
        UObject* ai=Pawn?Pawn->Child<UObject*>("Mesh")->Call<UObject*>("GetAnimInstance"):nullptr;
        if(ai){ bool j=ai->Child<bool>("bIsJumping"), f=ai->Child<bool>("bIsFalling"); if(mv||j||f){ bIsEmoting=false; Pawn->Call("ServerRootMotionInterruptNotifyStopMontage",Pawn->Call<UObject*>("GetCurrentMontage")); } } }
    if(g_bIsVehicleVersion&&Pawn){ bool v=Pawn->Call<bool>("IsInVehicle"); Pawn->Child<char>("Role")=v?(char)2:(char)3; if(v){ UObject* ve=Pawn->Call<UObject*>("GetVehicle"); if(ve&&!IsBadPtr(ve)) ve->Child<char>("Role")=(char)2; } }
    if(Pawn){ auto p=&Pawn->Child<bool>("bIsOutsideSafeZone"); if(!IsBadPtr(p)){ if((*p&&!l_bIsOutsideSafeZone)||(!*p&&l_bIsOutsideSafeZone)){ l_bIsOutsideSafeZone=!l_bIsOutsideSafeZone; Pawn->Call("OnRep_IsOutsideSafeZone"); } } }
}
void Athena::OnExitVehicle(){ if(!Pawn) return; Pawn->Child<char>("Role")=3; UObject* v=Pawn->Call<UObject*>("GetVehicle"); if(v&&!IsBadPtr(v)) v->Child<char>("Role")=3; }
void Athena::DropInventoryItem(FGuid g,int c,bool cc){
    if(!WorldInventory) return; auto ins=WorldInventory->Child<TArray<UObject*>>("ItemInstances");
    for(int i=0;i<ins.Count;i++){ if(!ins[i]) continue;
        if(kismetGuidLib->Call<bool>("EqualEqual_GuidGuid",ins[i]->Call<FGuid>("GetItemGuid"),g)){
            UObject* cd=ins[i]->Call<UObject*>("GetItemDefinitionBP"); int df=ins[i]->Child<int>("Count")-c;
            if(GenericArray_RemoveFn){ TArray<UObject*> ia=WorldInventory->Child<TArray<UObject*>>("ItemInstances"); TArray<char> ra=WorldInventory->Child<TArray<char>>("ReplicatedEntries"); GenericArray_RemoveFn(&ia,nullptr,i); GenericArray_RemoveFn(&ra,nullptr,i); }
            if(df!=0&&cc){ AddToInventory(cd,df,0,10); InventoryUpdate(); SpawnPickup(cd,c,Pawn->Call<FVector>("K2_GetActorLocation")); return; }
            InventoryUpdate(); SpawnPickup(cd,c,Pawn->Call<FVector>("K2_GetActorLocation")); return;
        } }
}

// ============================================================
// FCurl hook (Owen's approach) + curl_easy_setopt (backup)
// ============================================================
static FUEString* FCurlGetUrl(void* req){ return (FUEString*)((uintptr_t)req + (uintptr_t)ue_geturl_off); }

static int FCurlProcessRequestHook(void* req){
    if(!req||!FCurlOGProcessRequest) return FCurlOGProcessRequest?FCurlOGProcessRequest(req):0;
    if(!FCurlSetUrl) return FCurlOGProcessRequest(req);
    FUEString* u=FCurlGetUrl(req);
    if(!u||IsBadPtr(u)||u->length<=0||!u->data) return FCurlOGProcessRequest(req);
    char* og=FUEToCStr(u); if(!og) return FCurlOGProcessRequest(req);
    LOGI("[FCurl] URL=%s",og);
    struct Url url; Url_Parse(og,&url);
    if(ShouldRedirect(url.host)){
        char nu[URL_HOST_MAX+URL_PATH_MAX]; CreateUrl(g_backend_url,url.pathAndQuery,nu,sizeof(nu));
        LOGI("[FCurl] REWRITE -> %s",nu);
        FUEString n=FUEFromCStr(nu);
        FCurlSetUrl(req,&n);
        if(n.data) free(n.data);
    }
    free(og);
    return FCurlOGProcessRequest(req);
}

static int CurlSetoptHook(void* h,int opt,void* arg){
    if(!curlOGSetopt) return -1;
    if(opt==10002 && arg){
        const char* u=(const char*)arg;
        LOGI("[curl] URL=%s",u);
        struct Url url; Url_Parse(u,&url);
        if(ShouldRedirect(url.host)){
            static __thread char nu[URL_HOST_MAX+URL_PATH_MAX]; CreateUrl(g_backend_url,url.pathAndQuery,nu,sizeof(nu));
            LOGI("[curl] REWRITE -> %s",nu);
            return curlOGSetopt(h,opt,(void*)nu);
        }
    }
    if(opt==64||opt==81) return curlOGSetopt(h,opt,(void*)0);
    return curlOGSetopt(h,opt,arg);
}

// ============================================================
// ProcessEvent
// ============================================================
static void OnReadyToStartMatch(){
    LOGI(">>> ReadyToStartMatch");
    bLoadedInMatch=true; bInFrontend=false;
    LOGI(">>> SpawnPawn"); Athena::SpawnPawn();
    if(Pawn){ LOGI(">>> ShowSkin"); Athena::ShowSkin(); }
    if(CheatManager){ LOGI(">>> DestroyLods"); Athena::DestroyLods(); }
    LOGI(">>> DropLoadingScreen"); Athena::DropLoadingScreen();
    LOGI(">>> FixBuildingFoundations"); Athena::FixBuildingFoundations();
    if(Pawn){ LOGI(">>> InitInventory"); Athena::InitializeInventory(); Athena::GrantDefaultAbilities(); }
    LOGI(">>> SpawnBuildPreviews"); Athena::SpawnBuildPreviews();
    LOGI(">>> exit");
}
static void OnServerLoadingScreenDropped(){
    LOGI(">>> ServerLoadingScreenDropped");
    if(bInFrontend){ bPressedPlay=false; bLoadedInMatch=false; bDroppedFromAircraft=false; return; }
    Athena::RemoveNetDebugUI(); Athena::TeleportToSpawnIsland(); Athena::Fixbus(); Athena::SpawnFloorLoot();
}
static void* ProcessEventHook(UObject* s,UObject* f,void* p){
    if(!f||!orig_ProcessEvent) return orig_ProcessEvent(s,f,p);
    std::string n=f->GetName(); if(n.empty()) return orig_ProcessEvent(s,f,p);
    if(n.find("ReadyToStartMatch")!=std::string::npos && !bLoadedInMatch) OnReadyToStartMatch();
    else if(n.find("ServerHandlePickup")!=std::string::npos) Athena::ServerHandlePickup(*(UObject**)p);
    else if(n.find("GiveWeapon")!=std::string::npos){ struct P{FString Item;int Slot;int Count;}; auto* x=(P*)p; UObject* d=FindObject(x->Item.ToWString().c_str(),false,true); if(d) Athena::AddToInventory(d,x->Count,0,x->Slot); Athena::InventoryUpdate(); return nullptr; }
    else if(n.find("ServerCreateBuildingActor")!=std::string::npos) Athena::OnServerCreateBuildingActor(p);
    else if(n.find("ServerBeginEditingBuildingActor")!=std::string::npos) Athena::OnBeginEditActor(*(UObject**)p);
    else if(n.find("ServerEditBuildingActor")!=std::string::npos){ struct P{UObject* B;UObject* N;int R;bool M;}; auto* x=(P*)p; Athena::OnFinishEditActor(x->B,x->N,x->R,x->M,p); }
    else if(n.find("ServerLoadingScreenDropped")!=std::string::npos && bLoadedInMatch) OnServerLoadingScreenDropped();
    else if(n.find("ServerExecuteInventoryItem")!=std::string::npos) Athena::OnServerExecuteInventoryItem(*(FGuid*)p);
    else if(n.find("ServerExecuteInventoryWeapon")!=std::string::npos) Athena::OnServerExecuteInventoryWeapon(*(UObject**)p);
    else if(n.find("ServerAttemptAircraftJump")!=std::string::npos && !bDroppedFromAircraft) Athena::OnAircraftJump();
    else if(n.find("CheatScript")!=std::string::npos) Athena::CheatScript(((FString*)p)->ToString().c_str());
    else if(n.find("ServerAttemptInteract")!=std::string::npos) Athena::Loot(*(UObject**)p);
    else if(n.find("ServerPlayEmoteItem")!=std::string::npos) Athena::PlayEmoteItem(*(UObject**)p);
    else if(n.find("OnPawnEnterVehicle")!=std::string::npos) g_bIsVehicleVersion=true;
    else if(n.find("ServerAttemptExitVehicle")!=std::string::npos) Athena::OnExitVehicle();
    else if(n.find("Tick")!=std::string::npos && s==PlayerController) Athena::Tick();
    return orig_ProcessEvent(s,f,p);
}

// ============================================================
// Version detection + CoreSetup
// ============================================================
static uintptr_t FindStr(uintptr_t s, uintptr_t sz, const char* t, int wide){
    if(!wide){ size_t tl=strlen(t); for(uintptr_t a=s;a<s+sz-tl;a++) if(memcmp((void*)a,t,tl)==0) return a; }
    else { size_t tl=strlen(t); for(uintptr_t a=s;a<s+sz-tl*2;a++){ int m=1; for(size_t i=0;i<tl;i++) if(((uint8_t*)a)[i*2]!=(uint8_t)t[i]||((uint8_t*)a)[i*2+1]!=0){m=0;break;} if(m) return a; } }
    return 0;
}
static void DetectVersion(uintptr_t base, size_t size){
    for(auto& o:ue_offsets){
        if(strlen(o.version)<10) continue;
        if(FindStr(base,size,o.version,1)||FindStr(base,size,o.version,0)){
            detected_version=o.version; ue_pr_offset=o.pr_offset; ue_geturl_off=o.geturl_off; ue_seturl_off=o.seturl_off;
            LOGI("Version detected: %s",o.version); return;
        }
    }
    LOGE("Version NOT detected");
}

static bool TryInitGObjects(){
    if(!g_base) return false;
    if(!gGObjects) gGObjects=(GObjects_t*)(g_base+RVAs::GObjects);
    if(IsBadPtr(gGObjects)) return false;
    void* o=gGObjects->Objects; if(IsBadPtr(o)) return false;
    int32 n=gGObjects->NumElements; if(n<=0||n>0x2000000) return false;
    return true;
}
static void InitializeGlobals(){
    if(!gGObjects) return;
    for(int32 i=0;i<gGObjects->NumElements;i++){
        UObject* o=gGObjects->GetById(i); if(!o||IsBadPtr(o)) continue;
        std::string n=o->GetName();
        if(!PlayerController && (n=="FortPlayerControllerAthena"||n=="FortPlayerController")) PlayerController=o;
        if(!GameState && (n=="FortGameStateAthena"||n=="FortGameState")) GameState=o;
        if(!GameMode && (n=="FortGameModeAthena"||n=="FortGameMode")) GameMode=o;
        if(PlayerController && GameState && GameMode) break;
    }
    LOGI("PC=%p GS=%p GM=%p",PlayerController,GameState,GameMode);
    if(PlayerController){
        World=PlayerController->Call<UObject*>("GetWorld");
        auto pp=&PlayerController->Child<UObject*>("Player");
        if(!IsBadPtr(pp)&&*pp){ auto vp=&(*pp)->Child<UObject*>("ViewportClient"); if(!IsBadPtr(vp)) GameViewport=*vp; }
        CheatManager=FindObject(L"/Script/FortniteGame.FortCheatManager");
        if(CheatManager) PlayerController->Child<UObject*>("CheatManager")=CheatManager;
    }
    if(World){ auto pl=&World->Child<UObject*>("PersistentLevel");
        if(!IsBadPtr(pl)&&*pl){ auto ws=&(*pl)->Child<UObject*>("WorldSettings"); if(!IsBadPtr(ws)) WorldSettings=*ws; } }
    GameStatics=FindObject(L"/Script/Engine.Default__GameplayStatics");
    kismetMathLib=FindObject(L"/Script/Engine.Default__KismetMathLibrary");
    kismetStringLib=FindObject(L"/Script/Engine.Default__KismetStringLibrary");
    kismetSystemLib=FindObject(L"/Script/Engine.Default__KismetSystemLibrary");
    kismetGuidLib=FindObject(L"/Script/Engine.Default__KismetGuidLibrary");
    DataTableFunctionLibrary=FindObject(L"/Script/Engine.Default__DataTableFunctionLibrary");
    Athena::ConsoleKey();
}

static void CoreSetup(){
    LOGI("=== CoreSetup enter ===");
    g_base=LibFind("libUnreal.so");
    LOGI("g_base=%p",(void*)g_base);
    if(!g_base) return;

    FNameToStringFn=(decltype(FNameToStringFn))(g_base+RVAs::FNameToString);
    FreeInternalFn=(decltype(FreeInternalFn))(g_base+RVAs::FreeInternal);
    ReallocFn=(decltype(ReallocFn))(g_base+RVAs::Realloc);
    SpawnActorFn=(decltype(SpawnActorFn))(g_base+RVAs::SpawnActor_Thunk);
    GetDataTableRowFn=(decltype(GetDataTableRowFn))(g_base+RVAs::GetDataTableRowFromName_ORIG);
    GenericArray_RemoveFn=(decltype(GenericArray_RemoveFn))(g_base+RVAs::GenericArray_Remove);

    void* h=dlopen("libUnreal.so",RTLD_NOW|RTLD_NOLOAD);
    if(h){ StaticFindObjectFn=(decltype(StaticFindObjectFn))dlsym(h,"StaticFindObject"); StaticLoadObjectFn=(decltype(StaticLoadObjectFn))dlsym(h,"StaticLoadObject"); }
    LOGI("StaticFindObject=%p",StaticFindObjectFn);

    void* pPE=(void*)(g_base+RVAs::ProcessEvent);
    if(DobbyHook(pPE,(void*)ProcessEventHook,(void**)&orig_ProcessEvent)==0){ PE=reinterpret_cast<void*(*)(void*,void*,void*)>(orig_ProcessEvent); LOGI("ProcessEvent hooked @ %p",pPE); }
    else LOGE("ProcessEvent hook FAILED");

    uintptr_t libsz = 0x10000000;
    FILE* mf=fopen("/proc/self/maps","r");
    if(mf){ char line[512]; while(fgets(line,sizeof(line),mf)){ if(strstr(line,"libUnreal.so")){ uintptr_t s,e; if(sscanf(line,"%lx-%lx",&s,&e)==2) if(e-s>libsz) libsz=e-g_base; } } fclose(mf); }
    DetectVersion(g_base,libsz);

    if(ue_pr_offset){
        void* pr=(void*)(g_base+ue_pr_offset);
        FCurlSetUrl=(decltype(FCurlSetUrl))(g_base+ue_seturl_off);
        if(DobbyHook(pr,(void*)FCurlProcessRequestHook,(void**)&FCurlOGProcessRequest)==0){
            LOGI("FCurl hooked @ %p (SetUrl=%p, GetUrlOff=0x%x)",pr,(void*)FCurlSetUrl,ue_geturl_off);
        } else LOGE("FCurl hook FAILED");
    }

    void* pCurl=(void*)(g_base+RVAs::curl_easy_setopt);
    if(DobbyHook(pCurl,(void*)CurlSetoptHook,(void**)&curlOGSetopt)==0) LOGI("curl_easy_setopt hooked @ %p",pCurl);
    else LOGE("curl hook FAILED");

    LOGI("=== CoreSetup done ===");
}

static void* GObjectsWatch(void*){
    int t=0; while(!TryInitGObjects() && t++<600) usleep(100000);
    if(TryInitGObjects()){ LOGI("GObjects ready after %d tries: Objects=%p NumElements=%d",t,(void*)gGObjects->Objects,gGObjects->NumElements); InitializeGlobals(); }
    else LOGE("GObjects never ready");
    return nullptr;
}

static void* MainThread(void*){
    LOGI("EraCH3 starting");
    InitLogFile();
    for(int i=0;i<600;i++){ if(LibFind("libUnreal.so")) break; usleep(100000); }
    if(!LibFind("libUnreal.so")){ LOGE("wait timeout"); return nullptr; }
    usleep(300000);
    CoreSetup();
    pthread_t gt; pthread_create(&gt,nullptr,GObjectsWatch,nullptr); pthread_detach(gt);
    return nullptr;
}

extern "C" __attribute__((constructor))
void libEraCH3_main(){ pthread_t t; pthread_create(&t,nullptr,MainThread,nullptr); pthread_detach(t); }

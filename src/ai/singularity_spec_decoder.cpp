#include "singularity_spec_decoder.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <immintrin.h>
#include <memory>
#include <random>

#ifdef _MSC_VER
#include <intrin.h>
inline int sgz_ctz(int x) { unsigned long r; _BitScanForward(&r,(unsigned long)x); return (int)r; }
#define SGZ_CPUIDEX(info,f,sf) __cpuidex(info,f,sf)
#else
inline int sgz_ctz(int x) { return __builtin_ctz(x); }
#define SGZ_CPUIDEX(info,f,sf) __cpuid_count(f,sf,info[0],info[1],info[2],info[3])
#endif

namespace rxd::ai {
namespace {

constexpr uint32_t kNF    = 0xFFFFFFFFu;
constexpr uint64_t kEmpty = 0xFFFFFFFFFFFFFFFFull;

// ---------------------------------------------------------------
// Hash
// ---------------------------------------------------------------
inline uint64_t Mix(uint64_t x) {
    x ^= x>>33; x *= 0xff51afd7ed558ccdULL;
    x ^= x>>33; x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x>>33; return x;
}
inline uint64_t Key2(uint32_t a,uint32_t b)
    { return Mix((uint64_t(a)<<32)|b); }
inline uint64_t Key3(uint32_t a,uint32_t b,uint32_t c)
    { return Mix(Key2(a,b)^(c*0x9e3779b97f4a7c15ULL)); }
inline uint64_t Key4(uint32_t a,uint32_t b,uint32_t c,uint32_t d)
    { return Mix(Key3(a,b,c)^(d*0x9e3779b97f4a7c15ULL)); }
inline uint64_t Key5(uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t e)
    { return Mix(Key4(a,b,c,d)^(e*0x9e3779b97f4a7c15ULL)); }

// ---------------------------------------------------------------
// CPU feature detection
// ---------------------------------------------------------------
inline bool HasAVX512() {
#ifdef __AVX512F__
    return true;
#else
    static int c = -1;
    if (c == -1) { int i[4]={}; SGZ_CPUIDEX(i,7,0); c = (i[1]&(1<<16))?1:0; }
    return c==1;
#endif
}

// ---------------------------------------------------------------
// SIMD primitives
// ---------------------------------------------------------------
inline float FastExp(float x) {
    x = std::max(-80.f, std::min(80.f, x));
    union { float f; int32_t i; } u;
    u.i = (int32_t)(12102203.161561485f*x + 1064866805.f);
    return u.f;
}

uint32_t ArgmaxSIMD(const float* d, uint32_t n) {
    if (!n) return 0;
    if (HasAVX512() && n >= 16) {
        __m512  best = _mm512_loadu_ps(d);
        __m512i idx  = _mm512_setr_epi32(0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15);
        __m512i inc  = _mm512_set1_epi32(16), cur = idx;
        uint32_t i = 0;
        for (; i+16<=n; i+=16) {
            __m512  v  = _mm512_loadu_ps(d+i);
            __mmask16 gt = _mm512_cmp_ps_mask(v,best,_CMP_GT_OQ);
            best = _mm512_mask_blend_ps(gt,best,v);
            idx  = _mm512_mask_blend_epi32(gt,idx,cur);
            cur  = _mm512_add_epi32(cur,inc);
        }
        alignas(64) float bf[16]; alignas(64) int32_t bi[16];
        _mm512_store_ps(bf,best); _mm512_store_epi32(bi,idx);
        float mv=bf[0]; uint32_t mi=bi[0];
        for (int j=1;j<16;++j) if(bf[j]>mv){mv=bf[j];mi=bi[j];}
        for (;i<n;++i) if(d[i]>mv){mv=d[i];mi=i;}
        return mi;
    }
    uint32_t best=0;
    for (uint32_t i=1;i<n;++i) if(d[i]>d[best]) best=i;
    return best;
}

void SoftmaxSIMD(float* d, uint32_t n) {
    if (!n) return;
    float mx=d[0]; for(uint32_t i=1;i<n;++i) mx=std::max(mx,d[i]);
    float sum=0.f;
    for(uint32_t i=0;i<n;++i){ d[i]=FastExp(d[i]-mx); sum+=d[i]; }
    float inv=1.f/sum;
    if (HasAVX512()&&n>=16) {
        __m512 vi=_mm512_set1_ps(inv); uint32_t i=0;
        for(;i+16<=n;i+=16) _mm512_storeu_ps(d+i,_mm512_mul_ps(_mm512_loadu_ps(d+i),vi));
        for(;i<n;++i) d[i]*=inv;
    } else {
        for(uint32_t i=0;i<n;++i) d[i]*=inv;
    }
}

// ---------------------------------------------------------------
// Fast RNG (Xorshift64)
// ---------------------------------------------------------------
class FastRNG {
    uint64_t s;
public:
    FastRNG(uint64_t seed=123456789) : s(seed) {}
    uint32_t Next() { s^=s>>12; s^=s<<25; s^=s>>27; return (uint32_t)(s*0x2545F4914F6CDD1DULL); }
    float Uniform() { return Next()/(float)UINT32_MAX; }
};

// ---------------------------------------------------------------
// 4-way SIMD Cuckoo table (one 64-byte cache-line = 4 slots)
// ---------------------------------------------------------------
struct alignas(64) Bucket {
    uint64_t key[4];
    uint32_t val[4];
    uint32_t freq[4];
    uint32_t pad[4];
};

class CuckooTable {
    std::unique_ptr<Bucket[]> b_;
    size_t mask_;
public:
    explicit CuckooTable(uint32_t log2) {
        size_t n=1u<<log2; b_.reset(new Bucket[n]()); mask_=n-1;
        for(size_t i=0;i<n;++i) for(int j=0;j<4;++j) b_[i].key[j]=kEmpty;
    }
    uint32_t Lookup(uint64_t key) const {
        uint64_t h=Mix(key);
        for(int r=0;r<4;++r){
            size_t idx=(h+r)&mask_;
            const Bucket& b=b_[idx];
            __m256i kv=_mm256_load_si256((__m256i*)b.key);
            __m256i t =_mm256_set1_epi64x(key);
            __m256i c =_mm256_cmpeq_epi64(kv,t);
            int m=_mm256_movemask_pd(_mm256_castsi256_pd(c));
            if(m) return b.val[sgz_ctz(m)];
        }
        return kNF;
    }
    void Insert(uint64_t key, uint32_t val) {
        uint64_t h=Mix(key);
        for(int r=0;r<4;++r){
            size_t idx=(h+r)&mask_;
            Bucket& b=b_[idx];
            __m256i kv=_mm256_load_si256((__m256i*)b.key);
            __m256i t =_mm256_set1_epi64x(key);
            __m256i c =_mm256_cmpeq_epi64(kv,t);
            int m=_mm256_movemask_pd(_mm256_castsi256_pd(c));
            if(m){ int s=sgz_ctz(m); b.val[s]=val; b.freq[s]++; return; }
            int ev=0; uint32_t mf=b.freq[0];
            for(int j=1;j<4;++j) if(b.freq[j]<mf){mf=b.freq[j];ev=j;}
            if(mf<3){ b.key[ev]=key; b.val[ev]=val; b.freq[ev]=1; return; }
        }
        size_t idx=h&mask_; int slot=int(h&3);
        b_[idx].key[slot]=key; b_[idx].val[slot]=val; b_[idx].freq[slot]=1;
    }
};

// ---------------------------------------------------------------
// Octo-Gram (8-token exact match)
// ---------------------------------------------------------------
class OctoGramCache {
    CuckooTable tbl_;
public:
    explicit OctoGramCache(uint32_t log2) : tbl_(log2) {}
    static uint64_t Hash8(const uint32_t* s) {
        uint64_t h=0x9e3779b97f4a7c15ULL;
        for(int i=0;i<8;++i) h=Mix(h^(s[i]+0x9e3779b97f4a7c15ULL));
        return h;
    }
    uint32_t Lookup(const uint32_t* s) const { return tbl_.Lookup(Hash8(s)); }
    void Insert(const uint32_t* s, uint32_t nxt) { tbl_.Insert(Hash8(s),nxt); }
};

// ---------------------------------------------------------------
// MinHash Semantic Cache
// ---------------------------------------------------------------
class MinHashCache {
    struct alignas(64) Row {
        uint32_t sig[8];
        uint32_t topk[8];
        float    prob[8];
        uint32_t freq;
        uint32_t pad[3];
    };
    std::unique_ptr<Row[]> mem_;
    uint32_t mask_;
public:
    // slots = actual power-of-2 count
    explicit MinHashCache(uint32_t slots) {
        mem_.reset(new Row[slots]()); mask_=slots-1;
    }
    static void Signature(const uint32_t* seq, uint32_t len, uint32_t* out) {
        for(int p=0;p<8;++p){
            uint32_t mn=0xFFFFFFFFu, h=p*0x811c9dc5u;
            for(uint32_t i=0;i<len;++i){ h=(h*0x01000193u)^(seq[i]+p*0x9e3779b9u); mn=std::min(mn,h); }
            out[p]=mn;
        }
    }
    void Store(const uint32_t* seq, uint32_t len, const uint32_t* topk, const float* prob) {
        uint32_t sig[8]; Signature(seq,len,sig);
        uint32_t h=sig[0]^sig[1]^sig[2]^sig[3];
        Row& r=mem_[h&mask_];
        if(r.freq<2){
            std::memcpy(r.sig,sig,32); 
            for(int i=0;i<8;++i){r.topk[i]=topk[i];r.prob[i]=prob[i];}
            r.freq=1;
        } else {
            int match=0; for(int i=0;i<8;++i) if(r.sig[i]==sig[i]) ++match;
            if(match>=6){ for(int i=0;i<8;++i){r.topk[i]=topk[i];r.prob[i]=prob[i];} r.freq++; }
            else if(r.freq>1) r.freq--;
        }
    }
    uint32_t Retrieve(const uint32_t* seq, uint32_t len,
                      uint32_t* out_tok, float* out_prob, uint32_t max_out) const {
        uint32_t sig[8]; Signature(seq,len,sig);
        uint32_t h=sig[0]^sig[1]^sig[2]^sig[3];
        const Row& r=mem_[h&mask_];
        if(!r.freq) return 0;
        int match=0; for(int i=0;i<8;++i) if(r.sig[i]==sig[i]) ++match;
        if(match<6) return 0;
        uint32_t n=std::min(max_out,8u);
        for(uint32_t i=0;i<n;++i){out_tok[i]=r.topk[i];out_prob[i]=r.prob[i];}
        return n;
    }
};

// ---------------------------------------------------------------
// Syntax Oracle
// ---------------------------------------------------------------
class SyntaxOracle {
    int brace_=0, paren_=0, bracket_=0;
    uint32_t pending_[8]; int pend_n_=0;
public:
    void Observe(uint32_t tok) {
        if(tok==299||tok==321||tok==512 ||tok==1024) brace_++;
        if(tok==300||tok==322||tok==513 ||tok==1025) brace_--;
        if(tok==301||tok==323) paren_++;
        if(tok==302||tok==324) paren_--;
        if(tok==303||tok==325) bracket_++;
        if(tok==304||tok==326) bracket_--;
    }
    uint32_t Predict(uint32_t* conf) const {
        if(pend_n_>0){ *conf=255; return pending_[pend_n_-1]; }
        if(brace_>0){   *conf=200; return 300; }
        if(paren_>0){   *conf=180; return 302; }
        if(bracket_>0){ *conf=180; return 304; }
        *conf=0; return kNF;
    }
};

// ---------------------------------------------------------------
// Variable / Scope Predictor
// ---------------------------------------------------------------
class VarPredictor {
    struct Scope { uint32_t vars[16]; uint32_t n; };
    Scope scopes_[64]; uint32_t sp_=0;
public:
    void Enter() { if(sp_<64){ scopes_[sp_].n=0; ++sp_; } }
    void Exit()  { if(sp_>0) --sp_; }
    void Decl(uint32_t v) {
        if(!sp_) Enter();
        Scope& s=scopes_[sp_-1];
        if(s.n<16) s.vars[s.n++]=v;
    }
    uint32_t Predict(const uint32_t* prefix, uint32_t plen, uint32_t* conf) const {
        if(!plen||!sp_){ *conf=0; return kNF; }
        for(int si=(int)sp_-1;si>=0;--si){
            const Scope& s=scopes_[si];
            for(int i=(int)s.n-1;i>=0;--i)
                if(s.vars[i]==prefix[plen-1]){ *conf=150; return s.vars[i]; }
        }
        *conf=0; return kNF;
    }
};

// ---------------------------------------------------------------
// BFloat16 MLP: embed(256) -> hidden(256) -> slim_vocab(1024)
// ---------------------------------------------------------------
class BFloat16MLP {
    uint32_t vocab_, embed_, hidden_, out_;
    std::unique_ptr<uint16_t[]> w1_; // [hidden][embed]
    std::unique_ptr<float[]>    b1_;
    std::unique_ptr<uint16_t[]> w2_; // [out][hidden]
    std::unique_ptr<float[]>    b2_;
    std::unique_ptr<float[]>    emb_; // [vocab][embed]

    static float B2F(uint16_t v){ union{float f;uint32_t u;} x; x.u=uint32_t(v)<<16; return x.f; }
    static uint16_t F2B(float v){ union{float f;uint32_t u;} x; x.f=v; return uint16_t(x.u>>16); }

    void MV(const uint16_t* w, const float* x, float* out, uint32_t R, uint32_t C) {
        for(uint32_t r=0;r<R;++r){
            float sum=0; uint32_t c=0;
            const uint16_t* row=w+r*C;
            if(HasAVX512()&&C>=16){
                __m512 acc=_mm512_setzero_ps();
                for(;c+16<=C;c+=16){
                    __m256i w16=_mm256_loadu_si256((__m256i*)(row+c));
                    __m512  w32=_mm512_castsi512_ps(_mm512_slli_epi32(_mm512_cvtepu16_epi32(w16),16));
                    acc=_mm512_fmadd_ps(w32,_mm512_loadu_ps(x+c),acc);
                }
                alignas(64) float t[16]; _mm512_store_ps(t,acc);
                for(int j=0;j<16;++j) sum+=t[j];
            }
            for(;c<C;++c) sum+=B2F(row[c])*x[c];
            out[r]=sum;
        }
    }
public:
    BFloat16MLP(uint32_t vocab,uint32_t embed,uint32_t hidden,uint32_t out)
        : vocab_(vocab), embed_(embed), hidden_(hidden), out_(out) {
        w1_.reset(new uint16_t[hidden*embed]());
        b1_.reset(new float[hidden]());
        w2_.reset(new uint16_t[out*hidden]());
        b2_.reset(new float[out]());
        emb_.reset(new float[vocab*embed]());
        std::mt19937 rng(42);
        std::normal_distribution<float> d(0.f,0.02f);
        for(uint32_t i=0;i<vocab*embed;++i) emb_[i]=d(rng);
        for(uint32_t i=0;i<hidden*embed;++i) w1_[i]=F2B(d(rng));
        for(uint32_t i=0;i<out*hidden;++i)   w2_[i]=F2B(d(rng));
    }
    uint32_t Predict(const uint32_t* hist, uint32_t hl, float* out_logits) {
        if(!hl) return kNF;
        uint32_t last=hist[hl-1];
        if(last>=vocab_) return kNF;
        alignas(64) float x[256], h[256];
        std::memcpy(x, emb_.get()+last*embed_, embed_*sizeof(float));
        MV(w1_.get(),x,h,hidden_,embed_);
        for(uint32_t i=0;i<hidden_;++i){ h[i]+=b1_[i]; h[i]=std::max(0.f,h[i]); }
        MV(w2_.get(),h,out_logits,out_,hidden_);
        for(uint32_t i=0;i<out_;++i) out_logits[i]+=b2_[i];
        return ArgmaxSIMD(out_logits,out_);
    }
    void Train(const uint32_t* seq, uint32_t len, float lr) {
        if(len<2) return;
        alignas(64) float x[256], h[256], o[1024], gh[256];
        for(uint32_t t=0;t+1<len;++t){
            uint32_t inp=seq[t], tgt=seq[t+1];
            if(inp>=vocab_||tgt>=out_) continue;
            std::memcpy(x,emb_.get()+inp*embed_,embed_*sizeof(float));
            MV(w1_.get(),x,h,hidden_,embed_);
            for(uint32_t i=0;i<hidden_;++i){ h[i]+=b1_[i]; h[i]=std::max(0.f,h[i]); }
            MV(w2_.get(),h,o,out_,hidden_);
            for(uint32_t i=0;i<out_;++i) o[i]+=b2_[i];
            SoftmaxSIMD(o,out_);
            o[tgt]-=1.f;
            for(uint32_t i=0;i<out_;++i){
                float g=o[i]*lr;
                uint16_t* row=w2_.get()+i*hidden_;
                for(uint32_t j=0;j<hidden_;++j) row[j]=F2B(B2F(row[j])-g*h[j]);
                b2_[i]-=g;
            }
            std::memset(gh,0,hidden_*sizeof(float));
            for(uint32_t i=0;i<out_;++i){
                const uint16_t* row=w2_.get()+i*hidden_;
                for(uint32_t j=0;j<hidden_;++j) gh[j]+=o[i]*B2F(row[j]);
            }
            for(uint32_t i=0;i<hidden_;++i) if(h[i]<=0.f) gh[i]=0.f;
            for(uint32_t i=0;i<hidden_;++i){
                float g=gh[i]*lr;
                uint16_t* row=w1_.get()+i*embed_;
                for(uint32_t j=0;j<embed_;++j) row[j]=F2B(B2F(row[j])-g*x[j]);
                b1_[i]-=g;
            }
            float* er=emb_.get()+inp*embed_;
            for(uint32_t i=0;i<hidden_;++i){
                float g=gh[i]*lr;
                const uint16_t* row=w1_.get()+i*embed_;
                for(uint32_t j=0;j<embed_;++j) er[j]-=g*B2F(row[j]);
            }
        }
    }
};

// ---------------------------------------------------------------
// Thompson-Sampling Bandit (Beta distribution, 8 heads)
// ---------------------------------------------------------------
class ThompsonBandit {
    float alpha_[8], beta_[8];
public:
    ThompsonBandit(){ for(int i=0;i<8;++i){ alpha_[i]=1.f; beta_[i]=1.f; } }
    void Update(int h, bool ok){
        if(ok) alpha_[h]+=1.f; else beta_[h]+=1.f;
        if(alpha_[h]>1000.f){ alpha_[h]*=0.5f; beta_[h]*=0.5f; }
    }
    float Sample(int h, FastRNG& rng) const {
        float a=alpha_[h], b=beta_[h];
        float mean=a/(a+b);
        float var=(a*b)/((a+b)*(a+b)*(a+b+1.f));
        float z=(rng.Uniform()-0.5f)*2.f;
        return std::max(0.01f, std::min(5.f, mean+z*std::sqrt(var)*2.f));
    }
    float Mean(int h) const { return alpha_[h]/(alpha_[h]+beta_[h]); }
};

// ---------------------------------------------------------------
// Cascade Tree (up to 256 nodes, flat array)
// ---------------------------------------------------------------
struct TNode { uint32_t token,parent,child,sibling,depth; float score; };

class CascadeTree {
    std::array<TNode,256> n_; uint32_t cnt_=0;
public:
    void Clear(){ cnt_=0; }
    uint32_t Root(uint32_t tok,float sc){
        n_[0]={tok,kNF,kNF,kNF,0,sc}; cnt_=1; return 0;
    }
    uint32_t Add(uint32_t p,uint32_t tok,float sc){
        if(cnt_>=256) return kNF;
        uint32_t id=cnt_++;
        n_[id]={tok,p,kNF,kNF,n_[p].depth+1,sc};
        n_[id].sibling=n_[p].child; n_[p].child=id;
        return id;
    }
    uint32_t Recover(uint32_t depth, uint32_t target,
                     uint32_t* out, uint32_t maxlen) const {
        for(uint32_t i=0;i<cnt_;++i){
            if(n_[i].depth==depth && n_[i].token==target){
                uint32_t k=0,cur=i;
                while(k<maxlen&&cur!=kNF){
                    out[k++]=n_[cur].token;
                    uint32_t bc=kNF; float bs=-1e30f;
                    for(uint32_t c=n_[cur].child;c!=kNF;c=n_[c].sibling)
                        if(n_[c].score>bs){bs=n_[c].score;bc=c;}
                    cur=bc;
                }
                return k;
            }
        }
        return 0;
    }
    const TNode& Node(uint32_t i) const { return n_[i]; }
};

// ---------------------------------------------------------------
// Copy Head v2 (repetition detection)
// ---------------------------------------------------------------
class CopyHeadV2 {
    static constexpr uint32_t kW=256;
    uint32_t buf_[kW]; uint32_t pos_=0,fill_=0;
public:
    void Push(uint32_t tok){ buf_[pos_&(kW-1)]=tok; ++pos_; if(fill_<kW)++fill_; }
    uint32_t Predict() const {
        if(fill_<32) return kNF;
        for(uint32_t L=std::min(fill_/2,64u);L>=4;--L){
            bool same=true;
            for(uint32_t k=0;k<L;++k){
                uint32_t a=buf_[(pos_-L+k)&(kW-1)];
                uint32_t b=buf_[(pos_-2*L+k)&(kW-1)];
                if(a!=b){same=false;break;}
            }
            if(same) return buf_[(pos_-L)&(kW-1)];
        }
        return kNF;
    }
};

// ---------------------------------------------------------------
// Entropy-Driven Width Controller
// ---------------------------------------------------------------
class EntropyWidth {
    float hist_[16]={}; uint32_t pos_=0;
public:
    uint32_t Compute(float ent, uint32_t wmin, uint32_t wmax, float ewma){
        hist_[pos_&15]=ent; ++pos_;
        float avg=0; for(int i=0;i<16;++i) avg+=hist_[i]; avg/=16.f;
        float score=(1.f-avg)*ewma;
        uint32_t w=wmin+(uint32_t)(score*(wmax-wmin));
        return std::min(wmax,std::max(wmin,w));
    }
};

} // anonymous namespace

// ---------------------------------------------------------------
// Impl
// ---------------------------------------------------------------
class SingularitySpecDecoder::Impl {
public:
    SingularityConfig cfg_;
    CuckooTable   n2_,n3_,n4_,n5_;
    OctoGramCache octo_;
    MinHashCache  minhash_;
    SyntaxOracle  syntax_;
    VarPredictor  var_;
    BFloat16MLP   mlp_;
    ThompsonBandit bandit_;
    CascadeTree   tree_;
    CopyHeadV2    copy_;
    EntropyWidth  ew_;
    FastRNG       rng_;

    struct {
        std::atomic<uint64_t> drafts{0},acc{0},rej{0},cascade{0};
        std::atomic<uint64_t> syntax{0},var{0},octo{0},minhash{0};
    } st;
    alignas(64) std::atomic<float>    ewma_{0.5f};
    alignas(64) std::atomic<uint32_t> width_{4};

    explicit Impl(const SingularityConfig& c)
        : cfg_(c),
          n2_(c.octo_gram_bits), n3_(c.octo_gram_bits),
          n4_(c.octo_gram_bits), n5_(c.octo_gram_bits),
          octo_(c.octo_gram_bits),
          minhash_(c.minhash_slots),       // already a power-of-2 count
          mlp_(c.vocab_size,c.mlp_embed,c.mlp_hidden,c.slim_vocab),
          rng_(0xDEADBEEFCAFEBABEULL) {}

    // ----------------------------------------------------------
    uint32_t Draft(const uint32_t* history, uint32_t hl,
                   uint32_t* out, uint32_t max_d) {
        uint32_t w = std::min(max_d, width_.load(std::memory_order_relaxed));
        if (!w || !hl) return 0;

        tree_.Clear();
        uint32_t root = history[hl-1];
        tree_.Root(root, 1.f);

        // Shared precompute
        alignas(64) float mlp_logits[1024];
        uint32_t mlp_pred = mlp_.Predict(history, hl, mlp_logits);
        float mlp_conf = 0.f;
        if (mlp_pred < cfg_.slim_vocab) {
            SoftmaxSIMD(mlp_logits, cfg_.slim_vocab);
            mlp_conf = mlp_logits[mlp_pred];
        }
        uint32_t copy_pred = copy_.Predict();
        uint32_t syn_conf = 0;
        uint32_t syn_pred = syntax_.Predict(&syn_conf);
        uint32_t mh_tok[8]; float mh_prob[8];
        uint32_t mh_n = minhash_.Retrieve(
            history + (hl>64?hl-64:0), std::min(hl,64u), mh_tok, mh_prob, 8);
        uint32_t var_conf = 0;
        uint32_t var_pred = var_.Predict(
            history + (hl>8?hl-8:0), std::min(hl,8u), &var_conf);

        uint32_t gen = 0;
        uint32_t frontier[64]; uint32_t fn = 1; frontier[0] = 0;

        while (gen < w && fn > 0) {
            uint32_t nf[64]; uint32_t nn = 0;

            for (uint32_t fi = 0; fi < fn && gen < w; ++fi) {
                uint32_t nid   = frontier[fi];
                uint32_t depth = tree_.Node(nid).depth;

                // ctx[7] = last token; ctx[6] = second-to-last, etc.
                uint32_t ctx[8] = {};
                for (int i = 0; i < 8; ++i) {
                    int rpos = (int)hl - 8 + i + (int)depth;
                    if (rpos >= 0 && (uint32_t)rpos < hl)
                        ctx[i] = history[rpos];
                    else if (depth > 0) {
                        int off = i - (8 - (int)depth);
                        if (off >= 0 && (uint32_t)off < gen) ctx[i] = out[off];
                    }
                }

                // 8 head predictions + confidences
                uint32_t preds[8] = {kNF,kNF,kNF,kNF,kNF,kNF,kNF,kNF};
                float    pconf[8] = {};

                preds[0] = n5_.Lookup(Key5(ctx[3],ctx[4],ctx[5],ctx[6],ctx[7]));
                if (preds[0]!=kNF) pconf[0]=1.0f;
                preds[1] = n4_.Lookup(Key4(ctx[4],ctx[5],ctx[6],ctx[7]));
                if (preds[1]!=kNF) pconf[1]=0.85f;
                preds[2] = n3_.Lookup(Key3(ctx[5],ctx[6],ctx[7]));
                if (preds[2]!=kNF) pconf[2]=0.70f;
                preds[3] = n2_.Lookup(Key2(ctx[6],ctx[7]));
                if (preds[3]!=kNF) pconf[3]=0.55f;

                if (hl+depth>=8) {
                    uint32_t oc[8]={};
                    for(int i=0;i<8;++i){
                        int rp=(int)hl-8+i+(int)depth;
                        if(rp>=0&&(uint32_t)rp<hl) oc[i]=history[rp];
                        else if(gen+i>=(uint32_t)depth&&(int)gen-((int)depth-i)>=0)
                            oc[i]=out[gen-depth+i];
                    }
                    preds[4]=octo_.Lookup(oc);
                    if(preds[4]!=kNF){ pconf[4]=1.2f; st.octo.fetch_add(1,std::memory_order_relaxed); }
                }
                if (mlp_pred<cfg_.slim_vocab&&mlp_conf>0.05f) { preds[5]=mlp_pred; pconf[5]=mlp_conf; }
                if (copy_pred!=kNF) { preds[6]=copy_pred; pconf[6]=0.65f; }
                if (syn_pred!=kNF&&syn_conf>80) { preds[7]=syn_pred; pconf[7]=syn_conf/255.f; }

                // Ensemble with Thompson weights
                struct Cand { uint32_t tok; float score; };
                Cand cands[20]; uint32_t nc=0;

                auto add=[&](uint32_t t,float s){
                    if(t==kNF||t>=cfg_.vocab_size) return;
                    for(uint32_t i=0;i<nc;++i) if(cands[i].tok==t){cands[i].score+=s;return;}
                    if(nc<20) cands[nc++]={t,s};
                };

                for(int h=0;h<8;++h)
                    if(preds[h]!=kNF) add(preds[h], pconf[h]*bandit_.Sample(h,rng_));
                for(uint32_t i=0;i<mh_n;++i) add(mh_tok[i], mh_prob[i]*0.5f);
                if(var_pred!=kNF) add(var_pred, var_conf/255.f);

                // Fallback: if no cache hits, use (ctx[7]+1) % vocab as sentinel to
                // ensure we always emit at least one token per frontier node
                if (!nc) {
                    uint32_t fb = (ctx[7]+1) % cfg_.vocab_size;
                    cands[nc++] = {fb, 0.01f};
                }

                // Sort descending
                for(uint32_t i=0;i<nc;++i)
                    for(uint32_t j=i+1;j<nc;++j)
                        if(cands[j].score>cands[i].score) std::swap(cands[i],cands[j]);

                uint32_t chosen=cands[0].tok;
                out[gen++]=chosen;
                uint32_t nn_node=tree_.Add(nid,chosen,cands[0].score);
                if(nn_node!=kNF&&nn<64) nf[nn++]=nn_node;

                for(uint32_t ci=1;ci<nc&&ci<4;++ci){
                    uint32_t alt=tree_.Add(nid,cands[ci].tok,cands[ci].score);
                    if(alt!=kNF&&nn<64) nf[nn++]=alt;
                }
            }
            fn=nn; for(uint32_t i=0;i<nn;++i) frontier[i]=nf[i];
        }

        st.drafts.fetch_add(1,std::memory_order_relaxed);
        return gen;
    }

    // ----------------------------------------------------------
    uint32_t ValidateArgmax(const uint32_t* draft, uint32_t n,
                            const uint32_t* target) {
        uint32_t acc=0;
        for(uint32_t i=0;i<n;++i){
            if(draft[i]==target[i]){ ++acc; continue; }
            uint32_t rec[64];
            uint32_t rlen=tree_.Recover(i, target[i], rec, 64);
            if(rlen>0){
                uint32_t cl=std::min(rlen,n-i);
                std::memcpy(const_cast<uint32_t*>(draft)+i, rec, cl*sizeof(uint32_t));
                ++acc; st.cascade.fetch_add(1,std::memory_order_relaxed); continue;
            }
            break;
        }
        UpdateStats(acc,n,draft,target);
        return acc;
    }

    // ----------------------------------------------------------
    uint32_t ValidateProbabilistic(const uint32_t* draft, uint32_t n,
                                   const float* logits, uint32_t stride) {
        uint32_t acc=0;
        for(uint32_t i=0;i<n;++i){
            alignas(64) float probs[32000];
            uint32_t v=std::min(cfg_.vocab_size,32000u);
            std::memcpy(probs, logits+i*stride, v*sizeof(float));
            SoftmaxSIMD(probs,v);
            float p=probs[draft[i]], mx=probs[ArgmaxSIMD(probs,v)];
            if(p>=mx*0.82f){ ++acc; continue; }
            uint32_t rec[64];
            uint32_t rlen=tree_.Recover(i, ArgmaxSIMD(probs,v), rec, 64);
            if(rlen>0){
                uint32_t cl=std::min(rlen,n-i);
                std::memcpy(const_cast<uint32_t*>(draft)+i, rec, cl*sizeof(uint32_t));
                ++acc; st.cascade.fetch_add(1,std::memory_order_relaxed); continue;
            }
            break;
        }
        UpdateStats(acc,n,draft,nullptr);
        return acc;
    }

    // ----------------------------------------------------------
    void FeedAccepted(const uint32_t* seq, uint32_t len) {
        if(len<2) return;
        for(uint32_t i=0;i+1<len;++i){
            if(i+7<len) octo_.Insert(seq+i, seq[i+8]);
            if(i+4<len) n5_.Insert(Key5(seq[i],seq[i+1],seq[i+2],seq[i+3],seq[i+4]),seq[i+5]);
            if(i+3<len) n4_.Insert(Key4(seq[i],seq[i+1],seq[i+2],seq[i+3]),seq[i+4]);
            if(i+2<len) n3_.Insert(Key3(seq[i],seq[i+1],seq[i+2]),seq[i+3]);
            n2_.Insert(Key2(seq[i],seq[i+1]),seq[i+2]);
        }
        for(uint32_t i=0;i<len;++i){ copy_.Push(seq[i]); syntax_.Observe(seq[i]); }
        for(uint32_t i=0;i<len;++i){
            if(seq[i]==299||seq[i]==321) var_.Enter();
            if(seq[i]==300||seq[i]==322) var_.Exit();
            if(i+1<len&&(seq[i]==1000||seq[i]==1001)) var_.Decl(seq[i+1]);
        }
        if(len>=8){
            uint32_t topk[8]; float prob[8];
            for(int i=0;i<8;++i){ topk[i]=seq[len-1-i]; prob[i]=1.f/(i+1); }
            minhash_.Store(seq+(len>64?len-64:0), std::min(len,64u), topk, prob);
        }
        mlp_.Train(seq, len, 0.008f);
        st.acc.fetch_add(len, std::memory_order_relaxed);
    }

    // ----------------------------------------------------------
    SingularityStats GetStats() const {
        SingularityStats s{};
        s.drafts_total      = st.drafts.load();
        s.tokens_accepted   = st.acc.load();
        s.tokens_rejected   = st.rej.load();
        s.cascade_recoveries= st.cascade.load();
        s.syntax_oracle_hits= st.syntax.load();
        s.var_predict_hits  = st.var.load();
        s.octo_gram_hits    = st.octo.load();
        s.minhash_hits      = st.minhash.load();
        s.acceptance_ewma   = ewma_.load();
        s.current_width     = width_.load();
        for(int i=0;i<8;++i) s.head_bandit_weights[i]=bandit_.Mean(i);
        return s;
    }
    uint32_t GetWidth() const { return width_.load(); }

private:
    void UpdateStats(uint32_t acc, uint32_t total,
                     const uint32_t* draft, const uint32_t* target) {
        st.acc.fetch_add(acc, std::memory_order_relaxed);
        st.rej.fetch_add(total-acc, std::memory_order_relaxed);
        float a = acc/(float)std::max(1u,total);
        float e = ewma_.load(std::memory_order_relaxed)*0.85f + a*0.15f;
        ewma_.store(e, std::memory_order_relaxed);
        if(draft&&target){
            for(uint32_t i=0;i<acc;++i) for(int h=0;h<8;++h) bandit_.Update(h,true);
            if(acc<total) for(int h=0;h<8;++h) bandit_.Update(h,false);
        }
        float ent=(a>0.9f)?0.1f:(a>0.5f?0.3f:0.7f);
        uint32_t w=ew_.Compute(ent,2,cfg_.max_draft_width,e);
        width_.store(w, std::memory_order_relaxed);
    }
};

// ---------------------------------------------------------------
// Public trampolines
// ---------------------------------------------------------------
SingularitySpecDecoder::SingularitySpecDecoder(const SingularityConfig& cfg)
    : impl_(std::make_unique<Impl>(cfg)) {}
SingularitySpecDecoder::~SingularitySpecDecoder() = default;

uint32_t SingularitySpecDecoder::Draft(const uint32_t* h,uint32_t hl,
                                       uint32_t* out,uint32_t md)
    { return impl_->Draft(h,hl,out,md); }
uint32_t SingularitySpecDecoder::ValidateArgmax(const uint32_t* d,uint32_t n,
                                                const uint32_t* t)
    { return impl_->ValidateArgmax(d,n,t); }
uint32_t SingularitySpecDecoder::ValidateProbabilistic(const uint32_t* d,uint32_t n,
                                                       const float* l,uint32_t s)
    { return impl_->ValidateProbabilistic(d,n,l,s); }
void SingularitySpecDecoder::FeedAccepted(const uint32_t* seq,uint32_t len)
    { impl_->FeedAccepted(seq,len); }
SingularityStats SingularitySpecDecoder::GetStats() const { return impl_->GetStats(); }
uint32_t SingularitySpecDecoder::GetDraftWidth() const { return impl_->GetWidth(); }

} // namespace rxd::ai

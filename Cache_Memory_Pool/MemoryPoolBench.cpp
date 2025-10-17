// MemoryPoolBench.cpp
#include <bits/stdc++.h>
#include <new>
#ifdef _WIN32
    #include <windows.h>
    #include <psapi.h>
#else
    #include <sys/resource.h>
    #include <unistd.h>
#endif

using namespace std;
using FClock = chrono::steady_clock;

static size_t GetProcessMemoryKB()
{
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX Pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&Pmc, sizeof(Pmc)))
    {
        return static_cast<size_t>(Pmc.WorkingSetSize / 1024);
    }
    return 0;
#else
    struct rusage Usage{};
    if (getrusage(RUSAGE_SELF, &Usage) == 0)
    {
    #if defined(__APPLE__) || defined(__MACH__)
        return static_cast<size_t>(Usage.ru_maxrss / 1024);
    #else
        return static_cast<size_t>(Usage.ru_maxrss);
    #endif
    }
    return 0;
#endif
}

// 64바이트 캐시라인 단위로 정렬된 데이터
struct alignas(64) FNode
{
    long long Data[8];
};

// 단순 bump allocator 기반 메모리 풀
class FMemoryArena
{
public:
    explicit FMemoryArena(size_t InSize, size_t InAlign = alignof(FNode))
        : Capacity(InSize), Alignment(InAlign)
    {
        BasePtr = ::operator new[](InSize, std::align_val_t(Alignment));
        Cursor = 0;
    }

    ~FMemoryArena()
    {
        ::operator delete[](BasePtr, std::align_val_t(Alignment));
    }

    template<typename T, typename... Args>
    T* New(Args&&... ArgsList)
    {
        void* Ptr = Allocate(sizeof(T), alignof(T));
        if (!Ptr)
        {
            return nullptr;
        }
        return ::new (Ptr) T(std::forward<Args>(ArgsList)...);
    }

    void Reset()
    {
        Cursor = 0;
    }

private:
    void* Allocate(size_t Size, size_t Align)
    {
        size_t RawAddr = reinterpret_cast<size_t>(BasePtr) + Cursor;
        size_t Aligned = (RawAddr + (Align - 1)) & ~(Align - 1);
        size_t Next = Aligned - reinterpret_cast<size_t>(BasePtr) + Size;
        if (Next > Capacity)
        {
            return nullptr;
        }
        Cursor = Next;
        return reinterpret_cast<void*>(Aligned);
    }

private:
    void* BasePtr = nullptr;
    size_t Capacity = 0;
    size_t Cursor = 0;
    size_t Alignment = 0;
};

static inline double GetSeconds(FClock::time_point A, FClock::time_point B)
{
    return chrono::duration<double>(B - A).count();
}

long long RunHeapBenchmark(
    size_t Count,
    double& OutAlloc, double& OutWrite, double& OutSeq, double& OutRand,
    size_t& OutMemoryKB)
{
    vector<FNode*> Nodes;
    Nodes.reserve(Count);

    auto T0 = FClock::now();
    for (size_t i = 0; i < Count; ++i)
    {
        Nodes.push_back(new FNode());
    }
    auto T1 = FClock::now();

    for (size_t i = 0; i < Count; ++i)
    {
        FNode* Node = Nodes[i];
        for (int k = 0; k < 8; ++k)
        {
            Node->Data[k] = static_cast<long long>(i * 11400714819323198485ull + k);
        }
    }
    auto T2 = FClock::now();

    volatile long long Accumulator = 0;

    for (size_t i = 0; i < Count; ++i)
    {
        FNode* Node = Nodes[i];
        for (int k = 0; k < 8; ++k)
        {
            Accumulator += Node->Data[k];
        }
    }
    auto T3 = FClock::now();

    {
        vector<size_t> Index(Count);
        iota(Index.begin(), Index.end(), 0);
        shuffle(Index.begin(), Index.end(), mt19937(12345));
        for (size_t j = 0; j < Count; ++j)
        {
            FNode* Node = Nodes[Index[j]];
            for (int k = 0; k < 8; ++k)
            {
                Accumulator += Node->Data[k];
            }
        }
    }
    auto T4 = FClock::now();

    OutMemoryKB = GetProcessMemoryKB();

    for (FNode* Node : Nodes)
    {
        delete Node;
    }

    OutAlloc = GetSeconds(T0, T1);
    OutWrite = GetSeconds(T1, T2);
    OutSeq = GetSeconds(T2, T3);
    OutRand = GetSeconds(T3, T4);
    return Accumulator;
}

long long RunArenaBenchmark(
    size_t Count,
    double& OutAlloc, double& OutWrite, double& OutSeq, double& OutRand,
    size_t& OutMemoryKB)
{
    const size_t TotalBytes = Count * sizeof(FNode) + 64;

    auto T0 = FClock::now();
    FMemoryArena Arena(TotalBytes);
    vector<FNode*> Nodes;
    Nodes.reserve(Count);

    for (size_t i = 0; i < Count; ++i)
    {
        Nodes.push_back(Arena.New<FNode>());
    }
    auto T1 = FClock::now();

    for (size_t i = 0; i < Count; ++i)
    {
        FNode* Node = Nodes[i];
        for (int k = 0; k < 8; ++k)
        {
            Node->Data[k] = static_cast<long long>(i * 6364136223846793005ull + k);
        }
    }
    auto T2 = FClock::now();

    volatile long long Accumulator = 0;

    for (size_t i = 0; i < Count; ++i)
    {
        FNode* Node = Nodes[i];
        for (int k = 0; k < 8; ++k)
        {
            Accumulator += Node->Data[k];
        }
    }
    auto T3 = FClock::now();

    {
        vector<size_t> Index(Count);
        iota(Index.begin(), Index.end(), 0);
        shuffle(Index.begin(), Index.end(), mt19937(12345));
        for (size_t j = 0; j < Count; ++j)
        {
            FNode* Node = Nodes[Index[j]];
            for (int k = 0; k < 8; ++k)
            {
                Accumulator += Node->Data[k];
            }
        }
    }
    auto T4 = FClock::now();

    OutMemoryKB = GetProcessMemoryKB();

    OutAlloc = GetSeconds(T0, T1);
    OutWrite = GetSeconds(T1, T2);
    OutSeq = GetSeconds(T2, T3);
    OutRand = GetSeconds(T3, T4);
    return Accumulator;
}

int main(void)
{
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    size_t NodeCount = 3'000'000;

    cout << "Benchmarking with " << NodeCount << " nodes (64 bytes each)\n";

    // Warm-up
    {
        double A,B,C,D; size_t R;
        RunArenaBenchmark(1000, A,B,C,D,R);
        RunHeapBenchmark(1000, A,B,C,D,R);
    }

    double HeapAlloc, HeapWrite, HeapSeq, HeapRand;
    double ArenaAlloc, ArenaWrite, ArenaSeq, ArenaRand;
    size_t HeapRSS = 0, ArenaRSS = 0;

    long long HeapAcc = RunHeapBenchmark(NodeCount, HeapAlloc, HeapWrite, HeapSeq, HeapRand, HeapRSS);
    long long ArenaAcc = RunArenaBenchmark(NodeCount, ArenaAlloc, ArenaWrite, ArenaSeq, ArenaRand, ArenaRSS);

    cout << "\n=== Heap (new/delete per object) ===\n";
    cout << "Alloc: " << HeapAlloc << " s\n";
    cout << "Write: " << HeapWrite << " s\n";
    cout << "Seq  : " << HeapSeq << " s\n";
    cout << "Rand : " << HeapRand << " s\n";
    cout << "RSS  : " << HeapRSS << " KB\n";

    cout << "\n=== Arena (placement new, contiguous) ===\n";
    cout << "Alloc: " << ArenaAlloc << " s\n";
    cout << "Write: " << ArenaWrite << " s\n";
    cout << "Seq  : " << ArenaSeq << " s\n";
    cout << "Rand : " << ArenaRand << " s\n";
    cout << "RSS  : " << ArenaRSS << " KB\n";

    cout << "\nAcc(Heap)=" << HeapAcc << " Acc(Arena)=" << ArenaAcc << "\n";
    return 0;
}

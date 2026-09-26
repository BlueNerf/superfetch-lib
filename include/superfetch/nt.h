#pragma once

#define SUPERFETCH_VERSION 45
#define SUPERFETCH_MAGIC   'kuhC'

#define SystemSuperfetchInformation ((SYSTEM_INFORMATION_CLASS)79)

typedef enum _SUPERFETCH_INFORMATION_CLASS
{
    SuperfetchRetrieveTrace = 1,               // q: PF_SYSTEM_SUPERFETCH_RETRIEVE_TRACE // PfGetCompletedTrace
    SuperfetchSystemParameters,                // q: PF_SYSTEM_SUPERFETCH_PARAMETERS
    SuperfetchLogEvent,                        // s: PF_LOG_EVENT_DATA
    SuperfetchGenerateTrace,                   // s: PF_GENERATE_TRACE_CONTROL
    SuperfetchPrefetch,                        // s: PF_PREFETCH_REQUEST
    SuperfetchPfnQuery,                        // q: PF_PFN_PRIO_REQUEST
    SuperfetchPfnSetPriority,                  // s: PF_PFN_PRIO_REQUEST // MmSetPfnListInfo
    SuperfetchPrivSourceQuery,                 // q: PF_PRIVSOURCE_QUERY_REQUEST
    SuperfetchSequenceNumberQuery,             // q: PF_SEQUENCENUMBER_QUERY_REQUEST
    SuperfetchScenarioPhase,                   // s: PF_SCENARIO_PHASE_INFO // 10
    SuperfetchWorkerPriority,                  // s: PF_WORKER_PRIORITY_CONTROL
    SuperfetchScenarioQuery,                   // q: PF_SCENARIO_QUERY_INFO
    SuperfetchScenarioPrefetch,                // s: PF_SCENARIO_PREFETCH_INFO
    SuperfetchRobustnessControl,               // s: PF_ROBUSTNESS_CONTROL
    SuperfetchTimeControl,                     // s: PF_TIME_CONTROL
    SuperfetchMemoryListQuery,                 // q: PF_MEMORY_LIST_INFO
    SuperfetchMemoryRangesQuery,               // q: PF_PHYSICAL_MEMORY_RANGE_INFO_V1/V2
    SuperfetchTracingControl,                  // s: PF_ACCESS_TRACING_CONTROL
    SuperfetchTrimWhileAgingControl,           // s: PF_TRIM_WHILE_AGING_CONTROL
    SuperfetchRepurposedByPrefetch,            // q: PF_REPURPOSED_BY_PREFETCH_INFO // 20
    SuperfetchChannelPowerRequest,             // q: not implemented
    SuperfetchMovePages,                       // s: PF_PFN_PRIO_REQUEST // MmRelocatePfnList
    SuperfetchVirtualQuery,                    // q: PF_VIRTUAL_QUERY
    SuperfetchCombineStatsQuery,               // q: PF_PAGECOMBINE_AGGREGATE_STAT
    SuperfetchSetMinWsAgeRate,                 // s: PF_MIN_WS_AGE_RATE_CONTROL
    SuperfetchDeprioritizeOldPagesInWs,        // s: PF_DEPRIORITIZE_OLD_PAGES
    SuperfetchFileExtentsQuery,                // q: PF_FILE_EXTENTS_INFO
    SuperfetchGpuUtilizationQuery,             // q: PF_GPU_UTILIZATION_INFO
    SuperfetchPfnSet,                          // s: PF_PFN_PRIO_REQUEST // since WIN11
    SuperfetchInformationMax
} SUPERFETCH_INFORMATION_CLASS;

typedef struct _SUPERFETCH_INFORMATION
{
    _In_ ULONG Version;
    _In_ ULONG Magic;
    _In_ SUPERFETCH_INFORMATION_CLASS SuperfetchInformationClass;
    _Inout_ PVOID SuperfetchInformation;
    _Inout_ ULONG SuperfetchInformationLength;
} SUPERFETCH_INFORMATION, *PSUPERFETCH_INFORMATION;

typedef struct _SYSTEM_MEMORY_LIST_INFORMATION
{
    SIZE_T ZeroPageCount;
    SIZE_T FreePageCount;
    SIZE_T ModifiedPageCount;
    SIZE_T ModifiedNoWritePageCount;
    SIZE_T BadPageCount;
    SIZE_T PageCountByPriority[8];
    SIZE_T RepurposedPagesByPriority[8];
    SIZE_T ModifiedPageCountPageFile;
} SYSTEM_MEMORY_LIST_INFORMATION, *PSYSTEM_MEMORY_LIST_INFORMATION;

typedef struct _MEMORY_FRAME_INFORMATION
{
    ULONGLONG UseDescription : 4; // MMPFNUSE_*
    ULONGLONG ListDescription : 3; // MMPFNLIST_*
    ULONGLONG Cold : 1; // 19H1
    ULONGLONG Pinned : 1; // 1 - pinned, 0 - not pinned
    ULONGLONG DontUse : 48; // *_INFORMATION overlay
    ULONGLONG Priority : 3;
    ULONGLONG NonTradeable : 1;
    ULONGLONG Reserved : 3;
} MEMORY_FRAME_INFORMATION, *PMEMORY_FRAME_INFORMATION;

typedef struct _FILEOFFSET_INFORMATION
{
    ULONGLONG DontUse : 9; // MEMORY_FRAME_INFORMATION overlay
    ULONGLONG Offset : 48; // mapped files
    ULONGLONG Reserved : 7;
} FILEOFFSET_INFORMATION, *PFILEOFFSET_INFORMATION;

typedef struct _PAGEDIR_INFORMATION
{
    ULONGLONG DontUse : 9; // MEMORY_FRAME_INFORMATION overlay
    ULONGLONG PageDirectoryBase : 48; // private pages
    ULONGLONG Reserved : 7;
} PAGEDIR_INFORMATION, *PPAGEDIR_INFORMATION;

typedef struct _UNIQUE_PROCESS_INFORMATION
{
    ULONGLONG DontUse : 9; // MEMORY_FRAME_INFORMATION overlay
    ULONGLONG UniqueProcessKey : 48; // ProcessId
    ULONGLONG Reserved  : 7;
} UNIQUE_PROCESS_INFORMATION, *PUNIQUE_PROCESS_INFORMATION;

// private
typedef struct _MMPFN_IDENTITY
{
    union
    {
        MEMORY_FRAME_INFORMATION e1; // all
        FILEOFFSET_INFORMATION e2; // mapped files
        PAGEDIR_INFORMATION e3; // private pages
        UNIQUE_PROCESS_INFORMATION e4; // owning process
    } u1;
    ULONG_PTR PageFrameIndex; // all
    union
    {
        struct
        {
            ULONG_PTR Image : 1;
            ULONG_PTR Mismatch : 1;
        } e1;
        struct
        {
            ULONG_PTR CombinedPage;
        } e2;
        ULONG_PTR FileObject; // mapped files
        ULONG_PTR UniqueFileObjectKey;
        ULONG_PTR ProtoPteAddress;
        ULONG_PTR VirtualAddress;  // everything else
    } u2;
} MMPFN_IDENTITY, *PMMPFN_IDENTITY;


typedef struct _PFN_TRIPLET
{
    ULONGLONG MaskOrKey;        // Compared against identity with 0x1FFFFFFFFFFFE00 mask
    ULONGLONG Pfn;              // Page frame number
    ULONGLONG Flags;            // Request/result flags
} PFN_TRIPLET, *PPFN_TRIPLET;


typedef struct _PF_PFN_PRIO_REQUEST
{
    ULONG Version;
    ULONG RequestFlags;
    SIZE_T PfnCount;
    SYSTEM_MEMORY_LIST_INFORMATION MemInfo;
    union
    {
        // Input: (class 6/16) MmQueryPfnList fills identities here
        MMPFN_IDENTITY PageIdentities[256]; // ANYSIZE_ARRAY
        // Output: (class 7/29/1D) caller supplies PFN_TRIPLETs here
        PFN_TRIPLET Entries[256]; // ANYSIZE_ARRAY
    };
} PF_PFN_PRIO_REQUEST, *PPF_PFN_PRIO_REQUEST;

typedef struct _PHYSICAL_MEMORY_RUN {
    SIZE_T BasePage;
    SIZE_T PageCount;
} PHYSICAL_MEMORY_RUN, * PPHYSICAL_MEMORY_RUN;

typedef struct _PF_PHYSICAL_MEMORY_RANGE {
    ULONG_PTR BasePfn;
    ULONG_PTR PageCount;
} PF_PHYSICAL_MEMORY_RANGE, * PPF_PHYSICAL_MEMORY_RANGE;

typedef struct _PF_MEMORY_RANGE_INFO {
    ULONG  Version;
    ULONG  Flags;
    SIZE_T RangeCount;
    PF_PHYSICAL_MEMORY_RANGE Ranges[1];
} PF_MEMORY_RANGE_INFO, *PPF_MEMORY_RANGE_INFO;

#define SE_DEBUG_PRIVILEGE                (20L)
#define SE_PROF_SINGLE_PROCESS_PRIVILEGE  (13L)

extern "C" NTSTATUS NTAPI RtlAdjustPrivilege(
    IN ULONG Privilege,
    IN BOOLEAN NewValue,
    IN BOOLEAN ForThread,
    OUT PBOOLEAN OldValue
);
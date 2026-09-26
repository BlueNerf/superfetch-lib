#pragma once

#include <iostream>
#include <ntstatus.h>
#include <vector>
#include <Windows.h>
#include <winternl.h>

#include "nt.h"

struct SFMemoryInfo
{
    uint64_t Start;
    uint64_t End;
    int PageCount;
    uint64_t Size;
};

struct PfnList {
    bool isPool;
    std::uint32_t poolTag;
};

class Superfetch {
public:
    static Superfetch& getInstance() {
        static Superfetch instance;
        return instance;
    }

    [[nodiscard]] static int forceUpdateRanges() {
        return QueryMemoryRanges(&superfetchPhysicalRanges);
    }

    // Gets the raw
    [[nodiscard]] const PPF_MEMORY_RANGE_INFO getRawMemoryRanges() const {
        return superfetchPhysicalRanges;
    }

    Superfetch(const Superfetch&) = delete;
    Superfetch& operator=(const Superfetch&) = delete;

private:
    int refCount = 0;
    inline static PPF_MEMORY_RANGE_INFO superfetchPhysicalRanges;
    inline static PPF_PFN_PRIO_REQUEST pPfnRanges;

    Superfetch() {
        refCount++;
        int err{};
        try {
            err = sfSetupPrivileges();
            if (err) { throw std::runtime_error("[iX] Failed to setup privileges (possibly need administrator privileges)"); };
            err = sfSetupPages();
            if (err) { throw std::runtime_error("[iX] Failed to setup superfetch pages"); }
        } catch (const std::runtime_error& e) {
            std::cerr << e.what() << "(error: " << err << ")" << std::endl;

            superfetchPhysicalRanges = nullptr;
            pPfnRanges = nullptr;
        }
    }

    [[nodiscard]] int sfSetupPages() const
    {
        auto iRangesResponse = QueryMemoryRanges(&superfetchPhysicalRanges);
        if (iRangesResponse == 0) {
            auto iPagesResponse = QueryMemoryPages(&pPfnRanges);
            if (iPagesResponse == 0) {
                return 0;
            }
            return 2;
        }

        return 1;
    }

    [[nodiscard]] static int sfSetupPrivileges() {
        BOOLEAN old;
        NTSTATUS status = RtlAdjustPrivilege(SE_PROF_SINGLE_PROCESS_PRIVILEGE, TRUE, FALSE, &old);

        if (!NT_SUCCESS(status)) {
            std::cout << "[X] Could not give SE_PROF_SINGLE_PROCESS_PRIVILEGE privilege to the process [possibly need administrator privileges] (0x" << std::hex << status << ")" << std::endl;
            return -1;
        }

        status = RtlAdjustPrivilege(SE_DEBUG_PRIVILEGE, TRUE, FALSE, &old);
        if (!NT_SUCCESS(status)) {
            std::cout << "[X] Could not give SE_DEBUG_PRIVILEGE privilege to the process [possibly need administrator privileges] (0x" << std::hex << status << ")" << std::endl;
            return -1;
        }

        return 0;
    }

    static NTSTATUS QuerySuperfetchInfo(SUPERFETCH_INFORMATION* superfetch_information) {
        ULONG return_length = 0;

        return NtQuerySystemInformation(SystemSuperfetchInformation, superfetch_information, sizeof(*superfetch_information), &return_length);
    }

    template <typename T>
    static SUPERFETCH_INFORMATION createSfInformation(SUPERFETCH_INFORMATION_CLASS superfetch_information_class, T* superfetch_information, SIZE_T superfetch_information_length) {
        SUPERFETCH_INFORMATION sf_information{};
        sf_information.Version                     = SUPERFETCH_VERSION;
        sf_information.Magic                       = SUPERFETCH_MAGIC;
        sf_information.SuperfetchInformationClass  = superfetch_information_class;
        sf_information.SuperfetchInformation       = superfetch_information;
        sf_information.SuperfetchInformationLength = superfetch_information_length;

        return sf_information;
    }

    // old function will update to use the helper functions soon
    [[nodiscard]] static int QueryMemoryRanges(PPF_MEMORY_RANGE_INFO* pSuperfetchRanges) {
        PF_MEMORY_RANGE_INFO memory_range_info{};
        memory_range_info.Version = 2;
        memory_range_info.Flags   = 0;

        SUPERFETCH_INFORMATION superfetch_information{};
        superfetch_information.Version                     = SUPERFETCH_VERSION;
        superfetch_information.Magic                       = SUPERFETCH_MAGIC;
        superfetch_information.SuperfetchInformationClass  = SuperfetchMemoryRangesQuery;
        superfetch_information.SuperfetchInformation       = &memory_range_info;
        superfetch_information.SuperfetchInformationLength = sizeof(memory_range_info);

        ULONG return_length = 0;
        NTSTATUS st = NtQuerySystemInformation(
            SystemSuperfetchInformation,
            &superfetch_information,
            sizeof(superfetch_information),
            &return_length
        );

        PPF_MEMORY_RANGE_INFO ranges = &memory_range_info;

        if (st == STATUS_BUFFER_TOO_SMALL) {
            ranges = reinterpret_cast<PPF_MEMORY_RANGE_INFO>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, return_length));

            ranges->Version = 2;
            ranges->Flags   = 0;

            superfetch_information.SuperfetchInformation = ranges;
            superfetch_information.SuperfetchInformationLength = return_length;
            st = NtQuerySystemInformation(
                SystemSuperfetchInformation,
                &superfetch_information,
                sizeof(superfetch_information),
                &return_length
            );
        }

        if (!NT_SUCCESS(st)) {
            std::cout << "[X] Error getting superfetch pages: " << st << std::endl;
            return -1;
        }

        std::cout << "rangecount: " << ranges->RangeCount << std::endl;
        *pSuperfetchRanges = ranges;
        return 0;
    }

    [[nodiscard]] static int QueryMemoryPages(PPF_PFN_PRIO_REQUEST* pSuperfetchPages) {
        if (superfetchPhysicalRanges == nullptr) {
            std::cout << "[X] Query ranges before setting up pages. (rangecount -> " << superfetchPhysicalRanges->RangeCount << ")"<< std::endl;
            return -1;
        }

        PPHYSICAL_MEMORY_RUN physical_memory_run;
        ULONG_PTR MmHighestPageNumber = 0;
        for (ULONG i = 0; i < superfetchPhysicalRanges->RangeCount; i++) {
            physical_memory_run = reinterpret_cast<PPHYSICAL_MEMORY_RUN>(&superfetchPhysicalRanges->Ranges[i]);
            MmHighestPageNumber = physical_memory_run->BasePage + physical_memory_run->PageCount;
        }
        auto PfnCount = MmHighestPageNumber;
        auto MmPfnDatabaseSize = FIELD_OFFSET(PF_PFN_PRIO_REQUEST, PageIdentities) + PfnCount * sizeof(MMPFN_IDENTITY);
        auto MmPfnDatabase = static_cast<PPF_PFN_PRIO_REQUEST>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, MmPfnDatabaseSize));
        MmPfnDatabase->Version = 1;
        MmPfnDatabase->RequestFlags = 1;

        SUPERFETCH_INFORMATION sf_info = createSfInformation(SuperfetchPfnQuery, MmPfnDatabase, MmPfnDatabaseSize);

        for (auto k = 0, i = 0; i < superfetchPhysicalRanges->RangeCount; i++) {
            physical_memory_run = reinterpret_cast<PPHYSICAL_MEMORY_RUN>(&superfetchPhysicalRanges->Ranges[i]);

            for (SIZE_T j = physical_memory_run->BasePage; j < (physical_memory_run->BasePage + physical_memory_run->PageCount); j++) {
                auto Pfn1 = &MmPfnDatabase->PageIdentities[k++];
                Pfn1->PageFrameIndex = j;
            }
            MmPfnDatabase->PfnCount = k;
        }

        NTSTATUS st = QuerySuperfetchInfo(&sf_info);
        if (!NT_SUCCESS(st)) {
            std::cout << "[X] QueryMemoryPages failed: 0x" << std::hex << st << std::endl;

            HeapFree(GetProcessHeap(), 0, MmPfnDatabase);
            return -1;
        }

        *pSuperfetchPages = MmPfnDatabase;

        return 0;
    }

    ~Superfetch() = default;
};

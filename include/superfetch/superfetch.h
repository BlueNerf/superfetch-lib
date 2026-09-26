#pragma once

#include <iostream>
#include <ntstatus.h>
#include <Windows.h>
#include <winternl.h>

#include "nt.h"

struct PfnList {
    bool is_pool;
    std::uint32_t pool_tag;
};

class Superfetch {
public:
    static Superfetch& GetInstance() {
        static Superfetch instance;
        return instance;
    }

    [[nodiscard]] static int ForceUpdateRanges() {
        return QueryMemoryRanges(&superfetch_physical_ranges_);
    }

    // Gets the raw
     [[nodiscard]] static PPF_MEMORY_RANGE_INFO GetRawMemoryRanges() {
        return superfetch_physical_ranges_;
    }

    Superfetch(const Superfetch&) = delete;
    Superfetch& operator=(const Superfetch&) = delete;

private:
    int refCount_ = 0;
    inline static PPF_MEMORY_RANGE_INFO superfetch_physical_ranges_;
    inline static PPF_PFN_PRIO_REQUEST pfn_ranges_;

    Superfetch() {
        refCount_++;
        int err{};
        try {
            err = SfSetupPrivileges();
            if (err) { throw std::runtime_error("[iX] Failed to setup privileges (possibly need administrator privileges)"); };
            err = SfSetupPages();
            if (err) { throw std::runtime_error("[iX] Failed to setup superfetch pages"); }
        } catch (const std::runtime_error& e) {
            std::cerr << e.what() << "(error: " << err << ")" << std::endl;

            superfetch_physical_ranges_ = nullptr;
            pfn_ranges_ = nullptr;
        }
    }

    [[nodiscard]] static int SfSetupPages()
    {
        auto ranges_response = QueryMemoryRanges(&superfetch_physical_ranges_);
        if (ranges_response == 0) {
            auto pages_response = QueryMemoryPages(&pfn_ranges_);
            if (pages_response == 0) {
                return 0;
            }
            return 2;
        }

        return 1;
    }

    [[nodiscard]] static int SfSetupPrivileges() {
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

    static NTSTATUS QuerySuperfetchInfo(SUPERFETCH_INFORMATION* superfetch_information, PULONG return_length) {

        if (return_length != nullptr) {
            return NtQuerySystemInformation(SystemSuperfetchInformation, superfetch_information, sizeof(*superfetch_information), return_length);
        }
        ULONG unused_return_length;
        return NtQuerySystemInformation(SystemSuperfetchInformation, superfetch_information, sizeof(*superfetch_information), &unused_return_length);
    }

    template <typename T>
    static SUPERFETCH_INFORMATION CreateSfInformation(SUPERFETCH_INFORMATION_CLASS superfetch_information_class, T* superfetch_information, SIZE_T superfetch_information_length) {
        SUPERFETCH_INFORMATION sf_information{};
        sf_information.Version                     = SUPERFETCH_VERSION;
        sf_information.Magic                       = SUPERFETCH_MAGIC;
        sf_information.SuperfetchInformationClass  = superfetch_information_class;
        sf_information.SuperfetchInformation       = superfetch_information;
        sf_information.SuperfetchInformationLength = superfetch_information_length;

        return sf_information;
    }

    [[nodiscard]] static int QueryMemoryRanges(PPF_MEMORY_RANGE_INFO* superfetch_ranges) {
        PF_MEMORY_RANGE_INFO memory_range_info{};
        memory_range_info.Version = 2;
        memory_range_info.Flags   = 0;

        SUPERFETCH_INFORMATION superfetch_information = CreateSfInformation(SuperfetchMemoryRangesQuery, &memory_range_info, sizeof(memory_range_info));
        ULONG return_length;
        NTSTATUS st = QuerySuperfetchInfo(&superfetch_information, &return_length);

        PPF_MEMORY_RANGE_INFO ranges = &memory_range_info;

        if (st == STATUS_BUFFER_TOO_SMALL) {
            ranges = static_cast<PPF_MEMORY_RANGE_INFO>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, return_length));

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

        *superfetch_ranges = ranges;
        return 0;
    }

    [[nodiscard]] static int QueryMemoryPages(PPF_PFN_PRIO_REQUEST* superfetch_pages) {
        if (superfetch_physical_ranges_ == nullptr) {
            std::cout << "[X] Query ranges before setting up pages. (rangecount -> " << superfetch_physical_ranges_->RangeCount << ")"<< std::endl;
            return -1;
        }

        PPHYSICAL_MEMORY_RUN physical_memory_run;
        ULONG_PTR mm_highest_page_number = 0;
        for (ULONG i = 0; i < superfetch_physical_ranges_->RangeCount; i++) {
            physical_memory_run = reinterpret_cast<PPHYSICAL_MEMORY_RUN>(&superfetch_physical_ranges_->Ranges[i]);
            mm_highest_page_number = physical_memory_run->BasePage + physical_memory_run->PageCount;
        }
        auto pfn_count = mm_highest_page_number;
        auto mm_pfn_database_size = FIELD_OFFSET(PF_PFN_PRIO_REQUEST, PageIdentities) + pfn_count * sizeof(MMPFN_IDENTITY);
        auto mm_pfn_database = static_cast<PPF_PFN_PRIO_REQUEST>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, mm_pfn_database_size));
        mm_pfn_database->Version = 1;
        mm_pfn_database->RequestFlags = 1;

        SUPERFETCH_INFORMATION sf_info = CreateSfInformation(SuperfetchPfnQuery, mm_pfn_database, mm_pfn_database_size);

        for (auto k = 0, i = 0; i < superfetch_physical_ranges_->RangeCount; i++) {
            physical_memory_run = reinterpret_cast<PPHYSICAL_MEMORY_RUN>(&superfetch_physical_ranges_->Ranges[i]);

            for (SIZE_T j = physical_memory_run->BasePage; j < (physical_memory_run->BasePage + physical_memory_run->PageCount); j++) {
                auto pfn_1 = &mm_pfn_database->PageIdentities[k++];
                pfn_1->PageFrameIndex = j;
            }
            mm_pfn_database->PfnCount = k;
        }

        NTSTATUS st = QuerySuperfetchInfo(&sf_info, nullptr);
        if (!NT_SUCCESS(st)) {
            std::cout << "[X] QueryMemoryPages failed: 0x" << std::hex << st << std::endl;

            HeapFree(GetProcessHeap(), 0, mm_pfn_database);
            return -1;
        }

        *superfetch_pages = mm_pfn_database;

        return 0;
    }

    ~Superfetch() = default;
};

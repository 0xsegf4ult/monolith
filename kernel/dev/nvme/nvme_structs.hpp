#pragma once

#include <lib/types.hpp>

enum NVME_REGISTER
{
        NVME_REGISTER_CAPABILITY                = 0,
        NVME_REGISTER_VERSION                   = 0x8,
        NVME_REGISTER_INTMASK_SET               = 0xc,
        NVME_REGISTER_INTMASK_CLEAR             = 0x10,
        NVME_REGISTER_CONTROLLER_CONFIG         = 0x14,
        NVME_REGISTER_CONTROLLER_STATUS         = 0x1c,
        NVME_REGISTER_ADMIN_QUEUE_ATTR          = 0x24,
        NVME_REGISTER_ADMIN_SUBMISSION_QUEUE    = 0x28,
        NVME_REGISTER_ADMIN_COMPLETION_QUEUE    = 0x30,
        NVME_REGISTER_QUEUE_TAIL_DOORBELL_BASE  = 0x1000,
        NVME_REGISTER_QUEUE_HEAD_DOORBELL_BASE  = 0x1000
};

union __attribute__((packed)) nvme_capability
{
        struct __attribute__((packed))
        {
                uint16_t max_queue_entries;
                uint8_t contiguous_queue_required : 1;
                uint8_t arbitration_mechanism_supported : 2;
                uint8_t _reserved_0 : 5;
                uint8_t timeout;
                uint8_t doorbell_stride : 4;
                uint8_t subsystem_reset_supported : 1;
                uint8_t command_sets_supported;
                uint8_t boot_partition_supported : 1;
                uint8_t controller_power_scope : 2;
                uint8_t memory_pagesize_min : 4;
                uint8_t memory_pagesize_max : 4;
                uint8_t persistent_mem_region_supported : 1;
                uint8_t controller_mem_buffer_supported : 1;
                uint8_t subsystem_shtudown_supported : 1;
                uint8_t ready_modes_supported : 2;
                uint8_t shutdown_enhancements_supported : 1;
                uint8_t _reserved_1 : 2;
        };

        uint64_t raw;
};

union __attribute__((packed)) nvme_config
{
        struct __attribute__((packed))
        {
                uint8_t enabled : 1;
                uint8_t _reserved_0 : 3;
                uint8_t io_commandset_selected : 3;
                uint8_t memory_pagesize : 4;
                uint8_t arbitration_mechanism_selected : 3;
                uint8_t shutdown_notification : 2;
                uint8_t io_sq_entrysize : 4;
                uint8_t io_cq_entrysize : 4;
                uint8_t crime : 1;
                uint8_t _reserved_1 : 7;
        };

        uint32_t raw;
};

union __attribute__((packed)) nvme_status
{
        struct __attribute__((packed))
        {
                uint8_t ready : 1;
                uint8_t fatal_status : 1;
                uint8_t shutdown_status : 2;
                uint8_t subsystem_reset_occured : 1;
                uint8_t processing_paused : 1;
                uint8_t shutdown_type : 1;
                uint32_t _reserved : 25;
        };

        uint32_t raw;
};

enum NVME_ADMIN_OPCODE
{
        NVME_ADMIN_OP_IDENTIFY = 0x6
};

struct __attribute__((packed)) nvme_cmd_header
{
        uint8_t opcode;
        uint8_t fused : 2;
        uint8_t _reserved: 4;
        uint8_t psdt : 2;
        uint16_t command_id;
};

struct __attribute__((packed)) nvme_cmd
{
        nvme_cmd_header header;
        uint32_t nsid;

        uint32_t cmd_dword_2;
        uint32_t cmd_dword_3;
        uint64_t mptr;
        uint64_t prp1;
        uint64_t prp2;

        union
        {

                struct __attribute__((packed))
                {
                        uint32_t cmd_dword_10;
                        uint32_t cmd_dword_11;
                        uint32_t cmd_dword_12;
                        uint32_t cmd_dword_13;
                        uint32_t cmd_dword_14;
                        uint32_t cmd_dword_15;
                } dwords;

                struct __attribute__((packed))
                {
                        uint8_t cns;
                        uint8_t _reserved_0;
                        uint16_t controller_id;
                        uint16_t cnssid;
                        uint8_t _reserved_1;
                        uint8_t command_set_id;
                        uint64_t _reserved_2;
                        uint8_t uuid_index : 7;
                        uint32_t _reserved_3 : 25;
                } identify;
        };
};

struct __attribute__((packed)) nvme_completion
{
        uint32_t cmd_specific_0;
        uint32_t cmd_specific_1;
        uint16_t queue_head_pointer;
        uint16_t queue_id;
        uint16_t cmd_id;
        bool phase_tag : 1;
        uint16_t status : 15;
};

union __attribute__((packed)) nvme_identify_controller
{
	struct __attribute__((packed))
	{
		uint16_t pci_vid;
		uint16_t pci_ssvid;
		char serial_number[20];
		char model_number[40];
		char firmware_revision[8];
		uint8_t recommended_arbitration_burst;
		uint8_t ieee_oui[3];
		uint8_t cmic;
		uint8_t max_data_transfer_size;
		uint16_t controller_id;
		uint32_t version;
		uint32_t rtd3r;
		uint32_t rtd3e;
		uint32_t optional_async_events_supported;
		uint32_t controller_attributes;
		uint16_t rrls;
		uint8_t boot_partition_capabilities;
		uint8_t _reserved_0;
		uint32_t nssl;
		uint16_t _reserved_1;
		uint8_t plsi;
		uint8_t controller_type;
		uint64_t fguid[2];
		uint16_t crdt1;
		uint16_t crdt2;
		uint16_t crdt3;
		uint8_t crcap;
		uint8_t ciu;
		uint64_t cirn;
		char _reserved_2[109];
		uint8_t nvmsr;
		uint8_t vwci;
		uint8_t mec;

		uint16_t oacs;
		uint8_t abort_cmd_limit;
		uint8_t aerl;
		uint8_t fw_update;
		uint8_t log_page_attr;
		uint8_t elpe;
		uint8_t npss;
		uint8_t avscc;
		uint8_t apsta;
		uint16_t wctemp;
		uint16_t cctemp;
		uint16_t mtfa;
		uint32_t hostmem_pref;
		uint32_t hostmem_min;
		uint64_t total_nvm_capacity[2];
		uint64_t unallocated_nvm_capacity[2];
		uint32_t rpmbs;
		uint16_t edstt;
		uint8_t dsto;
		uint8_t fwug;
		uint16_t kas;
		uint16_t hctma;
		uint16_t mntmt;
		uint16_t mxntmt;
		uint32_t sanicap;
		uint32_t hmminds;
		uint16_t hmmaxd;
		uint16_t nvm_setid_max;
		uint16_t endgidmax;
		uint8_t anatt;
		uint8_t anacap;
		uint32_t anagrpmax;
		uint32_t nanagrpid;
		uint32_t pels;
		char _reserved_3[156];

		uint8_t submit_queue_entry_size;
		uint8_t completion_queue_entry_size;
		uint16_t maxcmd;
		uint32_t num_namespaces;
		uint16_t oncs;
		uint16_t fuses;
		uint8_t fna;
		uint8_t volatile_write_cache;
		uint16_t awun;
		uint16_t awupf;
		uint8_t nvscc;
		uint8_t nwpc;
		uint16_t acwu;
		uint16_t cdfs;
		uint32_t sgls;
		uint32_t max_namespaces;
		char _reserved_4[224];
		char subnqn[256];
		char _reserved_5[3072];
	};

	uint8_t raw[4096];
};

struct __attribute__((packed)) nvme_lbaf
{
	uint16_t metadata_size;
	uint8_t lba_data_size;
	uint8_t rp : 2;
	uint8_t reserved : 6;
};

union __attribute__((packed)) nvme_identify_namespace
{
	struct __attribute__((packed))
	{
		uint64_t nsze;
		uint64_t ncap;
		uint64_t nuse;
		uint8_t nsfeat;
		uint8_t nlbaf;
		uint8_t flbas;
		uint8_t mc;
		uint8_t dpc;
		uint8_t dps;
		uint8_t nmic;
		uint8_t rescap;
		uint8_t fpi;
		uint8_t dlfeat;
		uint16_t nawun;
		uint16_t nawupf;
		uint16_t nacwu;
		uint16_t nabsn;
		uint16_t nabo;
		uint16_t nabspf;
		uint16_t noiob;
		uint64_t nvmcap[2];
		uint16_t npwg;
		uint16_t npwa;
		uint16_t npdg;
		uint16_t npda;
		uint16_t nows;
		uint16_t mssrl;
		uint32_t mcl;
		uint8_t msrc;
		uint8_t _reserved_0;
		uint8_t nulbaf;
		uint8_t _reserved_1[9];
		uint32_t anagrpid;
		uint8_t _reserved_2[3];
		uint8_t nsattr;
		uint16_t nvmsetid;
		uint16_t endgid;
		uint64_t nguid[2];
		uint16_t eui64;
		nvme_lbaf lbaf[16];
		uint64_t lbstm;
		uint8_t vs[3704];
	};

	uint8_t raw[4096];
};

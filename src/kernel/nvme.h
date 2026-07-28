#ifndef EMULATOR_INCLUDE_EMULATOR_KERNEL_NVME_H_
#define EMULATOR_INCLUDE_EMULATOR_KERNEL_NVME_H_

#include "common/abi.h"
#include "common/common.h"
#include "common/subsystems.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <vector>

namespace Libs::LibKernel::Nvme {

KYTY_SUBSYSTEM_DEFINE(NvmeStorage);

// =============================================================================
// NVMe 1.4 Constants
// =============================================================================

constexpr uint64_t NVME_MMIO_BAR_SIZE          = 0x4000;   // 16 KiB BAR0
constexpr uint32_t NVME_ADMIN_QUEUE_SIZE        = 64;       // Admin queue entries
constexpr uint32_t NVME_DEFAULT_IO_QUEUE_SIZE   = 256;      // I/O queue entries
constexpr uint32_t NVME_MAX_IO_QUEUES           = 64;       // Max I/O queue pairs
constexpr uint16_t NVME_DEFAULT_NSID             = 1;        // Default namespace ID
constexpr uint32_t NVME_SECTOR_SIZE              = 512;      // Bytes per sector
constexpr uint32_t NVME_PRP_PAGE_SIZE            = 0x1000;   // 4 KiB PRP page
constexpr uint32_t NVME_MAX_DATA_TRANSFER        = (2 * 1024 * 1024); // 2 MiB max per command

constexpr uint32_t NVME_CAP_MPSMIN              = 0;        // 4 KiB minimum page size
constexpr uint32_t NVME_CAP_MPSMAX              = 0;        // 4 KiB maximum page size
constexpr uint32_t NVME_CAP_DSTRD               = 0;        // Doorbell stride: 4 bytes
constexpr uint32_t NVME_CAP_NSSRS               = 1;        // NVM Subsystem Reset supported
constexpr uint32_t NVME_CAP_CSS_NVM             = 0x01;     // NVM command set
constexpr uint32_t NVME_CAP_BPS                 = 0;        // Boot Partition Support: no
constexpr uint32_t NVME_CAP_MQES                = 1024;     // Max queue entries supported
constexpr uint64_t NVME_CAP_AMS                 = 0;        // Arbitration Mechanism
constexpr uint64_t NVME_CAP_TO                  = 120;      // Ready timeout in 500ms units (60s)

constexpr uint32_t NVME_VS_MAJOR                = 1;
constexpr uint32_t NVME_VS_MINOR                = 4;
constexpr uint32_t NVME_VS_TER                  = 0;

// Controller Configuration (CC) bitfields
constexpr uint32_t CC_ENABLE                    = (1u << 0u);
constexpr uint32_t CC_IOCQES_SHIFT              = 20;
constexpr uint32_t CC_IOSQES_SHIFT              = 16;
constexpr uint32_t CC_SHN_SHIFT                 = 14;
constexpr uint32_t CC_SHN_MASK                  = (3u << CC_SHN_SHIFT);
constexpr uint32_t CC_AMS_SHIFT                 = 11;
constexpr uint32_t CC_MPS_SHIFT                 = 7;

// Controller Status (CSTS) bitfields
constexpr uint32_t CSTS_RDY                     = (1u << 0u);
constexpr uint32_t CSTS_CFS                     = (1u << 1u);
constexpr uint32_t CSTS_SHST_SHIFT              = 2;
constexpr uint32_t CSTS_SHST_MASK               = (3u << CSTS_SHST_SHIFT);
constexpr uint32_t CSTS_NSSRO                   = (1u << 4u);
constexpr uint32_t CSTS_PP                      = (1u << 5u);

// =============================================================================
// NVMe Register Offsets (BAR0)
// =============================================================================

enum class NvmeRegister : uint32_t {
	CAP        = 0x0000,  // Controller Capabilities (8 bytes, RO)
	VS         = 0x0008,  // Version (4 bytes, RO)
	INTMS      = 0x000C,  // Interrupt Mask Set (4 bytes, RW)
	INTMC      = 0x0010,  // Interrupt Mask Clear (4 bytes, WO)
	CC         = 0x0014,  // Controller Configuration (4 bytes, RW)
	RSVD1      = 0x0018,  // Reserved
	CSTS       = 0x001C,  // Controller Status (4 bytes, RO)
	NSSR       = 0x0020,  // NVM Subsystem Reset (4 bytes, WO)
	AQA        = 0x0024,  // Admin Queue Attributes (4 bytes, RW)
	ASQ        = 0x0028,  // Admin SQ Base Address (8 bytes, RW)
	ACQ        = 0x0030,  // Admin CQ Base Address (8 bytes, RW)
	CMBLOC     = 0x0038,  // Controller Memory Buffer Location (4 bytes, RO)
	CMBSZ      = 0x003C,  // Controller Memory Buffer Size (4 bytes, RO)
	BPINFO     = 0x0040,  // Boot Partition Information (4 bytes, RO)
	BPRSEL     = 0x0044,  // Boot Partition Read Select (4 bytes, RW)
	BPMBL      = 0x0048,  // Boot Partition Memory Buffer Location (8 bytes, RO)
	CMBMSC     = 0x0050,  // Controller Memory Buffer Memory Space Control (4 bytes, RW)
	CMBSTS     = 0x0058,  // Controller Memory Buffer Status (4 bytes, RO)
	PMRCAP     = 0x0E00,  // Persistent Memory Region Capabilities (4 bytes, RO)
	PMRCTL     = 0x0E04,  // Persistent Memory Region Control (4 bytes, RW)
	PMRSTS     = 0x0E08,  // Persistent Memory Region Status (4 bytes, RO)
	PMREBS     = 0x0E0C,  // Persistent Memory Region Elasticity Buffer Size (4 bytes, RO)
	PMRSWTP    = 0x0E10,  // Persistent Memory Region Sustained Write Throughput (4 bytes, RO)
	PMRMSCL    = 0x0E14,  // Persistent Memory Region Memory Space Control Lower (4 bytes, RW)
	PMRMSCU    = 0x0E18,  // Persistent Memory Region Memory Space Control Upper (4 bytes, RW)
	DOORBELL_BASE = 0x1000, // SQ0 doorbell starts at +0x1000 (8 KiB offset)
};

// Doorbell: SQ0 at offset 0x1000, CQ0 at offset 0x1000 + (4 << CAP.DSTRD)
// Subsequent SQs at offset 0x1000 + (2*idx)*(4 << CAP.DSTRD)
// Subsequent CQs at offset 0x1000 + (2*idx+1)*(4 << CAP.DSTRD)

// =============================================================================
// NVMe Admin Command Opcodes
// =============================================================================

enum class NvmeAdminOpcode : uint8_t {
	DeleteIOSQ          = 0x00,
	CreateIOSQ          = 0x01,
	GetLogPage          = 0x02,
	DeleteIOCQ          = 0x04,
	CreateIOCQ          = 0x05,
	Identify            = 0x06,
	Abort               = 0x08,
	SetFeatures         = 0x09,
	GetFeatures         = 0x0A,
	AsyncEventRequest   = 0x0C,
	NamespaceManagement = 0x0D,
	FirmwareCommit      = 0x10,
	FirmwareImageDown   = 0x11,
	DeviceSelfTest      = 0x14,
	NamespaceAttachment = 0x15,
	KeepAlive           = 0x18,
	DirectiveSend       = 0x19,
	DirectiveReceive    = 0x1A,
	VirtualizationMgmt  = 0x1C,
	NVMeMISend          = 0x1D,
	NVMeMIReceive       = 0x1E,
	DoorbellBufferCfg   = 0x7C,
	FormatNVM           = 0x80,
	SecuritySend        = 0x81,
	SecurityReceive     = 0x82,
	Sanitize            = 0x84,
	GetLBAStatus        = 0x86,
};

// =============================================================================
// NVMe NVM (I/O) Command Opcodes
// =============================================================================

enum class NvmeNvmOpcode : uint8_t {
	Flush               = 0x00,
	Write               = 0x01,
	Read                = 0x02,
	WriteUncorrectable  = 0x04,
	Compare             = 0x05,
	WriteZeroes         = 0x08,
	DatasetManagement   = 0x09,
	Verify              = 0x0C,
	ReservationRegister = 0x0D,
	ReservationReport   = 0x0E,
	ReservationAcquire  = 0x11,
	ReservationRelease  = 0x15,
	Copy                = 0x19,
};

// =============================================================================
// NVMe Identify CNS values
// =============================================================================

enum class NvmeIdentifyCns : uint8_t {
	Namespace                  = 0x00,
	Controller                 = 0x01,
	ActiveNamespaceList        = 0x02,
	NamespaceDescriptorList    = 0x03,
	AllocatedNamespaceList     = 0x10,
	NamespaceAllocated         = 0x11,
	ControllerAllocated        = 0x12,
	IOCSAllocated              = 0x1A,
	NVMCommandSetIdentify      = 0x1C,
};

// =============================================================================
// NVMe Feature IDs
// =============================================================================

enum class NvmeFeatureId : uint8_t {
	Arbitration             = 0x01,
	PowerManagement         = 0x02,
	LBARangeType            = 0x03,
	TemperatureThreshold    = 0x04,
	ErrorRecovery           = 0x05,
	VolatileWriteCache      = 0x06,
	NumberOfQueues          = 0x07,
	InterruptCoalescing     = 0x08,
	InterruptVectorConfig   = 0x09,
	WriteAtomicityNormal    = 0x0A,
	AsyncEventConfig        = 0x0B,
	AutonomousPowerState    = 0x0C,
	HostMemoryBuffer        = 0x0D,
	Timestamp               = 0x0E,
	KeepAliveTimer          = 0x0F,
	HostControlledThermal   = 0x10,
	NonOperationalPowerState= 0x11,
	ReadRecoveryLevelConfig = 0x12,
	PredictableLatencyMode  = 0x15,
	PredictableLatencyWindow= 0x16,
	LBASpecificInfo         = 0x17,
	SoftwareProgressMarker  = 0x80,
	HostIdentifier           = 0x81,
	ReservationNotificationMask = 0x82,
	ReservationPersistence   = 0x83,
	NamespaceWriteProtection = 0x84,
};

// =============================================================================
// NVMe Data Structures
// =============================================================================

// Submission Queue Entry (64 bytes)
struct NvmeSQEntry {
	uint8_t  opcode;          // 00: Command opcode
	uint8_t  flags;           // 01: FUSE/PSDT
	uint16_t command_id;      // 02: Command identifier
	uint32_t nsid;            // 04: Namespace ID
	uint64_t reserved1;       // 08
	uint64_t metadata;        // 10: Metadata pointer
	union {
		struct {
			uint64_t prp1;    // 18: PRP Entry 1 / Data Pointer 1
			uint64_t prp2;    // 20: PRP Entry 2 / Data Pointer 2
		};
		uint64_t data_ptr[2];
	};
	union {
		struct {
			uint64_t slba;    // 28: Starting LBA (for NVM commands)
			uint16_t nlb;     // 30: Number of logical blocks (0-based)
			// ...
		};
		uint32_t cdw10;
		uint32_t cdw[6];      // 28-47: Command dwords 10-15
	};
};

static_assert(sizeof(NvmeSQEntry) == 64, "NvmeSQEntry must be 64 bytes");

// Completion Queue Entry (16 bytes)
struct NvmeCQEntry {
	uint32_t command_specific; // 00: Command specific
	uint32_t reserved;         // 04
	uint16_t sq_head;          // 08: SQ Head pointer
	uint16_t sq_id;            // 0A: SQ Identifier
	uint16_t command_id;       // 0C: Command Identifier
	struct {
		uint8_t phase   : 1;   // 0E: Phase tag (P)
		uint8_t status  : 7;   // 0E: Status field
	};
	uint8_t status_field;      // 0F: Status field type
};

static_assert(sizeof(NvmeCQEntry) == 16, "NvmeCQEntry must be 16 bytes");

// Identify Controller Data Structure (4096 bytes)
struct NvmeIdentifyController {
	uint16_t vid;              // PCI Vendor ID
	uint16_t ssvid;            // PCI Subsystem Vendor ID
	char     sn[20];           // Serial Number
	char     mn[40];           // Model Number
	char     fr[8];            // Firmware Revision
	uint8_t  rab;              // Recommended Arbitration Burst
	uint8_t  ieee[3];          // IEEE OUI Identifier
	uint8_t  cmic;             // Controller Multi-Path I/O and NS Sharing Caps
	uint8_t  mdts;             // Maximum Data Transfer Size
	uint16_t cntlid;           // Controller ID
	uint32_t ver;              // Version (same as VS register)
	uint32_t rtd3r;            // RTD3 Resume Latency
	uint32_t rtd3e;            // RTD3 Entry Latency
	uint32_t oaes;             // Optional Async Events Supported
	uint32_t ctratt;           // Controller Attributes
	uint16_t rrls;             // Read Recovery Levels Supported
	uint8_t  reserved1[9];
	uint8_t  cntrltype;        // Controller Type
	uint8_t  fguid[16];        // FRU Globally Unique Identifier
	uint16_t crdt1;            // Command Retry Delay Time 1
	uint16_t crdt2;
	uint16_t crdt3;
	uint8_t  reserved2[106];
	uint8_t  nvmsr;            // NVM Subsystem Report
	uint8_t  vwci;             // VPD Write Cycle Information
	uint8_t  mec;              // Management Endpoint Capabilities
	uint16_t oacs;             // Optional Admin Command Support
	uint8_t  acl;              // Abort Command Limit
	uint8_t  aerl;             // Async Event Request Limit
	uint8_t  frmw;             // Firmware Updates
	uint8_t  lpa;              // Log Page Attributes
	uint8_t  elpe;             // Error Log Page Entries
	uint8_t  npss;             // Number of Power States Support
	uint8_t  avscc;            // Admin Vendor Specific Command Config
	uint8_t  apsta;            // Autonomous Power State Transition Attributes
	uint16_t wctemp;           // Warning Composite Temperature Threshold
	uint16_t cctemp;           // Critical Composite Temperature Threshold
	uint16_t mtfa;             // Maximum Time for Firmware Activation
	uint32_t hmpre;            // Host Memory Buffer Preferred Size
	uint32_t hmmin;            // Host Memory Buffer Minimum Size
	uint64_t tnvmcap[2];       // Total NVM Capacity
	uint64_t unvmcap[2];       // Unallocated NVM Capacity
	uint32_t rpmbs;            // Replay Protected Memory Block Support
	uint16_t edstt;            // Extended Device Self-test Time
	uint8_t  dsto;             // Device Self-test Options
	uint8_t  fwug;             // Firmware Update Granularity
	uint16_t kas;              // Keep Alive Support
	uint16_t hctma;            // Host Controlled Thermal Management Attributes
	uint16_t mntmt;            // Minimum Thermal Management Temperature
	uint16_t mxtmt;            // Maximum Thermal Management Temperature
	uint32_t sanicap;          // Sanitize Capabilities
	uint32_t hmminds;          // Host Memory Buffer Minimum Descriptor Size
	uint16_t hmmaxd;           // Host Memory Maximum Descriptors
	uint16_t nsetidmax;        // NVM Set Identifier Maximum
	uint32_t endgidmax;        // Endurance Group Identifier Maximum
	uint8_t  anatt;            // ANA Transition Time
	uint8_t  anacap;           // Asymmetric Namespace Access Capabilities
	uint32_t anagrpmax;        // ANA Group Identifier Maximum
	uint32_t nanagrpid;        // Number of ANA Group Identifiers
	uint32_t pels;             // Persistent Event Log Size
	uint16_t domainid;         // Domain Identifier
	uint8_t  reserved3[10];
	uint8_t  sqes;             // Submission Queue Entry Size
	uint8_t  cqes;             // Completion Queue Entry Size
	uint16_t maxcmd;           // Maximum Outstanding Commands
	uint32_t nn;               // Number of Namespaces
	uint16_t oncs;             // Optional NVM Command Support
	uint16_t fuses;            // Fused Operation Support
	uint8_t  fna;              // Format NVM Attributes
	uint8_t  vwc;              // Volatile Write Cache
	uint16_t awun;             // Atomic Write Unit Normal
	uint16_t awupf;            // Atomic Write Unit Power Fail
	uint8_t  icsvscc;          // NVM Vendor Specific Command Config
	uint8_t  nwpc;             // Namespace Write Protection Capabilities
	uint16_t acwu;             // Atomic Compare & Write Unit
	uint16_t ocfw;             // Optional Copy Formats Supported
	uint8_t  reserved4[22];
	uint8_t  sgls[4];          // SGL Support
	uint8_t  mnan;             // Maximum Number of Allowed Namespaces
	uint8_t  reserved5[224];
	uint8_t  subnqn[256];      // NVM Subsystem NVMe Qualified Name
	uint8_t  reserved6[768];
	uint8_t  psd[32 * 32];     // Power State Descriptors (up to 32)
	uint8_t  vs[1024];         // Vendor Specific
	uint8_t  padding[384];      // Correct size to 4096
};

static_assert(sizeof(NvmeIdentifyController) == 4096, "Identify Controller must be 4096 bytes");

// Identify Namespace Data Structure (4096 bytes)
struct NvmeIdentifyNamespace {
	uint64_t nsze;             // Namespace Size (total LBAs)
	uint64_t ncap;             // Namespace Capacity
	uint64_t nuse;             // Namespace Utilization
	uint8_t  nsfeat;           // Namespace Features
	uint8_t  nlbaf;            // Number of LBA Formats
	uint8_t  flbas;            // Formatted LBA Size
	uint8_t  mc;               // Metadata Capabilities
	uint8_t  dpc;              // End-to-end Data Protection Capabilities
	uint8_t  dps;              // End-to-end Data Protection Type Settings
	uint8_t  nmic;             // Namespace Multi-path I/O and NS Sharing Caps
	uint8_t  rescap;           // Reservation Capabilities
	uint8_t  fpi;              // Format Progress Indicator
	uint8_t  dlfeat;           // Deallocate Logical Block Features
	uint16_t nawun;            // Namespace Atomic Write Unit Normal
	uint16_t nawupf;           // Namespace Atomic Write Unit Power Fail
	uint16_t nacwu;            // Namespace Atomic Compare & Write Unit
	uint16_t nabsn;            // Namespace Atomic Boundary Size Normal
	uint16_t nabo;             // Namespace Atomic Boundary Offset
	uint16_t nabspf;           // Namespace Atomic Boundary Size Power Fail
	uint16_t noiob;            // Namespace Optimal I/O Boundary
	uint64_t nvmcap[2];        // NVM Capacity
	uint16_t npwg;             // Namespace Preferred Write Granularity
	uint16_t npwa;             // Namespace Preferred Write Alignment
	uint16_t npdg;             // Namespace Preferred Deallocate Granularity
	uint16_t npda;             // Namespace Preferred Deallocate Alignment
	uint16_t nows;             // Namespace Optimal Write Size
	uint16_t mssrl;            // Maximum Single Source Range Length
	uint32_t mcl;              // Maximum Copy Length
	uint8_t  msrc;             // Maximum Source Range Count
	uint8_t  reserved1[11];
	uint32_t anagrpid;         // ANA Group Identifier
	uint8_t  reserved2[3];
	uint8_t  nsattr;           // Namespace Attributes
	uint16_t nvmsetid;         // NVM Set Identifier
	uint16_t endgid;           // Endurance Group Identifier
	uint8_t  nguid[16];        // Namespace Globally Unique Identifier
	uint8_t  eui64[8];         // IEEE Extended Unique Identifier
	uint8_t  lbaf[16][4];      // LBA Format support (up to 16 formats, 4 bytes each)
	uint8_t  reserved3[192];
	uint8_t  vs[3712];         // Vendor Specific
};

static_assert(sizeof(NvmeIdentifyNamespace) == 4096, "Identify Namespace must be 4096 bytes");

// Active Namespace ID List (4096 bytes)
struct NvmeNamespaceList {
	uint32_t nsid[1024];       // List of namespace IDs
};

static_assert(sizeof(NvmeNamespaceList) == 4096, "Namespace List must be 4096 bytes");

// =============================================================================
// NVMe Controller State
// =============================================================================

struct NvmeQueuePair {
	uint64_t base_addr;         // Physical address of queue in guest memory
	uint32_t size;              // Number of entries in queue
	uint32_t head;              // Head index
	uint32_t tail;              // Tail index
	uint32_t entry_size;        // Size of each entry (SQ: 64, CQ: 16)
	uint32_t phase;             // Current phase tag for CQ
	uint16_t irq_vector;        // Interrupt vector for this CQ
	bool     irq_enabled;       // Interrupts enabled for this CQ
};

struct NvmeControllerState {
	// Controller registers
	uint64_t cap;
	uint32_t vs;
	uint32_t intms;            // Interrupt Mask Set
	uint32_t cc;               // Controller Configuration
	uint32_t csts;             // Controller Status
	uint32_t aqa;              // Admin Queue Attributes
	uint64_t asq;              // Admin SQ Base Address
	uint64_t acq;              // Admin CQ Base Address

	// Admin queues
	NvmeQueuePair admin_sq;
	NvmeQueuePair admin_cq;

	// I/O queues (index 1..n)
	std::vector<NvmeQueuePair> io_sq;
	std::vector<NvmeQueuePair> io_cq;

	// Namespace backing
	std::filesystem::path backing_file;
	uint64_t namespace_size;   // Total size of namespace in bytes
	uint64_t namespace_lbas;   // Total LBAs

	// State
	bool     controller_enabled;
	bool     shutdown_complete;
	bool     initialized;
};

// =============================================================================
// NVMe Interrupt callback type
// =============================================================================

using NvmeInterruptCallback = std::function<void(uint16_t vector)>;

// =============================================================================
// Public API
// =============================================================================

// Initialize the NVMe subsystem with a backing file for the namespace.
// The backing file will be created if it doesn't exist.
void Initialize(const std::filesystem::path& backing_path, uint64_t size_bytes);

// Shutdown the NVMe subsystem.
void Shutdown();

// Set a callback for MSI-X interrupt delivery.
void SetInterruptCallback(NvmeInterruptCallback callback);

// Handle an MMIO read from the NVMe BAR0 region.
// offset: byte offset within BAR0
// size: 1, 2, 4, or 8 bytes
// value_out: filled with read value
// Returns true if the access was valid.
bool HandleMMIORead(uint32_t offset, uint32_t size, uint64_t* value_out);

// Handle an MMIO write to the NVMe BAR0 region.
// offset: byte offset within BAR0
// size: 1, 2, 4, or 8 bytes
// value: the value to write
// Returns true if the access was valid.
bool HandleMMIOWrite(uint32_t offset, uint32_t size, uint64_t value);

// Get the physical base address where NVMe MMIO registers are mapped.
uint64_t GetMMIOBaseAddress();

// Get the size of the MMIO region.
uint64_t GetMMIOSize();

// Get the number of namespaces.
uint32_t GetMaxNamespaces();

// Get the namespace size in bytes.
uint64_t GetNamespaceSize(uint32_t nsid = NVME_DEFAULT_NSID);

// Read directly from backing storage at the given LBA.
bool StorageRead(uint64_t lba, uint32_t num_sectors, void* buffer);

// Write directly to backing storage at the given LBA.
bool StorageWrite(uint64_t lba, uint32_t num_sectors, const void* buffer);

// Flush the backing storage.
bool StorageFlush();

// Check if the NVMe controller is ready.
bool IsControllerReady();

} // namespace Libs::LibKernel::Nvme

#endif /* EMULATOR_INCLUDE_EMULATOR_KERNEL_NVME_H_ */

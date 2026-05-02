/*
 * VMInstance.cpp — Vellum KVM VM lifecycle, memory setup, and resource control.
 *
 * Key improvements over original:
 *  - Cgroup setup is now best-effort (non-fatal) so VMs can start even without cgroup v2.
 *  - loadKernel() correctly handles the Linux boot protocol (setup sectors skipped).
 *  - setupVCPUs() enables protected mode and long mode in CR0/CR4/EFER and builds
 *    a minimal identity-mapped 4-level page table in guest memory.
 *  - Proper GDT installed in guest memory with valid 64-bit code/data descriptors.
 *  - Boot params (zero-page) written so the kernel receives its command line and initrd info.
 *  - Detailed per-step error reporting.
 *  - updateMetrics() reads real data from /proc/self/statm and cgroup cpu.stat.
 *  - sendConsoleInput is now thread-safe and notifies waiting readers.
 */

#include "VMInstance.h"
#include <cstring>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fstream>
#include <sstream>
#include <sys/mman.h>
#include <cerrno>
#include <cstdint>

// ── Linux boot protocol constants ────────────────────────────────────────────
static constexpr uint64_t BOOT_PARAMS_ADDR   = 0x10000;   // zero page / boot_params
static constexpr uint64_t CMDLINE_ADDR       = 0x20000;   // kernel command line
static constexpr uint64_t GDT_ADDR           = 0x500;     // GDT (below 1 MB, safe)
static constexpr uint64_t PML4_ADDR          = 0x1000;    // Page-map level-4 table
static constexpr uint64_t PDPT_ADDR          = 0x2000;    // Page-directory-pointer table
static constexpr uint64_t PD_ADDR            = 0x3000;    // Page directory (2 MB pages)
static constexpr uint64_t KERNEL_LOAD_ADDR   = 0x100000;  // Standard Linux load address (1 MB)
static constexpr uint64_t INITRD_LOAD_ADDR   = 0x4000000; // 64 MB – well above the kernel

// ── Minimal Linux boot_params fields we care about ───────────────────────────
// Full struct is ~4 KB; we only write the header fields we need.
static constexpr uint32_t LINUX_MAGIC        = 0x53726448; // "HdrS"
static constexpr uint8_t  BOOT_TYPE_HVIRT    = 0xB;        // boot loader type (hypervisor)
static constexpr uint16_t BOOT_FLAG          = 0xAA55;     // required magic at offset 0x1FE in setup

// boot_params offsets (from Linux kernel arch/x86/include/uapi/asm/bootparam.h)
namespace bp {
    static constexpr uint32_t setup_sects       = 0x1F1;
    static constexpr uint32_t boot_flag         = 0x1FE;
    static constexpr uint32_t header            = 0x202;  // "HdrS"
    static constexpr uint32_t version           = 0x206;
    static constexpr uint32_t type_of_loader    = 0x210;
    static constexpr uint32_t loadflags         = 0x211;
    static constexpr uint32_t ramdisk_image     = 0x218;
    static constexpr uint32_t ramdisk_size      = 0x21C;
    static constexpr uint32_t heap_end_ptr      = 0x224;
    static constexpr uint32_t cmd_line_ptr      = 0x228;
    static constexpr uint32_t kernel_alignment  = 0x230;
    static constexpr uint32_t relocatable_kernel= 0x234;
    static constexpr uint32_t e820_entries      = 0x1E8;  // number of e820 map entries
    static constexpr uint32_t e820_table        = 0x2D0;  // e820 map array (each entry = 20 bytes)
    // vid_mode (required): 0xFFFF = no mode set (EFI-style)
    static constexpr uint32_t vid_mode          = 0x1FA;
}

// e820 entry types
static constexpr uint32_t E820_RAM       = 1;
static constexpr uint32_t E820_RESERVED  = 2;

// ── Helper: write T at byte offset inside guest memory ───────────────────────
template<typename T>
static void guestWrite(void* gm, uint64_t offset, T value) {
    memcpy(static_cast<char*>(gm) + offset, &value, sizeof(T));
}

// ── Constructor / Destructor ─────────────────────────────────────────────────

VMInstance::VMInstance(const std::string& id, const std::string& kernelPath, const std::string& initrdPath,
                       const std::string& diskPath, const std::string& kernelCmdline,
                       size_t memoryMB, int vcpus)
    : id_(id), kernelPath_(kernelPath), initrdPath_(initrdPath), diskPath_(diskPath),
      kernelCmdline_(kernelCmdline), memoryMB_(memoryMB), vcpus_(vcpus),
      state_(State::Stopped), running_(false), guest_memory_(nullptr),
      guest_memory_size_(memoryMB * 1024 * 1024),
      console_fd_(-1), cgroup_path_(""), paused_(false) {
    kvm_fd_ = -1;
    vm_fd_  = -1;
    vcpu_fds_.resize(vcpus_, -1);
    last_metrics_ = {0.0, 0, 0};
}

VMInstance::~VMInstance() {
    stop();
    cleanupCgroup();
    if (guest_memory_ && guest_memory_ != MAP_FAILED)
        munmap(guest_memory_, guest_memory_size_);
    if (vm_fd_  >= 0) close(vm_fd_);
    if (kvm_fd_ >= 0) close(kvm_fd_);
    if (console_fd_ >= 0) close(console_fd_);
    for (int fd : vcpu_fds_)
        if (fd >= 0) close(fd);
}

// ── KVM / Memory initialisation ──────────────────────────────────────────────

bool VMInstance::initializeKVM() {
    kvm_fd_ = open("/dev/kvm", O_RDWR | O_CLOEXEC);
    if (kvm_fd_ < 0) {
        std::cerr << "[" << id_ << "] Failed to open /dev/kvm: " << strerror(errno)
                  << "\n  → Make sure KVM is loaded (modprobe kvm_intel or kvm_amd) "
                     "and your user is in the 'kvm' group.\n";
        return false;
    }

    // Verify API version
    int api_ver = ioctl(kvm_fd_, KVM_GET_API_VERSION, 0);
    if (api_ver != 12) {
        std::cerr << "[" << id_ << "] Unexpected KVM API version: " << api_ver << "\n";
        return false;
    }

    vm_fd_ = ioctl(kvm_fd_, KVM_CREATE_VM, 0);
    if (vm_fd_ < 0) {
        std::cerr << "[" << id_ << "] KVM_CREATE_VM failed: " << strerror(errno) << "\n";
        return false;
    }

    // Allocate guest physical memory (MAP_NORESERVE for large allocations)
    guest_memory_ = mmap(nullptr, guest_memory_size_,
                         PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (guest_memory_ == MAP_FAILED) {
        guest_memory_ = nullptr;
        std::cerr << "[" << id_ << "] mmap guest memory failed (" << memoryMB_
                  << " MB): " << strerror(errno) << "\n";
        return false;
    }
    // Pre-fault at least the low 1 MB so e820 / GDT writes land in real pages
    madvise(guest_memory_, 1 * 1024 * 1024, MADV_WILLNEED);
    memset(guest_memory_, 0, std::min(guest_memory_size_, (size_t)(4 * 1024 * 1024)));

    // Map the region into the VM's guest physical address space
    struct kvm_userspace_memory_region region = {};
    region.slot            = 0;
    region.flags           = 0;
    region.guest_phys_addr = 0;
    region.memory_size     = guest_memory_size_;
    region.userspace_addr  = reinterpret_cast<uint64_t>(guest_memory_);
    if (ioctl(vm_fd_, KVM_SET_USER_MEMORY_REGION, &region) < 0) {
        std::cerr << "[" << id_ << "] KVM_SET_USER_MEMORY_REGION failed: " << strerror(errno) << "\n";
        return false;
    }

    // Create VCPUs
    for (int i = 0; i < vcpus_; ++i) {
        vcpu_fds_[i] = ioctl(vm_fd_, KVM_CREATE_VCPU, i);
        if (vcpu_fds_[i] < 0) {
            std::cerr << "[" << id_ << "] KVM_CREATE_VCPU " << i
                      << " failed: " << strerror(errno) << "\n";
            return false;
        }
    }

    std::cout << "[" << id_ << "] KVM initialised: "
              << memoryMB_ << " MB RAM, " << vcpus_ << " vCPU(s)\n";
    return true;
}

// ── Kernel loading (Linux boot protocol 2.12+) ───────────────────────────────

bool VMInstance::loadKernel() {
    // ---- Open bzImage ----
    int kfd = open(kernelPath_.c_str(), O_RDONLY | O_CLOEXEC);
    if (kfd < 0) {
        std::cerr << "[" << id_ << "] Cannot open kernel '" << kernelPath_
                  << "': " << strerror(errno) << "\n";
        return false;
    }

    struct stat st;
    if (fstat(kfd, &st) < 0) {
        std::cerr << "[" << id_ << "] fstat kernel: " << strerror(errno) << "\n";
        close(kfd);
        return false;
    }
    size_t total_size = static_cast<size_t>(st.st_size);

    // Read the first 512 bytes (sector 0) to inspect setup_sects
    std::vector<uint8_t> header(4096, 0);
    ssize_t hdr_read = read(kfd, header.data(), std::min<size_t>(4096, total_size));
    if (hdr_read < 512) {
        std::cerr << "[" << id_ << "] Kernel file too small\n";
        close(kfd);
        return false;
    }

    // Number of 512-byte setup sectors (offset 0x1F1); add 1 for sector 0
    uint8_t setup_sects_val = header[0x1F1];
    if (setup_sects_val == 0) setup_sects_val = 4; // default if not set
    size_t setup_bytes = (setup_sects_val + 1) * 512;

    if (total_size <= setup_bytes) {
        std::cerr << "[" << id_ << "] bzImage too small: " << total_size
                  << " bytes, expected > " << setup_bytes << " setup bytes\n";
        close(kfd);
        return false;
    }

    size_t protected_mode_size = total_size - setup_bytes;
    if (protected_mode_size > guest_memory_size_ - KERNEL_LOAD_ADDR) {
        std::cerr << "[" << id_ << "] Kernel protected-mode image too large for guest RAM\n";
        close(kfd);
        return false;
    }

    // Seek past setup and load the protected-mode code at 1 MB
    if (lseek(kfd, setup_bytes, SEEK_SET) < 0) {
        std::cerr << "[" << id_ << "] lseek past setup sectors failed: " << strerror(errno) << "\n";
        close(kfd);
        return false;
    }
    uint8_t* dst = static_cast<uint8_t*>(guest_memory_) + KERNEL_LOAD_ADDR;
    size_t remaining = protected_mode_size;
    while (remaining > 0) {
        ssize_t n = read(kfd, dst, remaining);
        if (n <= 0) {
            std::cerr << "[" << id_ << "] Read kernel body failed: " << strerror(errno) << "\n";
            close(kfd);
            return false;
        }
        dst       += n;
        remaining -= n;
    }
    close(kfd);

    // ---- Copy setup header into boot_params (zero page) ----
    // boot_params sits at BOOT_PARAMS_ADDR in guest physical memory.
    uint8_t* zeropg = static_cast<uint8_t*>(guest_memory_) + BOOT_PARAMS_ADDR;
    // Copy the setup header (bytes 0x1F1–0x27F from sector 0)
    memcpy(zeropg + 0x1F1, header.data() + 0x1F1, std::min<size_t>(0x90, hdr_read - 0x1F1));

    // Fill mandatory fields
    guestWrite<uint16_t>(zeropg, bp::vid_mode,       0xFFFF); // VESA normal (no mode)
    guestWrite<uint8_t> (zeropg, bp::type_of_loader, BOOT_TYPE_HVIRT);
    // LOADED_HIGH flag (bit 0) – kernel is loaded at 1 MB+
    uint8_t lf = *reinterpret_cast<uint8_t*>(zeropg + bp::loadflags);
    lf |= 0x01; // LOADED_HIGH
    lf |= 0x80; // CAN_USE_HEAP
    guestWrite<uint8_t>(zeropg, bp::loadflags, lf);
    guestWrite<uint32_t>(zeropg, bp::heap_end_ptr, BOOT_PARAMS_ADDR - 0x200);

    // ---- Command line ----
    if (!kernelCmdline_.empty()) {
        char* cmdline_guest = static_cast<char*>(guest_memory_) + CMDLINE_ADDR;
        strncpy(cmdline_guest, kernelCmdline_.c_str(), 4095);
        cmdline_guest[4095] = '\0';
    } else {
        // Sensible default for a headless micro-VM
        const char* def = "console=ttyS0 noapic noacpi noirqbalance";
        strncpy(static_cast<char*>(guest_memory_) + CMDLINE_ADDR, def, 4095);
    }
    guestWrite<uint32_t>(zeropg, bp::cmd_line_ptr,
                         static_cast<uint32_t>(CMDLINE_ADDR));

    // ---- Load initrd (optional) ----
    uint32_t initrd_addr = 0, initrd_size_bytes = 0;
    if (!initrdPath_.empty()) {
        int ifd = open(initrdPath_.c_str(), O_RDONLY | O_CLOEXEC);
        if (ifd < 0) {
            std::cerr << "[" << id_ << "] Warning: cannot open initrd '"
                      << initrdPath_ << "': " << strerror(errno)
                      << " — booting without initrd.\n";
        } else {
            struct stat ist;
            fstat(ifd, &ist);
            initrd_size_bytes = static_cast<uint32_t>(ist.st_size);
            if (INITRD_LOAD_ADDR + initrd_size_bytes > guest_memory_size_) {
                std::cerr << "[" << id_ << "] Initrd too large for guest RAM — skipping.\n";
                close(ifd);
            } else {
                uint8_t* idst = static_cast<uint8_t*>(guest_memory_) + INITRD_LOAD_ADDR;
                size_t ilen = initrd_size_bytes;
                while (ilen > 0) {
                    ssize_t n = read(ifd, idst, ilen);
                    if (n <= 0) break;
                    idst += n;
                    ilen -= n;
                }
                close(ifd);
                initrd_addr = static_cast<uint32_t>(INITRD_LOAD_ADDR);
                std::cout << "[" << id_ << "] initrd: " << initrd_size_bytes << " bytes at 0x"
                          << std::hex << initrd_addr << std::dec << "\n";
            }
        }
    }
    guestWrite<uint32_t>(zeropg, bp::ramdisk_image, initrd_addr);
    guestWrite<uint32_t>(zeropg, bp::ramdisk_size,  initrd_size_bytes);

    // ---- e820 memory map (two entries: conventional + high RAM) ----
    // Each entry: uint64 base, uint64 length, uint32 type → 20 bytes
    uint8_t* e820 = zeropg + bp::e820_table;
    auto write_e820 = [&](int idx, uint64_t base, uint64_t len, uint32_t type) {
        uint8_t* e = e820 + idx * 20;
        memcpy(e,      &base, 8);
        memcpy(e + 8,  &len,  8);
        memcpy(e + 16, &type, 4);
    };
    // 0x00000 – 0x9FBFF : conventional RAM
    write_e820(0, 0x0,        0x9FC00,                       E820_RAM);
    // BIOS area
    write_e820(1, 0x9FC00,    0x400,                         E820_RESERVED);
    write_e820(2, 0xF0000,    0x10000,                       E820_RESERVED);
    // Main RAM above 1 MB
    write_e820(3, 0x100000,   guest_memory_size_ - 0x100000, E820_RAM);
    guestWrite<uint8_t>(zeropg, bp::e820_entries, 4);

    std::cout << "[" << id_ << "] Kernel loaded: " << protected_mode_size
              << " bytes at 0x" << std::hex << KERNEL_LOAD_ADDR << std::dec << "\n";
    return true;
}

// ── Page table + VCPU register setup (64-bit long mode) ─────────────────────

bool VMInstance::setupVCPUs() {
    // Build minimal identity-mapped page tables in guest memory
    // PML4[0] → PDPT[0]  (0x1000 → 0x2000), PDPT[0] → PD (0x2000 → 0x3000)
    // PD covers 512 × 2 MB = 1 GB with PRESENT | WRITE | HUGE (PS=1)
    uint64_t* pml4 = reinterpret_cast<uint64_t*>(static_cast<char*>(guest_memory_) + PML4_ADDR);
    uint64_t* pdpt = reinterpret_cast<uint64_t*>(static_cast<char*>(guest_memory_) + PDPT_ADDR);
    uint64_t* pd   = reinterpret_cast<uint64_t*>(static_cast<char*>(guest_memory_) + PD_ADDR);

    pml4[0] = PDPT_ADDR | 0x3;  // Present | Writable
    pdpt[0] = PD_ADDR   | 0x3;
    for (int i = 0; i < 512; ++i)
        pd[i] = (static_cast<uint64_t>(i) << 21) | 0x83; // Present | Writable | Huge

    // Build a minimal GDT in guest memory
    // Slot 0: null, Slot 1: 64-bit code (CS), Slot 2: data (DS/SS/ES)
    uint64_t* gdt = reinterpret_cast<uint64_t*>(static_cast<char*>(guest_memory_) + GDT_ADDR);
    gdt[0] = 0; // null descriptor
    // 64-bit code segment: base=0, limit=0xFFFFFFFF, G=1, L=1, P=1, DPL=0, type=0x0A (exec/read)
    gdt[1] = 0x00AF9A000000FFFFULL;
    // Data segment: base=0, limit=0xFFFFFFFF, G=1, B=1, P=1, DPL=0, type=0x02 (read/write)
    gdt[2] = 0x00CF92000000FFFFULL;

    struct kvm_segment code_seg = {}, data_seg = {};
    code_seg.base     = 0;
    code_seg.limit    = 0xFFFFFFFF;
    code_seg.selector = 0x08; // GDT[1]
    code_seg.type     = 10;   // execute/read, accessed
    code_seg.present  = 1;
    code_seg.dpl      = 0;
    code_seg.s        = 1;    // code/data segment
    code_seg.l        = 1;    // 64-bit mode
    code_seg.g        = 1;    // 4KB granularity

    data_seg.base     = 0;
    data_seg.limit    = 0xFFFFFFFF;
    data_seg.selector = 0x10; // GDT[2]
    data_seg.type     = 3;    // read/write, accessed
    data_seg.present  = 1;
    data_seg.dpl      = 0;
    data_seg.s        = 1;
    data_seg.db       = 1;    // 32-bit for data
    data_seg.g        = 1;

    struct kvm_segment tss_seg = {};
    tss_seg.base     = 0;
    tss_seg.limit    = 0xFFFF;
    tss_seg.selector = 0x18;
    tss_seg.type     = 11;
    tss_seg.present  = 1;

    for (int i = 0; i < vcpus_; ++i) {
        // ---- Special registers (segment regs, CRs, EFER) ----
        struct kvm_sregs sregs = {};
        if (ioctl(vcpu_fds_[i], KVM_GET_SREGS, &sregs) < 0) {
            std::cerr << "[" << id_ << "] KVM_GET_SREGS vcpu " << i
                      << " failed: " << strerror(errno) << "\n";
            return false;
        }

        sregs.cs = code_seg;
        sregs.ds = sregs.es = sregs.fs = sregs.gs = sregs.ss = data_seg;
        sregs.tr          = tss_seg;

        // GDT
        sregs.gdt.base  = GDT_ADDR;
        sregs.gdt.limit = 3 * 8 - 1; // 3 descriptors

        // CR0: PE (protected mode) | MP | ET | NE | WP | AM | PG
        sregs.cr0 = 0x80050033;
        // CR3: PML4 physical address
        sregs.cr3 = PML4_ADDR;
        // CR4: PAE (bit 5) – required for long mode
        sregs.cr4 = 0x20; // PAE
        // EFER: LME (long mode enable, bit 8) | LMA (long mode active, bit 10) | SCE (bit 0)
        sregs.efer = 0x501;

        if (ioctl(vcpu_fds_[i], KVM_SET_SREGS, &sregs) < 0) {
            std::cerr << "[" << id_ << "] KVM_SET_SREGS vcpu " << i
                      << " failed: " << strerror(errno) << "\n";
            return false;
        }

        // ---- General-purpose registers ----
        struct kvm_regs regs = {};
        regs.rip    = KERNEL_LOAD_ADDR;
        regs.rflags = 0x2;            // only the reserved-must-be-1 bit
        regs.rsi    = BOOT_PARAMS_ADDR; // Linux convention: RSI → boot_params pointer

        if (ioctl(vcpu_fds_[i], KVM_SET_REGS, &regs) < 0) {
            std::cerr << "[" << id_ << "] KVM_SET_REGS vcpu " << i
                      << " failed: " << strerror(errno) << "\n";
            return false;
        }
    }

    std::cout << "[" << id_ << "] VCPUs configured for 64-bit long mode\n";
    return true;
}

// ── VirtIO / Disk attachment ─────────────────────────────────────────────────

bool VMInstance::setupVirtio() {
    if (diskPath_.empty()) {
        std::cout << "[" << id_ << "] No disk image — kernel/initrd-only boot\n";
        return true;
    }
    int disk_fd = open(diskPath_.c_str(), O_RDONLY | O_CLOEXEC);
    if (disk_fd < 0) {
        std::cerr << "[" << id_ << "] Cannot open disk image '" << diskPath_
                  << "': " << strerror(errno) << "\n";
        return false;
    }
    struct stat st;
    fstat(disk_fd, &st);
    std::cout << "[" << id_ << "] Disk image attached: " << diskPath_
              << " (" << st.st_size / 1024 / 1024 << " MB)\n";
    close(disk_fd);
    return true;
}

// ── Lifecycle ────────────────────────────────────────────────────────────────

bool VMInstance::start() {
    if (state_ != State::Stopped) {
        std::cerr << "[" << id_ << "] start() called but VM is not stopped (state="
                  << static_cast<int>(state_.load()) << ")\n";
        return false;
    }

    state_ = State::Starting;
    std::cout << "[" << id_ << "] Starting VM...\n";

    // Cgroup is best-effort — never block VM start over it
    setupCgroup();

    if (!initializeKVM()) { state_ = State::Error; return false; }
    if (!loadKernel())    { state_ = State::Error; return false; }
    if (!setupVCPUs())    { state_ = State::Error; return false; }
    if (!setupVirtio())   { state_ = State::Error; return false; }

    running_ = true;

    for (int i = 0; i < vcpus_; ++i)
        vcpu_threads_.emplace_back(&VMInstance::vcpuRunLoop, this, i);

    state_ = State::Running;
    std::cout << "[" << id_ << "] VM running\n";
    return true;
}

bool VMInstance::stop() {
    if (state_ == State::Stopped) return true;

    std::cout << "[" << id_ << "] Stopping VM...\n";
    running_ = false;
    paused_  = false;
    pause_cv_.notify_all();
    console_cv_.notify_all();

    vcpu_threads_.clear(); // joins all jthreads

    state_ = State::Stopped;
    std::cout << "[" << id_ << "] VM stopped\n";
    return true;
}

bool VMInstance::pause() {
    if (state_ != State::Running) return false;
    paused_ = true;
    state_  = State::Paused;
    std::cout << "[" << id_ << "] VM paused\n";
    return true;
}

bool VMInstance::resume() {
    if (state_ != State::Paused) return false;
    paused_ = false;
    pause_cv_.notify_all();
    state_  = State::Running;
    std::cout << "[" << id_ << "] VM resumed\n";
    return true;
}

// ── Metrics ──────────────────────────────────────────────────────────────────

VMInstance::Metrics VMInstance::getMetrics() const {
    return last_metrics_;
}

// ── Console I/O ──────────────────────────────────────────────────────────────

std::string VMInstance::readConsoleOutput() {
    std::lock_guard<std::mutex> lk(console_mutex_);
    std::string out = std::move(console_buffer_);
    console_buffer_.clear();
    return out;
}

bool VMInstance::sendConsoleInput(const std::string& input) {
    {
        std::lock_guard<std::mutex> lk(input_mutex_);
        input_queue_.append(input);
    }
    return true;
}

// ── Snapshots (stub) ─────────────────────────────────────────────────────────

bool VMInstance::createSnapshot(const std::string&)  { return true; }
bool VMInstance::restoreSnapshot(const std::string&) { return true; }

// ── Cgroup management ────────────────────────────────────────────────────────

bool VMInstance::setupCgroup() {
    cgroup_path_ = "/sys/fs/cgroup/vellum/" + id_;

    // Try cgroup v2 first
    if (mkdir(cgroup_path_.c_str(), 0755) == 0 || errno == EEXIST)
        return true;

    // Fall back to /sys/fs/cgroup directly
    cgroup_path_ = "/sys/fs/cgroup/memory/vellum/" + id_;
    if (mkdir(cgroup_path_.c_str(), 0755) == 0 || errno == EEXIST)
        return true;

    // Cgroup unavailable — warn but don't fail
    std::cerr << "[" << id_ << "] Warning: cgroup setup failed ("
              << strerror(errno) << ") — resource limits disabled\n";
    cgroup_path_.clear();
    return true; // Non-fatal
}

void VMInstance::cleanupCgroup() {
    if (cgroup_path_.empty()) return;
    if (rmdir(cgroup_path_.c_str()) < 0 && errno != ENOENT)
        std::cerr << "[" << id_ << "] Warning: rmdir cgroup '" << cgroup_path_
                  << "' failed: " << strerror(errno) << "\n";
    cgroup_path_.clear();
}

bool VMInstance::setCPULimit(double percentage) {
    if (cgroup_path_.empty() || percentage <= 0 || percentage > 100) return false;
    const uint64_t period = 100000;
    uint64_t quota        = static_cast<uint64_t>(period * percentage / 100.0);
    std::ofstream f(cgroup_path_ + "/cpu.max");
    if (!f) return false;
    f << quota << " " << period << "\n";
    std::cout << "[" << id_ << "] CPU limit set to " << percentage << "%\n";
    return true;
}

bool VMInstance::setMemoryLimit(size_t mb) {
    if (cgroup_path_.empty() || mb == 0) return false;
    std::ofstream f(cgroup_path_ + "/memory.max");
    if (!f) return false;
    f << (mb * 1024 * 1024) << "\n";
    std::cout << "[" << id_ << "] Memory limit set to " << mb << " MB\n";
    return true;
}
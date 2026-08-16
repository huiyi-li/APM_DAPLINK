/*
 * MCU backend: offline programming via the on-board DAP (SWD) + a flash
 * algorithm loaded into the target RAM.
 *
 * The engine runs in its own ThreadX thread; the UI polls
 * app_flash_poll() from an lv_timer. FileX is used directly to stream
 * the firmware .bin from the W25Q128 volume.
 *
 * Algorithm execution model:
 *   - algo code is downloaded to the target RAM at ram_addr
 *   - a BKPT breakpoint is planted just after the algo image
 *   - to call a function we set R0..R3/SP/LR/PC, run the core and wait
 *     for halt at the breakpoint, then read R0 (0 == success)
 */

#include <stdio.h>
#include <string.h>

#include "app_flash.h"
#include "app_algo.h"
#include "app_fs.h"

#include "tx_api.h"
#include "fx_api.h"
#include "apm32f4xx_gpio.h"
#include "apm32f4xx_rcm.h"
#include "board.h"

#include "DAP_config.h"
#include "DAP.h"

#define FL_DBG(fmt, ...)  printf("[FL] " fmt "\r\n", ##__VA_ARGS__)

/* builtin algorithm + test firmware (filesystem access is bypassed for
 * this bring-up stage; see app_algo.h for the cfg key semantics) */
extern const uint8_t  algo_gd32f470_bin[];
extern const uint32_t algo_gd32f470_bin_len;
extern const uint8_t  test_fw_bin[];
extern const uint32_t test_fw_bin_len;

#define FL_STACK_SIZE       4096U
#define FL_THREAD_PRIO      0U
#define FL_TIMEOUT_MS       3000U
#define FL_MAX_FILE_SIZE    (2U * 1024U * 1024U)

/* target core debug registers (accessed through AP) */
#define DHCSR               0xE000EDF0U
#define DCRSR               0xE000EDF4U
#define DCRDR               0xE000EDF8U
#define DEMCR               0xE000EDFCU
#define DBGKEY              (0xA05FU << 16)
#define DHCSR_C_DEBUGEN     (1U << 0)
#define DHCSR_C_HALT        (1U << 1)
#define DHCSR_S_HALT        (1U << 17)
#define DHCSR_S_REGRDY      (1U << 16)
#define DEMCR_VC_CORERESET  (1U << 0)
#define DCRSR_REGWnR        (1U << 16)
#define XPSR_THUMB          0x01000000U

/* SWD request bits (CMSIS-DAP DAP_TRANSFER_*) */
#define SWD_AP              (1U << 0)
#define SWD_RNW             (1U << 1)
#define SWD_A2              (1U << 2)
#define SWD_A3              (1U << 3)
#define SWD_REG_ADR(a)      ((a) & 0x0CU)

/* AP registers (bank 0) */
#define AP_CSW              0x00U
#define AP_TAR              0x04U
#define AP_DRW              0x0CU
/* DP registers */
#define DP_ABORT            0x00U
#define DP_CTRL_STAT        0x04U
#define DP_SELECT           0x08U
#define DP_RDBUFF           0x0CU

/* DP_ABORT bits */
#define DAPABORT            0x00000001U
#define STKCMPCLR           0x00000002U
#define STKERRCLR           0x00000004U
#define WDERRCLR            0x00000008U
#define ORUNERRCLR          0x00000010U

/* DP_CTRL_STAT bits */
#define TRNNORMAL           0x00000000U
#define STICKYERR           0x00000020U
#define MASKLANE            0x00000F00U
#define CDBGPWRUPREQ        0x10000000U
#define CDBGPWRUPACK        0x20000000U
#define CSYSPWRUPREQ        0x40000000U
#define CSYSPWRUPACK        0x80000000U

/* AP_CSW bits (32-bit, auto-increment, debug master) */
#define CSW_SIZE32          0x00000002U
#define CSW_SADDRINC        0x00000010U
#define CSW_DBGSTAT         0x00000040U
#define CSW_MSTRDBG         0x20000000U
#define CSW_RESERVED        0x01000000U
#define CSW_HPROT           0x02000000U
#define CSW_VALUE           (CSW_RESERVED | CSW_MSTRDBG | CSW_HPROT | \
                             CSW_DBGSTAT | CSW_SADDRINC)

static volatile int      fl_state;     /* APP_FLASH_BUSY/OK/FAIL */
static volatile int      fl_active;    /* 1 while the engine thread runs.
                                        * (fl_state's zero-initialised value
                                        * equals APP_FLASH_BUSY, so it cannot
                                        * be used for re-entry checks.) */
static volatile uint32_t fl_done, fl_total;
static volatile int      fl_cancel_req;
static volatile uint32_t fl_run;       /* flash session counter */
static char              fl_reason[96];

static app_algo_t        fl_algo;
static uint32_t          fl_flash_base;
static char              fl_path[96];

static TX_THREAD         fl_thread;
static UCHAR             fl_thread_stack[FL_STACK_SIZE];

/* target RAM layout */
static uint32_t fl_break_addr;   /* BKPT planted here */
static uint32_t fl_stack_top;
static uint32_t fl_data_buf;     /* page staging buffer in target RAM */

bool app_flash_cancel_requested(void)
{
    return fl_cancel_req;
}

void app_flash_request_cancel(void)
{
    fl_cancel_req = 1;
}

const char *app_flash_last_reason(void)
{
    return fl_reason;
}

int app_flash_poll(uint32_t *done, uint32_t *total)
{
    if (done)  *done  = fl_done;
    if (total) *total = fl_total;
    return fl_state;
}

/* ------------------------- SWD primitives ------------------------- */
/* Raw SWD layer: drives SW_DP.c's SWD_Transfer/SWJ_Sequence directly
 * (the same primitives DAPLink swd_host.c uses), bypassing the
 * DAP_ProcessCommand command-framing layer and its response/post-read
 * machinery entirely. */

extern DAP_Data_t DAP_Data;
extern void Set_Clock_Delay(uint32_t clock);
extern void SWJ_Sequence(uint32_t count, const uint8_t *data);
extern uint8_t SWD_Transfer(uint32_t request, uint32_t *data);

/* PB13 is shared between SWCLK (DAP) and the LCD SPI3 SCK
 * (bsp_lcd_bus.c). Other threads may re-claim it at any time, so before
 * every SWD transaction we force PB13 back to GPIO output (SWCLK).
 * PC3 (SWDIO) is not shared; only PB13 needs this. */
static void swd_clk_pin_own(void)
{
    uint32_t moder = *(volatile uint32_t *)0x40020400U;

    if (((moder >> 26) & 3U) != 1U)
    {
        *(volatile uint32_t *)0x40020400U = (moder & ~(3U << 26)) | (1U << 26);
    }
}

static uint32_t swd_transfer_retry(uint32_t request, uint32_t *data)
{
    uint32_t ack;
    uint8_t  retry = 0;

    do
    {
        ack = SWD_Transfer(request, data);
    } while ((ack == DAP_TRANSFER_WAIT) && (retry++ < 8));
    return ack;
}

/* --- DP access (direct result, no data-phase pipelining) --- */

static bool swd_write_dp(uint32_t adr, uint32_t val)
{
    uint32_t req = (uint32_t)SWD_REG_ADR(adr);   /* DP write, A[3:2] */

    swd_clk_pin_own();
    return swd_transfer_retry(req, &val) == DAP_TRANSFER_OK;
}

static bool swd_read_dp(uint32_t adr, uint32_t *val)
{
    uint32_t req = SWD_RNW | (uint32_t)SWD_REG_ADR(adr);   /* DP read */

    swd_clk_pin_own();
    return swd_transfer_retry(req, val) == DAP_TRANSFER_OK;
}

/* --- AP access (DP_SELECT first; pipelined data phase handled here) --- */

static bool swd_write_ap(uint32_t adr, uint32_t val)
{
    uint32_t req;
    uint32_t rd;
    uint32_t apsel    = 0;                  /* AP0 */
    uint32_t bank_sel = adr & 0xF0U;

    swd_clk_pin_own();
    if (!swd_write_dp(DP_SELECT, apsel | bank_sel)) return false;
    req = SWD_AP | (uint32_t)SWD_REG_ADR(adr);
    if (swd_transfer_retry(req, &val) != DAP_TRANSFER_OK) return false;
    /* commit the write data phase and check for sticky errors */
    return swd_transfer_retry(SWD_RNW | (uint32_t)SWD_REG_ADR(DP_RDBUFF),
                              &rd) == DAP_TRANSFER_OK;
}

static bool swd_read_ap(uint32_t adr, uint32_t *val)
{
    uint32_t req;
    uint32_t dummy;
    uint32_t apsel    = 0;
    uint32_t bank_sel = adr & 0xF0U;

    swd_clk_pin_own();
    if (!swd_write_dp(DP_SELECT, apsel | bank_sel)) return false;
    req = SWD_AP | SWD_RNW | (uint32_t)SWD_REG_ADR(adr);
    /* first read returns stale data (MEM-AP pipeline), second is real */
    if (swd_transfer_retry(req, &dummy) != DAP_TRANSFER_OK) return false;
    return swd_transfer_retry(req, val) == DAP_TRANSFER_OK;
}

/* --- 32-bit auto-increment memory access (MEM-AP) --- */

static bool mem_write32(uint32_t addr, uint32_t val)
{
    if (!swd_write_ap(AP_CSW, CSW_VALUE | CSW_SIZE32)) return false;
    if (!swd_write_ap(AP_TAR, addr)) return false;
    return swd_write_ap(AP_DRW, val);
}

static bool mem_read32(uint32_t addr, uint32_t *val)
{
    uint32_t req, rd;

    if (!swd_write_ap(AP_CSW, CSW_VALUE | CSW_SIZE32)) return false;
    if (!swd_write_ap(AP_TAR, addr)) return false;
    /* post the DRW read, collect it via DP_RDBUFF */
    req = SWD_AP | SWD_RNW | (uint32_t)SWD_REG_ADR(AP_DRW);
    if (swd_transfer_retry(req, &rd) != DAP_TRANSFER_OK) return false;
    if (swd_transfer_retry(SWD_RNW | (uint32_t)SWD_REG_ADR(DP_RDBUFF),
                           &rd) != DAP_TRANSFER_OK) return false;
    *val = rd;
    return true;
}

static bool mem_write_buf(uint32_t addr, const uint8_t *buf, uint32_t size)
{
    uint32_t i = 0;

    if (size >= 4U)
    {
        /* bulk: set CSW/TAR once, DRW auto-increments */
        if (!swd_write_ap(AP_CSW, CSW_VALUE | CSW_SIZE32)) return false;
        if (!swd_write_ap(AP_TAR, addr)) return false;
        while (size - i >= 4U)
        {
            uint32_t v = (uint32_t)buf[i] | ((uint32_t)buf[i + 1] << 8) |
                         ((uint32_t)buf[i + 2] << 16) | ((uint32_t)buf[i + 3] << 24);
            if (!swd_write_ap(AP_DRW, v)) return false;
            i += 4U;
        }
    }
    while (i < size)
    {
        uint32_t v = buf[i];
        uint32_t old;
        if (!mem_read32(addr + (i & ~3U), &old)) return false;
        old &= ~(0xFFU << ((i & 3U) * 8U));
        old |= v << ((i & 3U) * 8U);
        if (!mem_write32(addr + (i & ~3U), old)) return false;
        i++;
    }
    return true;
}

/* Read a byte buffer from the target (MEM-AP, 32-bit auto-increment).
 * Each word: post an AP DRW read, collect it via DP_RDBUFF. */
static bool mem_read_buf(uint32_t addr, uint8_t *buf, uint32_t size)
{
    uint32_t v;
    uint32_t i;

    if (!swd_write_ap(AP_CSW, CSW_VALUE | CSW_SIZE32)) return false;
    if (!swd_write_ap(AP_TAR, addr)) return false;

    for (i = 0; size - i >= 4U; i += 4U)
    {
        if (swd_transfer_retry(SWD_AP | SWD_RNW | SWD_REG_ADR(AP_DRW),
                               &v) != DAP_TRANSFER_OK) return false;
        if (swd_transfer_retry(SWD_RNW | SWD_REG_ADR(DP_RDBUFF),
                               &v) != DAP_TRANSFER_OK) return false;
        buf[i]     = (uint8_t)(v & 0xFFU);
        buf[i + 1] = (uint8_t)((v >> 8) & 0xFFU);
        buf[i + 2] = (uint8_t)((v >> 16) & 0xFFU);
        buf[i + 3] = (uint8_t)((v >> 24) & 0xFFU);
    }
    while (i < size)
    {
        if (!mem_read32(addr + (i & ~3U), &v)) return false;
        buf[i] = (uint8_t)(v >> ((i & 3U) * 8U));
        i++;
    }
    return true;
}

/* ------------------------- core control ------------------------- */

static bool core_write_reg(uint32_t reg, uint32_t val)
{
    uint32_t dhcsr;
    int      i;

    if (!mem_write32(DCRDR, val)) return false;
    if (!mem_write32(DCRSR, reg | DCRSR_REGWnR)) return false;

    /* wait for S_REGRDY (write transfer done) */
    for (i = 0; i < 1000; i++)
    {
        if (!mem_read32(DHCSR, &dhcsr)) return false;
        if (dhcsr & DHCSR_S_REGRDY) return true;
    }
    return false;
}

static bool core_read_reg(uint32_t reg, uint32_t *val)
{
    uint32_t dhcsr;
    int      i;

    if (!mem_write32(DCRSR, reg)) return false;

    /* wait for S_REGRDY (read data available) */
    for (i = 0; i < 1000; i++)
    {
        if (!mem_read32(DHCSR, &dhcsr)) return false;
        if (dhcsr & DHCSR_S_REGRDY) break;
    }
    if (i == 1000) return false;

    return mem_read32(DCRDR, val);
}

static void core_halt(void)
{
    (void)mem_write32(DHCSR, DBGKEY | DHCSR_C_DEBUGEN | DHCSR_C_HALT);
}

static void core_run(void)
{
    (void)mem_write32(DHCSR, DBGKEY | DHCSR_C_DEBUGEN);
}

static bool swd_clear_errors(void);   /* defined in the SWD section below */

static bool core_wait_halt(uint32_t timeout_ms)
{
    const uint32_t start = tx_time_get();
    uint32_t dhcsr;
    uint32_t tmp;
    int      i;

    while (tx_time_get() - start < timeout_ms)
    {
        if (!mem_read32(DHCSR, &dhcsr))
        {
            /* nRST release may clear the DP power-up state again:
             * re-request it, then retry the read. */
            (void)swd_clear_errors();
            (void)swd_write_dp(DP_CTRL_STAT, CSYSPWRUPREQ | CDBGPWRUPREQ);
            for (i = 0; i < 20; i++)
            {
                if (!swd_read_dp(DP_CTRL_STAT, &tmp)) break;
                if ((tmp & (CDBGPWRUPACK | CSYSPWRUPACK)) ==
                    (CDBGPWRUPACK | CSYSPWRUPACK))
                {
                    break;
                }
            }
            tx_thread_sleep(1);
            continue;
        }             /* probe: last DHCSR seen */
        if (dhcsr & DHCSR_S_HALT) return true;
        tx_thread_sleep(1);
    }
    return false;
}

/* --- SWD connection init (mirrors DAPLink swd_host.c swd_init_debug) --- */

static bool swj_sequence(uint8_t count, const uint8_t *data)
{
    uint8_t rq[2 + 16], rp[4];
    int     i, n = (count + 7) / 8;

    swd_clk_pin_own();
    rq[0] = ID_DAP_SWJ_Sequence;
    rq[1] = count;
    for (i = 0; i < n && i < 16; i++)
    {
        rq[2 + i] = data[i];
    }
    (void)DAP_ProcessCommand(rq, rp);
    return rp[0] == ID_DAP_SWJ_Sequence;
}

static bool swd_clear_errors(void)
{
    return swd_write_dp(DP_ABORT, STKCMPCLR | STKERRCLR | WDERRCLR | ORUNERRCLR);
}

static bool swd_init_debug(void)
{
    uint8_t  seq[16];
    uint32_t dpidr, tmp;
    int      i;

    /* Configure the SWD engine through the DAP command layer (the same
     * commands pyocd sends); this also runs PORT_SWD_SETUP inside the
     * Connect handler. DAP_Setup() is NOT used: its PORT_OFF puts the
     * SWD pins into High-Z and disturbs the line. */
    {
        uint8_t rq[6], rp[4];

        rq[0] = ID_DAP_Connect;
        rq[1] = DAP_PORT_SWD;
        (void)DAP_ProcessCommand(rq, rp);

        rq[0] = ID_DAP_TransferConfigure;
        rq[1] = 2;                  /* idle cycles */
        rq[2] = 150 & 0xFF;         /* wait retry */
        rq[3] = 150 >> 8;
        rq[4] = 0;                  /* match retry */
        rq[5] = 0;
        (void)DAP_ProcessCommand(rq, rp);

        rq[0] = ID_DAP_SWD_Configure;
        rq[1] = 0x00;               /* turnaround=1, data_phase=0 */
        (void)DAP_ProcessCommand(rq, rp);

        rq[0] = ID_DAP_SWJ_Clock;
        rq[1] = 0x40;   /* 1000000 = 0x000F4240, little-endian */
        rq[2] = 0x42;
        rq[3] = 0x0F;
        rq[4] = 0x00;
        (void)DAP_ProcessCommand(rq, rp);
    }

    /* 1. SWJ line reset: >= 50 clock cycles of 1 */
    memset(seq, 0xFF, sizeof(seq));
    if (!swj_sequence(51, seq)) { return false; }

    /* 2. idle cycles, then read DPIDR (one line reset only, like
     *    pyocd's _swd_reset; a second consecutive reset disturbs the DP) */
    seq[0] = 0x00;
    (void)swj_sequence(8, seq);
    for (i = 0; i < 3; i++)
    {
        uint8_t rq[8], rp[16];
        rq[0] = ID_DAP_Transfer;
        rq[1] = 0;
        rq[2] = 1;
        rq[3] = (uint8_t)(SWD_RNW | SWD_REG_ADR(0x00));
        rq[4] = 0; rq[5] = 0; rq[6] = 0; rq[7] = 0;
        (void)DAP_ProcessCommand(rq, rp);
        if (rp[2] == DAP_TRANSFER_OK)
        {
            dpidr = (uint32_t)rp[3] | ((uint32_t)rp[4] << 8) |
                    ((uint32_t)rp[5] << 16) | ((uint32_t)rp[6] << 24);
            break;
        }
    }
    if (i == 3)
    {
        /* probe: compare with DAP_ProcessCommand path */
        uint8_t rq[8], rp[16];
        uint32_t via_dap;

        rq[0] = ID_DAP_Transfer;
        rq[1] = 0;
        rq[2] = 1;
        rq[3] = (uint8_t)(SWD_RNW | SWD_REG_ADR(0x00));
        rq[4] = 0; rq[5] = 0; rq[6] = 0; rq[7] = 0;
        (void)DAP_ProcessCommand(rq, rp);
        via_dap = (uint32_t)rp[3] | ((uint32_t)rp[4] << 8) |
                  ((uint32_t)rp[5] << 16) | ((uint32_t)rp[6] << 24);   /* GPIOC MODE */
        return false;
    }

    /* 3. clear sticky errors */
    if (!swd_clear_errors()) { return false; }

    /* 4. select AP0 / bank 0 */
    if (!swd_write_dp(DP_SELECT, 0)) { return false; }

    /* 5. request power-up and wait for acknowledge */
    if (!swd_write_dp(DP_CTRL_STAT, CSYSPWRUPREQ | CDBGPWRUPREQ)) { return false; }
    for (i = 0; i < 100; i++)
    {
        if (!swd_read_dp(DP_CTRL_STAT, &tmp)) return false;
        if ((tmp & (CDBGPWRUPACK | CSYSPWRUPACK)) ==
            (CDBGPWRUPACK | CSYSPWRUPACK))
        {
            break;
        }
    }
    if (i == 100) { return false; }

    /* 6. final CTRL/STAT configuration */
    if (!swd_write_dp(DP_CTRL_STAT, CSYSPWRUPREQ | CDBGPWRUPREQ |
                      TRNNORMAL | MASKLANE)) { return false; }

    FL_DBG("swd connect ok, dpidr=0x%08lx", (unsigned long)dpidr);
    return true;
}

static bool target_reset_halt(void)
{
    uint32_t demcr;
    int      attempt;

    /* Reset then explicitly halt (no dependency on vector catch, which
     * nRST seems to clear on release with this target):
     *  1. pulse nRST to get a clean core state.
     *  2. reconnect SWD (nRST re-initialised the SW-DP).
     *  3. assert C_DEBUGEN|C_HALT (works even from LOCKUP).
     *  4. confirm S_HALT. */

    for (attempt = 0; attempt < 3; attempt++)
    {
        /* 1. pulse target reset */
        GPIO_ResetBit(BOARD_TARGET_RESET_PORT, BOARD_TARGET_RESET_PIN);
        tx_thread_sleep(10);
        GPIO_SetBit(BOARD_TARGET_RESET_PORT, BOARD_TARGET_RESET_PIN);
        tx_thread_sleep(10);

        /* 2. reconnect SWD + power up */
        if (!swd_init_debug())
        {
            return false;
        }

        /* 3. enable debug and halt the core; verify C_DEBUGEN landed */
        if (!mem_write32(DHCSR, DBGKEY | DHCSR_C_DEBUGEN | DHCSR_C_HALT))
        {
            continue;
        }
        if (!mem_read32(DHCSR, &demcr) || (demcr & DHCSR_C_DEBUGEN) == 0U)
        {
            continue;
        }

        /* 4. confirm halt */
        if (core_wait_halt(FL_TIMEOUT_MS))
        {
            return true;
        }
        (void)mem_read32(DHCSR, &demcr);
        return false;
    }

    return false;
}

/* call algo function: fn offset inside the loaded algo image */
static bool algo_call(uint32_t fn, uint32_t a0, uint32_t a1,
                      uint32_t a2, uint32_t a3, uint32_t *ret)
{
    if (!core_write_reg(0, a0)) return false;
    if (!core_write_reg(1, a1)) return false;
    if (!core_write_reg(2, a2)) return false;
    if (!core_write_reg(3, a3)) return false;
    if (!core_write_reg(9, fl_algo.ram_addr)) return false;
    if (!core_write_reg(13, fl_stack_top)) return false;
    /* LR must be the Thumb (odd) address of the planted BKPT: the algo
     * returns with `bx lr`, and bit0==0 would branch in ARM state. */
    if (!core_write_reg(14, fl_break_addr | 1U)) return false;
    if (!core_write_reg(15, fl_algo.ram_addr + fn)) return false;
    if (!core_write_reg(16, XPSR_THUMB)) return false;  /* T bit = 1 */

    core_halt();
    core_run();
    if (!core_wait_halt(FL_TIMEOUT_MS)) return false;

    if (ret)
    {
        (void)core_read_reg(0, ret);
    }
    return true;
}

/* ------------------------- flash engine ------------------------- */

static void fl_fail(const char *why)
{
    snprintf(fl_reason, sizeof(fl_reason), "%s", why);
    fl_state = APP_FLASH_FAIL;
}

/* Resume the USB DAP handler thread that fl_engine suspends. Must be
 * called on every exit path, or the CMSIS-DAP endpoint goes deaf. */
static void fl_resume_dap(void)
{
    extern TX_THREAD ThreadInit;
    (void)tx_thread_resume(&ThreadInit);
}

static void fl_engine(ULONG arg)
{
    uint32_t addr;
    uint32_t ret;
    uint32_t fw_size = test_fw_bin_len;

    (void)arg;

    if (fw_size == 0U)
    {
        fl_fail("empty firmware");
        return;
    }

    /* 1. connect SWD (line reset, JTAG2SWD, DP power-up) and halt target.
     *    The USB DAP handler (InitThread) must not run SWD concurrently:
     *    suspend it for the whole flash session and resume afterwards. */
    {
        extern TX_THREAD ThreadInit;
        (void)tx_thread_suspend(&ThreadInit);
    }

    /* NOTE: DAP_Setup() re-initialises the GPIOs (PORT_OFF puts the SWD
     * pins into High-Z) and resets DAP_Data. The USB DAP path never calls
     * it; skip it here too (the SWD pins are configured by the DAP
     * library's Connect command below). */
    /* DAP_Setup(); */
    if (!swd_init_debug())
    {
        fl_fail("swd connect failed");
        FL_DBG("FAIL: swd connect");
        fl_resume_dap();
        return;
    }
    core_halt();
    if (!target_reset_halt())
    {
        fl_fail("target connect failed");
        FL_DBG("FAIL: target connect/reset-halt");
        fl_resume_dap();
        return;
    }
    FL_DBG("target halted at reset vector");

    /* 2. download the builtin algorithm into target RAM */
    if (!mem_write_buf(fl_algo.ram_addr, algo_gd32f470_bin,
                       fl_algo.algo_size))
    {
        fl_fail("algo download failed");
        FL_DBG("FAIL: algo download");
        fl_resume_dap();
        return;
    }
    FL_DBG("algo %u bytes -> 0x%08lx", (unsigned)fl_algo.algo_size,
           (unsigned long)fl_algo.ram_addr);

    /* plant BKPT 0xAB after the algo image */
    if (!mem_write_buf(fl_break_addr, (const uint8_t *)"\xAB\xBE", 2))
    {
        fl_fail("breakpoint failed");
        FL_DBG("FAIL: breakpoint");
        fl_resume_dap();
        return;
    }

    /* 3. Init(flash_base, clk, fnc=ERASE) - Keil fnc: 1=Erase 2=Program */
    if (!algo_call(fl_algo.fn_init, fl_flash_base, 0, 1, 0, &ret) || ret != 0)
    {
        fl_fail("algo init failed");
        FL_DBG("FAIL: init(fnc=1) ret=0x%08lx", (unsigned long)ret);
        fl_resume_dap();
        return;
    }
    FL_DBG("algo init(erase) ok");

    /* 4. erase the sectors the image covers */
    {
        uint32_t covered = 0;
        uint32_t need    = fw_size;

        for (uint32_t si = 0;
             fl_algo.sectors[si].size != 0U && covered < need; si++)
        {
            uint32_t sbase = fl_algo.sectors[si].base;
            uint32_t ssize = fl_algo.sectors[si].size;

            if (covered >= need) break;
            if (sbase + ssize > need || sbase < covered)
            {
                /* skip sectors fully before the image */
                if (sbase + ssize <= covered) continue;
            }

            if (!algo_call(fl_algo.fn_erase_sector, fl_flash_base + sbase,
                           0, 0, 0, &ret) || ret != 0)
            {
                fl_fail("sector erase failed");
                FL_DBG("FAIL: erase_sector(0x%08lx) ret=0x%08lx",
                       (unsigned long)(fl_flash_base + sbase),
                       (unsigned long)ret);
                fl_resume_dap();
        fl_resume_dap();
                return;
            }
            covered = sbase + ssize;
            fl_done = covered / 2U;    /* erase phase: 0..50% */
            FL_DBG("erased 0x%08lx (0x%lx bytes)", (unsigned long)covered,
                   (unsigned long)ssize);
            if (fl_cancel_req)
            {
                fl_fail("cancelled");
                fl_resume_dap();
        return;
            }
        }
    }

    /* 5. program page by page from the builtin firmware */
    if (!algo_call(fl_algo.fn_init, fl_flash_base, 0, 2, 0, &ret) || ret != 0)
    {
        fl_fail("algo init failed");
        FL_DBG("FAIL: init(fnc=2) ret=0x%08lx", (unsigned long)ret);
        fl_resume_dap();
        return;
    }
    FL_DBG("algo init(program) ok");
    addr = fl_flash_base;
    while (addr < fl_flash_base + fw_size)
    {
        uint32_t chunk = fl_algo.page_size;

        if (fl_cancel_req)
        {
            fl_fail("cancelled");
            break;
        }
        if (fw_size - (addr - fl_flash_base) < chunk)
        {
            chunk = fw_size - (addr - fl_flash_base);
        }

        if (!mem_write_buf(fl_data_buf, &test_fw_bin[addr - fl_flash_base],
                           chunk))
        {
            fl_fail("staging write failed");
            break;
        }
        if (!algo_call(fl_algo.fn_program_page, addr, chunk, fl_data_buf, 0,
                       &ret) || ret != 0)
        {
            fl_fail("program failed");
            FL_DBG("FAIL: program_page(0x%08lx,0x%lx) ret=0x%08lx",
                   (unsigned long)addr, (unsigned long)chunk,
                   (unsigned long)ret);
            break;
        }
        addr += chunk;
        fl_done = (fw_size + (addr - fl_flash_base)) / 2U;  /* program phase: 50..100% */
        if ((fl_done % (4U * 1024U)) == 0U)
        {
            FL_DBG("programmed %lu bytes", (unsigned long)fl_done);
        }
    }

    /* 6. UnInit(last fnc = PROGRAM) */
    (void)algo_call(fl_algo.fn_uninit, 2, 0, 0, 0, NULL);

    /* 7. verify: read the whole image back and compare with the source */
    if (addr >= fl_flash_base + fw_size)
    {
        static uint8_t s_verify_buf[256];
        uint32_t vaddr = fl_flash_base;
        bool     vok   = true;

        while (vaddr < fl_flash_base + fw_size)
        {
            uint32_t chunk = fw_size - (vaddr - fl_flash_base);
            if (chunk > sizeof(s_verify_buf)) chunk = sizeof(s_verify_buf);

            if (!mem_read_buf(vaddr, s_verify_buf, chunk))
            {
                fl_fail("verify read failed");
                vok = false;
                break;
            }
            if (memcmp(s_verify_buf, &test_fw_bin[vaddr - fl_flash_base],
                       chunk) != 0)
            {
                fl_fail("verify failed");
                vok = false;
                break;
            }
            vaddr += chunk;
        }
        if (vok)
        {
            FL_DBG("verify ok (%lu bytes)", (unsigned long)fw_size);
        }
    }

    if (fl_active)
    {
        fl_state = (addr >= fl_flash_base + fw_size) ? APP_FLASH_OK
                                                     : APP_FLASH_FAIL;
    }
    FL_DBG("flash %s", (fl_state == APP_FLASH_OK) ? "OK" : "FAIL");
    fl_active = 0;

    /* resume the USB DAP handler thread */
    {
        extern TX_THREAD ThreadInit;
        (void)tx_thread_resume(&ThreadInit);
    }
}

int app_flash_start(const char *path)
{
    TX_THREAD *prev = tx_thread_identify();

    (void)path;

    if (fl_active)
    {
        return -1;
    }

    /* builtin GD32F470 algorithm parameters (see flm2bin.py output) */
    memset(&fl_algo, 0, sizeof(fl_algo));
    snprintf(fl_algo.name, sizeof(fl_algo.name), "GD32F470");
    fl_algo.flash_size_kb = 1024;
    fl_algo.flash_base    = 0x08000000U;
    fl_algo.ram_addr      = 0x20000000U;
    fl_algo.algo_size     = algo_gd32f470_bin_len;
    fl_algo.fn_init       = 0x78U;
    fl_algo.fn_uninit     = 0x10CU;
    fl_algo.fn_erase_chip = 0x12CU;
    fl_algo.fn_erase_sector = 0x18CU;
    fl_algo.fn_program_page = 0x23CU;
    fl_algo.page_size     = 0x400U;
    fl_algo.sector_size   = 0x4000U;
    /* GD32F470 1MB: 4x16KB + 1x64KB + 7x128KB */
    {
        static const struct { uint32_t size; uint32_t base; } sec[] = {
            { 0x4000U, 0x0U }, { 0x4000U, 0x4000U }, { 0x4000U, 0x8000U },
            { 0x4000U, 0xC000U }, { 0x10000U, 0x10000U },
            { 0x20000U, 0x20000U }, { 0x20000U, 0x40000U },
            { 0x20000U, 0x60000U }, { 0x20000U, 0x80000U },
            { 0x20000U, 0xA0000U }, { 0x20000U, 0xC0000U },
            { 0x20000U, 0xE0000U },
        };
        for (size_t i = 0; i < sizeof(sec) / sizeof(sec[0]); i++)
        {
            fl_algo.sectors[i].size = sec[i].size;
            fl_algo.sectors[i].base = sec[i].base;
        }
        fl_algo.sectors[12].size = 0;
    }

    fl_flash_base = fl_algo.flash_base;
    fl_break_addr = fl_algo.ram_addr + ((fl_algo.algo_size + 3U) & ~3U);
    fl_stack_top  = fl_break_addr + 0x100U;
    fl_data_buf   = fl_stack_top + 0x100U;

    fl_total   = test_fw_bin_len;
    fl_done    = 0;
    fl_cancel_req = 0;
    fl_state   = APP_FLASH_BUSY;
    fl_active  = 1;
    fl_run++;

    /* A previous session's thread may still hold the fl_thread control
     * block (ThreadX rejects re-creating a TCB that has an ID). Delete the
     * terminated thread first so retries work. */
    if (fl_run > 0U)
    {
        (void)tx_thread_delete(&fl_thread);
    }

    {
        UINT create_status = tx_thread_create(
            &fl_thread, "flash algo", fl_engine, 0,
            fl_thread_stack, sizeof(fl_thread_stack),
            FL_THREAD_PRIO, FL_THREAD_PRIO, TX_NO_TIME_SLICE, TX_AUTO_START);
        if (create_status != TX_SUCCESS)
        {
            fl_active  = 0;
            fl_state = APP_FLASH_FAIL;
            snprintf(fl_reason, sizeof(fl_reason), "thread create failed (%u)",
                     (unsigned)create_status);
            return -1;
        }
    }
    (void)prev;
    return 0;
}

#include "config.h"
#include "loop.h"
#include "hardware.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <avr/pgmspace.h>
#include <afw/misc.h>

#include "cal.h"
#include "psu.h"

void send_ack(void)
{
    send_msg(LOOP_ADDR_RESPONSE, CMD_ACK, NULL, 0);
}

bool check_datalen(uint8_t len)
{
    if (g_loop_msg.datalen != len) {
        send_msg(LOOP_ADDR_RESPONSE, CMD_NACK_DATA, NULL, 0);
        return true;
    } else {
        return false;
    }
}

static void cmd_nop()
{
    send_ack();
}

static void cmd_idn()
{
    send_msg_F(LOOP_ADDR_RESPONSE, CMD_ACK, FSTR(IDN_STR), sizeof(IDN_STR) - 1);
}

static void cmd_serial()
{
    crc_init();
    crc_process_byte(READ_PRODSIG(LOTNUM5));
    crc_process_byte(READ_PRODSIG(LOTNUM4));
    crc_process_byte(READ_PRODSIG(LOTNUM3));
    crc_process_byte(READ_PRODSIG(LOTNUM2));
    crc_process_byte(READ_PRODSIG(LOTNUM1));
    crc_process_byte(READ_PRODSIG(LOTNUM0));
    crc_process_byte(READ_PRODSIG(WAFNUM));
    crc_process_byte(READ_PRODSIG(COORDX1));
    crc_process_byte(READ_PRODSIG(COORDX0));
    crc_process_byte(READ_PRODSIG(COORDY1));
    crc_process_byte(READ_PRODSIG(COORDY0));
    uint16_t crc = crc_get_checksum();

    char buffer[6];
    sprintf(buffer, "%"PRIu16, crc);
    send_msg(LOOP_ADDR_RESPONSE, CMD_ACK, buffer, strlen(buffer));
}

static void cmd_count()
{
    if (check_datalen(1)) return;
    uint8_t count = g_loop_msg.data[0] + 1;
    send_msg(0, CMD_COUNT, &count, 1);
}

static void cmd_output_en()
{
    if (check_datalen(1)) return;
    psu_enable(g_loop_msg.data[0]);
    send_ack();
}

static void cmd_set_voltage(void)
{
    if (check_datalen(4)) return;
    uint32_t millivolts = U8_to_U32(g_loop_msg.data[0], g_loop_msg.data[1],
            g_loop_msg.data[2], g_loop_msg.data[3]);
    psu_vset((uint16_t) millivolts);
    send_ack();
}

static void cmd_set_current(void)
{
    if (check_datalen(4)) return;
    uint32_t microamps = U8_to_U32(g_loop_msg.data[0], g_loop_msg.data[1],
            g_loop_msg.data[2], g_loop_msg.data[3]);
    psu_iset((uint16_t) (microamps / 1000));
    send_ack();
}

static void cmd_q_output(void)
{
    if (check_datalen(0)) return;
    uint8_t status = 0;
    if (psu_enabled()) {
        enum psu_reg_mode mode = psu_get_reg_mode();
        switch (mode) {
        case PSU_REG_CV:
            status = 1;
            break;
        case PSU_REG_CC:
            status = 2;
            break;
        case PSU_OSCILLATING:
        default:
            status = 0xff;
        }
    }
    send_msg(LOOP_ADDR_RESPONSE, CMD_ACK, &status, 1);
}

static void cmd_q_voltage(void)
{
    if (check_datalen(0)) return;
    uint16_t mv = psu_vget();
    int32_t mv_i32 = (uint32_t) mv;
    send_msg(LOOP_ADDR_RESPONSE, CMD_ACK, &mv_i32, sizeof(mv_i32));
}

static void cmd_q_current(void)
{
    if (check_datalen(0)) return;
    uint16_t ma = psu_iget();
    int32_t ua_i32 = (uint32_t) ma * 1000;
    send_msg(LOOP_ADDR_RESPONSE, CMD_ACK, &ua_i32, sizeof(ua_i32));
}

static void cmd_q_prereg(void)
{
    if (check_datalen(0)) return;
    uint16_t mv = psu_prereg_vget();
    uint8_t buffer[2] = {U16_BYTE(mv, 0), U16_BYTE(mv, 1)};
    send_msg(LOOP_ADDR_RESPONSE, CMD_ACK, buffer, 2);
}

static void cmd_q_setvolt(void)
{
    if (check_datalen(0)) return;
    uint16_t mv = psu_get_vsetpt();
    int32_t mv_i32 = (uint32_t) mv;
    send_msg(LOOP_ADDR_RESPONSE, CMD_ACK, &mv_i32, sizeof(mv_i32));
}

static void cmd_q_setcurr(void)
{
    if (check_datalen(0)) return;
    uint16_t ma = psu_get_isetpt();
    int32_t ua_i32 = (uint32_t) ma * 1000;
    send_msg(LOOP_ADDR_RESPONSE, CMD_ACK, &ua_i32, sizeof(ua_i32));
}

void (* const __flash CMD_HANDLERS[256])() = {
    [CMD_NOP] = &cmd_nop,
    [CMD_IDN] = &cmd_idn,
    [CMD_SERIAL] = &cmd_serial,
    [CMD_COUNT] = &cmd_count,
    [CMD_ACK] = &cmd_nop,

    [CMD_OUTPUT_EN]  = &cmd_output_en,
    [CMD_SET_VOLTAGE] = &cmd_set_voltage,
    [CMD_SET_CURRENT] = &cmd_set_current,
    [CMD_QOUTPUT]    = &cmd_q_output,
    [CMD_QVOLTAGE]   = &cmd_q_voltage,
    [CMD_QCURRENT]   = &cmd_q_current,
    [CMD_QSET_VOLT]  = &cmd_q_setvolt,
    [CMD_QSET_CURR]  = &cmd_q_setcurr,
    [CMD_QPREREG]    = &cmd_q_prereg,

    [CMD_CAL_COUNT]  = &cmd_cal_count,
    [CMD_CAL_SELECT] = &cmd_cal_select,
    [CMD_CAL_NAME]   = &cmd_cal_name,
    [CMD_CAL_STATUS] = &cmd_cal_status,
    [CMD_CAL_USER]   = &cmd_cal_user,
    [CMD_CAL_RUN]    = &cmd_cal_run,
    [CMD_CAL_SAVE]   = &cmd_cal_save,
    [CMD_CAL_ABORT]  = &cmd_cal_abort,
    [CMD_CAL_READ]   = &cmd_cal_read,
    [CMD_CAL_WRITE]  = &cmd_cal_write,
};

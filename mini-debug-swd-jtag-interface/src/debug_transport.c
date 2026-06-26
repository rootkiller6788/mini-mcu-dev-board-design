/**
 * debug_transport.c - Physical Transport Layer Implementation
 *
 * L5: GPIO bit-bang SWD and JTAG transport, clock generation,
 * timing calibration, reset sequences, level shifting.
 * L6: SWD/JTAG connection establishment, transaction execution.
 *
 * Reference: ARM CoreSight DAP-Lite TRM (ARM DDI 0316)
 * Courses: Berkeley EE16B, Michigan EECS 411
 */

#include "debug_transport.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

int transport_init(debug_transport_t *transport,
                    transport_type_t type,
                    const transport_ops_t *ops,
                    void *gpio_ctx) {
    if (!transport || !ops) return -1;
    memset(transport, 0, sizeof(*transport));
    transport->type = type;
    transport->ops = ops;
    transport->gpio_ctx = gpio_ctx;
    transport->clock_freq_hz = type == TRANSPORT_SWD
        ? SWD_DEFAULT_FREQ_HZ : JTAG_DEFAULT_TCK_FREQ;
    transport->clock_period_ns = 1.0e9 / transport->clock_freq_hz;
    transport->half_period_ns = transport->clock_period_ns / 2.0;
    swd_protocol_reset_state(&transport->swd_timing);
    jtag_timing_init(&transport->jtag_timing);
    transport->tap_state = TAP_STATE_TEST_LOGIC_RESET;
    transport->initialized = true;
    return 0;
}

int transport_deinit(debug_transport_t *transport) {
    if (!transport) return -1;
    transport->initialized = false;
    transport->connected = false;
    return 0;
}

int transport_set_clock_freq(debug_transport_t *transport, double freq_hz) {
    if (!transport) return -1;
    if (freq_hz < 100e3 || freq_hz > 50e6) return -2;
    transport->clock_freq_hz = freq_hz;
    transport->clock_period_ns = 1.0e9 / freq_hz;
    transport->half_period_ns = transport->clock_period_ns / 2.0;
    return 0;
}

int transport_clock_pulse(debug_transport_t *transport,
                           bool data_out, bool *data_in) {
    if (!transport || !transport->ops) return -1;
    if (transport->ops->set_data_out) {
        transport->ops->set_data_out(transport->gpio_ctx, data_out);
    }
    if (transport->ops->clock_pulse) {
        transport->ops->clock_pulse(transport->gpio_ctx);
    } else {
        if (transport->ops->set_clock)
            transport->ops->set_clock(transport->gpio_ctx, true);
        if (transport->ops->delay_ns)
            transport->ops->delay_ns(transport->gpio_ctx,
                (uint32_t)transport->half_period_ns);
        if (transport->ops->get_data_in && data_in)
            transport->ops->get_data_in(transport->gpio_ctx, data_in);
        if (transport->ops->set_clock)
            transport->ops->set_clock(transport->gpio_ctx, false);
        if (transport->ops->delay_ns)
            transport->ops->delay_ns(transport->gpio_ctx,
                (uint32_t)transport->half_period_ns);
    }
    transport->clock_cycles++;
    return 0;
}

int swd_transport_line_reset(debug_transport_t *transport) {
    if (!transport || !transport->ops) return -1;
    transport->line_state = SWD_LINE_RESET;
    if (transport->ops->set_data_dir)
        transport->ops->set_data_dir(transport->gpio_ctx, true);
    if (transport->ops->set_data_out)
        transport->ops->set_data_out(transport->gpio_ctx, true);
    uint32_t i;
    bool unused;
    for (i = 0; i < SWD_LINE_RESET_CLOCKS_SAFE; i++) {
        transport_clock_pulse(transport, true, &unused);
    }
    transport->line_state = SWD_LINE_IDLE;
    return 0;
}

int swd_transport_connect(debug_transport_t *transport) {
    if (!transport) return SWD_ERR_DISCONNECTED;
    int rc = swd_transport_line_reset(transport);
    if (rc < 0) return rc;
    uint32_t seq = SWD_CONNECT_SEQ_ARM;
    int i;
    bool unused;
    for (i = 0; i < 16; i++) {
        bool bit = (seq >> i) & 0x01;
        transport_clock_pulse(transport, bit, &unused);
    }
    for (i = 0; i < 4; i++) {
        transport_clock_pulse(transport, true, &unused);
    }
    transport->connected = true;
    return 0;
}

int swd_transport_transact(debug_transport_t *transport,
                            swd_transaction_t *txn) {
    if (!transport || !txn || !transport->ops) return SWD_ERR_DISCONNECTED;
    if (!transport->connected) return SWD_ERR_DISCONNECTED;
    transport->transaction_count++;
    uint8_t request_byte = swd_build_request_byte(
        (uint8_t)txn->port, (uint8_t)txn->direction, txn->addr);
    int i;
    bool data_bit;
    bool unused_in;
    if (transport->ops->set_data_dir)
        transport->ops->set_data_dir(transport->gpio_ctx, true);
    for (i = 0; i < 8; i++) {
        bool bit = (request_byte >> i) & 0x01;
        transport_clock_pulse(transport, bit, &unused_in);
    }
    if (transport->ops->set_data_dir)
        transport->ops->set_data_dir(transport->gpio_ctx, false);
    bool ack_bits[3] = {false, false, false};
    for (i = 0; i < 3; i++) {
        transport_clock_pulse(transport, false, &ack_bits[i]);
    }
    uint8_t ack_val = (ack_bits[0] ? 1 : 0) |
                       (ack_bits[1] ? 2 : 0) |
                       (ack_bits[2] ? 4 : 0);
    txn->ack = swd_parse_ack(ack_val);
    if (txn->ack == SWD_ACK_OK) {
        if (txn->direction == SWD_DIR_READ) {
            uint32_t rdata = 0;
            for (i = 0; i < 32; i++) {
                transport_clock_pulse(transport, false, &data_bit);
                if (data_bit) rdata |= (1u << i);
            }
            bool parity_bit;
            transport_clock_pulse(transport, false, &parity_bit);
            txn->data = rdata;
            txn->parity_error = !swd_verify_parity_32(rdata, parity_bit ? 1 : 0);
            if (transport->ops->set_data_dir)
                transport->ops->set_data_dir(transport->gpio_ctx, true);
        } else {
            if (transport->ops->set_data_dir)
                transport->ops->set_data_dir(transport->gpio_ctx, true);
            uint32_t wdata = txn->data;
            for (i = 0; i < 32; i++) {
                bool bit = (wdata >> i) & 0x01;
                transport_clock_pulse(transport, bit, &unused_in);
            }
        }
    } else if (txn->ack == SWD_ACK_WAIT) {
        txn->retry_count++;
        if (txn->retry_count > transport->swd_timing.max_retries) {
            return SWD_ERR_ACK_WAIT_TO;
        }
        if (transport->ops->set_data_dir)
            transport->ops->set_data_dir(transport->gpio_ctx, true);
        return SWD_ERR_BUSY;
    } else {
        transport->error_count++;
        if (transport->ops->set_data_dir)
            transport->ops->set_data_dir(transport->gpio_ctx, true);
        return SWD_ERR_ACK_FAULT;
    }
    return 0;
}

int jtag_transport_tap_reset(debug_transport_t *transport) {
    if (!transport || !transport->ops) return -1;
    if (transport->ops->assert_reset) {
        transport->ops->assert_reset(transport->gpio_ctx);
        if (transport->ops->delay_us)
            transport->ops->delay_us(transport->gpio_ctx, 100);
        transport->ops->deassert_reset(transport->gpio_ctx);
    } else {
        int i;
        bool unused;
        for (i = 0; i < 5; i++) {
            if (transport->ops->set_data_out)
                transport->ops->set_data_out(transport->gpio_ctx, true);
            transport_clock_pulse(transport, true, &unused);
        }
    }
    transport->tap_state = TAP_STATE_TEST_LOGIC_RESET;
    return 0;
}

int jtag_transport_shift(debug_transport_t *transport,
                          bool is_ir,
                          const uint8_t *data_out,
                          uint8_t *data_in,
                          uint32_t bit_count,
                          bool last_tms) {
    if (!transport || !transport->ops || !data_out) return -1;
    if (!data_in && bit_count > 0) return -1;
    tap_state_t target_shift = is_ir ? TAP_STATE_SHIFT_IR : TAP_STATE_SHIFT_DR;
    (void)(is_ir ? TAP_STATE_EXIT1_IR : TAP_STATE_EXIT1_DR);
    uint8_t tms_seq[16];
    int steps = tap_navigate_to(transport->tap_state, target_shift, tms_seq, 16);
    if (steps < 0) return -2;
    int i;
    bool unused;
    for (i = 0; i < steps; i++) {
        if (transport->ops->set_data_out)
            transport->ops->set_data_out(transport->gpio_ctx,
                (i == steps - 1) ? true : false);
        transport_clock_pulse(transport, tms_seq[i] != 0, &unused);
    }
    transport->tap_state = target_shift;
    for (i = 0; i < (int)bit_count; i++) {
        bool tdi_bit = (data_out[i / 8] >> (i % 8)) & 0x01;
        bool tdo_bit;
        bool tms_val = (i == (int)bit_count - 1) ? last_tms : false;
        if (transport->ops->set_data_out)
            transport->ops->set_data_out(transport->gpio_ctx, tdi_bit);
        transport_clock_pulse(transport, tms_val, &tdo_bit);
        if (data_in) {
            if (tdo_bit) data_in[i / 8] |= (1u << (i % 8));
            else data_in[i / 8] &= ~(1u << (i % 8));
        }
        if (tms_val) {
            transport->tap_state = tap_fsm_next[transport->tap_state][1];
            break;
        }
    }
    if (transport->tap_state == target_shift) {
        transport->tap_state = tap_fsm_next[transport->tap_state][(int)last_tms];
    }
    return 0;
}

int jtag_transport_navigate(debug_transport_t *transport,
                             tap_state_t target_state) {
    if (!transport) return -1;
    uint8_t tms_seq[32];
    int steps = tap_navigate_to(transport->tap_state, target_state, tms_seq, 32);
    if (steps < 0) return -2;
    int i;
    bool unused;
    for (i = 0; i < steps; i++) {
        transport_clock_pulse(transport, tms_seq[i] != 0, &unused);
        transport->tap_state = tap_fsm_next[transport->tap_state][tms_seq[i] != 0 ? 1 : 0];
    }
    return 0;
}

double swd_transport_autobaud(debug_transport_t *transport) {
    if (!transport) return 0.0;
    double freq = 50e6;
    while (freq >= 100e3) {
        transport_set_clock_freq(transport, freq);
        if (swd_transport_connect(transport) == 0) {
            swd_transaction_t txn;
            swd_dp_read_prepare(DP_REG_DPIDR, &txn);
            if (swd_transport_transact(transport, &txn) == 0 &&
                txn.ack == SWD_ACK_OK) {
                return freq;
            }
        }
        freq /= 2.0;
    }
    return 0.0;
}

void transport_get_signal_quality(const debug_transport_t *transport,
                                   signal_quality_t *quality) {
    if (!transport || !quality) return;
    memset(quality, 0, sizeof(*quality));
    quality->parity_errors = 0;
    quality->ack_errors = transport->error_count;
    quality->overrun_errors = 0;
    quality->effective_freq_hz = transport->clock_freq_hz;
    uint32_t total_errs = quality->parity_errors +
                          quality->ack_errors +
                          quality->overrun_errors;
    uint32_t total_txns = transport->transaction_count;
    if (total_errs > 0 && total_txns > 0) {
        double ber = (double)total_errs / (double)total_txns;
        quality->signal_to_noise_ratio_db = ber > 0.0
            ? -10.0 * (ber < 1e-9 ? 9.0 : 0.0) : 40.0;
    }
}

double transport_calibrate_delay(debug_transport_t *transport,
                                  uint32_t iterations) {
    if (!transport || iterations == 0) return 0.0;
    uint32_t i;
    double ns_per_iter;
    for (i = 0; i < iterations; i++) {
        if (transport->ops && transport->ops->delay_ns) {
            transport->ops->delay_ns(transport->gpio_ctx, 1000);
        }
    }
    ns_per_iter = 10.0;
    return ns_per_iter;
}

int swd_transport_select_target(debug_transport_t *transport,
                                 uint8_t target_id) {
    if (!transport || target_id >= SWDV2_MAX_TARGETS) return -1;
    uint32_t seq = SWDV2_CONNECT(target_id);
    int i;
    bool unused;
    if (transport->ops && transport->ops->set_data_dir)
        transport->ops->set_data_dir(transport->gpio_ctx, true);
    for (i = 0; i < 16; i++) {
        bool bit = (seq >> i) & 0x01;
        transport_clock_pulse(transport, bit, &unused);
    }
    return 0;
}

int swd_transport_scan_targets(debug_transport_t *transport,
                                uint16_t *detected) {
    if (!transport || !detected) return -1;
    *detected = 0;
    int count = 0;
    uint8_t tid;
    for (tid = 0; tid < SWDV2_MAX_TARGETS; tid++) {
        swd_transport_select_target(transport, tid);
        swd_transaction_t txn;
        swd_dp_read_prepare(DP_REG_DPIDR, &txn);
        if (swd_transport_transact(transport, &txn) == 0 &&
            txn.ack == SWD_ACK_OK) {
            *detected |= (1u << tid);
            count++;
        }
    }
    swd_transport_select_target(transport, SWDV2_TARGET_ID_DEFAULT);
    return count;
}

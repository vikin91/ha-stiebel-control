/*
 * Trial WPF10M request table for the ESP32-S3/MCP2515 installation.
 * Starts with the deployed s3.yaml routes. Upstream WPF10 probes stay opt-in.
 */
#ifndef SIGNAL_REQUESTS_WPF10M_LOCAL_H
#define SIGNAL_REQUESTS_WPF10M_LOCAL_H

#include "config.h"
#include "signal_requests_base.h"

extern const SignalRequest signalRequests[] = {
    // Enable only after the deployed sensor routes have been checked on hardware.
#ifdef WPF10M_UPSTREAM_PROBES
    SIGNAL_REQUESTS_BASE

    // Upstream WPF10 model probes, retained for comparison.
    {"KESSELSOLLTEMP",             FREQ_30S, cm_manager},
    {"SPEICHERSOLLTEMP",           FREQ_30S, cm_manager},
    {"RUECKLAUFISTTEMP",           FREQ_30S, cm_manager},
    {"ABTAUUNGAKTIV",              FREQ_1MIN, cm_heizmodul},
    {"BETRIEBSART_WP",             FREQ_10MIN, cm_manager},
    {"RAUMSOLLTEMP_I",             FREQ_30S, cm_manager},
    {"HEIZKURVE",                  FREQ_10MIN, cm_manager},
    {"ANTILEGIONELLEN",            FREQ_10MIN, cm_manager},
#endif

    // Requests from the working s3.yaml configuration.
    {"AUSSENTEMP",                 FREQ_10MIN, cm_kessel},
    {"RUECKLAUFISTTEMP",           FREQ_10MIN, cm_kessel},
    {"MATERIALNUMMER_HIGH",        FREQ_10MIN, cm_kessel}, // 0x02CA: HK1 flow is unverified
    {"VORLAUFSOLLTEMP",            FREQ_1MIN, cm_bedienmodul_2}, // FE7X 0x301
    {"SPEICHERISTTEMP",            FREQ_10MIN, cm_kessel},
    {"QUELLE_IST",                 FREQ_10MIN, cm_kessel},
    {"HILFSKESSELSOLL",            FREQ_10MIN, cm_kessel},
    {"SPEICHERSOLLTEMP",           FREQ_1MIN, cm_kessel},
    {"WPVORLAUFIST",               FREQ_10MIN, cm_kessel},
    {"PUFFERSOLL",                 FREQ_10MIN, cm_kessel},
    // FEHLERMELDUNG (0x0001) is blacklisted by upstream; the model YAML
    // sends its two read requests separately every five minutes.
    // COMPRESSOR_RUNNING (0x005F) is passively observed; it is never polled.
};

extern const size_t SIGNAL_REQUEST_COUNT_VALUE = sizeof(signalRequests) / sizeof(SignalRequest);

#endif

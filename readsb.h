// Part of readsb, a Mode-S/ADSB/TIS message decoder.
//
// readsb.h: main program header
//
// Copyright (c) 2020 Michael Wolf <michael@mictronics.de>
//
// This code is based on a detached fork of dump1090-fa.
//
// Copyright (c) 2014-2016 Oliver Jowett <oliver@mutability.co.uk>
//
// This file is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// any later version.
//
// This file is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// This file incorporates work covered by the following copyright and
// license:
//
// Copyright (C) 2012 by Salvatore Sanfilippo <antirez@gmail.com>
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//  *  Redistributions of source code must retain the above copyright
//     notice, this list of conditions and the following disclaimer.
//
//  *  Redistributions in binary form must reproduce the above copyright
//     notice, this list of conditions and the following disclaimer in the
//     documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef _READSB_H
#define _READSB_H

// Default version number, if not overriden by the Makefile
#ifndef MODES_READSB_VERSION
#define MODES_READSB_VERSION     "Unknown"
#endif

#ifndef MODES_READSB_VARIANT
#define MODES_READSB_VARIANT     "Mictronics"
#endif

// ============================= Include files ==========================
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdint.h>
#include <stdatomic.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <time.h>
#include <limits.h>
#include <semaphore.h>
#include <protobuf-c/protobuf-c.h>

#include "compat/compat.h"

#if PROTOBUF_C_VERSION_NUMBER < 1003001
#error This code is incompatible with your libprotobuf-c headers and protoc-c compiler. Please compile with a newer version (>1.3.1+) of protoc-c.
#endif

// ============================= #defines ===============================

#define MODES_DEFAULT_FREQ      1090000000
#define MODES_RTL_BUFFERS       16                         // Number of RTL buffers
#define MODES_RTL_BUF_SIZE      (16*16384)                 // 256k
#define MODES_MAG_BUF_SAMPLES   (MODES_RTL_BUF_SIZE / 2)   // Each sample is 2 bytes
#define MODES_MAG_BUFFERS       12                         // Number of magnitude buffers (should be smaller than RTL_BUFFERS for flowcontrol to work)
#define MODES_AUTO_GAIN         -100                       // Use automatic gain
#define MODES_MAX_GAIN          999999                     // Use max available gain
#define MODEAC_MSG_BYTES        2

#define MODES_PREAMBLE_US       8   // microseconds = bits
#define MODES_PREAMBLE_SAMPLES  (MODES_PREAMBLE_US       * 2)
#define MODES_PREAMBLE_SIZE     (MODES_PREAMBLE_SAMPLES  * sizeof(uint16_t))
#define MODES_LONG_MSG_BYTES    14
#define MODES_SHORT_MSG_BYTES   7
#define MODES_LONG_MSG_BITS     (MODES_LONG_MSG_BYTES    * 8)
#define MODES_SHORT_MSG_BITS    (MODES_SHORT_MSG_BYTES   * 8)
#define MODES_LONG_MSG_SAMPLES  (MODES_LONG_MSG_BITS     * 2)
#define MODES_SHORT_MSG_SAMPLES (MODES_SHORT_MSG_BITS    * 2)
#define MODES_LONG_MSG_SIZE     (MODES_LONG_MSG_SAMPLES  * sizeof(uint16_t))
#define MODES_SHORT_MSG_SIZE    (MODES_SHORT_MSG_SAMPLES * sizeof(uint16_t))

#define MODES_OS_PREAMBLE_SAMPLES  (20)
#define MODES_OS_PREAMBLE_SIZE     (MODES_OS_PREAMBLE_SAMPLES  * sizeof(uint16_t))
#define MODES_OS_LONG_MSG_SAMPLES  (268)
#define MODES_OS_SHORT_MSG_SAMPLES (135)
#define MODES_OS_LONG_MSG_SIZE     (MODES_LONG_MSG_SAMPLES  * sizeof(uint16_t))
#define MODES_OS_SHORT_MSG_SIZE    (MODES_SHORT_MSG_SAMPLES * sizeof(uint16_t))

#define MODES_OUT_BUF_SIZE         (16*1024)
#define MODES_OUT_FLUSH_SIZE       (15*1024)
#define MODES_OUT_FLUSH_INTERVAL   (60000)

#define MODES_USER_LATLON_VALID (1<<0)

#define INVALID_ALTITUDE (-9999)

/* Where did a bit of data arrive from? In order of increasing priority */
typedef enum {
    SOURCE_INVALID = 0, /* data is not valid */
    SOURCE_MODE_AC = 1, /* A/C message */
    SOURCE_MLAT = 2, /* derived from mlat */
    SOURCE_MODE_S = 3, /* data from a Mode S message, no full CRC */
    SOURCE_MODE_S_CHECKED = 4, /* data from a Mode S message with full CRC */
    SOURCE_TISB = 5, /* data from a TIS-B extended squitter message */
    SOURCE_ADSR = 6, /* data from a ADS-R extended squitter message */
    SOURCE_ADSB = 7, /* data from a ADS-B extended squitter message */
} datasource_t;

typedef enum {
    UNIT_FEET,
    UNIT_METERS
} altitude_unit_t;

typedef enum {
    ALTITUDE_BARO,
    ALTITUDE_GEOM
} altitude_source_t;

typedef enum {
    CPR_SURFACE, CPR_AIRBORNE, CPR_COARSE
} cpr_type_t;

typedef enum {
    HEADING_INVALID, // Not set
    HEADING_GROUND_TRACK, // Direction of track over ground, degrees clockwise from true north
    HEADING_TRUE, // Heading, degrees clockwise from true north
    HEADING_MAGNETIC, // Heading, degrees clockwise from magnetic north
    HEADING_MAGNETIC_OR_TRUE, // HEADING_MAGNETIC or HEADING_TRUE depending on the HRD bit in opstatus
    HEADING_TRACK_OR_HEADING // GROUND_TRACK / MAGNETIC / TRUE depending on the TAH bit in opstatus
} heading_type_t;

typedef enum {
    COMMB_UNKNOWN,
    COMMB_AMBIGUOUS,
    COMMB_EMPTY_RESPONSE,
    COMMB_DATALINK_CAPS,
    COMMB_GICB_CAPS,
    COMMB_AIRCRAFT_IDENT,
    COMMB_ACAS_RA,
    COMMB_VERTICAL_INTENT,
    COMMB_TRACK_TURN,
    COMMB_HEADING_SPEED
} commb_format_t;

typedef enum {
    NAV_MODE_AUTOPILOT = 1,
    NAV_MODE_VNAV = 2,
    NAV_MODE_ALT_HOLD = 4,
    NAV_MODE_APPROACH = 8,
    NAV_MODE_LNAV = 16,
    NAV_MODE_TCAS = 32
} nav_modes_t;

typedef enum {
    NAV_ALT_INVALID,
    NAV_ALT_UNKNOWN,
    NAV_ALT_AIRCRAFT,
    NAV_ALT_MCP,
    NAV_ALT_FMS
} nav_altitude_source_t;

#define MODES_NON_ICAO_ADDRESS       (1<<24) // Set on addresses to indicate they are not ICAO addresses

#define MODES_INTERACTIVE_REFRESH_TIME 250      // Milliseconds
#define MODES_INTERACTIVE_DISPLAY_TTL 60000     // Delete from display after 60 seconds

#define MODES_NET_HEARTBEAT_INTERVAL 60000      // milliseconds

#define MODES_CLIENT_BUF_SIZE (64*1024)
#define MODES_NET_SNDBUF_SIZE (64*1024)
#define MODES_NET_SNDBUF_MAX  (7)

#define NET_MAX_CONNECTORS 256

#define HISTORY_SIZE 120
#define HISTORY_INTERVAL 30000

#define MODES_NOTUSED(V) ((void) V)

#define AIRCRAFTS_BUCKETS 2048

// Include subheaders after all the #defines are in place

#include "util.h"
#include "anet.h"
#include "net_io.h"
#include "crc.h"
#include "demod_2400.h"
#include "stats.h"
#include "cpr.h"
#include "icao_filter.h"
#include "convert.h"
#include "sdr.h"
#include "readsb.pb-c.h"
#include "geomag.h"
#include "fifo.h"

//======================== structure declarations =========================

typedef enum {
    SDR_NONE = 0, SDR_IFILE, SDR_RTLSDR, SDR_BLADERF, SDR_MICROBLADERF, SDR_MODESBEAST, SDR_PLUTOSDR, SDR_GNS
} sdr_type_t;

// Program global state

struct _Modes { // Internal state
    pthread_t reader_thread;
    unsigned trailing_samples; // extra trailing samples in magnitude buffers
    atomic_int exit; // Exit from the main loop when true
    int8_t dc_filter; // should we apply a DC filter?
    uint32_t show_only; // Only show messages from this ICAO
    int fd; // --ifile option file descriptor
    input_format_t input_format; // --iformat option
    iq_convert_fn converter_function;
    char * dev_name;
    int gain;
    int enable_agc;
    sdr_type_t sdr_type; // where are we getting data from?
    int freq;
    int ppm_error;
    char aneterr[ANET_ERR_LEN];
    int beast_fd; // Local Modes-S Beast handler
    int beast_baudrate; // Mode-S beast and similar baud rate
    struct net_service *services; // Active services
    struct aircraft *aircrafts[AIRCRAFTS_BUCKETS];
    struct net_writer raw_out; // Raw output
    struct net_writer beast_out; // Beast-format output
    struct net_writer beast_reduce_out; // Reduced data Beast-format output
    struct net_writer sbs_out; // SBS-format output
    struct net_writer vrs_out; // SBS-format output
    struct net_writer fatsv_out; // FATSV-format output
    sem_t* stats_semptr; // Statistics semaphore to syncronize with readsbrrd

    // Configuration
    Receiver receiver; // Receiver configuration
    int8_t nfix_crc; // Number of crc bit error(s) to correct
    int8_t check_crc; // Only display messages with good CRC
    int8_t raw; // Raw output format
    int8_t mode_ac; // Enable decoding of SSR Modes A & C
    int8_t mode_ac_auto; // allow toggling of A/C by Beast commands
    int8_t net; // Enable networking
    int8_t net_only; // Enable just networking
    uint32_t preambleThreshold;
    int net_output_flush_size; // Minimum Size of output data
    uint32_t net_connector_delay;
    int filter_persistence; // Maximum number of consecutive implausible positions from global CPR to invalidate a known position.
    uint32_t net_heartbeat_interval; // TCP heartbeat interval (milliseconds)
    uint32_t net_output_flush_interval; // Maximum interval (in milliseconds) between outputwrites
    double maxRange; // Absolute maximum decoding range, in *metres*
    double sample_rate; // actual sample rate in use (in hz)
    uint32_t interactive_display_ttl; // Interactive mode: TTL display
    uint64_t stats; // Interval (millis) between stats dumps,
    uint64_t startup_time; // Readsb startup epoch
    uint64_t ifile_now; // ifile timestamp
    uint32_t output_interval; // Interval between rewriting the aircraft file, in milliseconds; also the advertised map refresh interval
    char *net_output_raw_ports; // List of raw output TCP ports
    char *net_input_raw_ports; // List of raw input TCP ports
    char *net_output_sbs_ports; // List of SBS output TCP ports
    char *net_input_sbs_ports; // List of SBS input TCP ports
    char *net_input_beast_ports; // List of Beast input TCP ports
    char *net_output_beast_ports; // List of Beast output TCP ports
    char *net_output_beast_reduce_ports; // List of Beast output TCP ports
    uint32_t net_output_beast_reduce_interval; // Position update interval for data reduction
    char *net_output_vrs_ports; // List of VRS output TCP ports
    int8_t basestation_is_mlat; // Basestation input is from MLAT
    struct net_connector **net_connectors; // client connectors
    int net_connectors_count;
    int net_connectors_size;
    char *filename; // Input form file, --ifile option
    char *net_bind_address; // Bind address
    char *output_dir; // Path to output base directory, or NULL not to write any output.
    char *beast_serial; // Modes-S Beast device path
    int net_sndbuf_size; // TCP output buffer size (64Kb * 2^n)
    int8_t net_verbatim; // if true, send the original message, not the CRC-corrected one
    int8_t forward_mlat; // allow forwarding of mlat messages to output ports
    int8_t quiet; // Suppress stdout
    int8_t interactive; // Interactive mode
    int8_t stats_polar_range; // Collect/show a range histogram?
    int8_t onlyaddr; // Print only ICAO addresses
    int8_t metric; // Use metric units
    int8_t use_gnss; // Use GNSS altitudes with H suffix ("HAE", though it isn't always) when available
    int8_t mlat; // Use Beast ascii format for raw data output, i.e. @...; iso *...;
    int8_t rx_location_accuracy; // Accuracy of location metadata: 0=none, 1=approx, 2=exact
    int aircraft_history_next;
    int aircraft_history_full;
    int stats_latest_1min;
    int bUserFlags; // Flags relating to the user details
    int8_t biastee;
    struct stats stats_current;
    struct stats stats_alltime;
    struct stats stats_periodic;
    struct stats stats_1min[15];
    struct stats stats_5min;
    struct stats stats_15min;
    struct range_stats stats_range;
    struct timespec reader_cpu_accumulator; // accumulated CPU time used by the reader thread
    struct timespec reader_cpu_start; // start time for the last reader thread CPU measurement
    pthread_mutex_t reader_cpu_mutex; // mutex protecting reader_cpu_accumulator
};

extern struct _Modes Modes;

// The struct we use to store information about a decoded message.

struct modesMessage {
    uint64_t timestampMsg; // Timestamp of the message (12MHz clock)
    uint64_t sysTimestampMsg; // Timestamp of the message (system time)
    // Generic fields
    unsigned char msg[MODES_LONG_MSG_BYTES]; // Binary message.
    unsigned char verbatim[MODES_LONG_MSG_BYTES]; // Binary message, as originally received before correction
    int msgbits; // Number of bits in message
    int msgtype; // Downlink format #
    uint32_t crc; // Message CRC
    int correctedbits; // No. of bits corrected
    uint32_t addr; // Address Announced
    AircraftMeta__AddrType addrtype; // address format / source
    int remote; // If set this message is from a remote station
    int score; // Scoring from scoreModesMessage, if used
    int sbs_in; // Signifies this message is coming from basestation input
    int reduce_forward; // forward this message for reduced beast output
    datasource_t source; // Characterizes the overall message source
    double signalLevel; // RSSI, in the range [0..1], as a fraction of full-scale power
    // Raw data, just extracted directly from the message
    // The names reflect the field names in Annex 4
    unsigned IID; // extracted from CRC of DF11s
    unsigned AA;
    unsigned AC;
    unsigned CA;
    unsigned CC;
    unsigned CF;
    unsigned DR;
    unsigned FS;
    unsigned ID;
    unsigned KE;
    unsigned ND;
    unsigned RI;
    unsigned SL;
    unsigned UM;
    unsigned VS;
    unsigned metype; // DF17/18 ME type
    unsigned mesub; // DF17/18 ME subtype

    unsigned char MB[7];
    unsigned char MD[10];
    unsigned char ME[7];
    unsigned char MV[7];

    // Decoded data
    unsigned altitude_baro_valid : 1;
    unsigned altitude_geom_valid : 1;
    unsigned track_valid : 1;
    unsigned track_rate_valid : 1;
    unsigned heading_valid : 1;
    unsigned roll_valid : 1;
    unsigned gs_valid : 1;
    unsigned ias_valid : 1;
    unsigned tas_valid : 1;
    unsigned mach_valid : 1;
    unsigned baro_rate_valid : 1;
    unsigned geom_rate_valid : 1;
    unsigned squawk_valid : 1;
    unsigned callsign_valid : 1;
    unsigned cpr_valid : 1;
    unsigned cpr_odd : 1;
    unsigned cpr_decoded : 1;
    unsigned cpr_relative : 1;
    unsigned category_valid : 1;
    unsigned geom_delta_valid : 1;
    unsigned from_mlat : 1;
    unsigned from_tisb : 1;
    unsigned spi_valid : 1;
    unsigned spi : 1;
    unsigned alert_valid : 1;
    unsigned alert : 1;
    unsigned emergency_valid : 1;
    unsigned padding : 13;

    // valid if altitude_baro_valid:
    int altitude_baro; // Altitude in either feet or meters
    altitude_unit_t altitude_baro_unit; // the unit used for altitude

    // valid if altitude_geom_valid:
    int altitude_geom; // Altitude in either feet or meters
    altitude_unit_t altitude_geom_unit; // the unit used for altitude

    // following fields are valid if the corresponding _valid field is set:
    int geom_delta; // Difference between geometric and baro alt
    float heading; // ground track or heading, degrees (0-359). Reported directly or computed from from EW and NS velocity
    heading_type_t heading_type; // how to interpret 'track_or_heading'
    float track_rate; // Rate of change of track, degrees/second
    float roll; // Roll, degrees, negative is left roll

    struct {
        // Groundspeed, kts, reported directly or computed from from EW and NS velocity
        // For surface movement, this has different interpretations for v0 and v2; both
        // fields are populated. The tracking layer will update "gs.selected".
        float v0;
        float v2;
        float selected;
    } gs;
    unsigned ias; // Indicated airspeed, kts
    unsigned tas; // True airspeed, kts
    double mach; // Mach number
    int baro_rate; // Rate of change of barometric altitude, feet/minute
    int geom_rate; // Rate of change of geometric (GNSS / INS) altitude, feet/minute
    unsigned squawk; // 13 bits identity (Squawk), encoded as 4 hex digits
    char callsign[16]; // 8 chars flight number, NUL-terminated
    unsigned category; // A0 - D7 encoded as a single hex byte
    AircraftMeta__Emergency emergency; // emergency/priority status

    // valid if cpr_valid
    cpr_type_t cpr_type; // The encoding type used (surface, airborne, coarse TIS-B)
    unsigned cpr_lat; // Non decoded latitude.
    unsigned cpr_lon; // Non decoded longitude.
    unsigned cpr_nucp; // NUCp/NIC value implied by message type

    AircraftMeta__AirGround airground; // air/ground state

    // valid if cpr_decoded:
    double decoded_lat;
    double decoded_lon;
    unsigned decoded_nic;
    unsigned decoded_rc;

    commb_format_t commb_format; // Inferred format of a comm-b message

    // various integrity/accuracy things

    struct {
        unsigned nic_a_valid : 1;
        unsigned nic_b_valid : 1;
        unsigned nic_c_valid : 1;
        unsigned nic_baro_valid : 1;
        unsigned nac_p_valid : 1;
        unsigned nac_v_valid : 1;
        unsigned gva_valid : 1;
        unsigned sda_valid : 1;

        unsigned nic_a : 1; // if nic_a_valid
        unsigned nic_b : 1; // if nic_b_valid
        unsigned nic_c : 1; // if nic_c_valid
        unsigned nic_baro : 1; // if nic_baro_valid

        unsigned nac_p : 4; // if nac_p_valid
        unsigned nac_v : 3; // if nac_v_valid

        unsigned sil : 2; // if sil_type != SIL_INVALID

        unsigned gva : 2; // if gva_valid

        unsigned sda : 2; // if sda_valid
        unsigned padding : 7;
        AircraftMeta__SilType sil_type;
    } accuracy;

    // Operational Status

    struct {
        AircraftMeta__SilType sil_type;
        heading_type_t tah;
        heading_type_t hrd;

        enum {
            ANGLE_HEADING, ANGLE_TRACK
        } track_angle;

        unsigned cc_lw;
        unsigned cc_antenna_offset;

        unsigned valid : 1;
        unsigned version : 3;

        unsigned om_acas_ra : 1;
        unsigned om_ident : 1;
        unsigned om_atc : 1;
        unsigned om_saf : 1;

        unsigned cc_acas : 1;
        unsigned cc_cdti : 1;
        unsigned cc_1090_in : 1;
        unsigned cc_arv : 1;
        unsigned cc_ts : 1;
        unsigned cc_tc : 2;
        unsigned cc_uat_in : 1;
        unsigned cc_poa : 1;
        unsigned cc_b2_low : 1;
        unsigned cc_lw_valid : 1;
        unsigned padding : 13;
    } opstatus;

    // combined:
    //   Target State & Status (ADS-B V2 only)
    //   Comm-B BDS4,0 Vertical Intent

    struct {
        unsigned fms_altitude; // FMS selected altitude
        unsigned mcp_altitude; // MCP/FCU selected altitude
        float qnh; // altimeter setting (QFE or QNH/QNE), millibars
        float heading; // heading, degrees (0-359) (could be magnetic or true heading; magnetic recommended)
        unsigned heading_valid : 1;
        unsigned fms_altitude_valid : 1;
        unsigned mcp_altitude_valid : 1;
        unsigned qnh_valid : 1;
        unsigned modes_valid : 1;
        unsigned padding : 27;
        heading_type_t heading_type;

        nav_altitude_source_t altitude_source;

        nav_modes_t modes;
    } nav;
};

/* All the program options */
enum {
    OptDeviceType = 700,
    OptDevice,
    OptGain,
    OptFreq,
    OptInteractive,
    OptNoInteractive,
    OptInteractiveTTL,
    OptRaw,
    OptPreambleThreshold,
    OptModeAc,
    OptNoModeAcAuto,
    OptForwardMlat,
    OptLat,
    OptLon,
    OptMaxRange,
    OptFix,
    OptNoFix,
    OptNoCrcCheck,
    OptAggressive,
    OptMlat,
    OptStats,
    OptStatsRange,
    OptStatsEvery,
    OptOnlyAddr,
    OptMetric,
    OptGnss,
    OptSnip,
    OptQuiet,
    OptShowOnly,
    OptOutputDir,
    OptOutputTime,
    OptRxLocAcc,
    OptDcFilter,
    OptBiasTee,
    OptNet,
    OptNetOnly,
    OptNetBindAddr,
    OptNetRiPorts,
    OptNetRoPorts,
    OptNetSbsPorts,
    OptNetSbsInPorts,
    OptNetBiPorts,
    OptNetBoPorts,
    OptNetBeastReducePorts,
    OptNetBeastReduceInterval,
    OptNetVRSPorts,
    OptNetRoSize,
    OptNetRoRate,
    OptNetRoIntervall,
    OptNetConnector,
    OptNetConnectorDelay,
    OptNetHeartbeat,
    OptNetBuffer,
    OptNetVerbatim,
    OptRtlSdrEnableAgc,
    OptRtlSdrPpm,
    OptBeastSerial,
    OptBeastBaudrate,
    OptBeastDF1117,
    OptBeastDF045,
    OptBeastMlatTimeOff,
    OptBeastCrcOff,
    OptBeastFecOff,
    OptBeastModeAc,
    OptIfileName,
    OptIfileFormat,
    OptIfileThrottle,
    OptBladeFpgaDir,
    OptBladeDecim,
    OptBladeBw,
    OptPlutoUri,
    OptPlutoNetwork,
};

// This one needs modesMessage:
#include "track.h"
#include "mode_s.h"
#include "comm_b.h"

// ======================== function declarations =========================

#ifdef __cplusplus
extern "C" {
#endif

    //
    // Functions exported from mode_ac.c
    //
    void decodeModeAMessage(struct modesMessage *mm, int ModeA);
    void modeACInit();
    int modeAToModeC(unsigned int modeA);
    unsigned modeCToModeA(int modeC);

    //
    // Functions exported from interactive.c
    //
    void interactiveInit(void);
    void interactiveShowData(void);
    void interactiveCleanup(void);

    // Provided by readsb.c & viewadsb.c
    void receiverPositionChanged(float lat, float lon, float alt);

#ifdef __cplusplus
}
#endif

#endif // _READSB_H


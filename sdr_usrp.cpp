// Part of readsb, a Mode-S/ADSB/TIS message decoder.
//
// sdr_usrp.c: usrp 4.0 support
//
// Copyright (c) 2024 Julian Casallas <julian.casallas@gmail.com>
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




#include <uhd/utils/thread.hpp>
#include <uhd/types/tune_request.hpp>
#include <uhd/usrp/multi_usrp.hpp>
#include <sys/types.h>
#include <iostream>
#include <string.h>
#include <chrono>
#include <vector>
#include <functional>
#ifdef __cplusplus
extern "C" {
#endif
#include "readsb.h"
#include "sdr.h"
#include "fifo.h"
#ifdef __cplusplus
}
#endif
#include "sdr_usrp.hpp"



/*!\brief structure holds the parameters to configure USRP devices*/
typedef struct device_t device;

using namespace std::chrono_literals;

typedef std::function<uhd::sensor_value_t(const std::string&)> get_sensor_fn_t;

static bool check_locked_sensor(std::vector<std::string> sensor_names,
    const char* sensor_name,
    get_sensor_fn_t get_sensor_fn,
    double setup_time)
{
    if (std::find(sensor_names.begin(), sensor_names.end(), sensor_name)
        == sensor_names.end())
        return false;

    const auto setup_timeout = std::chrono::steady_clock::now() + (setup_time * 1s);
    bool lock_detected       = false;

    std::cout << "Waiting for \"" << sensor_name << "\": ";
    std::cout.flush();

    while (true) {
        if (lock_detected and (std::chrono::steady_clock::now() > setup_timeout)) {
            std::cout << " locked." << std::endl;
            break;
        }
        if (get_sensor_fn(sensor_name).to_bool()) {
            std::cout << "+";
            std::cout.flush();
            lock_detected = true;
        } else {
            if (std::chrono::steady_clock::now() > setup_timeout) {
                std::cout << std::endl;
                std::string error_ms = "timed out waiting for consecutive locks on sensor " + 
                                       std::string(sensor_name);
                throw std::runtime_error(error_ms);
            }
            std::cout << "_";
            std::cout.flush();
        }
        std::this_thread::sleep_for(100ms);
    }
    std::cout << std::endl;
    return true;
}






static struct {
    std::string device_str;
    std::string args;
    std::string ant;
    std::string fmt;
    size_t chan;
    size_t spb;
    double rate;
    double freq_Hz;
    double gain;
    double bw;
    float setup_time;
    float lo_offset;
    uhd::usrp::multi_usrp::sptr usrp;
    iq_convert_fn converter; // Not sure what it does.
    struct converter_state *converter_state;
    std::string wirefmt;
    int16_t* readbuf;
} usrpSDR;  


extern "C" void usrpInitConfig() {
    usrpSDR.spb =  MODES_MAG_BUF_SAMPLES;
    usrpSDR.args = "type=x300,addr=192.168.60.2";
    usrpSDR.wirefmt = "sc16";
    usrpSDR.chan = 0;
    usrpSDR.setup_time = 1.0;
    usrpSDR.lo_offset = 0.0;
    usrpSDR.gain = 10;
    usrpSDR.bw = 1750000;
    usrpSDR.fmt = "sc16";
}
extern "C" bool usrpOpen() {
  // create a usrp device
  std::cout << std::endl;
  std::cout << "Creating the usrp device with: " << usrpSDR.args << "..." << std::endl;
  usrpSDR.usrp = uhd::usrp::multi_usrp::make(usrpSDR.args);
  
  std::cout << "Using Device: " << usrpSDR.usrp->get_pp_string() << std::endl;
  usrpSDR.rate = Modes.sample_rate;
  
  printf("Setting RX Rate: %f Msps...\n", usrpSDR.rate / 1e6);
  usrpSDR.usrp->set_rx_rate(usrpSDR.rate);

  usrpSDR.freq_Hz = Modes.freq;
  printf("Setting RX Freq: %f MHz...\n", usrpSDR.freq_Hz / 1e6);
  printf("Setting RX LO Offset: %f MHz...\n", usrpSDR.lo_offset / 1e6);
  //uhd::tune_request_t tune_request(usrpSDR.freq_Hz, usrpSDR.lo_offset);
  uhd::tune_request_t tune_request(usrpSDR.freq_Hz);
  
  //usrpSDR.usrp->set_rx_freq(tune_request, usrpSDR.chan);
  usrpSDR.usrp->set_rx_freq(tune_request);
  printf("Actual RX Freq: %f MHz...\n\n", usrpSDR.usrp->get_rx_freq(usrpSDR.chan) / 1e6);

  printf("Setting RX Gain: %f dB...\n", usrpSDR.gain);
  usrpSDR.usrp->set_rx_gain(usrpSDR.gain);
  

  printf("Setting RX Bandwidth: %f MHz...\n", usrpSDR.bw / 1e6);
  //usrpSDR.usrp->set_rx_bandwidth(usrpSDR.bw, usrpSDR.chan);
  usrpSDR.usrp->set_rx_bandwidth(usrpSDR.bw);

  std::cout << "Locking LO on channel " << usrpSDR.chan << std::endl;
    
  std::this_thread::sleep_for(std::chrono::seconds(1)); // allow for some setup time
  check_locked_sensor(
                usrpSDR.usrp->get_rx_sensor_names(usrpSDR.chan),
                "lo_locked",
                [](const std::string& sensor_name) {
                    return usrpSDR.usrp->get_rx_sensor(sensor_name, usrpSDR.chan);
                },
                usrpSDR.setup_time);




  if (!(usrpSDR.readbuf = (int16_t*)malloc(MODES_RTL_BUF_SIZE * 4))) {
    fprintf(stderr, "plutosdr: Failed to allocate read buffer\n");
    usrpClose();
    return false;
}

  usrpSDR.converter = init_converter(INPUT_SC16,
            Modes.sample_rate,
            Modes.dc_filter,
            &usrpSDR.converter_state);

  if (!usrpSDR.converter) {
    fprintf(stderr, "usrpSDR: Can't initialize sample converter\n");
    //usrpsdrClose(); TODO: needs to be defined.
    return false;
  }

  printf("Finishing setting up USRP for running...\n\n\n");

  return true;

 


}




extern "C" bool usrpHandleOption(int argc, char *argv) {
    printf ("%d %s",argc, argv);

    return true;
}


static void usrpCallback(int16_t *buf, uint32_t len) {
  static unsigned dropped = 0;
  static uint64_t sampleCounter = 0;
  
  sdrMonitor();

  unsigned samples_read = len / 2; 
  if (!samples_read)
    return;
  
  struct mag_buf *outbuf = fifo_acquire(0);
  if(!outbuf) {
    // FIFO is full. Drop this block.
    dropped += samples_read;
    sampleCounter += samples_read;
    return;
  }

  outbuf->flags = static_cast<mag_buf_flags>(0);
  outbuf->dropped = 0;

  if (dropped) {
      // We previously dropped some samples due to no buffers being available
      outbuf->flags = static_cast<mag_buf_flags>(outbuf->flags | MAGBUF_DISCONTINUOUS);
      outbuf->dropped = dropped;

      // reset dropped counter
      dropped = 0;
  }

  outbuf->sampleTimestamp = sampleCounter * 12e6 / Modes.sample_rate;
  sampleCounter += samples_read;
  uint64_t block_duration = 1e3 * samples_read / Modes.sample_rate;
  outbuf->sysTimestamp = mstime() - block_duration;



  // Convert the new data
  unsigned to_convert = samples_read;
  if (to_convert + outbuf->overlap > outbuf->totalLength) {
      // how did that happen?
      to_convert = outbuf->totalLength - outbuf->overlap;
      dropped = samples_read - to_convert;
  }

  usrpSDR.converter(buf, &outbuf->data[outbuf->overlap], to_convert, usrpSDR.converter_state, &outbuf->mean_level, &outbuf->mean_power);
  outbuf->validLength = outbuf->overlap + to_convert;

  // Push to the demodulation thread
  fifo_enqueue(outbuf);

}



extern "C" void usrpRun() {
  printf("starting the runing USRP function...\n");

  uhd::stream_args_t stream_args(usrpSDR.fmt, usrpSDR.wirefmt);
  //stream_args.channels = 1;

  printf("checking frequency %f\n",usrpSDR.usrp->get_rx_freq()/1e6);
  uhd::rx_streamer::sptr rx_stream = usrpSDR.usrp->get_rx_stream(stream_args);

  uhd::rx_metadata_t md;

  std::vector<std::complex<short>> buff(usrpSDR.spb);  
  printf("Setting vector of size: %zd ...\n", usrpSDR.spb);

  uhd::stream_cmd_t stream_cmd(uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS);
  stream_cmd.stream_now = true;
  rx_stream->issue_stream_cmd(stream_cmd);
  
  uint32_t num_acc_samps =  0;
  printf("ABout to start the infinite loop rx streammer...\n");

  while (!Modes.exit) {
    int16_t* p = usrpSDR.readbuf;

    uint32_t num_rx_samps = rx_stream->recv(&buff.front(), buff.size(), md);

    // handle the error codes
    switch (md.error_code) {
      case uhd::rx_metadata_t::ERROR_CODE_NONE:
          break;

      case uhd::rx_metadata_t::ERROR_CODE_TIMEOUT:
          if (num_acc_samps == 0)
              continue;
          std::cout << "Got timeout before all samples received, \n \
          possible packet loss, exiting loop..."
                    << std::endl;
          goto done_loop;

      default:
          printf("Got error code 0x%x, exiting loop...\n", md.error_code);
          goto done_loop;
    }
    num_acc_samps += num_rx_samps;
    
    
    
    for (uint32_t i = 0; i < num_rx_samps; i++) {
        p[2*i] = buff[i].real();
        p[2*i+1] = buff[i].imag();
    }

    usrpCallback(usrpSDR.readbuf, num_rx_samps);
  }
  done_loop:
  rx_stream->issue_stream_cmd(uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS);

  // finished

  std::cout << std::endl << "Done!" << std::endl << std::endl;

}

extern "C" void usrpClose() {
    if (usrpSDR.readbuf) {
    printf("Closing usrpSDR buffer...\n");
    free(usrpSDR.readbuf);
    }
}
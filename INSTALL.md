# Build and installation

Example build and installation process, e.g. Ubuntu Server 20.04 or Dietpi. Package build tested on DietPi armv6 (armv7l) and armv8 (aarch64).

## General dependencies

Building the package with `dpkg-buildpackage -b` or by `make all` requires the following libraries and tools (with sub-dependencies) being installed:

* collectd-core
* rrdtool
* librrd-dev
* libprotobuf-c-dev
* protobuf-c-compiler
* libncurses5-dev
* libusb-1.0-0-dev
* lighttpd
* build-essential
* binutils
* cmake
* debhelper
* dh-systemd
* pkg-config
* fakeroot
* git

Install build dependencies:

```
sudo apt-get update
sudo apt-get upgrade

sudo apt-get --no-install-suggests --no-install-recommends install collectd-core rrdtool librrd-dev \
libncurses5-dev libusb-1.0-0-dev \
lighttpd build-essential binutils cmake debhelper dh-systemd pkg-config \
fakeroot git
```

## Build and install UHD

```
sudo apt-get install autoconf automake build-essential ccache cmake cpufrequtils doxygen ethtool \
g++ git inetutils-tools libboost-all-dev libncurses5 libncurses5-dev libusb-1.0-0 libusb-1.0-0-dev \
libusb-dev python3-dev python3-mako python3-numpy python3-requests python3-scipy python3-setuptools \
python3-ruamel.yaml

git clone https://github.com/EttusResearch/uhd.git

export GIT_SSL_NO_VERIFY=1 //If certs is an issue (proceed at you own risk).
cd uhd/host
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=/opt/uhd ../ 
make
sudo make install
sudo ldconfig
``` 


## Build readsb

```
git clone --branch usrp --depth 1 --single-branch https://github.com/mictronics/readsb-protobuf.git
cd readsb-protobuf
```


### Building manually

You can probably just run `make`. By default make builds with no specific library support. See below.
Binaries are built in the source directory; you will need to arrange to install them (and a method for starting them) yourself.

`make USRPSDR=yes` will enable rtl-sdr support and add the dependency on librtlsdr.


### Configuration

After installation, either by manual building or from package, you need to configure readsb service and web application.

Edit __/etc/default/readsb__ to set the service options, device type, network ports etc.

The web application is configured by editing __/usr/share/readsb/html/script/readsb/defaults.js__ or __src/script/readsb/default.ts__ prior to compilation. Several settings can be modified through web browser. These settings are stored inside browser indexedDB and are individual to users or browser profiles.

## Note about bias tee support

Bias tee support is available for RTL-SDR.com V3 dongles. If you wish to enable bias tee support,
you must ensure that you are building this package with a version of librtlsdr installed that supports this capability.
You can find suitable source packages [here](https://github.com/librtlsdr/librtlsdr). To enable the necessary
support code when building, be sure to include preprocessor define macro HAVE_BIASTEE, e.g.:

`make HAVE_BIASTEE=yes` will enable biastee support for RTLSDR interfaces.

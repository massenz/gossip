# Gossip Protocol for Fault Detectors


Project   | gossip
:---      | ---:
Author    | [M. Massenzio](https://bitbucket.org/marco)
Repository| <https://bitbucket.org/marco/gossip>
Release   | 1.0.2
Updated   | 2020-05-07

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)

# SWIM Gossip and Consensus algorithm

This project is a  reference implementation of a failure detector based on the [SWIM Gossip protocol](SWIM); 
the project creates a shared library `libgossip` for use in your own system; a demo implementation 
is provided in the [`gossip_example`](src/examples/gossip_example.cpp) server.

[Build and test](#build_and_test) instructions for the impatient are further below.

### PING demo server

This is based on [ZeroMQ C++ bindings](http://api.zeromq.org/2-1:zmq-cpp) and is an example implementation using the `SwimServer` class.

The same binary can act both as one continuously listening on a given
`port` or as a client sending a one-off status update to a `destination` TCP socket.

See the [Install & Build](#install_and_build) section to build the `swim_server_demo` target, then start the listening server:

```bash
./build/bin/swim_server_demo receive --port=3003
```

and it will be listening for incoming [`SwimStatus`](proto/swim.proto) messages on port `3003`; 
a client can then send a message updates for five seconds using:

```bash
./build/bin/swim_server_demo send --host=tcp://localhost:3003 --duration=5
```

(obviously, change the hostname if you are running the two on separate machines/VMs/containers).

Use the `--help` option to see the full list of available options.


## Full-fledged SWIM Detector implementation


Again, use:

    $ ./build/bin/gossip_detector_example -h

to see all the available options; [`bin/run_example`](bin/run_example) will start three detectors in background and will 
connect each other to demonstrate how they can detect failures (you can keep killing and restarting them, and see how the reports change).

Below is a short description of how this works, full details of the protocol in the [references](#references).

### Server states

![States Diagram](docs/images/distlib-states.png)

After startup, a `SwimServer` can `ping()` one or more of the servers that have been named as `seeds` (perhaps, as a CLI 
`--seeds` arguments, or named in a config file) and will be inserted into the list of `Alive` peers; afterwards, other 
peers will learn of this server's `Alive` state via the Gossip protocol, as this is `reported` to them as being `Alive`.

Subsequently, at regular intervals (but randomly, from the list of `Alive` servers) successful `ping()` requests will 
keep this server in the pool of peers' lists of `Alive` peers.

If one of the pings fails, or the server is reported by one of the peers to be `Suspected`, it will be placed in the 
appropriate list and, after a (configurable) timeout lapses, it will be considered as `Terminated` and removed from the list.

Once in the `Suspected` list, a server is never pinged again from the detector; however, it may come back `Alive` 
under one of these conditions (and __before__ the timeout expires):

- _indirectly_, when one of the other peers reports it to be `Alive`;
- _directly_, if the server receives a `ping()` request from the `Suspected` server which arrives __after__ a similar 
  `ping()` request from this server had failed (and caused the other server to be `Suspected`);
- _mediated_, when a third server reports a successful response to a `forward ping()` request that this server had 
  requested, with the `Suspected` server as the object.

When the (configurable) timeout expires, the `Suspected` server is simply removed from the list and assumed to be 
__terminated__ (both the `Started` and `terminated` states are only "logical", no list is kept, or no special meaning 
is associated to them).

In particular, we do not gossip about servers that have been determined to be in either state, and we will assume that 
each of the peers will take care of removing suspected servers from their lists once the timeout (which is not necessarily 
the same for the entire pool - or even constant over time for a given server) expires.

In the example below, `1` knows about `2` directly; and `2` knows about `7` indirectly (when it receives a `Report` 
from `5`, who learned in turn from `6`); `1` "suspects" `3` (either due to a transient network failure; or because `3` 
timed out, possibly due to a temporary load), but then confirms that it is indeed "alive" by asking `4` to forward a ping, 
and the latter successfully contacts `3`, reporting back to `1`.

![SWIM Protocol](docs/images/SWIM-gossip.png)

See the [SWIM Paper](SWIM) for more details.

__Gossip__

At regular intervals, a gossiper will pick at random from the list of `Alive` peers _k_ server,
to which it will send a `SwimReport` containing __only__ the changes since the last report that
was sent out (regardless of _who_ it was send out to), as gossipers do not care much for old news.

_Timing_ is an issue at present, in that we want to prevent stale news to overwrite newer state
information - however, using the report's records' timestamps is unreliable, as different
servers will have different system clocks and, potentially, they will be further diverging over
time.

`TODO: this has not been addressed for now`


# Install & Build

## Pre-requisites

The scripts in this repository take advantage of shared common utility functions
in [this common utils repository](https://bitbucket.org/marco/common-utils): clone it
somewhere, and make `$COMMON_UTILS_DIR` point to it:

```shell script
git clone git@bitbucket.org:marco/common-utils.git
export COMMON_UTILS_DIR="$(pwd)/common-utils"
```

To build/test the project, link to the scripts there:

```shell script
ln -s ${COMMON_UTILS_DIR}/build.sh build
ln -s ${COMMON_UTILS_DIR}/test.sh test
```

## Conan packages

This project is built using CMake and relies on [Conan](https://conan.io) to manage the binary dependencies 
(shared libraries): these are listed in `conanfile.text`.

In order to build the libraries with Conan, a `default` profile will have to be defined (typically, 
in `~/.conan/profiles/default`); something like this:

```
[settings]
build_type=Release

os=Macos
os_build=Macos
arch=x86_64
arch_build=x86_64

compiler=clang
compiler.version=11.0
compiler.libcxx=libc++
```

adjust those values to match your system's configuration.

**NOTE** this project has been built and tested using `clang++` (and the `bin/build` scripts assumes that too); if you prefer using `gcc++` YMMV.


See also `CMakeLists.txt` for the changes necessary to add Conan's builds to the targets.

See [the Conan documentation](https://docs.conan.io/en/latest/) for more information, and 
[this post](https://medium.com/swlh/converting-a-c-project-to-cmake-conan-61ba9a998cb4) for a walkthrough.

## Google Protobuf

The [gossip protocol implementation](#swim_gossip_and_consensus_algorithm) uses 
[Google Protocol Buffers](https://developers.google.com/protocol-buffers/docs/overview) 
as the serialization protocol to exchange status messages between servers; the main library
dependencies are installed via [Conan](#conan-packages); however, the build will fail if `protoc` 
is not on your system's `$PATH`.

The easiest way to do so is to download it from [the Protobuf github releases page](https://github.com/protocolbuffers/protobuf/releases?after=v3.10.0) 
(look for a `protoc-...zip` file), unzip it and put it in a folder on the `$PATH`.

The most recent build (`1.0.2`) has been built with [`protoc-3.9.1`](https://github.com/protocolbuffers/protobuf/releases/download/v3.9.1/protoc-3.9.1-linux-x86_64.zip);
while it is probably not very critical, it is best to match the version in the `conanfile.txt` Protobuf dependency.

```shell script
PROTOC=protoc-3.9.1-linux-x86_64.zip
wget https://github.com/protocolbuffers/protobuf/releases/download/v3.9.1/$PROTOC
unzip $PROTOC
mv protoc /usr/local/bin/
```

## Additional dependencies

The project relies additionally on:

* [`distlib`](libdist) (a library of utilities for distributed systems); 
* [`SimpleHttpRequests`](https://github.com/massenz/SimpleHttpRequest)[^simplehttpreq] (an HTTP client, only used for integration tests); and
* [`SimpleApiServer`](https://bitbucket.org/marco/simpleapiserver)

They all rely on `Conan` for managing their respective dependencies, and have automated build scripts: simply `git clone` 
the respective repositories, run the `build` scripts, making sure that the `$INSTALL_DIR` environment variable is set 
[^INSTALL_DIR] and it will all "just work."

Shared libraries and headers will be installed, respectively, in `$INSTALL_DIR/lib` and `$INSTALL_DIR/include/{project}`; 
these are referenced in the `CMakeLists.txt` build descriptor.


### HTTP Server

To serve the REST API, we use [libmicrohttpd](https://www.gnu.org/software/libmicrohttpd/), 
"a small C library that is supposed to make it easy to run an HTTP server as part of another application."

Unfortunately, there is no Conan recipe to build and install it, so this needs to be done manually.

This can be either installed directly as a package under most Linux distributions (for example, on Ubuntu, 
`sudo apt-get install libmicrohttpd-dev` will do the needful), or can be built from source:

```
wget http://open-source-box.org/libmicrohttpd/libmicrohttpd-0.9.55.tar.gz
tar xfz libmicrohttpd-0.9.55.tar.gz
cd libmicrohttpd-0.9.55/
./configure --prefix ${INSTALL_DIR}/libmicrohttpd
make && make install
```

The include file and libraries will be, respectively, in `${INSTALL_DIR}/libmicrohttpd/include` and `lib` folders.

See [the tutorial](https://www.gnu.org/software/libmicrohttpd/tutorial.html) for more information about usage.


## Build, Test & Install

To build the project:

```shell script
$ export INSTALL_DIR=/some/path
$ ./bin/build && ./bin/test
```

or to simply run a subset of the tests with full debug logging:

    $ ./bin/test -v --gtest_filter="Bucket*"     

To install the generated binaries (`.so` or `.dylib` shared libraries) 
and headers so that other projects can find them:

    $ cd build && make install

See the scripts in the `${COMMON_UTILS_DIR}` folder for more options.

## API Documentations

All the classes are documented using [Doxygen]() and are generated by running `doxygen`; HTML documentation will be generated in `docs/apidocs`.

---

# References

 * [SWIM: Scalable Weakly-consistent Infection-style Process Group Membership Protocol](SWIM)
 * [Unreliable Distributed Failure Detectors for Reliable Systems](detectors)
 * [A Gossip-Style Failure Detection Service](gossip)
 * [libdist - C++ utilities for Distributed Computing](libdist)


[SWIM]: https://www.cs.cornell.edu/projects/Quicksilver/public_pdfs/SWIM.pdf
[detectors]: https://goo.gl/6yuh9T
[gossip]: https://goo.gl/rxAIa6
[libdist]: https://bitbucket.org/marco/distlib

# Footnotes

[^simplehttpreq]: Use the forked version on `massenz/simp
[^INSTALL_DIR]: This is the base directory for both looking up shared libraries and header files, and where the build targets will be installed to: if it's not defined the build will fail.

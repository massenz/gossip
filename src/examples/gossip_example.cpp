// Copyright (c) 2016 AlertAvert.com. All rights reserved.
// Created by M. Massenzio (marco@alertavert.com) on 10/8/16.


#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>
#include <csignal>
#include <thread>

#include <glog/logging.h>
#include <google/protobuf/util/json_util.h>

#include "swim.pb.h"

#include "apiserver/api/rest/ApiServer.hpp"
#include "distlib/utils/ParseArgs.hpp"
#include "swim/GossipFailureDetector.hpp"


using namespace std;
using namespace swim;

namespace {

const unsigned int kDefaultHttpPort = 30396;

/**
 * Prints out usage instructions for this application.
 */
void usage(const string &progname) {
  std::cout << "Usage: " << progname << R"( --seeds=SEEDS_LIST [--port=PORT]
    [--timeout=TIMEOUT] [--ping=PING_SEC] [--http [--http-port=HTTP_PORT]]
    [--grace-period=GRACE_PERIOD]
    [--debug] [--version] [--help]

    --debug       verbose output (LOG_v = 2)
    --trace       trace output (LOG_v = 3)
                  Using either option will also cause the logs to be emitted to stdout,
                  otherwise the default Google Logs logging directory/files will be used.
    --help        prints this message and exits;
    --version     prints the version string for this demo and third-party libraries
                  and exits

    --http        whether this server should expose a REST API (true by default,
                  use --no-http to disable);

    PORT          an integer value specifying the TCP port the server will listen on,
                  if not specified, uses )" << kDefaultPort << R"( by default;

    HTTP_PORT     the HTTP port for the REST API, if server exposes it (see --http);
                  if not specified, uses )" << kDefaultHttpPort << R"( by default;

    TIMEOUT       in milliseconds, how long to wait for the server to respond to the ping

    GRACE_PERIOD  in seconds, how long to wait before evicting suspected servers

    PING_SEC      interval, in seconds, between pings to servers in the Gossip Circle

    SEEDS_LIST    a comma-separated list of host:port of peers that this server will
                  initially connect to, and part of the Gossip ring: from these "seeds"
                  the server will learn eventually of ALL the other servers and
                  connect to them too.
                  The `host` part may be either an IP address (such as 192.168.1.1) or
                  the DNS-resolvable `hostname`; for example:
                    192.168.1.101:8080,192.168.1.102:8081
                    node1.example.com:9999,node1.example.com:9999,node3.example.com:9999
                  Both host and port are required and no spaces must be left
                  between entries; the hosts may not ALL be active.

    The server will run forever in foreground, use Ctrl-C to terminate.
  )";
}


} // namespace

std::shared_ptr<GossipFailureDetector> detector;

void shutdown(int signum) {
  LOG(INFO) << "Terminating server...";
  if (detector) {
    detector->StopAllBackgroundThreads();
  }
  LOG(INFO) << "... cleanup complete, exiting now";
  exit(EXIT_SUCCESS);
}


int main(int argc, const char *argv[]) {
  GOOGLE_PROTOBUF_VERIFY_VERSION;
  google::InitGoogleLogging(argv[0]);

  ::utils::ParseArgs parser(argv, argc);
  if (parser.Enabled("debug") || parser.Enabled("trace")) {
    FLAGS_logtostderr = true;
    FLAGS_v = parser.Enabled("debug") ? 2 : 3;
  }

  utils::PrintVersion("SWIM Gossip Server Demo", RELEASE_STR);
  if (parser.has("version")) {
    return EXIT_SUCCESS;
  }

  if (parser.has("help")) {
    usage(parser.progname());
    return EXIT_SUCCESS;
  }

  // TODO: remove after demo
  cout << "PID: " << ::getpid() << endl;

  try {
    int port = parser.GetInt("port", ::kDefaultPort);
    if (port < 0 || port > 65535) {
      LOG(ERROR) << "Port number must be a positive integer, less than 65,535. "
                 << "Found: " << port;
      return EXIT_FAILURE;
    }
    LOG(INFO) << "Gossip Detector (PID: " << ::getpid()
              << ") listening on incoming TCP port " << port;

    long ping_timeout_msec = parser.GetInt("timeout", swim::kDefaultTimeoutMsec);
    long ping_interval_sec = parser.GetInt("ping", swim::kDefaultPingIntervalSec);
    long grace_period_sec = parser.GetInt("grace-period", swim::kDefaultGracePeriodSec);

    detector = std::make_shared<GossipFailureDetector>(
        port,
        ping_interval_sec,
        grace_period_sec,
        ping_timeout_msec);

    if (!parser.has("seeds")) {
      usage(parser.progname());
      LOG(ERROR) << "A list of peers (possibly just one) is required to start the Gossip Detector";
      return EXIT_FAILURE;
    }


    auto seedNames = ::utils::split(parser.Get("seeds"));
    LOG(INFO) << "Connecting to initial Gossip Circle: "
              << ::utils::Vec2Str(seedNames, ", ");

    std::for_each(seedNames.begin(), seedNames.end(),
                   [&](const std::string &name) {
                     try {
                       auto ipPort = ::utils::ParseHostPort(name);
                       string ip;
                       string host{std::get<0>(ipPort)};
                       if (::utils::ParseIp(host)) {
                         ip = host;
                       } else {
                         ip = ::utils::InetAddress(host);
                       }
                       Server server;
                       server.set_hostname(host);
                       server.set_port(std::get<1>(ipPort));
                       server.set_ip_addr(ip);

                       LOG(INFO) << "Adding neighbor: " << server;
                       detector->AddNeighbor(server);

                     } catch (::utils::parse_error& e) {
                       LOG(ERROR) << "Could not parse '" << name << "': " << e.what();
                     }
                   }
    );

    LOG(INFO) << "Waiting for server to start...";
    int waitCycles = 10;
    while (!detector->gossip_server().isRunning() && waitCycles-- > 0) {
      std::this_thread::sleep_for(seconds(1));
    }

    LOG(INFO) << "Gossip Server " << detector->gossip_server().self()
              << " is running. Starting all background threads";
    detector->InitAllBackgroundThreads();

    LOG(INFO) << "Threads started; detector process running"; // TODO: << PID?



    std::unique_ptr<api::rest::ApiServer> apiServer;
    if (parser.Enabled("http")) {
      if (parser.has("cors")) {
        LOG(INFO) << "+++++ Enabling CORS for domain(s): " << parser.Get("cors");
      }
      unsigned int httpPort = parser.getUInt("http-port", ::kDefaultHttpPort);
      LOG(INFO) << "Enabling HTTP REST API: http://"
                << utils::Hostname() << ":" << httpPort;
      apiServer = std::make_unique<api::rest::ApiServer>(httpPort);

      apiServer->AddGet("report", [&parser] (const api::rest::Request& request) {
        auto response = api::rest::Response::ok();
        auto report = detector->gossip_server().PrepareReport();
        std::string json_body;

        ::google::protobuf::util::MessageToJsonString(report, &json_body);
        response.set_body(json_body);
        if (parser.has("cors")) {
          response.AddHeader("Access-Control-Allow-Origin", parser.Get("cors"));
        }
        return response;
      });

      apiServer->AddPost("server", [](const api::rest::Request &request) {
        Server neighbor;

        auto status = ::google::protobuf::util::JsonStringToMessage(request.body(), &neighbor);
        if (status.ok()) {
          detector->AddNeighbor(neighbor);
          LOG(INFO) << "Added server " << neighbor;

          std::string body{R"({ "result": "added", "server": )"};
          std::string server;
          ::google::protobuf::util::MessageToJsonString(neighbor, &server);
          auto response = api::rest::Response::created("/server/" + neighbor.hostname());
          response.set_body(body + server + "}");
          return response;
        }

        LOG(ERROR) << "Not valid JSON: " << request.body();
        return api::rest::Response::bad_request("Not a valid JSON representation of a server: "
            + request.body());
      });
      LOG(INFO) << "Starting API Server";
      apiServer->Start();
      LOG(INFO) << ">>>Done";
    } else {
      LOG(INFO) << "REST API will not be available";
    }

    // "trap" the SIGINT (Ctrl-C) and execute a graceful exit
    signal(SIGINT, shutdown);
    while (true) {
      std::this_thread::sleep_for(milliseconds(300));
    }

  } catch (::utils::parse_error& error) {
    LOG(ERROR) << "A parsing error occurred: " << error.what();
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

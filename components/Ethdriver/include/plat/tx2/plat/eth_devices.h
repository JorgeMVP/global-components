/*
 * Copyright 2019, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DATA61_GPL)
 */
#pragma once

#include <camkes-BPMPServer.h>
#include <camkes-ClockServer.h>
#include <camkes-ResetServer.h>

#define HARDWARE_ETHERNET_EXTRA_IMPORTS             \
    import <BPMP.idl4>;                             \
    import <BPMPServer/BPMPServer.camkes>;          \
    import <ClockServer/ClockServer.camkes>;        \
    import <ResetServer/ResetServer.camkes>;        \
    import <GPIOMUXServer/GPIOMUXServer.camkes>;

#define HARDWARE_ETHERNET_COMPONENT

#define HARDWARE_ETHERNET_INTERFACES    \
    consumes Dummy EthDriver;           \
    emits Dummy dummy_source;           \
    ClockServer_client_interfaces(clock)\
    ResetServer_client_interfaces(reset)\
    uses GPIO gpio;


#define HARDWARE_ETHERNET_COMPOSITION                                                   \
    component BPMPServer bpmp_server;                                                   \
    component ClockServer clock_server;                                                 \
    component ResetServer reset_server;                                                 \
    component GPIOMUXServer gpiomux_server;                                             \
    connection seL4DTBHardware ethdriver_conn(from dummy_source, to EthDriver);         \
    BPMPServer_client_connections(bpmp, clock_server, the_bpmp, bpmp_server)            \
    BPMPServer_client_connections(bpmp, reset_server, the_bpmp, bpmp_server)            \
    ClockServer_client_connections_embedded(clock, the_clock, clock_server) \
    ResetServer_client_connections_embedded(reset, the_reset, reset_server) \
    connection seL4RPCCall ethernet_gpio(from gpio, to gpiomux_server.the_gpio);



#define HARDWARE_ETHERNET_CONFIG                                \
    EthDriver.dtb = dtb({ "path" : "/ether_qos@2490000" });     \
    EthDriver.generate_interrupts = 1; \
    BPMPServer_client_configurations(bpmp, clock_server) \
    BPMPServer_client_configurations(bpmp, reset_server) \
    ClockServer_client_configurations_embedded(clock) \
    ResetServer_client_configurations_embedded(reset)
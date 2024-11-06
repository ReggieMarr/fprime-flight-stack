// ======================================================================
// \title  Main.cpp
// \brief main program for the F' application. Intended for CLI-based systems (Linux, macOS)
//
// ======================================================================
// Used to access topology functions
#include <obcA/Top/obcATopology.hpp>
// Used for signal handling shutdown
#include <signal.h>
// Used for command line argument processing
#include <getopt.h>
// Used for printf functions
#include <cstdlib>

#include <cstdio>
#include <cstring>
#include <ctype.h>
#include <cstdlib>

// Used to get the Os::Console
#include <Os/Os.hpp>
#include <Os/Console.hpp>

/**
 * \brief print command line help message
 *
 * This will print a command line help message including the available command line arguments.
 *
 * @param app: name of application
 */
void print_usage(const char* app) {
    (void) printf("Usage: ./%s [options]\n"
                  "-p, --persist\t\tstay up regardless of component failure\n"
                  "-d, --downlink PORT\tset downlink port\n"
                  "-u, --uplink PORT\tset uplink port\n"
                  "-a, --address HOST\tset hostname/IP address\n"
                  "-h, --help\t\tshow this help message\n", app);
}

enum {
    EXIT_CODE_OK = 0,
    EXIT_CODE_STARTUP_FAILURE,
};
static volatile int EXIT_RET = EXIT_CODE_OK;

/**
 * \brief shutdown topology cycling on signal
 *
 * The reference topology allows for a simulated cycling of the rate groups. This simulated cycling needs to be stopped
 * in order for the program to shutdown. This is done via handling signals such that it is performed via Ctrl-C
 *
 * @param signum
 */
static void signalHandler(int signum) {
    obcA::stopSimulatedCycle();
}

/**
 * \brief execute the program
 *
 * This F´ program is designed to run in standard environments (e.g. Linux/macOs running on a laptop). Thus it uses
 * command line inputs to specify how to connect.
 *
 * @param argc: argument count supplied to program
 * @param argv: argument values supplied to program
 * @return: 0 on success, something else on failure
 */
int main(int argc, char* argv[]) {
    U32 uplink_port = 0; // Invalid port number forced
    U32 downlink_port = 0; // Invalid port number forced
    I32 option;
    char* hostname;
    option = 0;
    hostname = nullptr;

    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"downlink", required_argument, 0, 'd'},
        {"uplink", required_argument, 0, 'u'},
        {"address", required_argument, 0, 'a'},
        {0, 0, 0, 0}
    };

    int option_index = 0;
    while ((option = getopt_long(argc, argv, "hd:u:a:p", long_options, &option_index)) != -1) {
        switch(option) {
            case 'h':
                print_usage(argv[0]);
                return 0;
            case 'd':
                downlink_port = static_cast<U32>(atoi(optarg));
                break;
            case 'u':
                uplink_port = static_cast<U32>(atoi(optarg));
                break;
            case 'a':
                hostname = optarg;
                break;
            case '?':
            default:
                EXIT_RET = EXIT_CODE_STARTUP_FAILURE;
        }
    }

    // Check if required variables are set
    if (EXIT_RET != EXIT_CODE_OK || !hostname || uplink_port == 0 || downlink_port == 0) {
        fprintf(stderr, "Missing required parameters. Please provide all required options.\n");
        print_usage(argv[0]);
        return EXIT_RET;
    }

    Os::Console::init();

    // Object for communicating state to the reference topology
    obcA::TopologyState state(hostname, uplink_port, downlink_port);

    // Setup program shutdown via Ctrl-C
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    (void)printf("Hit Ctrl-C to quit\n");

    // Setup, cycle, and teardown topology
    obcA::setupTopology(state);
    obcA::startSimulatedCycle(Fw::TimeInterval(1, 0));  // Program loop cycling rate groups at 1Hz
    obcA::teardownTopology(state);
    (void)printf("Exiting...\n");
    return 0;
}

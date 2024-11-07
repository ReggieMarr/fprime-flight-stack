// ======================================================================
// \title  obcBTopology.cpp
// \brief cpp file containing the topology instantiation code
//
// ======================================================================
// Provides access to autocoded functions
#include <obcB/Top/obcBTopologyAc.hpp>
#include <obcB/Top/obcBPacketsAc.hpp>
#include <obcB/Top/obcBTopologyDefs.hpp>
#include <obcB/Top/obcBTopology.hpp>

// Necessary project-specified types
#include <Fw/Types/MallocAllocator.hpp>
#include <Os/Console.hpp>
#include <Svc/FramingProtocol/FprimeProtocol.hpp>

// Used for 1Hz synthetic cycling
#include <Os/Mutex.hpp>

// Allows easy reference to objects in FPP/autocoder required namespaces
using namespace obcB;

// Instantiate a system logger that will handle Fw::Logger::logMsg calls
Os::Console logger;

// The reference topology uses a malloc-based allocator for components that need to allocate memory during the
// initialization phase.
Fw::MallocAllocator mallocator;

// The reference topology uses the F´ packet protocol when communicating with the ground and therefore uses the F´
// framing and deframing implementations.
Svc::FprimeFraming framing;
Svc::FprimeDeframing deframing;
Svc::FprimeFraming hubFraming;
Svc::FprimeDeframing hubDeframing;
Svc::ComQueue::QueueConfigurationTable configurationTable;

// The reference topology divides the incoming clock signal (1Hz) into sub-signals: 1Hz, 1/2Hz, and 1/4Hz with 0 offset
Svc::RateGroupDriver::DividerSet rateGroupDivisorsSet{{{1, 0}, {2, 0}, {4, 0}}};

// Rate groups may supply a context token to each of the attached children whose purpose is set by the project. The
// reference topology sets each token to zero as these contexts are unused in this project.
NATIVE_INT_TYPE rateGroup1Context[Svc::ActiveRateGroup::CONNECTION_COUNT_MAX] = {};
NATIVE_INT_TYPE rateGroup2Context[Svc::ActiveRateGroup::CONNECTION_COUNT_MAX] = {};
NATIVE_INT_TYPE rateGroup3Context[Svc::ActiveRateGroup::CONNECTION_COUNT_MAX] = {};

// A number of constants are needed for construction of the topology. These are specified here.
enum TopologyConstants {
    CMD_SEQ_BUFFER_SIZE = 5 * 1024,
    FILE_DOWNLINK_TIMEOUT = 30000,
    FILE_DOWNLINK_COOLDOWN = 1000,
    FILE_DOWNLINK_CYCLE_TIME = 1000,
    FILE_DOWNLINK_FILE_QUEUE_DEPTH = 10,
    HEALTH_WATCHDOG_CODE = 0x123,
    COMM_PRIORITY = 100,
    // bufferManager constants
    FRAMER_BUFFER_SIZE = FW_MAX(FW_COM_BUFFER_MAX_SIZE, FW_FILE_BUFFER_MAX_SIZE + sizeof(U32)) + HASH_DIGEST_LENGTH + Svc::FpFrameHeader::SIZE,
    FRAMER_BUFFER_COUNT = 30,
    DEFRAMER_BUFFER_SIZE = FW_MAX(FW_COM_BUFFER_MAX_SIZE, FW_FILE_BUFFER_MAX_SIZE + sizeof(U32)),
    DEFRAMER_BUFFER_COUNT = 30,
    COM_DRIVER_BUFFER_SIZE = 3000,
    COM_DRIVER_BUFFER_COUNT = 30,
    BUFFER_MANAGER_ID = 200,
};

// Hub Constants
const char* REMOTE_HUB_IP_ADDRESS = "192.168.0.76";
const U32 REMOTE_HUB_PORT = 50500;

// Ping entries are autocoded, however; this code is not properly exported. Thus, it is copied here.
Svc::Health::PingEntry pingEntries[] = {
    {PingEntries::obcB_blockDrv::WARN, PingEntries::obcB_blockDrv::FATAL, "blockDrv"},
    {PingEntries::obcB_tlmSend::WARN, PingEntries::obcB_tlmSend::FATAL, "chanTlm"},
    {PingEntries::obcB_cmdDisp::WARN, PingEntries::obcB_cmdDisp::FATAL, "cmdDisp"},
    {PingEntries::obcB_cmdSeq::WARN, PingEntries::obcB_cmdSeq::FATAL, "cmdSeq"},
    {PingEntries::obcB_eventLogger::WARN, PingEntries::obcB_eventLogger::FATAL, "eventLogger"},
    {PingEntries::obcB_fileDownlink::WARN, PingEntries::obcB_fileDownlink::FATAL, "fileDownlink"},
    {PingEntries::obcB_fileManager::WARN, PingEntries::obcB_fileManager::FATAL, "fileManager"},
    {PingEntries::obcB_fileUplink::WARN, PingEntries::obcB_fileUplink::FATAL, "fileUplink"},
    {PingEntries::obcB_prmDb::WARN, PingEntries::obcB_prmDb::FATAL, "prmDb"},
    {PingEntries::obcB_rateGroup1::WARN, PingEntries::obcB_rateGroup1::FATAL, "rateGroup1"},
    {PingEntries::obcB_rateGroup2::WARN, PingEntries::obcB_rateGroup2::FATAL, "rateGroup2"},
    {PingEntries::obcB_rateGroup3::WARN, PingEntries::obcB_rateGroup3::FATAL, "rateGroup3"},
};

/**
 * \brief configure/setup components in project-specific way
 *
 * This is a *helper* function which configures/sets up each component requiring project specific input. This includes
 * allocating resources, passing-in arguments, etc. This function may be inlined into the topology setup function if
 * desired, but is extracted here for clarity.
 */
void configureTopology() {
    // Buffer managers need a configured set of buckets and an allocator used to allocate memory for those buckets.
    Svc::BufferManager::BufferBins upBuffMgrBins;
    memset(&upBuffMgrBins, 0, sizeof(upBuffMgrBins));
    upBuffMgrBins.bins[0].bufferSize = FRAMER_BUFFER_SIZE;
    upBuffMgrBins.bins[0].numBuffers = FRAMER_BUFFER_COUNT;
    upBuffMgrBins.bins[1].bufferSize = DEFRAMER_BUFFER_SIZE;
    upBuffMgrBins.bins[1].numBuffers = DEFRAMER_BUFFER_COUNT;
    upBuffMgrBins.bins[2].bufferSize = COM_DRIVER_BUFFER_SIZE;
    upBuffMgrBins.bins[2].numBuffers = COM_DRIVER_BUFFER_COUNT;
    bufferManager.setup(BUFFER_MANAGER_ID, 0, mallocator, upBuffMgrBins);
    
    // Framer and Deframer components need to be passed a protocol handler
    framer.setup(framing);
    deframer.setup(deframing);
    hubFramer.setup(hubFraming);
    hubDeframer.setup(hubDeframing);

    // Command sequencer needs to allocate memory to hold contents of command sequences
    // cmdSeq.allocateBuffer(0, mallocator, CMD_SEQ_BUFFER_SIZE);

    // Rate group driver needs a divisor list
    rateGroupDriver.configure(rateGroupDivisorsSet);

    // Rate groups require context arrays.
    rateGroup1.configure(rateGroup1Context, FW_NUM_ARRAY_ELEMENTS(rateGroup1Context));
    rateGroup2.configure(rateGroup2Context, FW_NUM_ARRAY_ELEMENTS(rateGroup2Context));
    rateGroup3.configure(rateGroup3Context, FW_NUM_ARRAY_ELEMENTS(rateGroup3Context));

    // File downlink requires some project-derived properties.
    fileDownlink.configure(FILE_DOWNLINK_TIMEOUT, FILE_DOWNLINK_COOLDOWN, FILE_DOWNLINK_CYCLE_TIME,
                           FILE_DOWNLINK_FILE_QUEUE_DEPTH);

    // Parameter database is configured with a database file name, and that file must be initially read.
    prmDb.configure("PrmDb.dat");
    prmDb.readParamFile();

    // Health is supplied a set of ping entires.
    health.setPingEntries(pingEntries, FW_NUM_ARRAY_ELEMENTS(pingEntries), HEALTH_WATCHDOG_CODE);

    // Note: Uncomment when using Svc:TlmPacketizer
    // tlmSend.setPacketList(obcBPacketsPkts, obcBPacketsIgnore, 1);

    // Events (highest-priority)
    configurationTable.entries[0] = {.depth = 100, .priority = 0};
    // Telemetry
    configurationTable.entries[1] = {.depth = 500, .priority = 2};
    // File Downlink
    configurationTable.entries[2] = {.depth = 100, .priority = 1};
    // Allocation identifier is 0 as the MallocAllocator discards it
    comQueue.configure(configurationTable, 0, mallocator);
    hubComQueue.configure(configurationTable, 0, mallocator);
}

// Public functions for use in main program are namespaced with deployment name obcB
namespace obcB {
void setupTopology(const TopologyState& state) {
    configureTopology();
    setup(state);
    // Initialize socket communication if and only if there is a valid specification
    if (state.hostName != nullptr && state.uplinkPort != 0) {
        Os::TaskString name("ReceiveTask");
        // Uplink is configured for receive so a socket task is started
        comDriver.configure(state.hostName, state.uplinkPort);
        comDriver.start(name, true, COMM_PRIORITY, Default::STACK_SIZE);
    }

    hubComDriver.configure(REMOTE_HUB_IP_ADDRESS, REMOTE_HUB_PORT);
    Os::TaskString hubName("hub");
    hubComDriver.start(hubName, true, COMM_PRIORITY, Default::STACK_SIZE);
}

// Variables used for cycle simulation
Os::Mutex cycleLock;
volatile bool cycleFlag = true;

void startSimulatedCycle(Fw::TimeInterval interval) {
    cycleLock.lock();
    bool cycling = cycleFlag;
    cycleLock.unLock();

    // Main loop
    while (cycling) {
        obcB::blockDrv.callIsr();
        Os::Task::delay(interval);

        cycleLock.lock();
        cycling = cycleFlag;
        cycleLock.unLock();
    }
}

void stopSimulatedCycle() {
    cycleLock.lock();
    cycleFlag = false;
    cycleLock.unLock();
}

void teardownTopology(const TopologyState& state) {
    // Autocoded (active component) task clean-up. Functions provided by topology autocoder.
    stopTasks(state);
    freeThreads(state);

    // Other task clean-up.
    comDriver.stop();
    (void)comDriver.join();
    hubComDriver.stop();
    (void)hubComDriver.join();

    // Resource deallocation
    cmdSeq.deallocateBuffer(mallocator);
    bufferManager.cleanup();
}
};  // namespace obcB

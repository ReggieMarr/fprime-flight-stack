module obcB {

  # ----------------------------------------------------------------------
  # Symbolic constants for port numbers
  # ----------------------------------------------------------------------

    enum Ports_RateGroups {
      rateGroup1
      rateGroup2
      rateGroup3
    }

  topology obcB {

    # ----------------------------------------------------------------------
    # Instances used in the topology
    # ----------------------------------------------------------------------

    instance $health
    instance blockDrv
    instance tlmSend
    instance cmdDisp
    instance cmdSeq
    instance comDriver
    instance comQueue
    instance comStub
    instance deframer
    instance eventLogger
    instance fatalAdapter
    instance fatalHandler
    instance fileDownlink
    instance fileManager
    instance fileUplink
    instance bufferManager
    instance framer
    instance posixTime
    instance prmDb
    instance rateGroup1
    instance rateGroup2
    instance rateGroup3
    instance rateGroupDriver
    instance textLogger
    instance systemResources

    instance hub
    instance hubComDriver
    instance hubComStub
    instance hubComQueue
    instance hubDeframer
    instance hubFramer
    instance proxyGroundInterface
    instance proxySequencer

    # ----------------------------------------------------------------------
    # Pattern graph specifiers
    # ----------------------------------------------------------------------

    command connections instance cmdDisp

    # event connections instance eventLogger
    event connections instance hub

    param connections instance prmDb

    # telemetry connections instance tlmSend
    telemetry connections instance hub

    text event connections instance textLogger

    time connections instance posixTime

    health connections instance $health

    # ----------------------------------------------------------------------
    # Direct graph specifiers
    # ----------------------------------------------------------------------

    # connections Downlink {

    #   eventLogger.PktSend -> comQueue.comQueueIn[0]
    #   tlmSend.PktSend -> comQueue.comQueueIn[1]
    #   fileDownlink.bufferSendOut -> comQueue.buffQueueIn[0]

    #   comQueue.comQueueSend -> framer.comIn
    #   comQueue.buffQueueSend -> framer.bufferIn

    #   framer.framedAllocate -> bufferManager.bufferGetCallee
    #   framer.framedOut -> comStub.comDataIn
    #   framer.bufferDeallocate -> fileDownlink.bufferReturn

    #   comDriver.deallocate -> bufferManager.bufferSendIn
    #   comDriver.ready -> comStub.drvConnected

    #   comStub.comStatus -> framer.comStatusIn
    #   framer.comStatusOut -> comQueue.comStatusIn
    #   comStub.drvDataOut -> comDriver.$send

    # }

    connections FaultProtection {
      eventLogger.FatalAnnounce -> fatalHandler.FatalReceive
    }

    connections RateGroups {
      # Block driver
      blockDrv.CycleOut -> rateGroupDriver.CycleIn

      # Rate group 1
      rateGroupDriver.CycleOut[Ports_RateGroups.rateGroup1] -> rateGroup1.CycleIn
      # rateGroup1.RateGroupMemberOut[0] -> tlmSend.Run
      rateGroup1.RateGroupMemberOut[0] -> fileDownlink.Run
      rateGroup1.RateGroupMemberOut[1] -> systemResources.run

      # Rate group 2
      rateGroupDriver.CycleOut[Ports_RateGroups.rateGroup2] -> rateGroup2.CycleIn
      # rateGroup2.RateGroupMemberOut[0] -> cmdSeq.schedIn

      # Rate group 3
      rateGroupDriver.CycleOut[Ports_RateGroups.rateGroup3] -> rateGroup3.CycleIn
      rateGroup3.RateGroupMemberOut[0] -> $health.Run
      rateGroup3.RateGroupMemberOut[1] -> blockDrv.Sched
      rateGroup3.RateGroupMemberOut[2] -> bufferManager.schedIn
    }

    # connections Sequencer {
    #   cmdSeq.comCmdOut -> cmdDisp.seqCmdBuff
    #   cmdDisp.seqCmdStatus -> cmdSeq.cmdResponseIn
    # }

    # connections Uplink {

    #   comDriver.allocate -> bufferManager.bufferGetCallee
    #   comDriver.$recv -> comStub.drvDataIn
    #   comStub.comDataOut -> deframer.framedIn

    #   deframer.framedDeallocate -> bufferManager.bufferSendIn
    #   deframer.comOut -> cmdDisp.seqCmdBuff

    #   cmdDisp.seqCmdStatus -> deframer.cmdResponseIn

    #   deframer.bufferAllocate -> bufferManager.bufferGetCallee
    #   deframer.bufferOut -> fileUplink.bufferSendIn
    #   deframer.bufferDeallocate -> bufferManager.bufferSendIn
    #   fileUplink.bufferSendOut -> bufferManager.bufferSendIn
    # }

    connections obcB {
      # Add here connections to user-defined components
    }

    connections send_hub {
      hub.dataOut -> hubFramer.bufferIn
      hub.dataOutAllocate -> bufferManager.bufferGetCallee
      
      hubFramer.framedOut -> hubComDriver.$send
      hubFramer.bufferDeallocate -> bufferManager.bufferSendIn
      hubFramer.framedAllocate -> bufferManager.bufferGetCallee
      
      hubComDriver.deallocate -> bufferManager.bufferSendIn
    }

    connections recv_hub {
      hubComDriver.$recv -> hubDeframer.framedIn
      hubComDriver.allocate -> bufferManager.bufferGetCallee

      hubDeframer.bufferOut -> hub.dataIn
      hubDeframer.bufferAllocate -> bufferManager.bufferGetCallee
      hubDeframer.framedDeallocate -> bufferManager.bufferSendIn

      hub.dataInDeallocate -> bufferManager.bufferSendIn
    }

    connections hub {
      hub.portOut[0] -> proxyGroundInterface.seqCmdBuf
      hub.portOut[1] -> proxySequencer.seqCmdBuf

      proxyGroundInterface.comCmdOut -> cmdDisp.seqCmdBuff
      proxySequencer.comCmdOut -> cmdDisp.seqCmdBuff
      
      cmdDisp.seqCmdStatus -> proxyGroundInterface.cmdResponseIn
      cmdDisp.seqCmdStatus -> proxySequencer.cmdResponseIn

      proxyGroundInterface.seqCmdStatus -> hub.portIn[0]
      proxySequencer.seqCmdStatus -> hub.portIn[1]

      hub.buffersOut -> bufferManager.bufferSendIn
    }

  }

}

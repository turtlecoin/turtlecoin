---
name: Release Candidate
about: The purpose of this template is to provide a reproducible testing process for
  new releases

---

# TurtleCoin <!-- Insert semver # including build number here --> Release Candidate Test Log

Build from: <!-- link to release candidate branch -->

Binaries for Testing:
  - Windows: <!-- link to release candidate binaries for this platform -->
  - Linux: <!-- link to release candidate binaries for this platform -->
  - MacOS: <!-- link to release candidate binaries for this platform -->
  - AARCH64: <!-- link to release candidate binaries for this platform -->

### Project
- [ ] Currently Passing Travis & AppVeyor build tests
- [ ] `Version.h.in` updated to correct semver value
- [ ] Release Notes Prepared (<!-- link to proposed copy of release notes -->)

### TurtleCoind Tests
- Daemon connects to local DB
  - [ ] Windows
  - [ ] Linux
  - [ ] MacOS
- Local DB does not require resync
  - [ ] Windows
  - [ ] Linux
  - [ ] MacOS
- Peer ID assigned
  - [ ] Windows
  - [ ] Linux
  - [ ] MacOS
- Connections to multiple peers are made
  - [ ] Windows
  - [ ] Linux
  - [ ] MacOS
- Daemon able to sync with external checkpoints
  - [ ] Windows
  - [ ] Linux
  - [ ] MacOS
- Daemon able to sync without external checkpoints
  - [ ] Windows
  - [ ] Linux
  - [ ] MacOS
- Daemon able to sync from 0 with external checkpoints
  - [ ] Windows
  - [ ] Linux
  - [ ] MacOS
- Daemon able to sync from 0 without external checkpoints
  - [ ] Windows
  - [ ] Linux
  - [ ] MacOS
- Daemon stays synchronized for 24 hours
  - [ ] Windows
  - [ ] Linux
  - [ ] MacOS
 
### ZedWallet Tests
- Connect to local daemon
  - [ ] Windows
  - [ ] Linux
  - [ ] MacOS
- Connect to a remote daemon
  - [ ] Windows
  - [ ] Linux
  - [ ] MacOS
- Send a transaction
  - [ ] Windows
  - [ ] Linux
  - [ ] MacOS
- Receive a transaction
  - [ ] Windows
  - [ ] Linux
  - [ ] MacOS
- Perform a fusion transaction
  - [ ] Windows
  - [ ] Linux
  - [ ] MacOS
- Export keys and seeds
  - [ ] Windows
  - [ ] Linux
  - [ ] MacOS
- Import from keys and seeds correctly
  - [ ] Windows
  - [ ] Linux
  - [ ] MacOS
- Perform a full reset
  - [ ] Windows
  - [ ] Linux
  - [ ] MacOS
- Integrated Addresses work
  - [ ] Windows
  - [ ] Linux
  - [ ] MacOS
- Scan From Height Works Correctly
  - [ ] Windows
  - [ ] Linux
  - [ ] MacOS

### Turtle-Service Tests
- Connect to local daemon
  - [ ] Windows
  - [ ] Linux
  - [ ] MacOS
- Connect to a remote daemon
  - [ ] Windows
  - [ ] Linux
  - [ ] MacOS
- Send a transaction
  - [ ] Windows
  - [ ] Linux
  - [ ] MacOS
- Receive a transaction
  - [ ] Windows
  - [ ] Linux
  - [ ] MacOS
- Perform a fusion transaction
  - [ ] Windows
  - [ ] Linux
  - [ ] MacOS
- Export keys and seeds
  - [ ] Windows
  - [ ] Linux
  - [ ] MacOS
- Import from keys and seeds correctly
  - [ ] Windows
  - [ ] Linux
  - [ ] MacOS
- Perform a full reset
  - [ ] Windows
  - [ ] Linux
  - [ ] MacOS
- Integrated Addresses work
  - [ ] Windows
  - [ ] Linux
  - [ ] MacOS
- All RPC endpoints and methods are operational
  - [ ] Windows
  - [ ] Linux
  - [ ] MacOS
- Scan From Height Works Correctly
  - [ ] Windows
  - [ ] Linux
  - [ ] MacOS

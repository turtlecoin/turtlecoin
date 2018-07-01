// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "ICryptoNoteProtocolQueryStub.h"

bool ICryptoNoteProtocolQueryStub::addObserver(CryptoNote::ICryptoNoteProtocolObserver* observer) {
  return false;
}

bool ICryptoNoteProtocolQueryStub::removeObserver(CryptoNote::ICryptoNoteProtocolObserver* observer) {
  return false;
}

uint32_t ICryptoNoteProtocolQueryStub::getObservedHeight() const {
  return observedHeight;
}

uint32_t ICryptoNoteProtocolQueryStub::getBlockchainHeight() const {
  return blockchainHeight;
}

size_t ICryptoNoteProtocolQueryStub::getPeerCount() const {
  return peers;
}

bool ICryptoNoteProtocolQueryStub::isSynchronized() const {
  return synchronized;
}

void ICryptoNoteProtocolQueryStub::setPeerCount(uint32_t count) {
  peers = count;
}

void ICryptoNoteProtocolQueryStub::setObservedHeight(uint32_t height) {
  observedHeight = height;
}

void ICryptoNoteProtocolQueryStub::setSynchronizedStatus(bool status) {
    synchronized = status;
}

// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#include <CryptoNote.h>

#include <Errors/Errors.h>

#include <Nigel/Nigel.h>

#include <Serialization/SerializationTools.h>

#include <SubWallets/SubWallets.h>

#include <vector>

#include <WalletTypes.h>

namespace SendTransaction
{
    std::tuple<Error, Crypto::Hash> sendFusionTransactionBasic(
        const std::shared_ptr<Nigel> daemon,
        const std::shared_ptr<SubWallets> subWallets);

    std::tuple<Error, Crypto::Hash> sendFusionTransactionAdvanced(
        const uint64_t mixin,
        const std::vector<std::string> addressesToTakeFrom,
        std::string destination,
        const std::shared_ptr<Nigel> daemon,
        const std::shared_ptr<SubWallets> subWallets);

    std::tuple<Error, Crypto::Hash> sendTransactionBasic(
        std::string destination,
        const uint64_t amount,
        std::string paymentID,
        const std::shared_ptr<Nigel> daemon,
        const std::shared_ptr<SubWallets> subWallets);

    std::tuple<Error, Crypto::Hash> sendTransactionAdvanced(
        std::vector<std::pair<std::string, uint64_t>> addressesAndAmounts,
        const uint64_t mixin,
        const uint64_t fee,
        std::string paymentID,
        const std::vector<std::string> addressesToTakeFrom,
        std::string changeAddress,
        const std::shared_ptr<Nigel> daemon,
        const std::shared_ptr<SubWallets> subWallets,
        const uint64_t unlockTime);

    std::vector<WalletTypes::TransactionDestination> setupDestinations(
        std::vector<std::pair<std::string, uint64_t>> addressesAndAmounts,
        const uint64_t changeRequired,
        const std::string changeAddress);

    std::tuple<Error, std::vector<WalletTypes::ObscuredInput>> prepareRingParticipants(
        std::vector<WalletTypes::TxInputAndOwner> sources,
        const uint64_t mixin,
        const std::shared_ptr<Nigel> daemon);

    std::tuple<Error, std::vector<CryptoNote::KeyInput>, std::vector<Crypto::SecretKey>> setupInputs(
        const std::vector<WalletTypes::ObscuredInput> inputsAndFakes,
        const Crypto::SecretKey privateViewKey);

    std::tuple<std::vector<WalletTypes::KeyOutput>, CryptoNote::KeyPair> setupOutputs(
        std::vector<WalletTypes::TransactionDestination> destinations);

    std::tuple<Error, CryptoNote::Transaction> generateRingSignatures(
        CryptoNote::Transaction tx,
        const std::vector<WalletTypes::ObscuredInput> inputsAndFakes,
        const std::vector<Crypto::SecretKey> tmpSecretKeys);

    std::vector<uint64_t> splitAmountIntoDenominations(uint64_t amount);

    std::vector<CryptoNote::TransactionInput> keyInputToTransactionInput(
        const std::vector<CryptoNote::KeyInput> keyInputs);

    std::vector<CryptoNote::TransactionOutput> keyOutputToTransactionOutput(
        const std::vector<WalletTypes::KeyOutput> keyOutputs);

    std::tuple<Error, std::vector<CryptoNote::RandomOuts>> getRingParticipants(
        const uint64_t mixin,
        const std::shared_ptr<Nigel> daemon,
        const std::vector<WalletTypes::TxInputAndOwner> sources);

    struct TransactionResult
    {
        /* The error, if any */
        Error error;

        /* The raw transaction */
        CryptoNote::Transaction transaction;

        /* The transaction outputs, before converted into boost uglyness, used
           for determining key inputs from the tx that belong to us */
        std::vector<WalletTypes::KeyOutput> outputs;
        
        /* The random key pair we generated */
        CryptoNote::KeyPair txKeyPair;
    };

    TransactionResult makeTransaction(
        const uint64_t mixin,
        const std::shared_ptr<Nigel> daemon,
        const std::vector<WalletTypes::TxInputAndOwner> ourInputs,
        const std::string paymentID,
        const std::vector<WalletTypes::TransactionDestination> destinations,
        const std::shared_ptr<SubWallets> subWallets,
        const uint64_t unlockTime);

    std::tuple<Error, Crypto::Hash> relayTransaction(
        const CryptoNote::Transaction tx,
        const std::shared_ptr<Nigel> daemon);

    std::tuple<CryptoNote::KeyPair, Crypto::KeyImage> genKeyImage(
        const WalletTypes::ObscuredInput input,
        const Crypto::SecretKey privateViewKey);

    void storeSentTransaction(
        const Crypto::Hash hash,
        const uint64_t fee,
        const std::string paymentID,
        const std::vector<WalletTypes::TxInputAndOwner> ourInputs,
        const std::string changeAddress,
        const uint64_t changeRequired,
        const std::shared_ptr<SubWallets> subWallets);

    Error isTransactionPayloadTooBig(
        const CryptoNote::Transaction tx,
        const uint64_t currentHeight);

    void storeUnconfirmedIncomingInputs(
        const std::shared_ptr<SubWallets> subWallets,
        const std::vector<WalletTypes::KeyOutput> keyOutputs,
        const Crypto::PublicKey txPublicKey,
        const Crypto::Hash txHash);

    /* Verify all amounts in the transaction given are PRETTY_AMOUNTS */
    bool verifyAmounts(const CryptoNote::Transaction tx);

    /* Verify all amounts given are PRETTY_AMOUNTS */
    bool verifyAmounts(const std::vector<uint64_t> amounts);

    /* Verify fee is as expected */
    bool verifyTransactionFee(const uint64_t expectedFee, CryptoNote::Transaction tx);

    /* Template so we can do transaction, and transactionprefix */
    template<typename T>
    Crypto::Hash getTransactionHash(T tx)
    {
        std::vector<uint8_t> data = toBinaryArray(tx);
        return Crypto::cn_fast_hash(data.data(), data.size());
    }
}

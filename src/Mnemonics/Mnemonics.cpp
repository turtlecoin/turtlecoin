//
// Copyright 2014-2018 The Monero Developers
// Copyright 2018 The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include <algorithm>
#include <boost/crc.hpp>

#include "Mnemonics.h"
#include "WordList.h"

namespace Mnemonics
{
    std::tuple<std::string, Crypto::SecretKey> MnemonicToPrivateKey(const std::string words)
    {
        std::vector<std::string> wordsList;

        std::istringstream stream(words);

        /* Convert whitespace separated string into vector of words */
        for(std::string s; stream >> s;)
        {
            wordsList.push_back(s);
        }

        return MnemonicToPrivateKey(wordsList);
    }

    /* Note - if the returned string is not empty, it is an error message, and
       the returned secret key is not initialized. */
    std::tuple<std::string, Crypto::SecretKey> MnemonicToPrivateKey(const std::vector<std::string> words)
    {
        Crypto::SecretKey key;

        const int len = words.size();

        /* Mnemonics must be 25 words long */
        if (len != 25)
        {
            std::stringstream str;

            const std::string words = len == 1 ? "word" : "words";

            str << "Mnemonic seed is wrong length - It should be 25 words "
                << "long, but it is " << len << " " << words << " long!";

            return std::make_tuple(str.str(), key);
        }

        /* All words must be present in the word list */
        for (const auto &word : words)
        {
            if (std::find(WordList::English.begin(),
                          WordList::English.end(), word) == WordList::English.end())
            {
                std::stringstream str;

                str << "Mnemonic seed has invalid word - "
                    << word << " is not in the English word list!";

                return std::make_tuple(str.str(), key);
            }
        }

        /* The checksum must be correct */
        if (!HasValidChecksum(words))
        {
            return std::make_tuple("Mnemonic seed has incorrect checksum!",
                                   key);
        }

        auto wordIndexes = GetWordIndexes(words);

        std::vector<uint8_t> data;

        for (size_t i = 0; i < words.size() - 1; i += 3)
        {
            /* Take the indexes of these three words in the word list */
            const uint32_t w1 = wordIndexes[i];
            const uint32_t w2 = wordIndexes[i + 1];
            const uint32_t w3 = wordIndexes[i + 2];

            /* Word list length */
            const uint32_t wlLen = WordList::English.size();

            /* no idea what this does lol */
            const uint32_t val = w1 + wlLen * (((wlLen - w1) + w2) % wlLen) + wlLen 
                                            * wlLen * (((wlLen - w2) + w3) % wlLen);

            /* Don't know what this is testing either */
            if (!(val % wlLen == w1))
            {
                return std::make_tuple("Mnemonic seed is invalid!", key);
            }

            /* Interpret val as 4 uint8_t's */
            const auto ptr = reinterpret_cast<const uint8_t *>(&val);

            /* Append to private key */
            for (int j = 0; j < 4; j++)
            {
                data.push_back(ptr[j]);
            }
        }

        /* Copy the data to the secret key */
        std::copy(data.begin(), data.end(), key.data);

        return std::make_tuple(std::string(), key);
    }

    std::string PrivateKeyToMnemonic(const Crypto::SecretKey privateKey)
    {
        std::vector<std::string> words;

        for (int i = 0; i < 32 - 1; i += 4)
        {
            /* Read the array as a uint32_t array */
            auto ptr = (uint32_t *)&privateKey.data[i];

            /* Take the first element of the array (since we have already 
               done the offset */
            const uint32_t val = ptr[0];

            uint32_t wlLen = WordList::English.size();

            const uint32_t w1 = val % wlLen;
            const uint32_t w2 = ((val / wlLen) + w1) % wlLen;
            const uint32_t w3 = (((val / wlLen) / wlLen) + w2) % wlLen;

            words.push_back(WordList::English[w1]);
            words.push_back(WordList::English[w2]);
            words.push_back(WordList::English[w3]);
        }

        words.push_back(GetChecksumWord(words));

        std::string result;

        for (auto it = words.begin(); it != words.end(); it++)
        {
            if (it != words.begin())
            {
                result += " ";
            }

            result += *it;
        }

        return result;
    }

    /* Assumes the input is 25 words long */
    bool HasValidChecksum(const std::vector<std::string> words)
    {
        /* Make a copy since erase() is mutating */
        auto wordsNoChecksum = words;

        /* Remove the last checksum word */
        wordsNoChecksum.erase(wordsNoChecksum.end());

        /* Assert the last word (the checksum word) is equal to the derived
           checksum */
        return words[words.size() - 1] == GetChecksumWord(wordsNoChecksum);
    }

    std::string GetChecksumWord(const std::vector<std::string> words)
    {
        std::string trimmed;

        /* Take the first 3 char from each of the 24 words */
        for (const auto &word : words)
        {
            trimmed += word.substr(0, 3);
        }

        boost::crc_32_type crc32;
        crc32.process_bytes(trimmed.data(), trimmed.length());

        return words[crc32.checksum() % words.size()];
    }

    std::vector<int> GetWordIndexes(const std::vector<std::string> words)
    {
        std::vector<int> result;

        for (const auto &word : words)
        {
            /* Find the iterator of our word in the wordlist */
            const auto it = std::find(WordList::English.begin(),
                                      WordList::English.end(), word);

            /* Take it away from the beginning of the vector, giving us the
               index of the item in the vector */
            result.push_back(std::distance(WordList::English.begin(), it));
        }

        return result;
    }
}

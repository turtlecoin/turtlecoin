// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <string>

namespace Common
{

    std::string NativePathToGeneric(const std::string &nativePath);

    std::string GetPathDirectory(const std::string &path);

    std::string GetPathFilename(const std::string &path);

    void SplitPath(const std::string &path, std::string &directory, std::string &filename);

    std::string CombinePath(const std::string &path1, const std::string &path2);

    std::string GetExtension(const std::string &path);

    std::string RemoveExtension(const std::string &path);

    std::string ReplaceExtenstion(const std::string &path, const std::string &extension);

    bool HasParentPath(const std::string &path);

}

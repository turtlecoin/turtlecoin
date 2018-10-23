// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information

#pragma once

const std::string windowsAsciiArt =
      "\n _______         _   _       _____      _        \n"
              "|__   __|       | | | |     / ____|    (_)      \n"
              "   | |_   _ _ __| |_| | ___| |     ___  _ _ __  \n"
              "   | | | | | '__| __| |/ _ \\ |    / _ \\| | '_ \\ \n"
              "   | | |_| | |  | |_| |  __/ |___| (_) | | | | |\n"
      "   |_|\\__ _|_|   \\__|_|\\___|\\_____\\___/|_|_| |_|\n";

const std::string nonWindowsAsciiArt = 
      "\n                                                                            \n"
        "████████╗██╗  ██╗██████╗ ████████╗██╗    ██████╗ █████╗ █████╗ ██╗███╗   ██╗\n"
        "╚══██╔══╝██║  ██║██╔══██╗╚══██╔══╝██║    ██╔═══╝██╔═══╝██╔══██╗██║████╗  ██║\n"
        "   ██║   ██║  ██║██████╔╝   ██║   ██║    ████╗  ██║    ██║  ██║██║██╔██╗ ██║\n"
        "   ██║   ██║  ██║██╔══██╗   ██║   ██║    ██╔═╝  ██║    ██║  ██║██║██║╚██╗██║\n"
        "   ██║   ╚█████╔╝██║  ██║   ██║   ██████╗██████╗╚█████╗╚█████╔╝██║██║ ╚████║\n"
        "   ╚═╝    ╚════╝ ╚═╝  ╚═╝   ╚═╝   ╚═════╝╚═════╝ ╚════╝ ╚════╝ ╚═╝╚═╝  ╚═══╝\n";

/* Windows has some characters it won't display in a terminal. If your ascii
   art works fine on Windows and Linux terminals, just replace 'asciiArt' with
   the art itself, and remove these two #ifdefs and above ascii arts */
#ifdef _WIN32
const std::string asciiArt = windowsAsciiArt;
#else
const std::string asciiArt = nonWindowsAsciiArt;
#endif

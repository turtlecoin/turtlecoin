// Copyright (c) 2018, The TurtleCoin Developers
//
// Copyright (c) 2018, Nibble Developers
// Please see the included LICENSE file for more information

#pragma once

const std::string windowsAsciiArt =
      "\n | \ | (_) |   | |   | |      /  __ \ |             (_)  \n"
         "|  \| |_| |__ | |__ | | ___  | /  \/ | __ _ ___ ___ _  ___ \n"
        " | . ` | | '_ \| '_ \| |/ _ \ | |   | |/ _` / __/ __| |/ __|\n"
        " | |\  | | |_) | |_) | |  __/ | \__/\ | (_| \__ \__ \ | (__    \n"
        " \_| \_/_|_.__/|_.__/|_|\___|  \____/_|\__,_|___/___/_|\___| \n"
      "\n";

const std::string nonWindowsAsciiArt = 
      "\n | \ | (_) |   | |   | |      /  __ \ |             (_)    \n"
        " |  \| |_| |__ | |__ | | ___  | /  \/ | __ _ ___ ___ _  ___\n"
        " | . ` | | '_ \| '_ \| |/ _ \ | |   | |/ _` / __/ __| |/ __| \n"
        " | |\  | | |_) | |_) | |  __/ | \__/\ | (_| \__ \__ \ | (__ \n"
        " \_| \_/_|_.__/|_.__/|_|\___|  \____/_|\__,_|___/___/_|\___|\n"
        " \n";

/* Windows has some characters it won't display in a terminal. If your ascii
   art works fine on Windows and Linux terminals, just replace 'asciiArt' with
   the art itself, and remove these two #ifdefs and above ascii arts */
#ifdef _WIN32
const std::string asciiArt = windowsAsciiArt;
#else
const std::string asciiArt = nonWindowsAsciiArt;
#endif

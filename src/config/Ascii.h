// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information

#pragma once

const std::string windowsAsciiArt =


	"\n	  ____   _  _               _      _______  _  \n"            
		"|  _ \ (_)| |             (_)    |__   __|(_)                   \n"
		"| |_) | _ | |_  ___  ___   _  _ __  | |    _   __ _   ___  _ __ \n"
		"|  _ < | || __|/ __|/ _ \ | || '_ \ | |   | | / _` | / _ \| '__|\n"
		"| |_) || || |_| (__| (_) || || | | || |   | || (_| ||  __/| |   \n"
		"|____/ |_| \__|\___|\___/ |_||_| |_||_|   |_| \__, | \___||_|   \n"
													  "__/ |            \n"
													  "|___/             \n";



const std::string nonWindowsAsciiArt = 
   
	"\n	  ____   _  _               _      _______  _  \n"            
		"|  _ \ (_)| |             (_)    |__   __|(_)                   \n"
		"| |_) | _ | |_  ___  ___   _  _ __  | |    _   __ _   ___  _ __ \n"
		"|  _ < | || __|/ __|/ _ \ | || '_ \ | |   | | / _` | / _ \| '__|\n"
		"| |_) || || |_| (__| (_) || || | | || |   | || (_| ||  __/| |   \n"
		"|____/ |_| \__|\___|\___/ |_||_| |_||_|   |_| \__, | \___||_|   \n"
													  "__/ |            \n"
													  "|___/             \n";
													  
													  
/* Windows has some characters it won't display in a terminal. If your ascii
   art works fine on Windows and Linux terminals, just replace 'asciiArt' with
   the art itself, and remove these two #ifdefs and above ascii arts */
#ifdef _WIN32
const std::string asciiArt = windowsAsciiArt;
#else
const std::string asciiArt = nonWindowsAsciiArt;
#endif

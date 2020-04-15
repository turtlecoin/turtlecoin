// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information

#pragma once

#include <string>

const std::string windowsAsciiArt = 

 

 ___________ _____ _____ _____  _   _ _____  ___   _   _ 
|___  /  _  \  _  /  ___/  __ \| | | |_   _|/ _ \ | \ | |
   / /| | | | | | \ `--.| /  \/| |_| | | | / /_\ \|  \| |
  / / | | | | | | |`--. \ |    |  _  | | | |  _  || . ` |
./ /__| |/ /\ \_/ /\__/ / \__/\| | | |_| |_| | | || |\  |
\_____/___/  \___/\____/ \____/\_| |_/\___/\_| |_/\_| \_/
                                                         
                                                         

                              
                                                                        
                                                                        



const std::string nonWindowsAsciiArt =
 

 
                    | | |       | (_)                                   
 _ __ ___   __ _  __| | | ____ _| |_                                    
| '_ ` _ \ / _` |/ _` | |/ / _` | | |                                   
| | | | | | (_| | (_| |   < (_| | | |                                   
|_| |_| |_|\__,_|\__,_|_|\_\__,_|_|_|                                   
                                                                        
                                                                        


/* Windows has some characters it won't display in a terminal. If your ascii
   art works fine on Windows and Linux terminals, just replace 'asciiArt' with
   the art itself, and remove these two #ifdefs and above ascii arts */
#ifdef _WIN32

const std::string asciiArt = windowsAsciiArt;

#else
const std::string asciiArt = nonWindowsAsciiArt;
#endif

// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <common/ConsoleTools.h>
#include <iomanip>
#include <ostream>
#include <string>

template<typename T> class ColouredMsg
{
  public:
    ColouredMsg(const T msg, const Common::Console::Color colour): msg(msg), colour(colour) {}

    ColouredMsg(const T msg, const int padding, const Common::Console::Color colour):
        msg(msg),
        colour(colour),
        padding(padding),
        pad(true)
    {
    }

    /* Set the text colour, write the message, then reset. We use a class
       as it seems the only way to have a valid << operator. We need this
       so we can nicely do something like:

       std::cout << "Hello " << GreenMsg("user") << std::endl;

       Without having to write:

       std::cout << "Hello ";
       GreenMsg("user");
       std::cout << std::endl; */

    friend std::ostream &operator<<(std::ostream &os, const ColouredMsg &m)
    {
        Common::Console::setTextColor(m.colour);

        if (m.pad)
        {
            os << std::left << std::setw(m.padding) << m.msg;
        }
        else
        {
            os << m.msg;
        }

        Common::Console::setTextColor(Common::Console::Color::Default);
        return os;
    }

  protected:
    /* Can be any class that supports the << operator */
    T msg;

    /* The colour to use */
    const Common::Console::Color colour;

    /* The amount to pad the message to */
    const int padding = 0;

    /* If we should pad the message */
    const bool pad = false;
};

template<typename T> class SuccessMsg : public ColouredMsg<T>
{
  public:
    explicit SuccessMsg(T msg): ColouredMsg<T>(msg, Common::Console::Color::Green) {}

    explicit SuccessMsg(T msg, int padding): ColouredMsg<T>(msg, padding, Common::Console::Color::Green) {}
};

template<typename T> class InformationMsg : public ColouredMsg<T>
{
  public:
    explicit InformationMsg(T msg): ColouredMsg<T>(msg, Common::Console::Color::BrightYellow) {}

    explicit InformationMsg(T msg, int padding): ColouredMsg<T>(msg, padding, Common::Console::Color::BrightYellow) {}
};

template<typename T> class WarningMsg : public ColouredMsg<T>
{
  public:
    explicit WarningMsg(T msg): ColouredMsg<T>(msg, Common::Console::Color::BrightRed) {}

    explicit WarningMsg(T msg, int padding): ColouredMsg<T>(msg, padding, Common::Console::Color::BrightRed) {}
};

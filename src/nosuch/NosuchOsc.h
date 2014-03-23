#ifndef _NOSUCH_OSC_H
#define _NOSUCH_OSC_H

#include "osc/OscReceivedElements.h"
#include <string>
#include "NosuchOscInput.h"

int ArgAsInt32(const osc::ReceivedMessage& m, unsigned int n);
float ArgAsFloat(const osc::ReceivedMessage& m, unsigned int n);
std::string ArgAsString(const osc::ReceivedMessage& m, unsigned n);

#endif

/*
 *  Copyright (C) 2023 Bastian Stahmer
 *
 *  This file is part of ha-stiebel-control.
 *  ha-stiebel-control is free software: : you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program. If not, see http://www.gnu.org/licenses/ .
 */

#if !defined(heatingpump_H)
#define heatingpump_H
#include "ElsterTable.h"
#include "KElsterTable.h"
#include "CanMessageMqttLogger.h"

typedef struct
{
  const char *Name;
  uint32_t CanId;
  uint8_t ReadId[2];
  uint8_t WriteId[2];
  uint8_t ConfirmationId[2];
} CanMember;

/*
#################################################################
#Stiebel Eltron WPL 13 E (2013)
#WPM3i software version 391-08
#FEK software version 416 - 02
#################################################################
#WPL 13 E: CAN ID 180: read - 3100, write - 3000 # Pump
#WPL 13 E: CAN ID 500: read - A100, write - A000 # Heating Module
#WPL 13 E: CAN ID 480: read - 9100, write - 9000 # Manager

#OLD: CAN ID 301: read - 0c01, FEK-device (no active can request, only listening)
#
#OLD: other addresses
#OLD:   180 read: 3100  write: 3000
#OLD:  	301	read: 6101  write: 6001
#OLD: 	480	read: 9100  write: 9000    WMPme Wärmepumpenmanager
#OLD: 	601	read: C101  write: C001
#OLD: 	680	confirmation: D200
#################################################################
*/

static const CanMember CanMembers[] =
    {
        //  Name              CanId     ReadId          WriteId         ConfirmationID
        {"ESPCLIENT", 0x680, {0x00, 0x00}, {0x00, 0x00}, {0xE2, 0x00}}, // The ESP Home Client, thus no valid read/write IDs
        {"PUMP", 0x180, {0x31, 0x00}, {0x30, 0x00}, {0x00, 0x00}},
        {"FE7X", 0x301, {0x61, 0x01}, {0x00, 0x00}, {0x00, 0x00}},
        {"FEK", 0x302, {0x61, 0x02}, {0x00, 0x00}, {0x00, 0x00}},
        {"MANAGER", 0x480, {0x91, 0x00}, {0x90, 0x00}, {0x00, 0x00}},
        {"FE7", 0x602, {0xC1, 0x02}, {0x00, 0x00}, {0x00, 0x00}}};

typedef enum
{
  // Die Reihenfolge muss mit CanMembers übereinstimmen!
  cm_espclient = 0,
  cm_pump,
  cm_fe7x,
  cm_fek,
  cm_manager,
  cm_fe7,
} CanMemberType;

const ElsterIndex *processCanMessage(unsigned short can_id, std::string &signalValue, std::vector<unsigned char> msg)
{
    // Return if the message is too small
    if (msg.size() < 7)
    {
        ESP_LOGW("processCanMessage()", "CAN message too short: %d bytes", (int)msg.size());
        return &ElsterTable[0];
    }
    
    // Enhanced validation based on Jürg's Stiebel-Eltron protocol specs
    // Check if this looks like a valid Stiebel-Eltron message format
    if ((msg[0] != 0xa0 && msg[0] != 0xa1 && msg[0] != 0x60 && msg[0] != 0x61) ||
        (msg[1] != 0x00 && msg[1] != 0x01 && msg[1] != 0x72 && msg[1] != 0x73 && msg[1] != 0x79 && msg[1] != 0x08 && msg[1] != 0xa0 && msg[1] != 0xa1)) {
        ESP_LOGD("processCanMessage()", "Possibly non-Stiebel message format: %02x %02x %02x %02x %02x %02x %02x",
                 msg[0], msg[1], msg[2], msg[3], msg[4], msg[5], msg[6]);
    }

  const ElsterIndex *ei;
  unsigned char byte1;
  unsigned char byte2;
  char charValue[16];

  if (int(msg[2]) == 0xfa)
  {
    byte1 = msg[5];
    byte2 = msg[6];
    ei = GetElsterIndex(int((msg[4]) + ((msg[3]) << 8)));
  }
  else
  {
    byte1 = msg[3];
    byte2 = msg[4];
    ei = GetElsterIndex(int(msg[2]));
  }

    // Fix signed integer handling for older devices - Jürg's protocol uses signed 16-bit values
    int rawValue = int(byte2 + (byte1 << 8));
    // Convert to signed 16-bit if needed (for negative temperatures, etc.)
    if (rawValue > 32767) {
        rawValue = rawValue - 65536;  // Convert unsigned to signed 16-bit
    }
    
    switch (ei->Type)
    {
    case et_double_val:
        SetDoubleType(charValue, ei->Type, double(rawValue));
        break;
    case et_triple_val:
        SetDoubleType(charValue, ei->Type, double(rawValue));
        break;
    default:
        SetValueType(charValue, ei->Type, rawValue);
        break;
    }

    // Enhanced logging for older device compatibility (based on Jürg's work)
    if (ei->Index == 0x0000) {
        unsigned short unknownIndex;
        if (int(msg[2]) == 0xfa) {
            unknownIndex = int((msg[4]) + ((msg[3]) << 8));
        } else {
            unknownIndex = int(msg[2]);
        }
        ESP_LOGW("processCanMessage()", "%d:\tUNKNOWN_INDEX_0x%04X:\t%s\t(raw: %02x %02x %02x %02x %02x %02x %02x)",
                 can_id, unknownIndex, charValue, msg[0], msg[1], msg[2], msg[3], msg[4], msg[5], msg[6]);
        
        // Log common unknown indices that might be from older devices needing ElsterTable updates
        if (unknownIndex == 0x3c || unknownIndex == 0xbe || unknownIndex == 0xf2 || 
            unknownIndex == 0x5f || unknownIndex == 0x56 || unknownIndex == 0x16) {
            ESP_LOGI("processCanMessage()", "Common older device index 0x%04X - consider updating ElsterTable", unknownIndex);
        }
    } else {
        ESP_LOGI("processCanMessage()", "%d:\t%s:\t%s\t(%s)", can_id, ei->EnglishName, charValue, ElsterTypeStr[ei->Type]);
        
        // Enhanced validation for older device values (Jürg's protocol ranges)
        double value = std::stod(charValue);
        if (strstr(ei->EnglishName, "TEMP")) {
            if (value < -50.0 || value > 150.0) {
                ESP_LOGW("processCanMessage()", "Temperature out of range for %s: %s (possible older device index mismatch)", 
                        ei->EnglishName, charValue);
            }
        } else if (strstr(ei->EnglishName, "ACTIVE") || strstr(ei->EnglishName, "STATUS")) {
            if (value != 0.0 && value != 1.0 && (value < -10 || value > 1000)) {
                ESP_LOGW("processCanMessage()", "Suspicious status value for %s: %s", ei->EnglishName, charValue);
            }
        }
    }

    signalValue = (std::string)charValue;
    
    // Publish to MQTT (non-blocking, failures won't affect CAN processing)
    publishCanMessageToMqtt(can_id, msg, ei, signalValue, byte1, byte2, rawValue);
    
    return ei;
}

void readSignal(const CanMember *member, const ElsterIndex *ei)
{
  bool use_extended_id = 0; // No use of extended ID
  uint8_t IndexByte1 = (uint8_t)(ei->Index >> 8);
  uint8_t IndexByte2 = (uint8_t)(ei->Index - ((ei->Index >> 8) << 8));
  std::vector<uint8_t> data;

  if (IndexByte1 == 0x00)
  {
    data.insert(data.end(), {member->ReadId[0],
                             member->ReadId[1],
                             IndexByte2,
                             0x00,
                             0x00,
                             0x00,
                             0x00});
  }
  else
  {
    data.insert(data.end(), {member->ReadId[0],
                             member->ReadId[1],
                             0xfa,
                             IndexByte1,
                             IndexByte2,
                             0x00,
                             0x00});
  }

  char logmsg[255];
  sprintf(logmsg, "READ \"%s\" (0x%04x) FROM %s (0x%02x {0x%02x, 0x%02x}): %02x, %02x, %02x, %02x, %02x, %02x, %02x", ei->EnglishName, ei->Index, member->Name, member->CanId, member->ReadId[0], member->ReadId[1], data[0], data[1], data[2], data[3], data[4], data[5], data[6]);
  ESP_LOGI("readSignal()", "%s", logmsg);

  id(my_mcp2515).send_data(CanMembers[cm_espclient].CanId, use_extended_id, data);

  return;
}

void writeSignal(const CanMember *member, const ElsterIndex *ei, const char *&str)
{
  bool use_extended_id = 0; // No use of extended ID
  int writeValue = TranslateString(str, ei->Type);
  uint8_t IndexByte1 = (uint8_t)(ei->Index >> 8);
  uint8_t IndexByte2 = (uint8_t)(ei->Index - ((ei->Index >> 8) << 8));
  std::vector<uint8_t> data;

  if (IndexByte1 == 0x00)
  {
    data.insert(data.end(), {member->WriteId[0],
                             member->WriteId[1],
                             IndexByte2,
                             ((uint8_t)(writeValue >> 8)),
                             ((uint8_t)(writeValue - ((writeValue >> 8) << 8))),
                             0x00,
                             0x00});
  }
  else
  {
    data.insert(data.end(), {member->WriteId[0],
                             member->WriteId[1],
                             0xfa,
                             IndexByte1,
                             IndexByte2,
                             ((uint8_t)(writeValue >> 8)),
                             ((uint8_t)(writeValue - ((writeValue >> 8) << 8)))});
  }

  char logmsg[120];
  sprintf(logmsg, "WRITE \"%s\" (0x%04x): \"%d\" TO: %s (0x%02x {0x%02x, 0x%02x}): %02x, %02x, %02x, %02x, %02x, %02x, %02x", ei->Name, ei->Index, writeValue, member->Name, member->CanId, member->ReadId[0], member->ReadId[1], data[0], data[1], data[2], data[3], data[4], data[5], data[6]);
  ESP_LOGI("writeSignal()", "%s", logmsg);

  id(my_mcp2515).send_data(CanMembers[cm_espclient].CanId, use_extended_id, data);

  return;
}

// void publishDate()
// {
//     int ijahr = (int)id(JAHR).state;
//     std::string jahr;
//     if (ijahr >= 0 & ijahr < 99)
//     {
//         if (ijahr < 10)
//             jahr = "0" + to_string(ijahr);
//         else
//             jahr = to_string(ijahr);
//     }
//     else
//     {
//         jahr = "00";
//     }

//     int imonat = (int)id(MONAT).state;
//     std::string monat;
//     if (imonat > 0 & imonat <= 12)
//     {
//         if (imonat < 10)
//             monat = "0" + to_string(imonat);
//         else
//             monat = to_string(imonat);
//     }
//     else
//     {
//         monat = "00";
//     }

//     int itag = (int)id(TAG).state;
//     std::string tag;
//     if (itag > 0 & itag <= 31)
//     {
//         if (itag < 10)
//             tag = "0" + to_string(itag);
//         else
//             tag = to_string(itag);
//     }
//     else
//     {
//         tag = "00";
//     }

//     id(DATUM).publish_state("20" + jahr + "-" + monat + "-" + tag);

//     return;

//     //              id(DATUM).publish_state("20" + to_string((int)id(JAHR).state) + "-" + to_string((int)id(MONAT).state) + "-" + to_string((int)id(TAG).state));
// }

// void publishTime()
// {
//     int istunde = (int)id(STUNDE).state;
//     std::string stunde;
//     if (istunde >= 0 & istunde < 60)
//     {
//         if (istunde < 10)
//             stunde = "0" + to_string(istunde);
//         else
//             stunde = to_string(istunde);
//     }
//     else
//     {
//         stunde = "00";
//     }

//     int iminute = (int)id(MINUTE).state;
//     std::string minute;
//     if (iminute >= 0 & iminute < 60)
//     {
//         if (iminute < 10)
//             minute = "0" + to_string(iminute);
//         else
//             minute = to_string(iminute);
//     }
//     else
//     {
//         minute = "00";
//     }

//     int isekunde = (int)id(SEKUNDE).state;
//     std::string sekunde;
//     if (isekunde >= 0 & isekunde < 60)
//     {
//         if (isekunde < 10)
//             sekunde = "0" + to_string(isekunde);
//         else
//             sekunde = to_string(isekunde);
//     }
//     else
//     {
//         sekunde = "00";
//     }

//     id(ZEIT).publish_state(stunde + ":" + minute + ":" + sekunde);
//     return;

//     //              id(DATUM).publish_state("20" + to_string((int)id(JAHR).state) + "-" + to_string((int)id(MONAT).state) + "-" + to_string((int)id(TAG).state));
// }

#endif

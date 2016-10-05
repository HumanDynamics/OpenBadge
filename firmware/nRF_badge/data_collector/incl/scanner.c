#include "scanner.h"



static unsigned short hexStringToShort(unsigned char * string)
{
    unsigned short result = 0;
    for (int place = 0; place < 4; place++)  {
        result *= 16;
        if (string[place] >= '0' && string[place] <= '9')  {
            result += string[place] - '0';
        }
        else if (string[place] >= 'A' && string[place] <= 'F')  {
            result += string[place] - 'A' + 0xA;
        }
        else if (string[place] >= 'a' && string[place] <= 'f')  {
            result += string[place] - 'a' + 0xa;
        }
        else return 0xffff;  // invalid string
    }
    return result;
}


//================================ Scanning + scan result storage ===============================
//===============================================================================================

void scanner_init()
{   
    scan_params.active = 0;  //passive scanning, only looking for advertising packets
    scan_params.selective = 0;  //non-selective, don't use whitelist
    scan_params.p_whitelist = NULL;  //no whitelist
    scan_params.interval = (SCAN_INTERVAL * 1000) / 625;   //scan_params uses interval in units of 0.625ms
    scan_params.window = (SCAN_WINDOW * 1000) / 625;       //window also in units of 0.625ms
    scan_params.timeout = SCAN_TIMEOUT;                     //timeout is in s
    
    scanPeriod_ms = 1000UL * SCAN_PERIOD;
    
    //scan_state = SCAN_IDLE;
}

static ble_gap_evt_adv_report_t get_mock_adv_report_for_badge_with_id(uint8_t id) {
    // Generate MAC address based on id.
    uint8_t peer_addr_based_on_id[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, id};

    ble_gap_evt_adv_report_t mock_adv_report;

    mock_adv_report.peer_addr.addr_type = BLE_GAP_ADDR_TYPE_PUBLIC;
    memcpy(mock_adv_report.peer_addr.addr, peer_addr_based_on_id, sizeof(mock_adv_report.peer_addr.addr));

    // Our mock reports are used to simulate scanning, so we don't need to include
    // a report_type.
    mock_adv_report.scan_rsp = 1;
    mock_adv_report.rssi = -42;

    // Create a mock device name.
    typedef struct {
        uint8_t name_length;
        uint8_t name_type;
        char name[sizeof(DEVICE_NAME)];
    }__attribute__((packed)) badge_name_data_t;

    badge_name_data_t badge_name_data;
    badge_name_data.name_length = sizeof(DEVICE_NAME);
    badge_name_data.name_type = BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME;
    memcpy(badge_name_data.name, DEVICE_NAME, sizeof(DEVICE_NAME));

    // Create mock custom manufacturer data for a badge.
    typedef struct {
        uint8_t manufacturer_data_len;
        uint8_t data_type;
        uint16_t company_id;
        custom_adv_data_t manufacturer_data;
    }__attribute__((packed)) manufacturer_adv_data_t;

    manufacturer_adv_data_t mock_manufacturer_adv_data;
    mock_manufacturer_adv_data.data_type = BLE_GAP_AD_TYPE_MANUFACTURER_SPECIFIC_DATA;
    mock_manufacturer_adv_data.company_id = 0xFF;
    mock_manufacturer_adv_data.manufacturer_data_len = 14;

    mock_manufacturer_adv_data.manufacturer_data.battery = 90;
    mock_manufacturer_adv_data.manufacturer_data.group = badgeAssignment.group;
    mock_manufacturer_adv_data.manufacturer_data.ID = id;
    memcpy(mock_manufacturer_adv_data.manufacturer_data.MAC, peer_addr_based_on_id, sizeof(peer_addr_based_on_id));
    mock_manufacturer_adv_data.manufacturer_data.statusFlags = 0x00;

    // Copy device name and manufacturer data into report advertising data.
    memcpy(&mock_adv_report.data[0], &badge_name_data, sizeof(badge_name_data_t));
    memcpy(&mock_adv_report.data[sizeof(badge_name_data)], &mock_manufacturer_adv_data, sizeof(mock_manufacturer_adv_data));
    mock_adv_report.dlen = sizeof(badge_name_data) + sizeof(mock_manufacturer_adv_data);

    return mock_adv_report;
}

uint32_t startScan()
{
    sd_ble_gap_scan_stop();  // stop any in-progress scans
    
    scan.num = 0;
    scan.timestamp = now();
    
    return sd_ble_gap_scan_start(&scan_params);
}
    

void BLEonAdvReport(ble_gap_evt_adv_report_t* advReport)
{
    //debug_log("BLECH\r\n");
    signed char rssi = advReport->rssi;
    
    if (rssi < MINIMUM_RSSI)  {
        return;  // ignore signals that are too weak.
    }
    
    unsigned char dataLength = advReport->dlen;
    unsigned char* data = (unsigned char*)advReport->data;
    unsigned char index = 0;
    
    unsigned char* namePtr = NULL;
    //unsigned char  nameLen = 0;
    unsigned char* manufDataPtr = NULL;
    unsigned char  manufDataLen = 0;
    
    unsigned char group = BAD_GROUP;
    unsigned short ID = BAD_ID;
    
    while ((namePtr == NULL || manufDataPtr == NULL) && index < dataLength)  {
        unsigned char fieldLen = data[index];
        index++;
        unsigned char fieldType = data[index];
        if (fieldType == BLE_GAP_AD_TYPE_SHORT_LOCAL_NAME || fieldType == BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME)  {
            namePtr = &data[index + 1];  // skip field type byte
            //nameLen = fieldLen - 1;
        }
        else if (fieldType == BLE_GAP_AD_TYPE_MANUFACTURER_SPECIFIC_DATA)  {
            manufDataPtr = &data[index + 1];  // skip field type byte
            manufDataLen = fieldLen - 1;
        }
        index += fieldLen;
    }
    
    if (manufDataLen == BADGE_MANUF_DATA_LEN)  {
        if (namePtr != NULL && memcmp(namePtr,(const uint8_t *)DEVICE_NAME,strlen(DEVICE_NAME)) == 0)  {
            custom_adv_data_t badgeAdvData;
            memcpy(&badgeAdvData,&manufDataPtr[2],CUSTOM_ADV_DATA_LEN);  // skip past company ID; ensure data is properly aligned
            ID = badgeAdvData.ID;
            group = badgeAdvData.group;
            debug_log("---Badge seen: group %d, ID %.4X, rssi %d.\r\n",(int)group,(int)ID,(int)rssi);
        }
    }
    else if (manufDataLen == IBEACON_MANUF_DATA_LEN)  {
        iBeacon_data_t iBeaconData;
        memcpy(&iBeaconData,manufDataPtr,IBEACON_MANUF_DATA_LEN);  // ensure data is properly aligned
        if (iBeaconData.companyID == COMPANY_ID_APPLE && iBeaconData.type == IBEACON_TYPE_PROXIMITY)  {
            // major/minor values are big-endian
            unsigned short major = ((unsigned short)iBeaconData.major[0] * 256) + iBeaconData.major[1];
            UNUSED_VARIABLE(major);
            unsigned short minor = ((unsigned short)iBeaconData.minor[0] * 256) + iBeaconData.minor[1];
            debug_log("---iBeacon seen: major %d, minor %d, rssi %d.\r\n",(int)major,minor,(int)rssi);
            group = iBeaconData.major[1];  // take only lower byte of major value
            ID = minor;
        }
    }
    
    if (ID != BAD_ID && group == badgeAssignment.group)  {
        bool prevSeen = false;
        for(int i=0; i<scan.num; i++)  {      // check through list of already seen badges
            if(ID == scan.IDs[i])  {
                scan.rssiSums[i] += rssi;
                scan.counts[i]++;
                prevSeen = true;
                break;
            }
        }
        if(!prevSeen)  {                             // is it a new badge
            scan.IDs[scan.num] = ID;
            scan.rssiSums[scan.num] = rssi;
            scan.counts[scan.num] = 1;
            scan.num++;
        }
    }
    
    /*
    
    // Parse the broadcast packet.  (find+check name, find custom data if present)
    while ((name == NULL || gotPayload == false) && index < dataLength)  {
        unsigned char fieldLen = data[index];
        index++;
        unsigned char fieldType = data[index];
        if (fieldType == BLE_GAP_AD_TYPE_SHORT_LOCAL_NAME || fieldType == BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME)  {
            name = &data[index + 1];
            if (memcmp(name,(const uint8_t *)DEVICE_NAME,strlen(DEVICE_NAME)) == 0)  {
                // name checks out
            }
            else if (name != NULL && memcmp(name,(const uint8_t *)BEACON_NAME,strlen(BEACON_NAME)) == 0)  {
                payload.group = BEACON_GROUP;
                // our beacons are named "[BEACON_NAME]XXXX", where XXXX is a hex ID number.
                payload.ID = hexStringToShort(&name[strlen(BEACON_NAME)]);
                if (payload.ID != 0xffff)  {  // was there a valid ID in name?
                    gotPayload = true;
                }
            }
            else  {
                gotPayload = false;
                break;  // if name doesn't match, stop parsing broadcast packet
            }
        }
        else if(fieldType == BLE_GAP_AD_TYPE_MANUFACTURER_SPECIFIC_DATA)  {
            int payloadLen = fieldLen - 3;  // length of field minus field type byte and manuf ID word
            if(payloadLen == CUSTOM_DATA_LEN)  {
                // Need to copy payload data so it is properly aligned.
                memcpy(&payload,&data[index+3],CUSTOM_DATA_LEN);  // skip past field type byte, manuf ID word
                gotPayload = true;
            }
        }
        index += fieldLen;
    }
    
    if(gotPayload)  {   // did we see a valid device, and retrieve relevant data?
        debug_log("---Badge seen: group %d, ID %hX, rssi %d.\r\n",(int)payload.group,payload.ID,(int)rssi);
        if(payload.group == badgeAssignment.group || payload.group == BEACON_GROUP)  {
            bool prevSeen = false;
            for(int i=0; i<scan.num; i++)  {      // check through list of already seen badges
                if(payload.ID == scan.IDs[i])  {
                    scan.rssiSums[i] += rssi;
                    scan.counts[i]++;
                    prevSeen = true;
                    break;
                }
            }
            if(!prevSeen)  {                             // is it a new badge
                scan.IDs[scan.num] = payload.ID;
                scan.rssiSums[scan.num] = rssi;
                scan.counts[scan.num] = 1;
                scan.num++;
            }
        }
    }
    */
    
    /*
    // step through data until we find both the name and custom data, or have reached the end of the data
    while ((gotPayload == false || name == NULL) && index < dataLength)  {
        unsigned char fieldLen = data[index];
        index++;
        unsigned char fieldType = data[index];
        if(fieldType == BLE_GAP_AD_TYPE_SHORT_LOCAL_NAME
            || fieldType == BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME)
        {
            name = &data[index+1];
        }
        else if(fieldType == BLE_GAP_AD_TYPE_MANUFACTURER_SPECIFIC_DATA)
        {
            payloadLen = fieldLen - 3;  // length of field minus field type byte and manuf ID word
            if(payloadLen == CUSTOM_DATA_LEN)
            {
                // Need to copy payload data so it is properly aligned.
                memcpy(&payload,&data[index+3],CUSTOM_DATA_LEN);  // skip past field type byte, manuf ID word
                gotPayload = true;
            }
        }
        index += fieldLen;
    }
    
    if(name != NULL && memcmp(name,(const uint8_t *)DEVICE_NAME,strlen(DEVICE_NAME)) == 0)  {  // is it a badge?
        if(gotPayload)  {   // is there custom data, and the correct amount?
            //debug_log("Badge seen: group %d, ID %hX, rssi %d.\r\n",(int)payload.group,payload.ID,(int)rssi);
            if(payload.group == badgeAssignment.group)  {
                bool prevSeen = false;
                for(int i=0; i<scan.num; i++)  {      // check through list of already seen badges
                    if(payload.ID == scan.IDs[i])  {
                        scan.rssiSums[i] += rssi;
                        scan.counts[i]++;
                        prevSeen = true;
                        break;
                    }
                }
                if(!prevSeen)  {                             // is it a new badge
                    scan.IDs[scan.num] = payload.ID;
                    scan.rssiSums[scan.num] = rssi;
                    scan.counts[scan.num] = 1;
                    scan.num++;
                }
            }
        }
        else  {
            //debug_log("Badge seen, rssi %d, but wrong/missing adv data, len %d?\r\n",(int)rssi,payloadLen);
        }
    }
    else if (name != NULL && memcmp(name,(const uint8_t *)BEACON_NAME,strlen(BEACON_NAME)) == 0)  {
        // beacons are named [BEACON_NAME]XXXX, where XXXX is a hex ID number.
        unsigned short beaconID = hexStringToShort(&name[strlen(BEACON_NAME)]);  
    }
    else  {
        //debug_log("Unknown device seen, name %.5s, rssi %d.\r\n",name,(int)rssi);
    }
    */
        
}

void BLEonScanTimeout()
{   
    scan_state = SCANNER_SAVE;
}


bool updateScanner()
{
    bool scannerActive = true;

    switch(scan_state)
    {
    case SCANNER_IDLE:   // nothing to do, but check whether it's time to scan again.
        if(scanner_enable)  {  // don't re-start scans if scanning is disabled
            if(millis() - lastScanTime >= scanPeriod_ms)  {  // start scanning if it's time to scan
                // don't start scans if BLE is inactive (paused)
                if(BLEgetStatus() != BLE_INACTIVE)  {
                    uint32_t result = startScan();
                    if(result == NRF_SUCCESS)  {
                        scan_state = SCANNER_SCANNING;
                        debug_log("SCANNER: Started scan.\r\n");
                    }
                    else  {
                        debug_log("ERR: error starting scan, #%u\r\n",(unsigned int)result);
                    }
                    lastScanTime = millis();
                }
            }
        }
        else  {
            scannerActive = false;
        }
        break;
    case SCANNER_SCANNING:
        // Scan timeout callback will exit scanning state.
        break;
    case SCANNER_SAVE:
        debug_log("SCANNER: Saving scan results. %d devices seen\r\n",scan.num);
        
        int numSaved = 0;
        int chunksUsed = 0;
        
        do  {
            // Fill chunk header
            scanBuffer[scan.to].timestamp = scan.timestamp;
            scanBuffer[scan.to].num = scan.num;
            int scaledBatteryLevel = (int)(100.0*getBatteryVoltage()) - 100;
            scanBuffer[scan.to].batteryLevel =  (scaledBatteryLevel <= 255) ? scaledBatteryLevel : 255;  // clip scaled level
            
            debug_log("  C:%d\r\n",scan.to);
        
            // Fill chunk with results
            int numLeft = scan.num - numSaved;
            int numThisChunk = (numLeft <= SCAN_DEVICES_PER_CHUNK) ? numLeft : SCAN_DEVICES_PER_CHUNK;
            for(int i = 0; i < numThisChunk; i++)  {
                scanBuffer[scan.to].devices[i].ID = scan.IDs[numSaved + i];
                scanBuffer[scan.to].devices[i].rssi = scan.rssiSums[numSaved + i] / scan.counts[numSaved + i];
                scanBuffer[scan.to].devices[i].count = scan.counts[numSaved + i];
                debug_log("    bdg ID#%.4hX, rssi %d, count %d\r\n", scanBuffer[scan.to].devices[i].ID,
                                                                (int)scanBuffer[scan.to].devices[i].rssi,
                                                                (int)scanBuffer[scan.to].devices[i].count );
            }
            numSaved += numThisChunk;  
            
            // Terminate chunk
            if(chunksUsed == 0)  {   // first chunk of saved results
                if(numSaved >= scan.num)  {  // did all the results fit in the first chunk
                    scanBuffer[scan.to].check = scanBuffer[scan.to].timestamp;  // mark as complete
                }
                else  {
                    scanBuffer[scan.to].check = CHECK_TRUNC;
                }
            }
            else  {  // else we're continuing results from a previous chunk
                scanBuffer[scan.to].check = CHECK_CONTINUE;
            }
            //debug_log("--num: %d\r\n",scanBuffer[scan.to].num);
            
            chunksUsed++;
            scan.to = (scan.to < LAST_SCAN_CHUNK) ? scan.to+1 : 0;
        } while(numSaved < scan.num);
        
        debug_log("SCANNER: Done saving results.  used %d chunks.\r\n",chunksUsed);
        
        scan_state = SCANNER_IDLE;
        
        break;
    default:
        break;
    }  // switch(scan_state)
    
    return scannerActive;
    
}


void startScanner(unsigned short window_ms,unsigned short interval_ms,unsigned short duration_s,unsigned short period_s)
{
    scan_params.interval = (interval_ms * 1000UL) / 625;   //scan_params uses interval in units of 0.625ms
    scan_params.window = (window_ms * 1000UL) / 625;       //window also in units of 0.625ms
    scan_params.timeout = duration_s;                     //timeout is in s
    
    scanPeriod_ms = 1000UL * period_s;
    
    scanner_enable = true;
}

void stopScanner()
{
    scanner_enable = false;
}


scan_state_t getScanState()  
{
    return scan_state;
}


unsigned long getScanTimestamp(int chunk)
{
    ext_eeprom_wait();
    scan_header_t header;
    ext_eeprom_read(EXT_ADDRESS_OF_CHUNK(chunk),header.buf,sizeof(header.buf));
    while(spi_busy());
    return header.timestamp;
}

unsigned long getScanCheck(int chunk)
{
    ext_eeprom_wait();
    scan_tail_t tail;
    ext_eeprom_read(EXT_ADDRESS_OF_CHUNK_CHECK(chunk),tail.buf,sizeof(tail.buf));
    while(spi_busy());
    return tail.check;
}

void getScanChunk(scan_chunk_t* destPtr,int chunk)
{
    ext_eeprom_wait();
    ext_eeprom_read(EXT_ADDRESS_OF_CHUNK(chunk),destPtr->buf,sizeof(destPtr->buf));
    while(spi_busy());
}


void printScanResult(scan_chunk_t* srcPtr)
{
    //scan_chunk_t readChunk;
    //ext_eeprom_read(EXT_ADDRESS_OF_CHUNK(chunk),readChunk.buf,sizeof(readChunk.buf));
    
    debug_log("Read: TS 0x%X, N %d, C 0x%X\r\n", (unsigned int)srcPtr->timestamp,
                                                            (int)srcPtr->num,
                                                            (unsigned int)srcPtr->check 
                                                            );
                                                            
    for(int i = 0; i < SCAN_DEVICES_PER_CHUNK; i++)  {
        // Print all seen devices
        if(i < srcPtr->num)  {
            debug_log("  #%X, R: %d, c: %d\r\n",(int)srcPtr->devices[i].ID,(signed int)srcPtr->devices[i].rssi
                                                    ,(int)srcPtr->devices[i].count);
        }
        // Prints the remaining contents of the chunk (not real data)
        //else  {
        //    debug_log("  // #%d, %d\r\n",(int)readChunk.devices[i].ID,(int)readChunk.devices[i].rssi);
        //}
        nrf_delay_ms(1);
    }
}

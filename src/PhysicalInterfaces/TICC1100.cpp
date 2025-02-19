/* Copyright 2013-2019 Homegear GmbH
 *
 * Homegear is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Homegear is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Homegear.  If not, see <http://www.gnu.org/licenses/>.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 */

#include "TICC1100.h"

#ifdef SPIINTERFACES
#include "../BidCoSPacket.h"
#include <homegear-base/BaseLib.h>
#include "../GD.h"

namespace BidCoS
{

TICC1100::TICC1100(std::shared_ptr<BaseLib::Systems::PhysicalInterfaceSettings> settings) : IBidCoSInterface(settings)
{
	try
	{
		_out.init(GD::bl);
		_out.setPrefix(GD::out.getPrefix() + "TI CC110X \"" + settings->id + "\": ");

		_sending = false;
		_sendingPending = false;
		_firstPacket = true;

		if(settings->listenThreadPriority == -1)
		{
			settings->listenThreadPriority = 45;
			settings->listenThreadPolicy = SCHED_FIFO;
		}

		if(settings->oscillatorFrequency < 0) settings->oscillatorFrequency = 26000000;
		if(settings->txPowerSetting < 0) settings->txPowerSetting = (gpioDefined(2)) ? 0x27 : 0xC0;
		_out.printDebug("Debug: PATABLE will be set to 0x" + BaseLib::HelperFunctions::getHexString(settings->txPowerSetting, 2));
		if(settings->interruptPin != 0 && settings->interruptPin != 2)
		{
			if(settings->interruptPin > 0) _out.printWarning("Warning: Setting for interruptPin for device CC1100 in homematicbidcos.conf is invalid.");
			settings->interruptPin = 2;
		}

		_transfer =  { (uint64_t)0, (uint64_t)0, (uint32_t)0, (uint32_t)4000000, (uint16_t)0, (uint8_t)8, (uint8_t)0, (uint32_t)0 };

		setConfig();
	}
    catch(const std::exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

TICC1100::~TICC1100()
{
	try
	{
		_stopCallbackThread = true;
		GD::bl->threadManager.join(_listenThread);
		closeDevice();
		closeGPIO(1);
	}
    catch(const std::exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void TICC1100::setConfig()
{
	if(_settings->oscillatorFrequency == 26000000)
	{
		_config = //Read from HM-CC-VD
		{
			(_settings->interruptPin == 2) ? (uint8_t)0x46 : (uint8_t)0x5B, //00: IOCFG2 (GDO2_CFG)
			0x2E, //01: IOCFG1 (GDO1_CFG to High impedance (3-state))
			(_settings->interruptPin == 0) ? (uint8_t)0x46 : (uint8_t)0x5B, //02: IOCFG0 (GDO0_CFG)
			0x07, //03: FIFOTHR (FIFO threshold to 33 (TX) and 32 (RX)
			0xE9, //04: SYNC1
			0xCA, //05: SYNC0
			0xFF, //06: PKTLEN (Maximum packet length)
			0x0C, //07: PKTCTRL1: CRC_AUTOFLUSH | APPEND_STATUS | NO_ADDR_CHECK
			0x45, //08: PKTCTRL0
			0x00, //09: ADDR
			0x00, //0A: CHANNR
			0x06, //0B: FSCTRL1
			0x00, //0C: FSCTRL0
			0x21, //0D: FREQ2
			0x65, //0E: FREQ1
			0x6A, //0F: FREQ0
			0xC8, //10: MDMCFG4
			0x93, //11: MDMCFG3
			0x03, //12: MDMCFG2
			0x22, //13: MDMCFG1
			0xF8, //14: MDMCFG0
			0x34, //15: DEVIATN
			0x07, //16: MCSM2
			0x30, //17: MCSM1: IDLE when packet has been received, RX after sending
			0x18, //18: MCSM0
			0x16, //19: FOCCFG
			0x6C, //1A: BSCFG
			0x03, //1B: AGCCTRL2
			0x40, //1C: AGCCTRL1
			0x91, //1D: AGCCTRL0
			0x87, //1E: WOREVT1
			0x6B, //1F: WOREVT0
			0xF8, //20: WORCRTL
			0x56, //21: FREND1
			0x10, //22: FREND0
			0xE9, //23: FSCAL3
			0x2A, //24: FSCAL2
			0x00, //25: FSCAL1
			0x1F, //26: FSCAL0
			0x41, //27: RCCTRL1
			0x00, //28: RCCTRL0
		};
	}
	else if(_settings->oscillatorFrequency == 27000000)
	{
		_config =
		{
			(_settings->interruptPin == 2) ? (uint8_t)0x46 : (uint8_t)0x5B, //00: IOCFG2 (GDO2_CFG: GDO2 connected to RPi interrupt pin, asserts when packet sent/received, active low)
			0x2E, //01: IOCFG1 (GDO1_CFG to High impedance (3-state))
			(_settings->interruptPin == 0) ? (uint8_t)0x46 : (uint8_t)0x5B, //02: IOCFG0 (GDO0_CFG, GDO0 (optionally) connected to CC1190 PA_EN, PA_PD, active low(?!))
			0x07, //03: FIFOTHR (FIFO threshold to 33 (TX) and 32 (RX)
			0xE9, //04: SYNC1
			0xCA, //05: SYNC0
			0xFF, //06: PKTLEN (Maximum packet length)
			0x0C, //07: PKTCTRL1 (CRC_AUTOFLUSH | APPEND_STATUS | NO_ADDR_CHECK)
			0x45, //08: PKTCTRL0 (WHITE_DATA = on, PKT_FORMAT = normal mode, CRC_EN = on, LENGTH_CONFIG = "Variable packet length mode. Packet length configured by the first byte after sync word")
			0x00, //09: ADDR
			0x00, //0A: CHANNR
			0x06, //0B: FSCTRL1 (0x06 gives f_IF=152.34375kHz@26.0MHz XTAL, 158.203125kHz@f_XOSC=27.0MHz; default value is 0x0F which gives f_IF=381kHz@f_XOSC=26MHz; formula is f_IF=(f_XOSC/2^10)*FSCTRL1[5:0])
			0x00, //0C: FSCTRL0
			0x20, //0D: FREQ2 (base freq f_carrier=(f_XOSC/2^16)*FREQ[23:0]; register value FREQ[23:0]=(2^16/f_XOSC)*f_carrier; 0x21656A gives f_carrier=868.299866MHz@f_XOSC=26.0MHz, 0x2028C5 gives f_carrier=868.299911MHz@f_XOSC=27.0MHz)
			0x28, //0E: FREQ1
			0xC5, //0F: FREQ0
			0xC8, //10: MDMCFG4 (CHANBW_E = 3, CHANBW_M = 0, gives BW_channel=f_XOSC/(8*(4+CHANBW_M)*2^CHANBW_E)=102kHz@f_XOSC=26MHz, 105kHz@f_XOSC=27MHz)
//			0x93, //11: MDMCFG3 (26MHz: DRATE_E = 0x8, DRATE_M = 0x93, gives R_DATA=((256+DRATE_M)*2^DRATE_E/2^28)*f_XOSC=9993Baud)
			0x84, //11: MDMCFG3 (27MHz: DRATE_M=(R_DATA*2^28)/(f_XOSC*2^DRATE_E)-256 ==> DRATE_E = 0x8, DRATE_M = 132=0x84, gives R_DATA=((256+DRATE_M)*2^DRATE_E/2^28)*f_XOSC=9991Baud)
			0x03, //12: MDMCFG2 (DEM_DCFILT_OFF = 0, MOD_FORMAT = 0 (2-FSK), MANCHESTER_EN = 0, SYNC_MODE = 3 = 30/32 sync word bits detected)
			0x22, //13: MDMCFG1 (FEC_EN = 0, NUM_PREAMBLE = 2 = 4 preamble bytes, CHANSPC_E = 2)
//			0xF8, //14: MDMCFG0 (CHANSPC_M = 248 = 0xF8, Delta f_channel=(f_XOSC/2^18)*(256+CHANSPC_M)*2^CHANSPC_E=199.951kHz@f_XOSC=26MHz)
			0xE5, //14: MDMCFG0 (CHANSPC_M=(Delta_F_channel*2^18/(f_XOSC*2^CHANSPC_E)-256 ==> CHANSPC_M = 229 = 0xE5, Delta_f_channel=(f_XOSC/2^18)*(256+CHANSPC_M)*2^CHANSPC_E=199.814kHz@f_XOSC=27MHz)
			0x34, //15: DEVIATN (DEVIATION_E = 3, DEVIATION_M = 4, gives f_dev=(f_XOSC/2^17)*(8+DEVIATION_M)*2^DEVIATION_E=19.043kHz@f_XOSC=26MHz, =19.775kHz@f_XOSC=27MHz)
			0x07, //16: MCSM2 (RX_TIME_RSSI = 0, RX_TIME_QUAL = 0, RX_TIME = 7)
			0x30, //17: MCSM1 (CCA_MODE = 0b00 = "Always", RXOFF_MODE = 0 = IDLE, TXOFF_MODE = 0 = IDLE)
			0x18, //18: MCSM0 (FS_AUTOCAL = 0b01 = cal@IDLE->RX/TX, PO_TIMEOUT = 0b10 = 149µs@27MHz, PIN_CTRL_EN = 0, XOSC_FORCE_ON = 0)
			0x16, //19: FOCCFG (FOD_BS_CS_GATE = 0, FOC_PRE_K = 0b10 = 3K, FOC_POST_K = 1 = K/2, FOC_LIMIT = 0b10)
			0x6C, //1A: BSCFG
			0x03, //1B: AGCCTRL2
			0x40, //1C: AGCCTRL1
			0x91, //1D: AGCCTRL0
			0x87, //1E: WOREVT1
			0x6B, //1F: WOREVT0
			0xF8, //20: WORCRTL
			0x56, //21: FREND1
			0x10, //22: FREND0
			0xE9, //23: FSCAL3
			0x2A, //24: FSCAL2
			0x00, //25: FSCAL1
			0x1F, //26: FSCAL0
			0x41, //27: RCCTRL1
			0x00, //28: RCCTRL0
		};
	}
	else _out.printError("Error: Unknown value for \"oscillatorFrequency\" in homematicbidcos.conf. Valid values are 26000000 and 27000000.");
}

void TICC1100::addPeer(PeerInfo peerInfo)
{
	try
	{
		if(peerInfo.address == 0) return;
		_peersMutex.lock();
		//Remove old peer first. removePeer() is not called, so we don't need to unlock _peersMutex
		if(_peers.find(peerInfo.address) != _peers.end()) _peers.erase(peerInfo.address);
		_peers[peerInfo.address] = peerInfo;
	}
    catch(const std::exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    _peersMutex.unlock();
}

void TICC1100::addPeers(std::vector<PeerInfo>& peerInfos)
{
	try
	{
		for(std::vector<PeerInfo>::iterator i = peerInfos.begin(); i != peerInfos.end(); ++i)
		{
			addPeer(*i);
		}
	}
    catch(const std::exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void TICC1100::removePeer(int32_t address)
{
	try
	{
		_peersMutex.lock();
		if(_peers.find(address) != _peers.end()) _peers.erase(address);
	}
    catch(const std::exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    _peersMutex.unlock();
}

void TICC1100::enableUpdateMode()
{
	try
	{
		_updateMode = true;
		while(_sending) std::this_thread::sleep_for(std::chrono::milliseconds(3));
		_txMutex.try_lock();
		sendCommandStrobe(CommandStrobes::Enum::SIDLE);
		writeRegister(Registers::Enum::FSCTRL1, 0x08, true); // gives higher IF, IF=203kHz@f_XOSC=26MHz, 211kHz@f_XOSC=27MHz
		writeRegister(Registers::Enum::MDMCFG4, 0x5B, true); // CHANBW_E = 1, CHANBW_M = 1, BW_channel = 325kHz@26MHz, 338kHz@27MHz (higher)
		writeRegister(Registers::Enum::MDMCFG3, (_settings->oscillatorFrequency == 26000000) ? 0xF8 : 0xE5, true); // 0xF8 gives R_DATA = 99.975kBaud@26MHz; 0xE5 gives R_DATA=99.907kBaud@27MHz
		writeRegister(Registers::Enum::DEVIATN, (_settings->oscillatorFrequency == 26000000) ? 0x47 : 0x46, true); // 47.607kHz deviation @26MHz, 46.143kHz dev @27MHz
		writeRegister(Registers::Enum::FOCCFG, 0x1D, true);
		writeRegister(Registers::Enum::BSCFG, 0x1C, true);
		writeRegister(Registers::Enum::AGCCTRL2, 0xC7, true);
		writeRegister(Registers::Enum::AGCCTRL1, 0x00, true);
		writeRegister(Registers::Enum::AGCCTRL0, 0xB2, true);
		writeRegister(Registers::Enum::FREND1, 0xB6, true);
		writeRegister(Registers::Enum::FSCAL3, 0xEA, true); //Changes, because of the change of data rate
		usleep(20);
		sendCommandStrobe(CommandStrobes::Enum::SFRX);
		sendCommandStrobe(CommandStrobes::Enum::SRX);
	}
    catch(const std::exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    _txMutex.unlock();
}

void TICC1100::disableUpdateMode()
{
	try
	{
		setConfig();
		stopListening();
        _updateMode = false;
		startListening();
	}
    catch(const std::exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void TICC1100::openDevice()
{
	try
	{
		if(_fileDescriptor->descriptor != -1) closeDevice();

		_lockfile = GD::bl->settings.lockFilePath() + "LCK.." + _settings->device.substr(_settings->device.find_last_of('/') + 1);
		int lockfileDescriptor = open(_lockfile.c_str(), O_WRONLY | O_EXCL | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
		if(lockfileDescriptor == -1)
		{
			if(errno != EEXIST)
			{
				_out.printCritical("Couldn't create lockfile " + _lockfile + ": " + strerror(errno));
				return;
			}

			int processID = 0;
			std::ifstream lockfileStream(_lockfile.c_str());
			lockfileStream >> processID;
			if(getpid() != processID && kill(processID, 0) == 0)
			{
				_out.printCritical("Rf device is in use: " + _settings->device);
				return;
			}
			unlink(_lockfile.c_str());
			lockfileDescriptor = open(_lockfile.c_str(), O_WRONLY | O_EXCL | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
			if(lockfileDescriptor == -1)
			{
				_out.printCritical("Couldn't create lockfile " + _lockfile + ": " + strerror(errno));
				return;
			}
		}
		dprintf(lockfileDescriptor, "%10i", getpid());
		close(lockfileDescriptor);

		_fileDescriptor = _bl->fileDescriptorManager.add(open(_settings->device.c_str(), O_RDWR | O_NONBLOCK));
		usleep(1000);

		if(_fileDescriptor->descriptor == -1)
		{
			_out.printCritical("Couldn't open rf device \"" + _settings->device + "\": " + strerror(errno));
			return;
		}

		setupDevice();
	}
	catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void TICC1100::closeDevice()
{
	try
	{
		_bl->fileDescriptorManager.close(_fileDescriptor);
		unlink(_lockfile.c_str());
	}
    catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void TICC1100::setup(int32_t userID, int32_t groupID, bool setPermissions)
{
	try
	{
		_out.printDebug("Debug: CC1100: Setting device permissions");
		if(setPermissions) setDevicePermission(userID, groupID);
		_out.printDebug("Debug: CC1100: Exporting GPIO");
		exportGPIO(1);
		if(gpioDefined(2)) exportGPIO(2);
		_out.printDebug("Debug: CC1100: Setting GPIO permissions");
		if(setPermissions) setGPIOPermission(1, userID, groupID, false);
		if(setPermissions && gpioDefined(2)) setGPIOPermission(2, userID, groupID, false);
		if(gpioDefined(2)) setGPIODirection(2, GPIODirection::OUT);
	}
    catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void TICC1100::setupDevice()
{
	try
	{
		if(_fileDescriptor->descriptor == -1) return;

		uint8_t mode = 0;
		uint8_t bits = 8;
		uint32_t speed = 4000000; //4MHz, see page 25 in datasheet

		if(ioctl(_fileDescriptor->descriptor, SPI_IOC_WR_MODE, &mode)) throw(BaseLib::Exception("Couldn't set spi mode on device " + _settings->device));
		if(ioctl(_fileDescriptor->descriptor, SPI_IOC_RD_MODE, &mode)) throw(BaseLib::Exception("Couldn't get spi mode off device " + _settings->device));

		if(ioctl(_fileDescriptor->descriptor, SPI_IOC_WR_BITS_PER_WORD, &bits)) throw(BaseLib::Exception("Couldn't set bits per word on device " + _settings->device));
		if(ioctl(_fileDescriptor->descriptor, SPI_IOC_RD_BITS_PER_WORD, &bits)) throw(BaseLib::Exception("Couldn't get bits per word off device " + _settings->device));

		if(ioctl(_fileDescriptor->descriptor, SPI_IOC_WR_MAX_SPEED_HZ, &speed)) throw(BaseLib::Exception("Couldn't set speed on device " + _settings->device));
		if(ioctl(_fileDescriptor->descriptor, SPI_IOC_RD_MAX_SPEED_HZ, &speed)) throw(BaseLib::Exception("Couldn't get speed off device " + _settings->device));
	}
	catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

/*void TICC1100::sendTest()
{
	try
	{
		if(_fileDescriptor->descriptor == -1 || _gpioDescriptors[1]->descriptor == -1 || _stopped) return;
		std::vector<uint8_t> encodedPacket;
		for(int32_t i = 0; i < 200; i++)
		{
			encodedPacket.push_back(0x55);
		}

		while(true)
		{
			_sendingPending = true;
			_txMutex.lock();
			_sendingPending = false;
			if(_stopCallbackThread || _fileDescriptor->descriptor == -1 || _gpioDescriptors[1]->descriptor == -1 || _stopped)
			{
				_txMutex.unlock();
				return;
			}
			_sending = true;

			sendCommandStrobe(CommandStrobes::Enum::SIDLE);
			sendCommandStrobe(CommandStrobes::Enum::SFTX);
			writeRegisters(Registers::Enum::FIFO, encodedPacket);
			sendCommandStrobe(CommandStrobes::Enum::STX);
			_out.printDebug("Debug: Sending test data...");
		}
	}
	catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}*/

void TICC1100::forceSendPacket(std::shared_ptr<BidCoSPacket> packet)
{
	try
	{
		if(_fileDescriptor->descriptor == -1 || _gpioDescriptors[1]->descriptor == -1 || _stopped) return;
		if(!packet) return;
        bool burst = packet->controlByte() & 0x10;
		std::vector<uint8_t> decodedPacket = packet->byteArray();
		std::vector<uint8_t> encodedPacket(decodedPacket.size());
		encodedPacket[0] = decodedPacket[0];
		encodedPacket[1] = (~decodedPacket[1]) ^ 0x89;
		uint32_t i = 2;
		for(; i < decodedPacket[0]; i++)
		{
			encodedPacket[i] = (encodedPacket[i - 1] + 0xDC) ^ decodedPacket[i];
		}
		encodedPacket[i] = decodedPacket[i] ^ decodedPacket[2];

		int64_t timeBeforeLock = BaseLib::HelperFunctions::getTime();
		_sendingPending = true;
		if(!_txMutex.try_lock_for(std::chrono::milliseconds(10000)))
		{
			_out.printCritical("Critical: Could not acquire lock for sending packet. This should never happen. Please report this error.");
			_txMutex.unlock();
			if(!_txMutex.try_lock_for(std::chrono::milliseconds(100)))
			{
				_sendingPending = false;
				return;
			}
		}
		_sendingPending = false;
		if(_stopCallbackThread || _fileDescriptor->descriptor == -1 || _gpioDescriptors[1]->descriptor == -1 || _stopped)
		{
			_txMutex.unlock();
			return;
		}
		_sending = true;
		sendCommandStrobe(CommandStrobes::Enum::SIDLE);
		sendCommandStrobe(CommandStrobes::Enum::SFTX);
		_lastPacketSent = BaseLib::HelperFunctions::getTime();
		if(_lastPacketSent - timeBeforeLock > 100)
		{
			_out.printWarning("Warning: Timing problem. Sending took more than 100ms. Do you have enough system resources?");
		}
		if(burst)
		{
			//int32_t waitIndex = 0;
			//while(waitIndex < 200)
			//{
				sendCommandStrobe(CommandStrobes::Enum::STX);
				//if(readRegister(Registers::MARCSTATE) == 0x13) break;
				//std::this_thread::sleep_for(std::chrono::milliseconds(2));
				//waitIndex++;
			//}
			//if(waitIndex == 200) _out.printError("Error sending BidCoS packet. No CCA within 400ms.");
			usleep(360000);
		}
		writeRegisters(Registers::Enum::FIFO, encodedPacket);
		if(!burst)
		{
			//int32_t waitIndex = 0;
			//while(waitIndex < 200)
			//{
				sendCommandStrobe(CommandStrobes::Enum::STX);
				//if(readRegister(Registers::MARCSTATE) == 0x13) break;
				//std::this_thread::sleep_for(std::chrono::milliseconds(2));
				//waitIndex++;
			//}
			//if(waitIndex == 200)
			//{
				//_out.printError("Error sending BidCoS packet. No CCA within 400ms.");
				//sendCommandStrobe(CommandStrobes::Enum::SFTX);
			//}
		}

		if(_bl->debugLevel > 3)
		{
			if(packet->timeSending() > 0)
			{
				_out.printInfo("Info: Sending (" + _settings->id + "): " + packet->hexString() + " Planned sending time: " + BaseLib::HelperFunctions::getTimeString(packet->timeSending()));
			}
			else
			{
				_out.printInfo("Info: Sending (" + _settings->id + "): " + packet->hexString());
			}
		}

		//Unlocking of _txMutex takes place in mainThread
	}
	catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void TICC1100::readwrite(std::vector<uint8_t>& data)
{
	try
	{
		std::lock_guard<std::mutex> sendGuard(_sendMutex);
		_transfer.tx_buf = (uint64_t)&data[0];
		_transfer.rx_buf = (uint64_t)&data[0];
		_transfer.len = (uint32_t)data.size();
		if(_bl->debugLevel >= 6) _out.printDebug("Debug: Sending: " + _bl->hf.getHexString(data));
		if(!ioctl(_fileDescriptor->descriptor, SPI_IOC_MESSAGE(1), &_transfer))
		{
			_out.printError("Couldn't write to device " + _settings->device + ": " + std::string(strerror(errno)));
			return;
		}
		if(_bl->debugLevel >= 6) _out.printDebug("Debug: Received: " + _bl->hf.getHexString(data));
	}
	catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

bool TICC1100::checkStatus(uint8_t statusByte, Status::Enum status)
{
	try
	{
		if(_fileDescriptor->descriptor == -1 || _gpioDescriptors[1]->descriptor == -1) return false;
		if((statusByte & (StatusBitmasks::Enum::CHIP_RDYn | StatusBitmasks::Enum::STATE)) != status) return false;
		return true;
	}
    catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return false;
}

uint8_t TICC1100::readRegister(Registers::Enum registerAddress)
{
	try
	{
		if(_fileDescriptor->descriptor == -1) return 0;
		std::vector<uint8_t> data({(uint8_t)(registerAddress | RegisterBitmasks::Enum::READ_SINGLE), 0x00});
		for(uint32_t i = 0; i < 5; i++)
		{
			readwrite(data);
			if(!(data.at(0) & StatusBitmasks::Enum::CHIP_RDYn)) break;
			data.at(0) = (uint8_t)(registerAddress  | RegisterBitmasks::Enum::READ_SINGLE);
			data.at(1) = 0;
			usleep(20);
		}
		return data.at(1);
	}
    catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return 0;
}

std::vector<uint8_t> TICC1100::readRegisters(Registers::Enum startAddress, uint8_t count)
{
	try
	{
		if(_fileDescriptor->descriptor == -1) return std::vector<uint8_t>();
		std::vector<uint8_t> data({(uint8_t)(startAddress | RegisterBitmasks::Enum::READ_BURST)});
		data.resize(count + 1, 0);
		for(uint32_t i = 0; i < 5; i++)
		{
			readwrite(data);
			if(!(data.at(0) & StatusBitmasks::Enum::CHIP_RDYn)) break;
			data.clear();
			data.push_back((uint8_t)(startAddress  | RegisterBitmasks::Enum::READ_BURST));
			data.resize(count + 1, 0);
			usleep(20);
		}
		return data;
	}
    catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return std::vector<uint8_t>();
}

uint8_t TICC1100::writeRegister(Registers::Enum registerAddress, uint8_t value, bool check)
{
	try
	{
		if(_fileDescriptor->descriptor == -1) return 0xFF;
		std::vector<uint8_t> data({(uint8_t)registerAddress, value});
		readwrite(data);
		if((data.at(0) & StatusBitmasks::Enum::CHIP_RDYn) || (data.at(1) & StatusBitmasks::Enum::CHIP_RDYn)) throw BaseLib::Exception("Error writing to register " + std::to_string(registerAddress) + ".");

		if(check)
		{
			data.at(0) = registerAddress | RegisterBitmasks::Enum::READ_SINGLE;
			data.at(1) = 0;
			readwrite(data);
			if(data.at(1) != value)
			{
				_out.printError("Error (check) writing to register " + std::to_string(registerAddress) + ".");
				return 0;
			}
		}
		return value;
	}
    catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return 0;
}

void TICC1100::writeRegisters(Registers::Enum startAddress, std::vector<uint8_t>& values)
{
	try
	{
		if(_fileDescriptor->descriptor == -1) return;
		std::vector<uint8_t> data({(uint8_t)(startAddress | RegisterBitmasks::Enum::WRITE_BURST) });
		data.insert(data.end(), values.begin(), values.end());
		readwrite(data);
		if((data.at(0) & StatusBitmasks::Enum::CHIP_RDYn)) _out.printError("Error writing to registers " + std::to_string(startAddress) + ".");
	}
    catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

uint8_t TICC1100::sendCommandStrobe(CommandStrobes::Enum commandStrobe)
{
	try
	{
		if(_fileDescriptor->descriptor == -1) return 0xFF;
		std::vector<uint8_t> data({(uint8_t)commandStrobe});
		for(uint32_t i = 0; i < 5; i++)
		{
			readwrite(data);
			if(!(data.at(0) & StatusBitmasks::Enum::CHIP_RDYn)) break;
			data.at(0) = (uint8_t)commandStrobe;
			usleep(20);
		}
		return data.at(0);
	}
    catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return 0;
}

void TICC1100::enableRX(bool flushRXFIFO)
{
	try
	{
		if(_fileDescriptor->descriptor == -1) return;
		std::lock_guard<std::timed_mutex> txGuard(_txMutex);
		if(flushRXFIFO) sendCommandStrobe(CommandStrobes::Enum::SFRX);
		sendCommandStrobe(CommandStrobes::Enum::SRX);
	}
    catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void TICC1100::initChip()
{
	try
	{
		if(_fileDescriptor->descriptor == -1)
		{
			_out.printError("Error: Could not initialize TI CC1100. The spi device's file descriptor is not valid.");
			return;
		}
		reset();

		int32_t index = 0;
		for(std::vector<uint8_t>::const_iterator i = _config.begin(); i != _config.end(); ++i)
		{
			if(writeRegister((Registers::Enum)index, *i, true) != *i)
			{
				closeDevice();
				return;
			}
			index++;
		}
		if(writeRegister(Registers::Enum::FSTEST, 0x59, true) != 0x59)
		{
			closeDevice();
			return;
		}
		if(writeRegister(Registers::Enum::TEST2, 0x81, true) != 0x81) //Determined by SmartRF Studio
		{
			closeDevice();
			return;
		}
		if(writeRegister(Registers::Enum::TEST1, 0x35, true) != 0x35) //Determined by SmartRF Studio
		{
			closeDevice();
			return;
		}
		if(writeRegister(Registers::Enum::PATABLE, _settings->txPowerSetting, true) != _settings->txPowerSetting)
		{
			closeDevice();
			return;
		}

		sendCommandStrobe(CommandStrobes::Enum::SFRX);

		enableRX(true);
	}
    catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void TICC1100::reset()
{
	try
	{
		if(_fileDescriptor->descriptor == -1) return;
		sendCommandStrobe(CommandStrobes::Enum::SRES);

		usleep(70); //Measured on HM-CC-VD
	}
    catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

bool TICC1100::crcOK()
{
	try
	{
		if(_fileDescriptor->descriptor == -1) return false;
		std::vector<uint8_t> result = readRegisters(Registers::Enum::LQI, 1);
		if((result.size() == 2) && (result.at(1) & 0x80)) return true;
	}
    catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return false;
}

void TICC1100::startListening()
{
	try
	{
		stopListening();
		initDevice();

		if(!_aesHandshake) return; //AES is not initialized

		if(!GD::family->getCentral())
		{
			_stopCallbackThread = true;
			_out.printError("Error: Could not get central address. Stopping listening.");
			return;
		}
		_myAddress = GD::family->getCentral()->getAddress();
		_aesHandshake->setMyAddress(_myAddress);

		IBidCoSInterface::startListening();

		_stopped = false;
		_firstPacket = true;
		_stopCallbackThread = false;
		if(_settings->listenThreadPriority > -1) GD::bl->threadManager.start(_listenThread, true, _settings->listenThreadPriority, _settings->listenThreadPolicy, &TICC1100::mainThread, this);
		else GD::bl->threadManager.start(_listenThread, true, &TICC1100::mainThread, this);

		//For sniffing update packets
		//std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		//enableUpdateMode();
	}
    catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void TICC1100::initDevice()
{
	try
	{
		openDevice();
		if(!_fileDescriptor || _fileDescriptor->descriptor == -1) return;

		initChip();
		_out.printDebug("Debug: CC1100: Setting GPIO direction");
		setGPIODirection(1, GPIODirection::IN);
		_out.printDebug("Debug: CC1100: Setting GPIO edge");
		setGPIOEdge(1, GPIOEdge::BOTH);
		openGPIO(1, true);
		if(!_gpioDescriptors[1] || _gpioDescriptors[1]->descriptor == -1)
		{
			_out.printError("Error: Couldn't listen to rf device, because the GPIO descriptor is not valid: " + _settings->device);
			return;
		}
		if(gpioDefined(2)) //Enable high gain mode
		{
			openGPIO(2, false);
			if(!getGPIO(2)) setGPIO(2, true);
			closeGPIO(2);
		}
	}
    catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void TICC1100::stopListening()
{
	try
	{
		IBidCoSInterface::stopListening();
		_stopCallbackThread = true;
		GD::bl->threadManager.join(_listenThread);
		_stopCallbackThread = false;
		if(_fileDescriptor->descriptor != -1) closeDevice();
		closeGPIO(1);
		_stopped = true;
	}
	catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void TICC1100::endSending()
{
	try
	{
		sendCommandStrobe(CommandStrobes::Enum::SIDLE);
		sendCommandStrobe(CommandStrobes::Enum::SFRX);
		sendCommandStrobe(CommandStrobes::Enum::SRX);
		_sending = false;
		_lastPacketSent = BaseLib::HelperFunctions::getTime();
	}
	catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void TICC1100::mainThread()
{
    try
    {
		int32_t pollResult;
		int32_t bytesRead;
		std::vector<char> readBuffer({'0'});

        while(!_stopCallbackThread)
        {
        	try
        	{
				if(_stopped)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(200));
					continue;
				}
				if(!_stopCallbackThread && (_fileDescriptor->descriptor == -1 || _gpioDescriptors[1]->descriptor == -1))
				{
					_out.printError("Connection to TI CC1101 closed unexpectedly... Trying to reconnect...");
					_stopped = true; //Set to true, so that sendPacket aborts
					if(_sending)
					{
						std::this_thread::sleep_for(std::chrono::milliseconds(1000));
						_sending = false;
					}
					_txMutex.unlock(); //Make sure _txMutex is unlocked

					closeGPIO(1);
					initDevice();
					closeGPIO(1);
					std::this_thread::sleep_for(std::chrono::milliseconds(1000));
					openGPIO(1, true);
					_stopped = false;
					continue;
				}

				pollfd pollstruct {
					(int)_gpioDescriptors[1]->descriptor,
					(short)(POLLPRI | POLLERR),
					(short)0
				};

				pollResult = poll(&pollstruct, 1, 100);
				/*if(pollstruct.revents & POLLERR)
				{
					_out.printWarning("Warning: Error polling GPIO. Reopening...");
					closeGPIO();
					std::this_thread::sleep_for(std::chrono::milliseconds(1000));
					openGPIO(_settings->gpio1);
				}*/
				if(pollResult > 0)
				{
					if(lseek(_gpioDescriptors[1]->descriptor, 0, SEEK_SET) == -1) throw BaseLib::Exception("Could not poll gpio: " + std::string(strerror(errno)));
					bytesRead = read(_gpioDescriptors[1]->descriptor, &readBuffer[0], 1);
					if(!bytesRead) continue;
					if(readBuffer.at(0) == 0x30)
					{
						if(!_sending) _txMutex.try_lock(); //We are receiving, don't send now
						continue; //Packet is being received. Wait for GDO high
					}
					if(_sending)
					{
						endSending();
						_txMutex.unlock();
					}
					else
					{
						//sendCommandStrobe(CommandStrobes::Enum::SIDLE);
						std::shared_ptr<BidCoSPacket> packet;
						if(crcOK())
						{
							uint8_t firstByte = readRegister(Registers::Enum::FIFO);
							std::vector<uint8_t> encodedData = readRegisters(Registers::Enum::FIFO, firstByte + 1); //Read packet + RSSI
							std::vector<uint8_t> decodedData(encodedData.size());
							if(decodedData.size() > 200)
							{
								if(!_firstPacket)
								{
									_out.printWarning("Warning: Too large packet received: " + BaseLib::HelperFunctions::getHexString(encodedData));
									closeDevice();
									_txMutex.unlock();
									continue;
								}
							}
							else if(encodedData.size() >= 9)
							{
								decodedData[0] = firstByte;
								decodedData[1] = (~encodedData[1]) ^ 0x89;
								uint32_t i = 2;
								for(; i < firstByte; i++)
								{
									decodedData[i] = (encodedData[i - 1] + 0xDC) ^ encodedData[i];
								}
								decodedData[i] = encodedData[i] ^ decodedData[2];
								decodedData[i + 1] = encodedData[i + 1]; //RSSI_DEVICE

								packet.reset(new BidCoSPacket(decodedData, true, BaseLib::HelperFunctions::getTime()));
							}
							else _out.printInfo("Info: Ignoring too small packet: " + BaseLib::HelperFunctions::getHexString(encodedData));
						}
						else _out.printDebug("Debug: BidCoS packet received, but CRC failed.");
						if(!_sendingPending)
						{
							sendCommandStrobe(CommandStrobes::Enum::SFRX);
							sendCommandStrobe(CommandStrobes::Enum::SRX);
						}
						_txMutex.unlock();
						if(packet)
						{
							if(_firstPacket) _firstPacket = false;
							else processReceivedPacket(packet);
						}
					}
				}
				else if(pollResult < 0)
				{
					_txMutex.unlock();
					_out.printError("Error: Could not poll gpio: " + std::string(strerror(errno)) + ". Reopening...");
					closeGPIO(1);
					std::this_thread::sleep_for(std::chrono::milliseconds(1000));
					openGPIO(1, true);
				}
				//pollResult == 0 is timeout
			}
			catch(const std::exception& ex)
			{
				_txMutex.unlock();
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(BaseLib::Exception& ex)
			{
				_txMutex.unlock();
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(...)
			{
				_txMutex.unlock();
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
			}
        }
    }
    catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    _txMutex.unlock();
}
}
#endif

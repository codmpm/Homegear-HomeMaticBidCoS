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

#include "../BidCoSPacket.h"
#include <homegear-base/BaseLib.h>
#include "../GD.h"
#include "Cul.h"

namespace BidCoS
{

Cul::Cul(std::shared_ptr<BaseLib::Systems::PhysicalInterfaceSettings> settings) : IBidCoSInterface(settings)
{
	_out.init(GD::bl);
	_out.setPrefix(GD::out.getPrefix() + "CUL \"" + settings->id + "\": ");

	_initComplete = true;

	if(settings->listenThreadPriority == -1)
	{
		settings->listenThreadPriority = 45;
		settings->listenThreadPolicy = SCHED_FIFO;
	}

	memset(&_termios, 0, sizeof(termios));
}

Cul::~Cul()
{
	try
	{
		_stopCallbackThread = true;
		GD::bl->threadManager.join(_listenThread);
		closeDevice();
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

void Cul::forceSendPacket(std::shared_ptr<BidCoSPacket> packet)
{
	try
	{
		if(_fileDescriptor->descriptor == -1)
		{
			_bl->out.printError("Error: Couldn't write to CUL device, because the file descriptor is not valid: " + _settings->device);
			return;
		}
		std::string packetString = packet->hexString();
		if(_bl->debugLevel >= 4) _out.printInfo("Info: Sending (" + _settings->id + "): " + packetString);
		writeToDevice("As" + packet->hexString() + "\n" + (_updateMode ? "" : "Ar\n"));
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

void Cul::enableUpdateMode()
{
	try
	{
		_updateMode = true;
		writeToDevice("AR\n");
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

void Cul::disableUpdateMode()
{
	try
	{
		stopListening();
		std::this_thread::sleep_for(std::chrono::milliseconds(2000));
		startListening();
		_updateMode = false;
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

void Cul::openDevice()
{
	try
	{
		if(_fileDescriptor->descriptor > -1) closeDevice();

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
				_out.printCritical("CUL device is in use: " + _settings->device);
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
		//std::string chmod("chmod 666 " + _lockfile);
		//system(chmod.c_str());

		_firstPacket = true;

		_fileDescriptor = _bl->fileDescriptorManager.add(open(_settings->device.c_str(), O_RDWR | O_NOCTTY));
		if(_fileDescriptor->descriptor == -1)
		{
			_out.printCritical("Couldn't open CUL device \"" + _settings->device + "\": " + strerror(errno));
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

void Cul::closeDevice()
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

void Cul::setupDevice()
{
	try
	{
		if(_fileDescriptor->descriptor == -1) return;
		memset(&_termios, 0, sizeof(termios));

		_termios.c_cflag = B38400 | CS8 | CREAD;
		_termios.c_iflag = 0;
		_termios.c_oflag = 0;
		_termios.c_lflag = 0;
		_termios.c_cc[VMIN] = 1;
		_termios.c_cc[VTIME] = 0;

		cfsetispeed(&_termios, B38400);
		cfsetospeed(&_termios, B38400);

		if(tcflush(_fileDescriptor->descriptor, TCIFLUSH) == -1) throw(BaseLib::Exception("Couldn't flush CUL device " + _settings->device));
		if(tcsetattr(_fileDescriptor->descriptor, TCSANOW, &_termios) == -1) throw(BaseLib::Exception("Couldn't set CUL device settings: " + _settings->device));

		std::this_thread::sleep_for(std::chrono::milliseconds(2000));

		int flags = fcntl(_fileDescriptor->descriptor, F_GETFL);
		if(!(flags & O_NONBLOCK))
		{
			if(fcntl(_fileDescriptor->descriptor, F_SETFL, flags | O_NONBLOCK) == -1)
			{
				throw(BaseLib::Exception("Couldn't set CUL device to non blocking mode: " + _settings->device));
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
}

std::string Cul::readFromDevice()
{
	try
	{
		if(_stopped) return "";
		if(_fileDescriptor->descriptor == -1)
		{
			_out.printCritical("Couldn't read from CUL device, because the file descriptor is not valid: " + _settings->device + ". Trying to reopen...");
			closeDevice();
			std::this_thread::sleep_for(std::chrono::milliseconds(5000));
			openDevice();
			if(!isOpen()) return "";
			if(_updateMode) writeToDevice("X21\nAR\n");
			else writeToDevice("X21\nAr\n");
		}
		std::string packet;
		int32_t i;
		char localBuffer[1] = "";
		fd_set readFileDescriptor;
		FD_ZERO(&readFileDescriptor);
		FD_SET(_fileDescriptor->descriptor, &readFileDescriptor);

		while((!_stopCallbackThread && localBuffer[0] != '\n' && _fileDescriptor->descriptor > -1))
		{
			FD_ZERO(&readFileDescriptor);
			FD_SET(_fileDescriptor->descriptor, &readFileDescriptor);
			//Timeout needs to be set every time, so don't put it outside of the while loop
			timeval timeout;
			timeout.tv_sec = 0;
			timeout.tv_usec = 500000;
			i = select(_fileDescriptor->descriptor + 1, &readFileDescriptor, NULL, NULL, &timeout);
			switch(i)
			{
				case 0: //Timeout
					if(!_stopCallbackThread) continue;
					else return "";
				case -1:
					_out.printError("Error reading from CUL device: " + _settings->device);
					return "";
				case 1:
					break;
				default:
					_out.printError("Error reading from CUL device: " + _settings->device);
					return "";
			}

			i = read(_fileDescriptor->descriptor, localBuffer, 1);
			if(i == -1)
			{
				if(errno == EAGAIN) continue;
				_out.printError("Error reading from CUL device: " + _settings->device);
				return "";
			}
			packet.push_back(localBuffer[0]);
			if(packet.size() > 200)
			{
				_out.printError("CUL was disconnected.");
				closeDevice();
				return "";
			}
		}
		return packet;
	}
	catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
	return "";
}

void Cul::writeToDevice(std::string data)
{
    try
    {
    	if(_stopped) return;
        if(_fileDescriptor->descriptor == -1) throw(BaseLib::Exception("Couldn't write to CUL device, because the file descriptor is not valid: " + _settings->device));
        int32_t bytesWritten = 0;
        int32_t i;
        _sendMutex.lock();
        while(bytesWritten < (signed)data.length())
        {
            i = write(_fileDescriptor->descriptor, data.c_str() + bytesWritten, data.length() - bytesWritten);
            if(i == -1)
            {
                if(errno == EAGAIN) continue;
                throw(BaseLib::Exception("Error writing to CUL device (3, " + std::to_string(errno) + "): " + _settings->device));
            }
            bytesWritten += i;
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
    _sendMutex.unlock();
    _lastPacketSent = BaseLib::HelperFunctions::getTime();
}

void Cul::startListening()
{
	try
	{
		stopListening();

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
		openDevice();
		if(_fileDescriptor->descriptor == -1) return;
		_stopped = false;
		writeToDevice("X21\nAr\n");
		std::this_thread::sleep_for(std::chrono::milliseconds(400));
		if(_settings->listenThreadPriority > -1) GD::bl->threadManager.start(_listenThread, true, _settings->listenThreadPriority, _settings->listenThreadPolicy, &Cul::listen, this);
		else GD::bl->threadManager.start(_listenThread, true, &Cul::listen, this);
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

void Cul::stopListening()
{
	try
	{
		IBidCoSInterface::stopListening();
		_stopCallbackThread = true;
		GD::bl->threadManager.join(_listenThread);
		_stopCallbackThread = false;
		if(_fileDescriptor->descriptor > -1)
		{
			//Other X commands than 00 seem to slow down data processing
			writeToDevice("Ax\nX00\n");
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			closeDevice();
		}
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

void Cul::listen()
{
    try
    {
        while(!_stopCallbackThread)
        {
        	if(_stopped)
        	{
        		std::this_thread::sleep_for(std::chrono::milliseconds(200));
        		if(_stopCallbackThread) return;
        		continue;
        	}
        	std::string packetHex = readFromDevice();
        	if(packetHex.size() > 200)
			{
        		if(_firstPacket) _firstPacket = false;
				else
				{
					_out.printError("Error: Too large packet received. Assuming CUL error. I'm closing and reopening device: " + packetHex);
					closeDevice();
        		}
			}
        	else if(packetHex.size() >= 21) //21 is minimal packet length (=10 bytes + CUL "A")
        	{
        		std::shared_ptr<BidCoSPacket> packet(new BidCoSPacket(packetHex, BaseLib::HelperFunctions::getTime()));
				processReceivedPacket(packet);
        	}
        	else if(!packetHex.empty())
        	{
        		if(packetHex.compare(0, 4, "LOVF") == 0) _out.printWarning("Warning: CUL with id " + _settings->id + " reached 1% limit. You need to wait, before sending is allowed again.");
        		else if(packetHex == "A") continue;
        		else
        		{
        			if(_firstPacket) _firstPacket = false;
        			else if(packetHex.size() < 21) // E. g.: A0686ECDDBBBBBAC4 (No idea where these short packets come from in my office)
        			{
                        _out.printInfo("Info: Ignoring too small packet: " + packetHex);
        			}
        		}
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
}

void Cul::setup(int32_t userID, int32_t groupID, bool setPermissions)
{
    try
    {
    	if(setPermissions) setDevicePermission(userID, groupID);
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

}

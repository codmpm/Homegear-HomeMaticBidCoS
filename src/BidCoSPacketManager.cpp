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

#include "BidCoSPacketManager.h"
#include "GD.h"

namespace BidCoS
{
BidCoSPacketInfo::BidCoSPacketInfo()
{
	time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

BidCoSPacketManager::BidCoSPacketManager()
{
	try
	{
		_stopWorkerThread = false;
		_disposing = false;
		GD::bl->threadManager.start(_workerThread, true, GD::bl->settings.workerThreadPriority(), GD::bl->settings.workerThreadPolicy(), &BidCoSPacketManager::worker, this);
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

BidCoSPacketManager::~BidCoSPacketManager()
{
	if(!_disposing) dispose();
	GD::bl->threadManager.join(_workerThread);
}

void BidCoSPacketManager::dispose(bool wait)
{
	_disposing = true;
	_stopWorkerThread = true;
}

void BidCoSPacketManager::worker()
{
	try
	{
		std::chrono::milliseconds sleepingTime(1000);
		uint32_t counter = 0;
		int32_t lastPacket;
		lastPacket = 0;
		while(!_stopWorkerThread)
		{
			try
			{
				std::this_thread::sleep_for(sleepingTime);
				if(_stopWorkerThread) return;
				if(counter > 100)
				{
					counter = 0;
					_packetMutex.lock();
					if(_packets.size() > 0)
					{
						int32_t packetsPerSecond = (_packets.size() * 1000) / sleepingTime.count();
						if(packetsPerSecond <= 0) packetsPerSecond = 1;
						int32_t timePerPacket = (GD::bl->settings.workerThreadWindow() * 10) / packetsPerSecond;
						if(timePerPacket < 10) timePerPacket = 10;
						sleepingTime = std::chrono::milliseconds(timePerPacket);
					}
					_packetMutex.unlock();
				}
				_packetMutex.lock();
				if(!_packets.empty())
				{
					std::unordered_map<int32_t, std::shared_ptr<BidCoSPacketInfo>>::iterator nextPacket = _packets.find(lastPacket);
					if(nextPacket != _packets.end())
					{
						nextPacket++;
						if(nextPacket == _packets.end()) nextPacket = _packets.begin();
					}
					else nextPacket = _packets.begin();
					lastPacket = nextPacket->first;
				}
				std::shared_ptr<BidCoSPacketInfo> packet;
				if(_packets.find(lastPacket) != _packets.end()) packet = _packets.at(lastPacket);
				_packetMutex.unlock();
				if(packet) deletePacket(lastPacket, packet->id);
				counter++;
			}
			catch(const std::exception& ex)
			{
				_packetMutex.unlock();
				GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(BaseLib::Exception& ex)
			{
				_packetMutex.unlock();
				GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(...)
			{
				_packetMutex.unlock();
				GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
			}
		}
	}
    catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

bool BidCoSPacketManager::set(int32_t address, std::shared_ptr<BidCoSPacket>& packet, int64_t time)
{
	try
	{
		if(_disposing) return false;
		_packetMutex.lock();
		if(_packets.find(address) != _packets.end())
		{
			std::shared_ptr<BidCoSPacket> oldPacket = _packets.at(address)->packet;
			if(oldPacket->equals(packet))
			{
				_packetMutex.unlock();
				return true;
			}
			_packets.erase(_packets.find(address));
		}
		_packetMutex.unlock();

		std::shared_ptr<BidCoSPacketInfo> info(new BidCoSPacketInfo());
		info->packet = packet;
		info->id = _id++;
		if(time > 0) info->time = time;
		_packetMutex.lock();
		_packets.insert(std::pair<int32_t, std::shared_ptr<BidCoSPacketInfo>>(address, info));
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    _packetMutex.unlock();
    return false;
}

void BidCoSPacketManager::deletePacket(int32_t address, uint32_t id)
{
	try
	{
		if(_disposing) return;
		_packetMutex.lock();
		if(_packets.find(address) != _packets.end() && _packets.at(address) && _packets.at(address)->id == id)
		{
			if(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() <= _packets.at(address)->time + 2000)
			{
				_packetMutex.unlock();
				return;
			}
			_packets.erase(address);
		}
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    _packetMutex.unlock();
}

std::shared_ptr<BidCoSPacket> BidCoSPacketManager::get(int32_t address)
{
	try
	{
		if(_disposing) return std::shared_ptr<BidCoSPacket>();
		_packetMutex.lock();
		//Make a copy to make sure, the element exists
		std::shared_ptr<BidCoSPacket> packet((_packets.find(address) != _packets.end()) ? _packets[address]->packet : nullptr);
		_packetMutex.unlock();
		return packet;
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    _packetMutex.unlock();
    return std::shared_ptr<BidCoSPacket>();
}

std::shared_ptr<BidCoSPacketInfo> BidCoSPacketManager::getInfo(int32_t address)
{
	try
	{
		if(_disposing) return std::shared_ptr<BidCoSPacketInfo>();
		//Make a copy to make sure, the element exists
		_packetMutex.lock();
		std::shared_ptr<BidCoSPacketInfo> info((_packets.find(address) != _packets.end()) ? _packets[address] : nullptr);
		_packetMutex.unlock();
		return info;
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    _packetMutex.unlock();
    return std::shared_ptr<BidCoSPacketInfo>();
}

void BidCoSPacketManager::keepAlive(int32_t address)
{
	try
	{
		if(_disposing) return;
		_packetMutex.lock();
		if(_packets.find(address) != _packets.end()) _packets[address]->time = BaseLib::HelperFunctions::getTime();
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    _packetMutex.unlock();
}
}
